#include "reals/audio/Engine.h"
#include "reals/audio/SoundTouchProcessor.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/TempoDetector.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <regex>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace reals::audio {

namespace {
constexpr auto kTag = "audio";
constexpr size_t kEnvBuckets = 180;
constexpr ma_uint64 kFastChunk = 65536;

#ifdef _WIN32
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    w.pop_back();
    return w;
}
#endif

struct DspAudioSource {
    ma_data_source_base base{};
    bool baseInited = false;
    ma_decoder decoder{};
    bool decoderInited = false;
    SoundTouchProcessor processor{44100, 2, true};
    mutable std::recursive_mutex dspMutex;
    bool loop = false;
    int sampleRate = 44100;
    int channels = 2;
    std::atomic<float> timeRatio{1.0f};
    std::atomic<float> pitchSemitones{0.0f};
    std::atomic<ma_uint64> cursorFrames{0};
    std::atomic<ma_uint64> totalFrames{0};
    std::vector<float> readBuffer;

    DspAudioSource() {
        readBuffer.resize(4096 * 8, 0.0f);
    }

    ~DspAudioSource() {
        close();
    }

    void close() {
        std::lock_guard lock(dspMutex);
        if (decoderInited) {
            ma_decoder_uninit(&decoder);
            decoderInited = false;
        }
        if (baseInited) {
            ma_data_source_uninit(&base);
            baseInited = false;
        }
        processor.clear();
        cursorFrames.store(0);
        totalFrames.store(0);
    }
};

ma_result dsp_on_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds || !pFramesOut) {
        if (pFramesRead) *pFramesRead = 0;
        return MA_INVALID_ARGS;
    }
    if (frameCount == 0) {
        if (pFramesRead) *pFramesRead = 0;
        return MA_SUCCESS;
    }

    std::lock_guard lock(ds->dspMutex);
    if (!ds->decoderInited) {
        if (pFramesRead) *pFramesRead = 0;
        return MA_AT_END;
    }

    float* out = reinterpret_cast<float*>(pFramesOut);
    const int channels = (ds->channels > 0) ? ds->channels : 2;
    const bool isBypass = (std::abs(ds->timeRatio.load() - 1.0f) < 0.001f && std::abs(ds->pitchSemitones.load()) < 0.01f);

    if (isBypass) {
        // Fast-path: Direct decoder reading with zero DSP overhead, zero latency, and bit-perfect quality
        ma_uint64 framesReadTotal = 0;
        while (framesReadTotal < frameCount) {
            const ma_uint64 framesToRead = frameCount - framesReadTotal;
            ma_uint64 framesRead = 0;
            const ma_result res = ma_decoder_read_pcm_frames(
                &ds->decoder,
                out + framesReadTotal * channels,
                framesToRead,
                &framesRead);
            framesReadTotal += framesRead;
            ds->cursorFrames.fetch_add(framesRead);

            if (res != MA_SUCCESS || framesRead == 0) {
                if (ds->loop) {
                    ma_decoder_seek_to_pcm_frame(&ds->decoder, 0);
                    ds->cursorFrames.store(0);
                    if (framesRead == 0 && framesReadTotal == 0) {
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        if (framesReadTotal < frameCount) {
            std::memset(out + framesReadTotal * channels, 0, (frameCount - framesReadTotal) * channels * sizeof(float));
        }

        if (pFramesRead) *pFramesRead = framesReadTotal;
        return (framesReadTotal > 0 || ds->loop) ? MA_SUCCESS : MA_AT_END;
    }

    // DSP mode: Time-Stretch / Pitch-Shift path via SoundTouch
    size_t totalReceived = 0;
    while (totalReceived < frameCount) {
        const size_t needed = frameCount - totalReceived;
        const size_t rec = ds->processor.receiveSamples(out + totalReceived * channels, needed);
        totalReceived += rec;
        if (totalReceived >= frameCount) break;

        constexpr ma_uint64 kChunkFrames = 1024;
        if (ds->readBuffer.size() < kChunkFrames * static_cast<size_t>(channels)) {
            ds->readBuffer.resize(kChunkFrames * static_cast<size_t>(channels));
        }

        ma_uint64 framesRead = 0;
        const ma_result res = ma_decoder_read_pcm_frames(&ds->decoder, ds->readBuffer.data(), kChunkFrames, &framesRead);
        if (res == MA_SUCCESS && framesRead > 0) {
            ds->processor.putSamples(ds->readBuffer.data(), static_cast<size_t>(framesRead));
            ds->cursorFrames.fetch_add(framesRead);
        } else {
            if (ds->loop) {
                ma_decoder_seek_to_pcm_frame(&ds->decoder, 0);
                ds->cursorFrames.store(0);
                if (framesRead == 0) {
                    ds->processor.flush();
                    const size_t drained = ds->processor.receiveSamples(out + totalReceived * channels, frameCount - totalReceived);
                    totalReceived += drained;
                    break;
                }
            } else {
                ds->processor.flush();
                const size_t drained = ds->processor.receiveSamples(out + totalReceived * channels, frameCount - totalReceived);
                totalReceived += drained;
                break;
            }
        }
    }

    if (totalReceived < frameCount) {
        std::memset(out + totalReceived * channels, 0, (frameCount - totalReceived) * channels * sizeof(float));
    }

    if (pFramesRead) *pFramesRead = totalReceived;
    return (totalReceived > 0 || ds->loop) ? MA_SUCCESS : MA_AT_END;
}

ma_result dsp_on_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds) return MA_INVALID_ARGS;
    std::lock_guard lock(ds->dspMutex);
    if (!ds->decoderInited) return MA_INVALID_OPERATION;
    const ma_result res = ma_decoder_seek_to_pcm_frame(&ds->decoder, frameIndex);
    if (res == MA_SUCCESS) {
        ds->processor.clear();
        ds->cursorFrames.store(frameIndex);
    }
    return res;
}

ma_result dsp_on_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds) return MA_INVALID_ARGS;
    const ma_uint32 ch = static_cast<ma_uint32>((ds->channels > 0) ? ds->channels : 2);
    const ma_uint32 sr = static_cast<ma_uint32>((ds->sampleRate > 0) ? ds->sampleRate : 44100);
    if (pFormat) *pFormat = ma_format_f32;
    if (pChannels) *pChannels = ch;
    if (pSampleRate) *pSampleRate = sr;
    if (pChannelMap && channelMapCap > 0) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, ch);
    }
    return MA_SUCCESS;
}

