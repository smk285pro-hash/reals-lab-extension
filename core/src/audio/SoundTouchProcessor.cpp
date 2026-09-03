#include "reals/audio/SoundTouchProcessor.h"

#include <SoundTouch.h>
#include <algorithm>
#include <cmath>

namespace reals::audio {

struct SoundTouchProcessor::Impl {
    soundtouch::SoundTouch st;
    int sampleRate = 44100;
    int channels = 2;
    float timeRatio = 1.0f;
    float pitchSemitones = 0.0f;
    bool lowLatency = false;

    void applyLowLatencySettings() {
        st.setSetting(SETTING_USE_AA_FILTER, 1);
        st.setSetting(SETTING_USE_QUICKSEEK, 0);
        if (lowLatency) {
            // Low-latency profile: sequence = 20ms, seek window = 8ms, overlap = 6ms, aa = 32
            // Pipeline latency ~28ms at 44.1kHz (< 30ms requirement)
            st.setSetting(SETTING_SEQUENCE_MS, 20);
            st.setSetting(SETTING_SEEKWINDOW_MS, 8);
            st.setSetting(SETTING_OVERLAP_MS, 6);
            st.setSetting(SETTING_AA_FILTER_LENGTH, 32);
        } else {
            // Studio Master profile: optimal for full acoustic clarity and punchy transient preservation.
            // sequenceMs = 0 enables SoundTouch dynamic automatic sequence scaling with tempo.
            // 18ms seek window provides accurate phase correlation across 45-65Hz fundamental.
            // 8ms overlap ensures smooth raised-cosine crossfade without destructive cancellation.
            st.setSetting(SETTING_SEQUENCE_MS, 0);
            st.setSetting(SETTING_SEEKWINDOW_MS, 18);
            st.setSetting(SETTING_OVERLAP_MS, 8);
            st.setSetting(SETTING_AA_FILTER_LENGTH, 64);
        }
    }
};

SoundTouchProcessor::SoundTouchProcessor(const int sampleRate, const int channels, const bool lowLatency)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->sampleRate = sampleRate > 0 ? sampleRate : 44100;
    m_impl->channels = channels > 0 ? channels : 2;
    m_impl->lowLatency = lowLatency;

    m_impl->st.setSampleRate(static_cast<uint>(m_impl->sampleRate));
    m_impl->st.setChannels(static_cast<uint>(m_impl->channels));
    m_impl->applyLowLatencySettings();
    m_impl->st.setTempo(1.0f);
    m_impl->st.setPitchSemiTones(0.0f);
}

SoundTouchProcessor::~SoundTouchProcessor() = default;
SoundTouchProcessor::SoundTouchProcessor(SoundTouchProcessor&&) noexcept = default;
SoundTouchProcessor& SoundTouchProcessor::operator=(SoundTouchProcessor&&) noexcept = default;

void SoundTouchProcessor::setSampleRate(const int sampleRate) {
    if (sampleRate <= 0 || sampleRate == m_impl->sampleRate) return;
    m_impl->sampleRate = sampleRate;
    m_impl->st.setSampleRate(static_cast<uint>(sampleRate));
    m_impl->applyLowLatencySettings();
}

void SoundTouchProcessor::setSampleRates(const int inSampleRate, const int /*outSampleRate*/) {
    // SoundTouch operates at the INPUT (native) sample rate.
    // The sample rate conversion factor is folded into the time ratio
    // by Engine::dsp_on_read(), so SoundTouch only needs to know the
    // native rate for correct WSOLA sequence/overlap window sizing.
    setSampleRate(inSampleRate > 0 ? inSampleRate : 44100);
}

int SoundTouchProcessor::getSampleRate() const {
    return m_impl->sampleRate;
}

void SoundTouchProcessor::setChannels(const int channels) {
    if (channels <= 0 || channels == m_impl->channels) return;
    m_impl->channels = channels;
    m_impl->st.setChannels(static_cast<uint>(channels));
}

