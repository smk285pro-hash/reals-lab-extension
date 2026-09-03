#include "reals/audio/Engine.h"
#include "reals/audio/ITimeStretchProcessor.h"
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
    std::atomic<bool> pcmLoaded{false};
    std::vector<float> pcmData;
    bool useDevice = true;
    SoundTouchProcessor defaultProcessor{44100, 2, false};
    std::shared_ptr<ITimeStretchProcessor> customProcessor;

    ITimeStretchProcessor& getProcessor() noexcept {
        if (customProcessor) return *customProcessor;
        return defaultProcessor;
    }
    std::atomic<bool> loop{false};
    std::atomic<int> sampleRate{44100};       // Output sample rate (device / host rate)
    std::atomic<int> nativeSampleRate{44100};  // Source file's native sample rate (before any conversion)
    std::atomic<int> channels{2};
    std::atomic<float> timeRatio{1.0f};
    std::atomic<float> pitchSemitones{0.0f};
    std::atomic<float> volume{1.0f};
    std::atomic<int64_t> pendingSeekFrame{-1};

    // Last values actually applied to the SoundTouch processor on the audio thread only.
    float appliedTimeRatio = 1.0f;
    float appliedPitchSemitones = 0.0f;
    bool dspActive = false;
    std::atomic<ma_uint64> cursorFrames{0};
    std::atomic<ma_uint64> totalFrames{0};
    std::atomic<ma_uint64> loopBoundaryFrames{0};
    std::chrono::steady_clock::time_point playStartTime;
    std::atomic<bool> isFirstRead{true};
    std::vector<float> readBuffer;

    // Start Onset Micro-Fade Ramp (~3ms raised-cosine)
    std::atomic<int> startFadeRemaining{0};
    int startFadeTotal = 144;
    int startFadeOffset = 0;

    // Lock-free Seek Crossfade Buffer (~144 samples)
    static constexpr size_t kCrossfadeMaxFrames = 256;
    std::vector<float> seekOldBuffer;
    int seekFadeRemaining = 0;
    int seekFadeTotal = 144;
    int seekFadeOffset = 0;

    DspAudioSource() {
        readBuffer.resize(4096 * 8, 0.0f);
        seekOldBuffer.resize(kCrossfadeMaxFrames * 2, 0.0f);
    }

    ~DspAudioSource() {
        close();
    }

    void close() {
        pcmLoaded.store(false, std::memory_order_release);
        if (!pcmData.empty()) {
            pcmData.clear();
            pcmData.shrink_to_fit();
        }
        if (baseInited) {
            ma_data_source_uninit(&base);
            baseInited = false;
        }
        getProcessor().clear();
        dspActive = false;
        appliedTimeRatio = 1.0f;
        appliedPitchSemitones = 0.0f;
        cursorFrames.store(0, std::memory_order_relaxed);
        totalFrames.store(0, std::memory_order_relaxed);
        loopBoundaryFrames.store(0, std::memory_order_relaxed);
        pendingSeekFrame.store(-1, std::memory_order_relaxed);
        startFadeRemaining.store(0, std::memory_order_relaxed);
        startFadeOffset = 0;
        seekFadeRemaining = 0;
        seekFadeOffset = 0;
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

    if (!ds->pcmLoaded.load(std::memory_order_acquire) || ds->pcmData.empty()) {
        if (pFramesRead) *pFramesRead = 0;
        return MA_AT_END;
    }

    const int channels = ds->channels.load(std::memory_order_relaxed) > 0 ? ds->channels.load(std::memory_order_relaxed) : 2;
    constexpr float kRatioEps = 0.003f; // ~0.3% BPM tolerance (inaudible, bit-perfect bypass)
    constexpr float kPitchEps = 0.02f;  // ~2 cents tolerance

    const float desiredRatio = ds->timeRatio.load(std::memory_order_relaxed);
    const float desiredPitch = ds->pitchSemitones.load(std::memory_order_relaxed);
    const bool isNeutral = (std::abs(desiredRatio - 1.0f) < kRatioEps &&
                            std::abs(desiredPitch) < kPitchEps);

    // Apply parameter changes published by the UI thread through atomics (CRIT-03)
    if (desiredRatio != ds->appliedTimeRatio) {
        ds->getProcessor().setTimeRatio(desiredRatio);
        ds->appliedTimeRatio = desiredRatio;
    }
    if (desiredPitch != ds->appliedPitchSemitones) {
        ds->getProcessor().setPitchSemitones(desiredPitch);
        ds->appliedPitchSemitones = desiredPitch;
    }

    const size_t totalAvailable = ds->pcmData.size() / static_cast<size_t>(channels);

    // Dynamic transition from Bypass to DSP mode mid-stream
    if (!ds->dspActive && !isNeutral) {
        ds->dspActive = true;
        ds->getProcessor().clear();
        ds->getProcessor().setTimeRatio(desiredRatio);
        ds->getProcessor().setPitchSemitones(desiredPitch);
        const size_t preRollNeeded = static_cast<size_t>(std::max(ds->getProcessor().latencyFrames() * 2, 2048));
        const size_t cur = static_cast<size_t>(ds->cursorFrames.load(std::memory_order_relaxed));
        if (cur < totalAvailable) {
            const size_t avail = totalAvailable - cur;
            const size_t toFeed = std::min(preRollNeeded, avail);
            if (toFeed > 0) {
                ds->getProcessor().putSamples(&ds->pcmData[cur * static_cast<size_t>(channels)], toFeed);
                ds->cursorFrames.fetch_add(toFeed, std::memory_order_relaxed);
            }
        }
    }

    // Handle pending lock-free seek requests
    const int64_t seekFrame = ds->pendingSeekFrame.exchange(-1, std::memory_order_acq_rel);
    if (seekFrame >= 0) {
        const int sr = ds->sampleRate.load(std::memory_order_relaxed) > 0 ? ds->sampleRate.load(std::memory_order_relaxed) : 44100;
        const int fadeLen = std::clamp(static_cast<int>(sr * 0.003f), 64, static_cast<int>(DspAudioSource::kCrossfadeMaxFrames));
        ds->seekFadeTotal = fadeLen;
        ds->seekFadeRemaining = fadeLen;
        ds->seekFadeOffset = 0;

        if (isNeutral) {
            ds->dspActive = false;
        }

        const bool isBypass = !ds->dspActive;
        if (isBypass) {
            const ma_uint64 oldCursor = ds->cursorFrames.load(std::memory_order_relaxed);
            for (int i = 0; i < fadeLen; ++i) {
                const size_t f = oldCursor + static_cast<size_t>(i);
                if (f < totalAvailable) {
                    ds->seekOldBuffer[i * 2] = ds->pcmData[f * static_cast<size_t>(channels)];
                    ds->seekOldBuffer[i * 2 + 1] = ds->pcmData[f * static_cast<size_t>(channels) + (channels > 1 ? 1 : 0)];
                } else {
                    ds->seekOldBuffer[i * 2] = 0.0f;
                    ds->seekOldBuffer[i * 2 + 1] = 0.0f;
                }
            }
        } else {
            const size_t drained = ds->getProcessor().receiveSamples(ds->seekOldBuffer.data(), static_cast<size_t>(fadeLen));
            if (drained < static_cast<size_t>(fadeLen)) {
                const float lastL = drained > 0 ? ds->seekOldBuffer[(drained - 1) * 2] : 0.0f;
                const float lastR = drained > 0 ? ds->seekOldBuffer[(drained - 1) * 2 + 1] : 0.0f;
                for (size_t i = drained; i < static_cast<size_t>(fadeLen); ++i) {
                    const float decay = 1.0f - static_cast<float>(i - drained + 1) / static_cast<float>(fadeLen - drained + 1);
                    ds->seekOldBuffer[i * 2] = lastL * decay;
                    ds->seekOldBuffer[i * 2 + 1] = lastR * decay;
                }
            }
            ds->getProcessor().clear();
        }

        ds->cursorFrames.store(static_cast<ma_uint64>(seekFrame), std::memory_order_relaxed);

        // Pre-roll / prime SoundTouch pipeline in DSP mode after seek
        if (!isBypass) {
            const size_t preRollNeeded = static_cast<size_t>(std::max(ds->getProcessor().latencyFrames() * 2, 2048));
            const size_t curSeek = static_cast<size_t>(seekFrame);
            if (curSeek < totalAvailable) {
                const size_t avail = totalAvailable - curSeek;
                const size_t toFeed = std::min(preRollNeeded, avail);
                if (toFeed > 0) {
                    ds->getProcessor().putSamples(&ds->pcmData[curSeek * static_cast<size_t>(channels)], toFeed);
                    ds->cursorFrames.fetch_add(toFeed, std::memory_order_relaxed);
                }
            }
        }
    }

    float* out = reinterpret_cast<float*>(pFramesOut);

    const ma_uint64 bound = ds->loopBoundaryFrames.load(std::memory_order_relaxed);
    const ma_uint64 totalF = ds->totalFrames.load(std::memory_order_relaxed);
    const ma_uint64 effectiveLoopFrames = (bound > 0 && bound <= totalF) ? bound : totalF;

    ds->isFirstRead.store(false, std::memory_order_relaxed);

    ma_uint64 actualFramesProduced = 0;
    const bool isBypass = !ds->dspActive;

    if (isBypass) {
        // Fast-path: Direct decoder reading with zero DSP overhead, zero latency, and bit-perfect quality
        ma_uint64 framesReadTotal = 0;
        ma_uint64 guardIterations = frameCount + 2;
        while (framesReadTotal < frameCount) {
            if (guardIterations-- == 0) {
                break;
            }
            const ma_uint64 currentCursor = ds->cursorFrames.load(std::memory_order_relaxed);
            ma_uint64 framesToRead = frameCount - framesReadTotal;
            if (ds->loop.load(std::memory_order_relaxed) && effectiveLoopFrames > 0 && currentCursor + framesToRead > effectiveLoopFrames) {
                framesToRead = (currentCursor < effectiveLoopFrames) ? (effectiveLoopFrames - currentCursor) : 0;
            }

            ma_uint64 framesRead = 0;
            ma_result res = MA_SUCCESS;
            if (framesToRead > 0) {
                const size_t avail = (ds->pcmData.size() / static_cast<size_t>(channels)) > currentCursor
                    ? (ds->pcmData.size() / static_cast<size_t>(channels)) - currentCursor : 0;
                framesRead = (framesToRead < avail) ? framesToRead : avail;
                if (framesRead > 0) {
                    std::memcpy(out + framesReadTotal * channels, &ds->pcmData[currentCursor * static_cast<size_t>(channels)], framesRead * static_cast<size_t>(channels) * sizeof(float));
                    res = MA_SUCCESS;
                } else { res = MA_AT_END; }
                framesReadTotal += framesRead;
                ds->cursorFrames.fetch_add(framesRead, std::memory_order_relaxed);
            }

            if (res != MA_SUCCESS || framesRead == 0 || (effectiveLoopFrames > 0 && ds->cursorFrames.load(std::memory_order_relaxed) >= effectiveLoopFrames)) {
                if (ds->loop.load(std::memory_order_relaxed)) {
                    ds->cursorFrames.store(0, std::memory_order_relaxed);
                    if (framesRead == 0 && framesReadTotal == 0 && framesToRead == 0) {
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        if (framesReadTotal < frameCount) {
            std::memset(out + framesReadTotal * channels, 0, (frameCount - framesReadTotal) * static_cast<size_t>(channels) * sizeof(float));
        }
        actualFramesProduced = framesReadTotal;
    } else {
        // DSP mode: Time-Stretch / Pitch-Shift path via SoundTouch
        size_t totalReceived = 0;
        size_t dspGuardIterations = frameCount + 2;
        while (totalReceived < frameCount) {
            if (dspGuardIterations-- == 0) {
                break;
            }
            const size_t needed = frameCount - totalReceived;
            const size_t rec = ds->getProcessor().receiveSamples(out + totalReceived * channels, needed);
            totalReceived += rec;
            if (totalReceived >= frameCount) break;

            const ma_uint64 currentCursor = ds->cursorFrames.load(std::memory_order_relaxed);
            constexpr ma_uint64 kChunkFrames = 1024;
            ma_uint64 framesToRead = kChunkFrames;
            if (ds->loop.load(std::memory_order_relaxed) && effectiveLoopFrames > 0 && currentCursor + framesToRead > effectiveLoopFrames) {
                framesToRead = (currentCursor < effectiveLoopFrames) ? (effectiveLoopFrames - currentCursor) : 0;
            }

            ma_uint64 framesRead = 0;
            ma_result res = MA_SUCCESS;
            if (framesToRead > 0) {
                const size_t avail = (ds->pcmData.size() / static_cast<size_t>(channels)) > currentCursor
                    ? (ds->pcmData.size() / static_cast<size_t>(channels)) - currentCursor : 0;
                framesRead = (framesToRead < avail) ? framesToRead : avail;
                if (framesRead > 0) {
                    std::memcpy(ds->readBuffer.data(), &ds->pcmData[currentCursor * static_cast<size_t>(channels)], framesRead * static_cast<size_t>(channels) * sizeof(float));
                    res = MA_SUCCESS;
                } else { res = MA_AT_END; }
            }

            if (res == MA_SUCCESS && framesRead > 0) {
                ds->getProcessor().putSamples(ds->readBuffer.data(), static_cast<size_t>(framesRead));
                ds->cursorFrames.fetch_add(framesRead, std::memory_order_relaxed);
            } else {
                if (ds->loop.load(std::memory_order_relaxed)) {
                    ds->cursorFrames.store(0, std::memory_order_relaxed);
                    if (framesRead == 0 && framesToRead == 0) {
                        ma_uint64 newRead = 0;
                        const size_t avail = ds->pcmData.size() / static_cast<size_t>(channels);
                        newRead = (kChunkFrames < avail) ? kChunkFrames : avail;
                        if (newRead > 0) {
                            std::memcpy(ds->readBuffer.data(), ds->pcmData.data(), newRead * static_cast<size_t>(channels) * sizeof(float));
                            ds->getProcessor().putSamples(ds->readBuffer.data(), static_cast<size_t>(newRead));
                            ds->cursorFrames.store(newRead, std::memory_order_relaxed);
                        }
                    }
                } else {
                    ds->getProcessor().flush();
                    const size_t drained = ds->getProcessor().receiveSamples(out + totalReceived * channels, frameCount - totalReceived);
                    totalReceived += drained;
                    break;
                }
            }
        }

        if (totalReceived < frameCount) {
            std::memset(out + totalReceived * channels, 0, (frameCount - totalReceived) * static_cast<size_t>(channels) * sizeof(float));
        }
        actualFramesProduced = totalReceived;
    }

    // Apply seek crossfade if active
    if (ds->seekFadeRemaining > 0 && actualFramesProduced > 0) {
        const int toFade = std::min(ds->seekFadeRemaining, static_cast<int>(actualFramesProduced));
        for (int i = 0; i < toFade; ++i) {
            const int k = ds->seekFadeOffset + i;
            if (k < ds->seekFadeTotal) {
                const float w = 0.5f * (1.0f - std::cos(3.14159265358979323846f * static_cast<float>(k) / static_cast<float>(ds->seekFadeTotal)));
                const float oldL = ds->seekOldBuffer[k * 2];
                const float oldR = ds->seekOldBuffer[k * 2 + 1];
                out[i * channels] = (1.0f - w) * oldL + w * out[i * channels];
                if (channels > 1) {
                    out[i * channels + 1] = (1.0f - w) * oldR + w * out[i * channels + 1];
                }
            }
        }
        ds->seekFadeRemaining -= toFade;
        ds->seekFadeOffset += toFade;
    }

    // Apply onset micro-fade ramp if active
    const int startRem = ds->startFadeRemaining.load(std::memory_order_relaxed);
    if (startRem > 0 && actualFramesProduced > 0) {
        const int toFade = std::min(startRem, static_cast<int>(actualFramesProduced));
        for (int i = 0; i < toFade; ++i) {
            const int k = ds->startFadeOffset + i;
            if (k < ds->startFadeTotal) {
                const float w = 0.5f * (1.0f - std::cos(3.14159265358979323846f * static_cast<float>(k) / static_cast<float>(ds->startFadeTotal)));
                for (int c = 0; c < channels; ++c) {
                    out[i * channels + c] *= w;
                }
            }
        }
        ds->startFadeRemaining.store(startRem - toFade, std::memory_order_relaxed);
        ds->startFadeOffset += toFade;
    }

    if (pFramesRead) *pFramesRead = actualFramesProduced;
    return (actualFramesProduced > 0 || ds->loop.load(std::memory_order_relaxed)) ? MA_SUCCESS : MA_AT_END;
}

ma_result dsp_on_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds) return MA_INVALID_ARGS;
    if (!ds->pcmLoaded.load(std::memory_order_acquire)) return MA_INVALID_OPERATION;
    
    const int ch = ds->channels.load(std::memory_order_relaxed) > 0 ? ds->channels.load(std::memory_order_relaxed) : 2;
    ma_uint64 target = frameIndex;
    ma_uint64 avail = ds->pcmData.size() / static_cast<size_t>(ch);
    if (target > avail) target = avail;
    
    ds->pendingSeekFrame.store(static_cast<int64_t>(target), std::memory_order_release);
    return MA_SUCCESS;
}

ma_result dsp_on_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds) return MA_INVALID_ARGS;
    const ma_uint32 ch = static_cast<ma_uint32>(ds->channels.load(std::memory_order_relaxed) > 0 ? ds->channels.load(std::memory_order_relaxed) : 2);
    const ma_uint32 sr = static_cast<ma_uint32>(ds->sampleRate.load(std::memory_order_relaxed) > 0 ? ds->sampleRate.load(std::memory_order_relaxed) : 44100);
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
    *pCursor = ds->cursorFrames.load(std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result dsp_on_get_length(ma_data_source* pDataSource, ma_uint64* pLength) {
    auto* ds = reinterpret_cast<DspAudioSource*>(pDataSource);
    if (!ds || !pLength) return MA_INVALID_ARGS;
    *pLength = ds->totalFrames.load(std::memory_order_relaxed);
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
    float volume = 1.0f;
    float timeRatio = 1.0f;
    float pitchSemitones = 0.0f;

    mutable std::recursive_mutex stateMutex;
    TrackInfo track;
    std::vector<float> env;
    std::atomic<int> targetSampleRate{0};
};

Engine& Engine::instance() {
    static Engine inst;
    return inst;
}

void Engine::setTargetSampleRate(const int sampleRate) {
    if (!m_impl) m_impl = std::make_unique<Impl>();
    m_impl->targetSampleRate.store(sampleRate, std::memory_order_relaxed);
}

int Engine::targetSampleRate() const {
    return m_impl ? m_impl->targetSampleRate.load(std::memory_order_relaxed) : 0;
}

Engine::~Engine() {
    shutdown();
    // m_impl releases via unique_ptr (MAJ-07). Impl is complete here.
}

bool Engine::init(bool useDevice) {
    if (!m_impl)
        m_impl = std::make_unique<Impl>();
    m_impl->dspSource.useDevice = useDevice;
    if (m_impl->engineInited)
        return true;
    if (!useDevice) {
        m_impl->engineInited = true;
        return true;
    }
    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS) {
        LOG_ERROR(kTag, "ma_engine_init failed");
        return false;
    }
    m_impl->engineInited = true;
    const ma_uint32 devSr = ma_engine_get_sample_rate(&m_impl->engine);
    if (devSr > 0 && m_impl->targetSampleRate.load(std::memory_order_relaxed) == 0) {
        m_impl->targetSampleRate.store(static_cast<int>(devSr), std::memory_order_relaxed);
    }
    LOG_INFO(kTag, "engine ready, targetSr=" + std::to_string(m_impl->targetSampleRate.load(std::memory_order_relaxed)));
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
    if (m_impl->dspSource.useDevice) {
        ma_sound_stop(&m_impl->sound);
        ma_sound_uninit(&m_impl->sound);
    }
    m_impl->dspSource.close();
    m_impl->soundLoaded = false;
    m_impl->track = TrackInfo{};
    m_impl->env.clear();
}

bool Engine::playFile(const std::string& path, const bool loop, const double startFraction,
                      const PhaseAnchor& phaseAnchor, const uint64_t nominalLoopFrames) {
    LOG_INFO(kTag, "playFile: entering for path: " + path);
    if (!m_impl || !m_impl->engineInited) {
        if (!init(true)) {
            LOG_ERROR(kTag, "playFile: init() failed");
            return false;
        }
    }
    stop();

    // unique_lock (not lock_guard) so the lock can be released around the
    // phaseAnchor() callback below — see rationale at that call site.
    std::unique_lock lock(m_impl->stateMutex);

    m_impl->track = probeFile(path);
    if (m_impl->track.sampleRate <= 0) {
        LOG_ERROR(kTag, "playFile: probeFile failed for: " + path);
        return false;
    }
    LOG_INFO(kTag, "playFile: probeFile ok, sr=" + std::to_string(m_impl->track.sampleRate) +
                   " ch=" + std::to_string(m_impl->track.channels) +
                   " dur=" + std::to_string(m_impl->track.durationSeconds));
    m_impl->env.clear();

    const int targetSr = (m_impl->targetSampleRate.load(std::memory_order_relaxed) > 0)
        ? m_impl->targetSampleRate.load(std::memory_order_relaxed)
        : m_impl->track.sampleRate;
    const int nativeSr = m_impl->track.sampleRate > 0 ? m_impl->track.sampleRate : targetSr;
    const int channels = 2; // Always decode & buffer as stereo float32 to prevent mono/stereo downsample artifacts

    // Decode at NATIVE sample rate (0 = keep file's original rate).
    // Resampling is handled by SoundTouch / élastique with high-quality
    // WSOLA + sinc filtering instead of miniaudio's linear interpolation
    // which caused audible high-frequency roll-off and aliasing.
    ma_decoder_config decConfig = ma_decoder_config_init(
        ma_format_f32,
        static_cast<ma_uint32>(channels),
        static_cast<ma_uint32>(nativeSr));

#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    ma_decoder localDec{};
    const ma_result decRes = ma_decoder_init_file_w(wpath.c_str(), &decConfig, &localDec);
#else
    ma_decoder localDec{};
    const ma_result decRes = ma_decoder_init_file(path.c_str(), &decConfig, &localDec);
#endif
    if (decRes != MA_SUCCESS) {
        LOG_ERROR(kTag, "playFile: ma_decoder_init_file failed with res=" + std::to_string(decRes));
        return false;
    }
    LOG_INFO(kTag, "playFile: decoder initialized, buffering to RAM at nativeSr=" + std::to_string(nativeSr) + " (stereo)...");

    std::vector<float> tempPcm;
    const size_t estimatedFrames = static_cast<size_t>(m_impl->track.durationSeconds * nativeSr);
    tempPcm.reserve(estimatedFrames * static_cast<size_t>(channels));

    const ma_uint32 decodeChannels = static_cast<ma_uint32>(channels);
    constexpr ma_uint64 kDecodeBufFrames = 2048;
    std::vector<float> readBuf(static_cast<size_t>(kDecodeBufFrames) * decodeChannels);
    while (true) {
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&localDec, readBuf.data(), kDecodeBufFrames, &framesRead);
        if (framesRead == 0) break;
        tempPcm.insert(tempPcm.end(), readBuf.data(), readBuf.data() + framesRead * decodeChannels);
    }
    ma_decoder_uninit(&localDec);

    // ---- High-quality offline resample (native → device rate) ----
    // When the file's native sample rate differs from the host device rate,
    // resample using SoundTouch's WSOLA + 64-tap sinc anti-aliasing filter
    // instead of miniaudio's linear interpolation (which caused audible
    // high-frequency roll-off and a "compressed" sound).
    if (nativeSr != targetSr && nativeSr > 0 && targetSr > 0 && !tempPcm.empty()) {
        LOG_INFO(kTag, "playFile: resampling " + std::to_string(nativeSr) + " → " + std::to_string(targetSr) + " Hz via SoundTouch sinc AA filter");
        // SoundTouch tempo: output_frames = input_frames / tempo
        // To produce more frames (e.g. 44100 → 48000): tempo < 1.0
        // tempo = nativeSr / targetSr = 44100/48000 ≈ 0.919
        const float resampleRatio = static_cast<float>(nativeSr) / static_cast<float>(targetSr);

        SoundTouchProcessor resampler(nativeSr, channels, false);
        resampler.setTimeRatio(resampleRatio);
        resampler.setPitchSemitones(0.0f); // Pure rate conversion, no pitch change

        const size_t totalNativeFrames = tempPcm.size() / static_cast<size_t>(channels);
        // output_frames ≈ input_frames / tempo = input_frames * (targetSr / nativeSr)
        const size_t estOutputFrames = static_cast<size_t>(
            static_cast<double>(totalNativeFrames) * static_cast<double>(targetSr) / static_cast<double>(nativeSr)) + 4096;

        std::vector<float> resampledPcm;
        resampledPcm.reserve(estOutputFrames * static_cast<size_t>(channels));

        constexpr size_t kFeedChunk = 2048;
        std::vector<float> outChunk(kFeedChunk * 2 * static_cast<size_t>(channels)); // generous output buffer
        size_t inputPos = 0;

        while (inputPos < totalNativeFrames) {
            const size_t remaining = totalNativeFrames - inputPos;
            const size_t toFeed = std::min(kFeedChunk, remaining);
            resampler.putSamples(&tempPcm[inputPos * static_cast<size_t>(channels)], toFeed);
            inputPos += toFeed;

            // Drain all available output
            while (true) {
                const size_t received = resampler.receiveSamples(outChunk.data(), kFeedChunk);
                if (received == 0) break;
                resampledPcm.insert(resampledPcm.end(), outChunk.data(), outChunk.data() + received * static_cast<size_t>(channels));
            }
        }

        // Flush remaining samples from the processor pipeline
        resampler.flush();
        while (true) {
            const size_t received = resampler.receiveSamples(outChunk.data(), kFeedChunk);
            if (received == 0) break;
            resampledPcm.insert(resampledPcm.end(), outChunk.data(), outChunk.data() + received * static_cast<size_t>(channels));
        }

        LOG_INFO(kTag, "playFile: resampled " + std::to_string(totalNativeFrames) + " → " + std::to_string(resampledPcm.size() / static_cast<size_t>(channels)) + " frames");
        tempPcm = std::move(resampledPcm);
    }

    m_impl->dspSource.pcmData = std::move(tempPcm);
    m_impl->dspSource.pcmLoaded.store(true, std::memory_order_release);
    m_impl->dspSource.loop.store(loop, std::memory_order_relaxed);
    m_impl->dspSource.channels.store(channels, std::memory_order_relaxed);
    m_impl->dspSource.sampleRate.store(targetSr, std::memory_order_relaxed);
    m_impl->dspSource.nativeSampleRate.store(nativeSr, std::memory_order_relaxed);
    m_impl->dspSource.timeRatio.store(m_impl->timeRatio, std::memory_order_relaxed);
    m_impl->dspSource.pitchSemitones.store(m_impl->pitchSemitones, std::memory_order_relaxed);
    m_impl->dspSource.volume.store(m_impl->volume, std::memory_order_relaxed);
    const ma_uint64 decodedFrames = static_cast<ma_uint64>(m_impl->dspSource.pcmData.size() / static_cast<size_t>(channels));
    m_impl->dspSource.totalFrames.store(decodedFrames, std::memory_order_relaxed);
    m_impl->dspSource.loopBoundaryFrames.store(nominalLoopFrames, std::memory_order_relaxed);
    m_impl->dspSource.pendingSeekFrame.store(-1, std::memory_order_relaxed);

    m_impl->track.sampleRate = targetSr;
    m_impl->track.totalFrames = static_cast<double>(decodedFrames);
    m_impl->track.channels = channels;

    m_impl->dspSource.getProcessor().setSampleRate(targetSr);
    m_impl->dspSource.getProcessor().setChannels(channels);
    m_impl->dspSource.defaultProcessor.setLowLatencyMode(false);
    m_impl->dspSource.getProcessor().setTimeRatio(m_impl->timeRatio);
    m_impl->dspSource.getProcessor().setPitchSemitones(m_impl->pitchSemitones);
    m_impl->dspSource.getProcessor().clear();
    // Keep the audio-thread "applied" trackers in sync with the initial
    // setup so dsp_on_read does not redundantly re-apply on block 1.
    m_impl->dspSource.appliedTimeRatio = m_impl->timeRatio;
    m_impl->dspSource.appliedPitchSemitones = m_impl->pitchSemitones;

    // Phase anchor: all file I/O and DSP configuration is done at this point
    // (the SoundTouch latency query inside the anchor reflects the active
    // tempo) — re-resolve the start fraction NOW so the caller's DAW transport
    // snapshot is taken at the last possible moment, eliminating decode-time
    // phase lag. Only the seek and sound start remain after this.
    double clampedFraction = std::clamp(startFraction, 0.0, 0.999);
    if (phaseAnchor) {
        // Release stateMutex before calling out. phaseAnchor is caller-supplied
        // (e.g. it may query a host transport/tempo, such as REAPER's
        // GetPlayPosition2Ex/TimeMap_GetDividedBpmAtTime for phase sync) and we
        // have no guarantee it returns quickly or never re-enters Engine. Holding
        // stateMutex here would otherwise stall unrelated calls like
        // positionFraction()/level() that UI code may be polling concurrently.
        // dspSource/track state needed below is already fully written at this
        // point, so releasing the lock here is safe; we just need to keep the
        // *value semantics* — no other thread will start a conflicting playFile()
        // until stop()'s own locking coordinates that.
        lock.unlock();
        const double resolved = phaseAnchor(clampedFraction);
        lock.lock();
        if (std::isfinite(resolved)) {
            clampedFraction = std::clamp(resolved, 0.0, 0.999);
        }
    }

    const ma_uint64 totalF = m_impl->dspSource.totalFrames.load(std::memory_order_relaxed);
    const ma_uint64 refFrames = (nominalLoopFrames > 0 && nominalLoopFrames <= totalF)
        ? nominalLoopFrames
        : totalF;

    const ma_uint64 startFrame = (refFrames > 0 && clampedFraction > 0.0)
        ? static_cast<ma_uint64>(clampedFraction * refFrames)
        : 0;

    m_impl->dspSource.cursorFrames.store(startFrame, std::memory_order_relaxed);

    const int fadeFrames = std::clamp(static_cast<int>(targetSr * 0.003f), 64, 256);
    m_impl->dspSource.startFadeTotal = fadeFrames;
    m_impl->dspSource.startFadeRemaining.store(fadeFrames, std::memory_order_relaxed);
    m_impl->dspSource.startFadeOffset = 0;
    m_impl->dspSource.seekFadeRemaining = 0;
    m_impl->dspSource.seekFadeOffset = 0;

    // Pre-roll / prime SoundTouch pipeline on start if in DSP mode
    constexpr float kRatioEps = 0.003f;
    constexpr float kPitchEps = 0.02f;
    const bool isInitialBypass = (std::abs(m_impl->timeRatio - 1.0f) < kRatioEps &&
                                  std::abs(m_impl->pitchSemitones) < kPitchEps);
    m_impl->dspSource.dspActive = !isInitialBypass;
    m_impl->dspSource.appliedTimeRatio = m_impl->timeRatio;
    m_impl->dspSource.appliedPitchSemitones = m_impl->pitchSemitones;
    if (!isInitialBypass) {
        m_impl->dspSource.getProcessor().setTimeRatio(m_impl->timeRatio);
        m_impl->dspSource.getProcessor().setPitchSemitones(m_impl->pitchSemitones);
        const size_t preRollNeeded = static_cast<size_t>(std::max(m_impl->dspSource.getProcessor().latencyFrames() * 2, 2048));
        const size_t totalAvailable = m_impl->dspSource.pcmData.size() / static_cast<size_t>(channels);
        const size_t startF = static_cast<size_t>(startFrame);
        if (startF < totalAvailable) {
            const size_t avail = totalAvailable - startF;
            const size_t toFeed = std::min(preRollNeeded, avail);
            if (toFeed > 0) {
                m_impl->dspSource.getProcessor().putSamples(&m_impl->dspSource.pcmData[startF * static_cast<size_t>(channels)], toFeed);
                m_impl->dspSource.cursorFrames.fetch_add(toFeed, std::memory_order_relaxed);
            }
        }
    }

    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_dspDataSourceVtable;
    const ma_result dsRes = ma_data_source_init(&baseConfig, &m_impl->dspSource.base);
    if (dsRes != MA_SUCCESS) {
        m_impl->dspSource.pcmData.clear();
        m_impl->dspSource.pcmLoaded.store(false, std::memory_order_release);
        LOG_ERROR(kTag, "playFile: ma_data_source_init failed with res=" + std::to_string(dsRes));
        return false;
    }
    m_impl->dspSource.baseInited = true;
    LOG_INFO(kTag, "playFile: data source base initialized");

    if (m_impl->dspSource.useDevice) {
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
        ma_sound_set_volume(&m_impl->sound, m_impl->volume);
        if (ma_sound_start(&m_impl->sound) != MA_SUCCESS) {
            LOG_ERROR(kTag, "playFile: ma_sound_start failed");
            return false;
        }
    }

    m_impl->soundLoaded = true;
    m_impl->loop = loop;
    m_impl->dspSource.playStartTime = std::chrono::steady_clock::now();
    m_impl->dspSource.isFirstRead.store(true, std::memory_order_relaxed);

    LOG_INFO(kTag, "playFile: ready to start. refFrames=" + std::to_string(refFrames) +
                   " startFrame=" + std::to_string(startFrame) +
                   " clampedFraction=" + std::to_string(clampedFraction) +
                   " loopBoundary=" + std::to_string(m_impl->dspSource.loopBoundaryFrames.load(std::memory_order_relaxed)) +
                   " totalFrames=" + std::to_string(totalF));

    LOG_INFO(kTag, "playFile: playback active successfully!");
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
    const ma_uint64 totalF = m_impl->dspSource.totalFrames.load(std::memory_order_relaxed);
    if (!m_impl->soundLoaded || totalF == 0)
        return;
    const double f = std::clamp(fraction, 0.0, 1.0);
    const ma_uint64 bound = m_impl->dspSource.loopBoundaryFrames.load(std::memory_order_relaxed);
    const ma_uint64 refFrames = (bound > 0 && bound <= totalF) ? bound : totalF;
    const ma_uint64 frame = static_cast<ma_uint64>(f * refFrames);
    dsp_on_seek(&m_impl->dspSource.base, frame);
}