ma_result dsp_on_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds || !pCursor) return MA_INVALID_ARGS;
    *pCursor = ds->cursorFrames.load();
    return MA_SUCCESS;
}

ma_result dsp_on_get_length(ma_data_source* pDataSource, ma_uint64* pLength) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds || !pLength) return MA_INVALID_ARGS;
    *pLength = ds->totalFrames.load();
    return MA_SUCCESS;
}

static ma_data_source_vtable g_dspDataSourceVtable = {
    dsp_on_read,
    dsp_on_seek,
    dsp_on_get_data_format,
    dsp_on_get_cursor,
    dsp_on_get_length,
    nullptr,
    0
};

} // namespace

struct Engine::Impl {
    ma_engine engine{};
    bool engineInited = false;

    ma_sound sound{};
    bool soundLoaded = false;
    DspAudioSource dspSource;

    bool loop = false;
    float volume = 0.9f;
    float timeRatio = 1.0f;
    float pitchSemitones = 0.0f;

    mutable std::recursive_mutex stateMutex;
    TrackInfo track;
    std::vector<float> env;
};

Engine& Engine::instance() {
    static Engine inst;
    return inst;
}

Engine::~Engine() {
    shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool Engine::init() {
    if (!m_impl)
        m_impl = new Impl();
    if (m_impl->engineInited)
        return true;
    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS) {
        LOG_ERROR(kTag, "ma_engine_init failed");
        return false;
    }
    m_impl->engineInited = true;
    LOG_INFO(kTag, "engine ready");
    return true;
}

void Engine::shutdown() {
    if (!m_impl)
        return;
    stop();
    if (m_impl->engineInited) {
        ma_engine_uninit(&m_impl->engine);
        m_impl->engineInited = false;
    }
}

