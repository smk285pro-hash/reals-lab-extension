#include "reals/audio/DragExporter.h"
#include "reals/audio/SoundTouchProcessor.h"
#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <miniaudio.h>

namespace fs = std::filesystem;

namespace reals::audio {

namespace {

constexpr auto kTag = "DragExporter";

// 64-bit FNV-1a hash function for strings
[[nodiscard]] uint64_t fnv1a64(std::string_view s) {
    uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char c : s) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] std::string getExportDirectory(const std::string& customDir) {
    if (!customDir.empty()) {
        platform::ensureDir(customDir);
        return customDir;
    }
    const std::string dir = platform::joinPath(platform::tempDir(), "RealsLab", "drag_export");
    platform::ensureDir(dir);
    return dir;
}

// Write standard 16-bit PCM or 32-bit Float RIFF WAV file to disk
bool writeRiffWav(
    const std::string& filePath,
    const float* pcm,
    size_t totalSamples,
    int channels,
    int sampleRate,
    bool float32
) {
    if (!pcm || channels <= 0 || sampleRate <= 0) return false;

    const size_t totalFrames = totalSamples / static_cast<size_t>(channels);
    const uint16_t bitsPerSample = float32 ? 32 : 16;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
    const uint32_t dataBytes = static_cast<uint32_t>(totalFrames * blockAlign);
    const uint32_t riffChunkSize = 36 + dataBytes;

    std::vector<uint8_t> header(44);

    // RIFF header
    std::memcpy(&header[0], "RIFF", 4);
    std::memcpy(&header[4], &riffChunkSize, 4);
    std::memcpy(&header[8], "WAVE", 4);

    // fmt chunk
    std::memcpy(&header[12], "fmt ", 4);
    const uint32_t fmtSize = 16;
    std::memcpy(&header[16], &fmtSize, 4);
    const uint16_t audioFormat = float32 ? 3 : 1; // 1 = PCM, 3 = IEEE Float
    std::memcpy(&header[20], &audioFormat, 2);
    const uint16_t numChannels = static_cast<uint16_t>(channels);
    std::memcpy(&header[22], &numChannels, 2);
    const uint32_t sampleRateU32 = static_cast<uint32_t>(sampleRate);
    std::memcpy(&header[24], &sampleRateU32, 4);
    std::memcpy(&header[28], &byteRate, 4);
    std::memcpy(&header[32], &blockAlign, 2);
    std::memcpy(&header[34], &bitsPerSample, 2);

    // data chunk
    std::memcpy(&header[36], "data", 4);
    std::memcpy(&header[40], &dataBytes, 4);

    auto destPath = platform::u8path(filePath);
    std::ofstream ofs(destPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        LOG_ERROR(kTag, "Failed to open destination file for write: " + filePath);
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));

    if (float32) {
        ofs.write(reinterpret_cast<const char*>(pcm), static_cast<std::streamsize>(totalSamples * sizeof(float)));
    } else {
        std::vector<int16_t> pcm16(totalSamples);
        int16_t* dst = pcm16.data();
        for (size_t i = 0; i < totalSamples; ++i) {
            float s = pcm[i];
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
            dst[i] = static_cast<int16_t>(s * 32767.0f);
        }
        ofs.write(reinterpret_cast<const char*>(dst), static_cast<std::streamsize>(totalSamples * sizeof(int16_t)));
    }

    ofs.flush();
    return ofs.good();
}

struct CachedEntry {
    fs::file_time_type srcMtime;
    DragExportResult result;
};

static std::unordered_map<std::string, CachedEntry> s_memCache;
static std::mutex s_cacheMutex;

} // namespace