void Engine::setLoopBoundaryFrames(const uint64_t frames) {
    if (!m_impl) return;
    const std::lock_guard lock(m_impl->stateMutex);
    m_impl->dspSource.loopBoundaryFrames.store(frames, std::memory_order_relaxed);
}

uint64_t Engine::loopBoundaryFrames() const {
    if (!m_impl) return 0;
    return m_impl->dspSource.loopBoundaryFrames.load(std::memory_order_relaxed);
}

double Engine::pipelineLatencySeconds() const {
    if (!m_impl) return 0.0;
    const std::lock_guard lock(m_impl->stateMutex);
    const int sr = m_impl->dspSource.sampleRate.load(std::memory_order_relaxed);
    if (m_impl->soundLoaded && sr > 0) {
        const double frames = static_cast<double>(m_impl->dspSource.getProcessor().latencyFrames());
        if (frames > 0.0)
            return frames / static_cast<double>(sr);
    }
    return 0.0;
}

double Engine::deviceBufferLatencySeconds() const {
    if (!m_impl || !m_impl->engineInited)
        return 0.030; // default 30ms fallback
    const ma_device* pDev = ma_engine_get_device(const_cast<ma_engine*>(&m_impl->engine));
    if (pDev && pDev->playback.internalSampleRate > 0) {
        const uint32_t bufFrames = pDev->playback.internalPeriodSizeInFrames * pDev->playback.internalPeriods;
        return static_cast<double>(bufFrames) / pDev->playback.internalSampleRate;
    }
    return 0.030;
}