bool Engine::isReady() const {
    return m_impl && m_impl->engineInited;
}

void Engine::stop() {
    if (!m_impl)
        return;
    const std::lock_guard lock(m_impl->stateMutex);
    if (!m_impl->soundLoaded)
        return;
    ma_sound_stop(&m_impl->sound);
    ma_sound_uninit(&m_impl->sound);
    m_impl->dspSource.close();
    m_impl->soundLoaded = false;
    m_impl->track = TrackInfo{};
    m_impl->env.clear();
}

bool Engine::playFile(const std::string& path, const bool loop, const double startFraction) {
    LOG_INFO(kTag, "playFile: entering for path: " + path);
    if (!init()) {
        LOG_ERROR(kTag, "playFile: init() failed");
        return false;
    }
    stop();

    const std::lock_guard lock(m_impl->stateMutex);

    m_impl->track = probeFile(path);
    if (m_impl->track.sampleRate <= 0) {
        LOG_ERROR(kTag, "playFile: probeFile failed for: " + path);
        return false;
    }
    LOG_INFO(kTag, "playFile: probeFile ok, sr=" + std::to_string(m_impl->track.sampleRate) +
                   " ch=" + std::to_string(m_impl->track.channels) +
                   " dur=" + std::to_string(m_impl->track.durationSeconds));
    m_impl->env.clear();

    ma_decoder_config decConfig = ma_decoder_config_init(
        ma_format_f32,
        static_cast<ma_uint32>(m_impl->track.channels),
        static_cast<ma_uint32>(m_impl->track.sampleRate));

#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    const ma_result decRes = ma_decoder_init_file_w(wpath.c_str(), &decConfig, &m_impl->dspSource.decoder);
#else
    const ma_result decRes = ma_decoder_init_file(path.c_str(), &decConfig, &m_impl->dspSource.decoder);
#endif
    if (decRes != MA_SUCCESS) {
        LOG_ERROR(kTag, "playFile: ma_decoder_init_file failed with res=" + std::to_string(decRes));
        return false;
    }
    LOG_INFO(kTag, "playFile: decoder initialized");

    const double clampedFraction = std::clamp(startFraction, 0.0, 0.999);
    const ma_uint64 startFrame = (m_impl->track.totalFrames > 0 && clampedFraction > 0.0)
        ? static_cast<ma_uint64>(clampedFraction * m_impl->track.totalFrames)
        : 0;

    if (startFrame > 0) {
        ma_decoder_seek_to_pcm_frame(&m_impl->dspSource.decoder, startFrame);
    }

    m_impl->dspSource.decoderInited = true;
    m_impl->dspSource.loop = loop;
    m_impl->dspSource.channels = m_impl->track.channels;
    m_impl->dspSource.sampleRate = m_impl->track.sampleRate;
    m_impl->dspSource.timeRatio.store(m_impl->timeRatio);
    m_impl->dspSource.pitchSemitones.store(m_impl->pitchSemitones);
    m_impl->dspSource.cursorFrames.store(startFrame);
    m_impl->dspSource.totalFrames.store(static_cast<ma_uint64>(m_impl->track.totalFrames));
    {
        std::lock_guard dspLock(m_impl->dspSource.dspMutex);
        m_impl->dspSource.processor.setSampleRate(m_impl->track.sampleRate);
        m_impl->dspSource.processor.setChannels(m_impl->track.channels);
        m_impl->dspSource.processor.setLowLatencyMode(true);
        m_impl->dspSource.processor.setTimeRatio(m_impl->timeRatio);
        m_impl->dspSource.processor.setPitchSemitones(m_impl->pitchSemitones);
        m_impl->dspSource.processor.clear();
    }

    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_dspDataSourceVtable;
    const ma_result dsRes = ma_data_source_init(&baseConfig, &m_impl->dspSource.base);
    if (dsRes != MA_SUCCESS) {
        ma_decoder_uninit(&m_impl->dspSource.decoder);
        m_impl->dspSource.decoderInited = false;
        LOG_ERROR(kTag, "playFile: ma_data_source_init failed with res=" + std::to_string(dsRes));
        return false;
    }
    m_impl->dspSource.baseInited = true;
    LOG_INFO(kTag, "playFile: data source base initialized");

    const ma_result soundRes = ma_sound_init_from_data_source(
        &m_impl->engine,
        &m_impl->dspSource.base,
        0,
        nullptr,
        &m_impl->sound);
    if (soundRes != MA_SUCCESS) {
        m_impl->dspSource.close();
        LOG_ERROR(kTag, "playFile: ma_sound_init_from_data_source failed with res=" + std::to_string(soundRes));
        return false;
    }
    LOG_INFO(kTag, "playFile: sound initialized from data source");

    m_impl->soundLoaded = true;
    m_impl->loop = loop;
    ma_sound_set_volume(&m_impl->sound, m_impl->volume);
    if (ma_sound_start(&m_impl->sound) != MA_SUCCESS) {
        LOG_ERROR(kTag, "playFile: ma_sound_start failed");
        return false;
    }
    LOG_INFO(kTag, "playFile: sound started successfully!");
    return true;
}