std::string DragExporter::getTempExportPath(
    const std::string& inputPath,
    const float timeRatio,
    const float pitchSemitones,
    const std::string& customOutputDir
) {
    const std::string exportDir = getExportDirectory(customOutputDir);
    const uint64_t pathHash = fnv1a64(platform::normalizePath(inputPath));

    const float clampedRatio = std::clamp(timeRatio, 0.25f, 4.0f);
    const float clampedPitch = std::clamp(pitchSemitones, -12.0f, 12.0f);

    const int ratioKey = static_cast<int>(std::round(clampedRatio * 10000.0f));
    const int pitchKey = static_cast<int>(std::round(clampedPitch * 100.0f));

    std::ostringstream ss;
    ss << "drag_" << std::hex << pathHash << "_" << std::dec << ratioKey << "_" << pitchKey << ".wav";

    return platform::joinPath(exportDir, ss.str());
}

DragExportResult DragExporter::exportTempWav(
    const std::string& inputPath,
    const DragExportOptions& options
) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    DragExportResult result;

    if (inputPath.empty()) {
        result.errorMessage = "Input path is empty";
        return result;
    }

    const float clampedRatio = std::clamp(options.timeRatio, 0.25f, 4.0f);
    const float clampedPitch = std::clamp(options.pitchSemitones, -12.0f, 12.0f);

    const std::string targetPath = getTempExportPath(inputPath, clampedRatio, clampedPitch, options.customOutputDir);
    result.renderedPath = targetPath;

    const auto targetFsPath = platform::u8path(targetPath);
    const auto srcFsPath = platform::u8path(inputPath);
    std::error_code ec;

    const auto srcMtime = fs::last_write_time(srcFsPath, ec);
    if (ec) {
        result.errorMessage = "Source file does not exist: " + inputPath;
        return result;
    }

    // 1. Fast in-memory cache lookup with mtime validation
    {
        std::lock_guard lock(s_cacheMutex);
        auto it = s_memCache.find(targetPath);
        if (it != s_memCache.end() && it->second.srcMtime == srcMtime && it->second.result.success) {
            if (fs::exists(targetFsPath, ec)) {
                DragExportResult hit = it->second.result;
                const auto endTime = std::chrono::high_resolution_clock::now();
                hit.renderTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                return hit;
            }
        }
    }

    // 2. Check deterministic disk cache
    if (fs::exists(targetFsPath, ec) && fs::file_size(targetFsPath, ec) > 44) {
        const auto targetMtime = fs::last_write_time(targetFsPath, ec);
        if (!ec && targetMtime >= srcMtime) {
                // Cache hit! Read basic format from file or probe
                std::ifstream ifs(targetFsPath, std::ios::binary);
                if (ifs.is_open()) {
                    uint8_t header[44];
                    ifs.read(reinterpret_cast<char*>(header), 44);
                    if (ifs.gcount() == 44 && std::memcmp(&header[0], "RIFF", 4) == 0) {
                        uint16_t channels = 2;
                        uint32_t sampleRate = 44100;
                        uint16_t bitsPerSample = 16;
                        uint32_t dataBytes = 0;
                        std::memcpy(&channels, &header[22], 2);
                        std::memcpy(&sampleRate, &header[24], 4);
                        std::memcpy(&bitsPerSample, &header[34], 2);
                        std::memcpy(&dataBytes, &header[40], 4);

                        const uint32_t bytesPerFrame = static_cast<uint32_t>(channels * (bitsPerSample / 8));
                        const uint32_t totalFrames = (bytesPerFrame > 0) ? (dataBytes / bytesPerFrame) : 0;

                        result.success = true;
                        result.channels = channels > 0 ? channels : 2;
                        result.sampleRate = sampleRate > 0 ? sampleRate : 44100;
                        result.durationSeconds = (result.sampleRate > 0) ? (static_cast<double>(totalFrames) / result.sampleRate) : 0.0;

                        const auto endTime = std::chrono::high_resolution_clock::now();
                        result.renderTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

                        {
                            std::lock_guard lock(s_cacheMutex);
                            s_memCache[targetPath] = CachedEntry{srcMtime, result};
                        }
                        return result;
                    }
                }
            }
        }

    // Decode source audio using miniaudio
    ma_decoder_config decConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;