void Engine::setTimeRatio(const float ratio) {
    if (!m_impl) m_impl = std::make_unique<Impl>();
    const std::lock_guard lock(m_impl->stateMutex);
    constexpr float kRatioEps = 0.003f;
    float r = std::clamp(ratio, 0.1f, 10.0f);
    if (std::abs(r - 1.0f) < kRatioEps) {
        r = 1.0f; // Snap to exact 1.0f for bit-perfect bypass
    }
    m_impl->timeRatio = r;
    // Lock-free publication (CRIT-03): the audio thread applies the new ratio
    // to SoundTouch at the next block boundary — no dspMutex on this path, so
    // a UI tempo change can never block the realtime callback.
    m_impl->dspSource.timeRatio.store(m_impl->timeRatio, std::memory_order_relaxed);
}

float Engine::getTimeRatio() const {
    if (!m_impl) return 1.0f;
    const std::lock_guard lock(m_impl->stateMutex);
    return m_impl->timeRatio;
}

void Engine::setPitchSemitones(const float semitones) {
    if (!m_impl) m_impl = std::make_unique<Impl>();
    const std::lock_guard lock(m_impl->stateMutex);
    constexpr float kPitchEps = 0.02f;
    float s = std::clamp(semitones, -12.0f, 12.0f);
    if (std::abs(s) < kPitchEps) {
        s = 0.0f; // Snap to exact 0.0f for bit-perfect bypass
    }
    m_impl->pitchSemitones = s;
    // Lock-free publication (CRIT-03) — same rationale as setTimeRatio().
    m_impl->dspSource.pitchSemitones.store(m_impl->pitchSemitones, std::memory_order_relaxed);
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
    if (!m_impl || !m_impl->soundLoaded) return false;
    if (m_impl->dspSource.useDevice) {
        return ma_sound_is_playing(&m_impl->sound) == MA_TRUE;
    }
    return m_impl->soundLoaded;
}