std::vector<float> Engine::computeEnvelope(const std::string& path) {
    std::vector<float> env(kEnvBuckets, 0.0f);
    ma_decoder dec{};
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    const ma_result decRes = ma_decoder_init_file_w(wpath.c_str(), &config, &dec);
#else
    const ma_result decRes = ma_decoder_init_file(path.c_str(), &config, &dec);
#endif
    if (decRes != MA_SUCCESS)
        return env;

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &totalFrames);

    if (totalFrames > 0 && dec.outputChannels > 0) {
        std::vector<float> buf(static_cast<size_t>(kFastChunk) * dec.outputChannels);
        ma_uint64 pos = 0;
        ma_uint64 read = 0;
        while (ma_decoder_read_pcm_frames(&dec, buf.data(), kFastChunk, &read) == MA_SUCCESS &&
               read > 0) {
            const size_t framesRead = static_cast<size_t>(read);
            const size_t channels = static_cast<size_t>(dec.outputChannels);
            
            for (size_t f = 0; f < framesRead; ++f) {
                float samplePeak = 0.0f;
                for (size_t ch = 0; ch < channels; ++ch) {
                    const float s = std::fabs(buf[f * channels + ch]);
                    if (s > samplePeak)
                        samplePeak = s;
                }
                const size_t b = std::min(kEnvBuckets - 1, static_cast<size_t>((pos + f) * kEnvBuckets / totalFrames));
                if (samplePeak > env[b])
                    env[b] = samplePeak;
            }
            pos += read;
        }
    }
    ma_decoder_uninit(&dec);

    float maxPeak = 0.0f;
    for (const float v : env) {
        if (v > maxPeak)
            maxPeak = v;
    }
    if (maxPeak > 0.01f) {
        const float scale = 0.95f / maxPeak;
        for (float& v : env) {
            v *= scale;
        }
    }

    return env;
}

void Engine::setEnvelope(const std::string& path, const std::vector<float>& env) {
    if (!m_impl)
        return;
    const std::lock_guard lock(m_impl->stateMutex);
    if (m_impl->track.path == path) {
        m_impl->env = env;
    }
}

TrackInfo Engine::probeFile(const std::string& path) {
    TrackInfo info;
    info.path = path;
    ma_decoder dec{};
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    const ma_result decRes = ma_decoder_init_file_w(wpath.c_str(), nullptr, &dec);
#else
    const ma_result decRes = ma_decoder_init_file(path.c_str(), nullptr, &dec);
#endif
    if (decRes != MA_SUCCESS)
        return info;
    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &totalFrames);
    info.sampleRate = dec.outputSampleRate;
    info.channels = dec.outputChannels;
    info.totalFrames = static_cast<double>(totalFrames);
    info.durationSeconds =
        dec.outputSampleRate > 0 ? static_cast<double>(totalFrames) / dec.outputSampleRate : 0.0;
    ma_decoder_uninit(&dec);
    return info;
}