#ifdef _WIN32
    const ma_result decRes = ma_decoder_init_file_w(srcFsPath.c_str(), &decConfig, &decoder);
#else
    const ma_result decRes = ma_decoder_init_file(inputPath.c_str(), &decConfig, &decoder);
#endif

    if (decRes != MA_SUCCESS) {
        result.errorMessage = "Failed to initialize decoder for " + inputPath + " (err=" + std::to_string(decRes) + ")";
        LOG_ERROR(kTag, result.errorMessage);
        return result;
    }

    const int channels = static_cast<int>(decoder.outputChannels);
    const int sampleRate = static_cast<int>(decoder.outputSampleRate);
    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    if (channels <= 0 || sampleRate <= 0 || totalFrames == 0) {
        ma_decoder_uninit(&decoder);
        result.errorMessage = "Invalid audio stream properties";
        return result;
    }

    std::vector<float> pcmBuffer(static_cast<size_t>(totalFrames * static_cast<ma_uint64>(channels)));
    ma_uint64 framesRead = 0;
    const ma_result readRes = ma_decoder_read_pcm_frames(&decoder, pcmBuffer.data(), totalFrames, &framesRead);
    ma_decoder_uninit(&decoder);

    if (readRes != MA_SUCCESS && framesRead == 0) {
        result.errorMessage = "Failed to read PCM frames from " + inputPath;
        return result;
    }

    pcmBuffer.resize(static_cast<size_t>(framesRead * static_cast<ma_uint64>(channels)));

    std::vector<float> outputPcm;
    const bool needsDsp = (std::abs(clampedRatio - 1.0f) > 0.001f || std::abs(clampedPitch) > 0.001f);

    if (needsDsp) {
        SoundTouchProcessor processor(sampleRate, channels, true);
        processor.setTimeRatio(clampedRatio);
        processor.setPitchSemitones(clampedPitch);
        outputPcm = processor.processBuffer(pcmBuffer.data(), static_cast<size_t>(framesRead));
    } else {
        outputPcm = std::move(pcmBuffer);
    }

    if (outputPcm.empty()) {
        result.errorMessage = "DSP processing returned empty buffer";
        return result;
    }

    const bool writeOk = writeRiffWav(
        targetPath,
        outputPcm.data(),
        outputPcm.size(),
        channels,
        sampleRate,
        options.forceFloat32
    );

    if (!writeOk) {
        result.errorMessage = "Failed to write rendered WAV to " + targetPath;
        return result;
    }

    const size_t outFrames = outputPcm.size() / static_cast<size_t>(channels);
    result.success = true;
    result.channels = channels;
    result.sampleRate = sampleRate;
    result.durationSeconds = static_cast<double>(outFrames) / static_cast<double>(sampleRate);

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.renderTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    {
        std::lock_guard lock(s_cacheMutex);
        s_memCache[targetPath] = CachedEntry{srcMtime, result};
    }

    LOG_INFO(kTag, "exportTempWav: rendered " + inputPath + " -> " + targetPath +
                   " in " + std::to_string(result.renderTimeMs) + "ms (ratio=" +
                   std::to_string(clampedRatio) + ", pitch=" + std::to_string(clampedPitch) + "st)");

    return result;
}

void DragExporter::cleanupTempFiles(uint64_t maxAgeSeconds, const std::string& customDir) {
    const std::string dir = getExportDirectory(customDir);
    std::error_code ec;
    const auto dirFs = platform::u8path(dir);
    if (!fs::exists(dirFs, ec) || !fs::is_directory(dirFs, ec)) return;

    const auto now = fs::file_time_type::clock::now();

    for (const auto& entry : fs::directory_iterator(dirFs, ec)) {
        if (entry.is_regular_file(ec)) {
            const auto mtime = entry.last_write_time(ec);
            if (!ec) {
                const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - mtime).count();
                if (age > static_cast<int64_t>(maxAgeSeconds)) {
                    fs::remove(entry.path(), ec);
                }
            }
        }
    }
}

} // namespace reals::audio