bool Engine::isPlayingPath(const std::string& path) const {
    return m_impl && m_impl->soundLoaded && m_impl->track.path == path && isPlaying();
}

void Engine::setVolume(const float linear) {
    if (!m_impl)
        return;
    // Lock for consistency with stop()/playFile() — setVolume can race with
    // a playback switch from another thread and touch an uninitialized sound.
    const std::lock_guard lock(m_impl->stateMutex);
    m_impl->volume = std::clamp(linear, 0.0f, 1.0f);
    m_impl->dspSource.volume.store(m_impl->volume, std::memory_order_relaxed);
    if (m_impl->soundLoaded && m_impl->dspSource.useDevice)
        ma_sound_set_volume(&m_impl->sound, m_impl->volume);
}

float Engine::volume() const {
    return m_impl ? m_impl->volume : 0.0f;
}

void Engine::setLoop(const bool loop) {
    if (!m_impl)
        return;
    const std::lock_guard lock(m_impl->stateMutex);
    m_impl->loop = loop;
    // Lock-free publication (CRIT-03): the audio callback reads this atomic
    // directly, so toggling loop mode cannot block the realtime path.
    m_impl->dspSource.loop.store(loop, std::memory_order_relaxed);
}

bool Engine::loop() const {
    return m_impl && m_impl->loop;
}