float Engine::detectBpm(const std::string& path) {
    // Decode up to 30s mono float and run TempoDetector
    constexpr ma_uint64 kMaxFrames = 44100 * 30;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    ma_decoder dec{};
    if (ma_decoder_init_file_w(wpath.c_str(), &cfg, &dec) != MA_SUCCESS)
        return 0.0f;
#else
    ma_decoder dec{};
    if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS)
        return 0.0f;
#endif
    const int sr = dec.outputSampleRate > 0 ? dec.outputSampleRate : 44100;
    std::vector<float> pcm;
    pcm.reserve(static_cast<size_t>(std::min<ma_uint64>(kMaxFrames, 44100 * 10)));
    std::vector<float> chunk(4096);
    ma_uint64 totalRead = 0;
    while (totalRead < kMaxFrames) {
        ma_uint64 framesToRead = std::min<ma_uint64>(chunk.size(), kMaxFrames - totalRead);
        ma_uint64 framesRead = 0;
        ma_result r = ma_decoder_read_pcm_frames(&dec, chunk.data(), framesToRead, &framesRead);
        if (r != MA_SUCCESS || framesRead == 0) break;
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + framesRead);
        totalRead += framesRead;
    }
    ma_decoder_uninit(&dec);
    if (pcm.size() < 4096)
        return 0.0f;
    auto res = ai::TempoDetector::detect(pcm.data(), pcm.size(), sr);
    if (res.confidence < 0.25f || res.bpm < 40.0f || res.bpm > 250.0f)
        return 0.0f;
    return res.bpm;
}

std::string Engine::detectKey(const std::string& path) {
    constexpr ma_uint64 kMaxFrames = 44100 * 30;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    ma_decoder dec{};
    if (ma_decoder_init_file_w(wpath.c_str(), &cfg, &dec) != MA_SUCCESS)
        return {};
#else
    ma_decoder dec{};
    if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS)
        return {};
#endif
    const int sr = dec.outputSampleRate > 0 ? dec.outputSampleRate : 44100;
    std::vector<float> pcm;
    pcm.reserve(static_cast<size_t>(std::min<ma_uint64>(kMaxFrames, 44100 * 10)));
    std::vector<float> chunk(4096);
    ma_uint64 totalRead = 0;
    while (totalRead < kMaxFrames) {
        ma_uint64 framesToRead = std::min<ma_uint64>(chunk.size(), kMaxFrames - totalRead);
        ma_uint64 framesRead = 0;
        ma_result r = ma_decoder_read_pcm_frames(&dec, chunk.data(), framesToRead, &framesRead);
        if (r != MA_SUCCESS || framesRead == 0) break;
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + framesRead);
        totalRead += framesRead;
    }
    ma_decoder_uninit(&dec);
    if (pcm.size() < 4096) return {};
    auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), sr);
    if (res.confidence < 0.4f || res.key.empty()) return {};
    if (!res.mode.empty() && res.mode != "major" && res.mode != "minor") {
        return res.key;
    }
    if (res.mode == "minor" || res.mode == "Minor") return res.key + "m";
    if (res.mode == "major" || res.mode == "Major") return res.key;
    return res.key;
}

void Engine::seekFraction(const double fraction) {
    if (!m_impl)
        return;
    const std::lock_guard lock(m_impl->stateMutex);
    if (!m_impl->soundLoaded || m_impl->track.totalFrames <= 0)
        return;
    const double f = std::clamp(fraction, 0.0, 1.0);
    const ma_uint64 frame = static_cast<ma_uint64>(f * m_impl->track.totalFrames);
    ma_data_source_seek_to_pcm_frame(&m_impl->dspSource.base, frame);
}

void Engine::setTimeRatio(const float ratio) {
    if (!m_impl) m_impl = new Impl();
    const std::lock_guard lock(m_impl->stateMutex);
    m_impl->timeRatio = std::clamp(ratio, 0.1f, 10.0f);
    m_impl->dspSource.timeRatio.store(m_impl->timeRatio);
    if (m_impl->soundLoaded) {
        std::lock_guard dspLock(m_impl->dspSource.dspMutex);
        m_impl->dspSource.processor.setTimeRatio(m_impl->timeRatio);
    }
}