int SoundTouchProcessor::getChannels() const {
    return m_impl->channels;
}

void SoundTouchProcessor::setLowLatencyMode(const bool lowLatency) {
    m_impl->lowLatency = lowLatency;
    m_impl->applyLowLatencySettings();
}

bool SoundTouchProcessor::isLowLatencyMode() const {
    return m_impl->lowLatency;
}

void SoundTouchProcessor::setTimeRatio(const float ratio) {
    const float clamped = std::clamp(ratio, 0.1f, 10.0f);
    m_impl->timeRatio = clamped;
    m_impl->st.setTempo(clamped);
}

float SoundTouchProcessor::getTimeRatio() const {
    return m_impl->timeRatio;
}

void SoundTouchProcessor::setPitchSemitones(const float semitones) {
    const float clamped = std::clamp(semitones, -12.0f, 12.0f);
    m_impl->pitchSemitones = clamped;
    m_impl->st.setPitchSemiTones(clamped);
}

float SoundTouchProcessor::getPitchSemitones() const {
    return m_impl->pitchSemitones;
}

void SoundTouchProcessor::setPitchOctaves(const float octaves) {
    setPitchSemitones(octaves * 12.0f);
}

float SoundTouchProcessor::getPitchOctaves() const {
    return m_impl->pitchSemitones / 12.0f;
}

void SoundTouchProcessor::setPitchRatio(const float ratio) {
    if (ratio <= 0.001f) return;
    const float semitones = 12.0f * (std::log(ratio) / 0.69314718056f);
    setPitchSemitones(semitones);
}

float SoundTouchProcessor::getPitchRatio() const {
    return std::pow(2.0f, m_impl->pitchSemitones / 12.0f);
}

void SoundTouchProcessor::resetPitch() {
    setPitchSemitones(0.0f);
}

void SoundTouchProcessor::setOriginalKey() {
    resetPitch();
}

void SoundTouchProcessor::putSamples(const float* interleavedSamples, const size_t numFrames) {
    if (!interleavedSamples || numFrames == 0) return;
    m_impl->st.putSamples(interleavedSamples, static_cast<uint>(numFrames));
}

size_t SoundTouchProcessor::receiveSamples(float* outInterleavedSamples, const size_t maxFrames) {
    if (!outInterleavedSamples || maxFrames == 0) return 0;
    const uint received = m_impl->st.receiveSamples(
        outInterleavedSamples, static_cast<uint>(maxFrames));
    return static_cast<size_t>(received);
}

size_t SoundTouchProcessor::numSamplesAvailable() const {
    return static_cast<size_t>(m_impl->st.numSamples());
}

void SoundTouchProcessor::flush() {
    m_impl->st.flush();
}

void SoundTouchProcessor::clear() {
    m_impl->st.clear();
}

int SoundTouchProcessor::latencyFrames() const {
    return m_impl->st.getSetting(SETTING_INITIAL_LATENCY);
}

float SoundTouchProcessor::latencyMilliseconds() const {
    if (m_impl->sampleRate <= 0) return 0.0f;
    return (static_cast<float>(latencyFrames()) * 1000.0f) / static_cast<float>(m_impl->sampleRate);
}

std::vector<float> SoundTouchProcessor::processBuffer(const float* interleavedSamples, const size_t numFrames) {
    std::vector<float> output;
    if (!interleavedSamples || numFrames == 0) return output;

    clear();
    putSamples(interleavedSamples, numFrames);
    flush();

    constexpr size_t kChunk = 1024;
    std::vector<float> chunk(kChunk * static_cast<size_t>(m_impl->channels));

    while (true) {
        const size_t received = receiveSamples(chunk.data(), kChunk);
        if (received == 0) break;
        output.insert(output.end(), chunk.begin(), chunk.begin() + received * static_cast<size_t>(m_impl->channels));
    }
    return output;
}

} // namespace reals::audio