LevelState Engine::level() const {
    LevelState st;
    if (!m_impl)
        return st;
    const std::lock_guard lock(m_impl->stateMutex);
    const ma_uint64 totalF = m_impl->dspSource.totalFrames.load(std::memory_order_relaxed);
    if (!m_impl->soundLoaded || m_impl->env.empty() || totalF == 0)
        return st;

    const double frac = static_cast<double>(m_impl->dspSource.cursorFrames.load(std::memory_order_relaxed)) / static_cast<double>(totalF);
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
    const ma_uint64 totalF = m_impl->dspSource.totalFrames.load(std::memory_order_relaxed);
    if (!m_impl->soundLoaded || totalF == 0)
        return 0.0;
    const ma_uint64 bound = m_impl->dspSource.loopBoundaryFrames.load(std::memory_order_relaxed);
    const double denom = (bound > 0 && bound <= totalF)
        ? static_cast<double>(bound)
        : static_cast<double>(totalF);
    const int64_t pending = m_impl->dspSource.pendingSeekFrame.load(std::memory_order_acquire);
    const ma_uint64 cur = (pending >= 0)
        ? static_cast<ma_uint64>(pending)
        : m_impl->dspSource.cursorFrames.load(std::memory_order_relaxed);
    return denom > 0.0 ? (static_cast<double>(cur) / denom) : 0.0;
}