float Engine::getTimeRatio() const {
    if (!m_impl) return 1.0f;
    const std::lock_guard lock(m_impl->stateMutex);
    return m_impl->timeRatio;
}

void Engine::setPitchSemitones(const float semitones) {
    if (!m_impl) m_impl = new Impl();
    const std::lock_guard lock(m_impl->stateMutex);
    m_impl->pitchSemitones = std::clamp(semitones, -12.0f, 12.0f);
    m_impl->dspSource.pitchSemitones.store(m_impl->pitchSemitones);
    if (m_impl->soundLoaded) {
        std::lock_guard dspLock(m_impl->dspSource.dspMutex);
        m_impl->dspSource.processor.setPitchSemitones(m_impl->pitchSemitones);
    }
}

float Engine::getPitchSemitones() const {
    if (!m_impl) return 0.0f;
    const std::lock_guard lock(m_impl->stateMutex);
    return m_impl->pitchSemitones;
}

void Engine::resetPitch() {
    setPitchSemitones(0.0f);
}

void Engine::setOriginalKey() {
    resetPitch();
}

void Engine::toggle(const std::string& path, const bool loop) {
    if (isPlayingPath(path))
        stop();
    else
        playFile(path, loop);
}

bool Engine::isPlaying() const {
    return m_impl && m_impl->soundLoaded && ma_sound_is_playing(&m_impl->sound) == MA_TRUE;
}

bool Engine::isPlayingPath(const std::string& path) const {
    return m_impl && m_impl->soundLoaded && m_impl->track.path == path && isPlaying();
}

void Engine::setVolume(const float linear) {
    if (!m_impl)
        return;
    m_impl->volume = std::clamp(linear, 0.0f, 1.0f);
    if (m_impl->soundLoaded)
        ma_sound_set_volume(&m_impl->sound, m_impl->volume);
}

float Engine::volume() const {
    return m_impl ? m_impl->volume : 0.0f;
}

void Engine::setLoop(const bool loop) {
    if (!m_impl)
        return;
    m_impl->loop = loop;
    if (m_impl->soundLoaded) {
        std::lock_guard dspLock(m_impl->dspSource.dspMutex);
        m_impl->dspSource.loop = loop;
    }
}

bool Engine::loop() const {
    return m_impl && m_impl->loop;
}

LevelState Engine::level() const {
    LevelState st;
    if (!m_impl)
        return st;
    const std::lock_guard lock(m_impl->stateMutex);
    if (!m_impl->soundLoaded || m_impl->env.empty())
        return st;

    const double frac = (!m_impl->soundLoaded || m_impl->track.totalFrames <= 0) ? 0.0
        : static_cast<double>(m_impl->dspSource.cursorFrames.load()) / m_impl->track.totalFrames;
    const size_t idx =
        std::min(m_impl->env.size() - 1, static_cast<size_t>(frac * m_impl->env.size()));
    st.peak = m_impl->env[idx];

    float sum = 0.0f;
    int count = 0;
    for (int d = -3; d <= 3; ++d) {
        const size_t i = std::min(m_impl->env.size() - 1,
                                  static_cast<size_t>(std::max(0, static_cast<int>(idx) + d)));
        sum += m_impl->env[i];
        ++count;
    }
    st.rms = sum / static_cast<float>(count);
    return st;
}

const TrackInfo& Engine::currentTrack() const {
    static TrackInfo empty;
    if (!m_impl) return empty;
    const std::lock_guard lock(m_impl->stateMutex);
    return m_impl->track;
}

const std::vector<float>& Engine::envelope() const {
    static const std::vector<float> empty;
    if (!m_impl) return empty;
    const std::lock_guard lock(m_impl->stateMutex);
    return m_impl->env;
}

double Engine::positionFraction() const {
    if (!m_impl)
        return 0.0;
    const std::lock_guard lock(m_impl->stateMutex);
    if (!m_impl->soundLoaded || m_impl->track.totalFrames <= 0)
        return 0.0;
    return static_cast<double>(m_impl->dspSource.cursorFrames.load()) / m_impl->track.totalFrames;
}

} // namespace reals::audio