void Engine::renderFrames(float* outL, float* outR, size_t frames) {
    if (!outL && !outR) return;
    if (frames == 0) return;

    if (!m_impl || !m_impl->soundLoaded || !m_impl->dspSource.pcmLoaded.load(std::memory_order_acquire) || m_impl->dspSource.useDevice) {
        if (outL) std::memset(outL, 0, frames * sizeof(float));
        if (outR) std::memset(outR, 0, frames * sizeof(float));
        return;
    }

    constexpr size_t kMaxBlockFrames = 8192;
    static thread_local float interleaved[kMaxBlockFrames * 2];

    size_t framesRemaining = frames;
    size_t frameOffset = 0;
    const float vol = m_impl->dspSource.volume.load(std::memory_order_relaxed);

    while (framesRemaining > 0) {
        const size_t chunk = std::min(framesRemaining, kMaxBlockFrames);
        ma_uint64 framesRead = 0;
        dsp_on_read(&m_impl->dspSource.base, interleaved, chunk, &framesRead);

        if (framesRead < chunk) {
            std::memset(interleaved + framesRead * 2, 0, (chunk - framesRead) * 2 * sizeof(float));
        }

        auto transparentLimit = [](float s) noexcept -> float {
            const float absS = std::fabs(s);
            if (absS <= 1.0f) return s; // 100% bit-exact uncompressed pass for all normal audio within 0 dBFS
            return std::copysign(1.0f - 0.0001f / (1.0f + (absS - 1.0f)), s);
        };

        if (outL && outR) {
            for (size_t i = 0; i < chunk; ++i) {
                outL[frameOffset + i] = transparentLimit(interleaved[i * 2] * vol);
                outR[frameOffset + i] = transparentLimit(interleaved[i * 2 + 1] * vol);
            }
        } else if (outL) {
            for (size_t i = 0; i < chunk; ++i) {
                outL[frameOffset + i] = transparentLimit(0.5f * (interleaved[i * 2] + interleaved[i * 2 + 1]) * vol);
            }
        } else if (outR) {
            for (size_t i = 0; i < chunk; ++i) {
                outR[frameOffset + i] = transparentLimit(0.5f * (interleaved[i * 2] + interleaved[i * 2 + 1]) * vol);
            }
        }

        frameOffset += chunk;
        framesRemaining -= chunk;
    }
}

void Engine::setTimeStretchProcessor(std::shared_ptr<ITimeStretchProcessor> proc) {
    if (!m_impl) m_impl = std::make_unique<Impl>();
    std::lock_guard lock(m_impl->stateMutex);
    m_impl->dspSource.customProcessor = std::move(proc);
}

} // namespace reals::audio