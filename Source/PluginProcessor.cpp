#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr std::array<const char*, 9> ids {
    "distort", "overcook", "warp", "glitch", "defrost",
    "bass", "reheat", "steam", "compress"
};
constexpr float pi = juce::MathConstants<float>::pi;
inline float clampAudio(float x) noexcept { return juce::jlimit(-3.0f, 3.0f, x); }
inline int wrapIndex(int position, int size) noexcept
{
    position %= size;
    return position < 0 ? position + size : position;
}
inline float readDelay(const std::vector<float>& line, int current, int delay) noexcept
{
    const int n = (int) line.size();
    return n == 0 ? 0.0f : line[(size_t) wrapIndex(current - juce::jlimit(1, n - 1, delay), n)];
}
inline float softClip(float x) noexcept { return std::tanh(x); }
}

juce::AudioProcessorValueTreeState::ParameterLayout LTAMicrowaveAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("running", 1), "START / STOP", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("power", 1), "POWER", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.55f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mix", 1), "MIX", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    for (const char* id : ids)
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), juce::String(id).toUpperCase(), false));
    return layout;
}

LTAMicrowaveAudioProcessor::LTAMicrowaveAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "MICROWAVE_SETTINGS", createParameters())
{
    for (auto& a : scope) a.store(0.0f, std::memory_order_relaxed);
}

juce::StringArray LTAMicrowaveAudioProcessor::effectNames()
{
    return { "DISTORT", "OVERCOOK", "WARP", "GLITCH", "DEFROST", "BASS", "REHEAT", "STEAM", "COMPRESS" };
}

const char* LTAMicrowaveAudioProcessor::effectID(int index)
{
    return ids[(size_t) juce::jlimit(0, effectCount - 1, index)];
}

bool LTAMicrowaveAudioProcessor::effectEnabled(int index) const
{
    if (index < 0 || index >= effectCount) return false;
    const auto* raw = state.getRawParameterValue(ids[(size_t) index]);
    return raw != nullptr && raw->load(std::memory_order_relaxed) >= 0.5f;
}

bool LTAMicrowaveAudioProcessor::isRunning() const
{
    return state.getRawParameterValue("running")->load(std::memory_order_relaxed) >= 0.5f;
}
float LTAMicrowaveAudioProcessor::getPower() const { return state.getRawParameterValue("power")->load(std::memory_order_relaxed); }
float LTAMicrowaveAudioProcessor::getMix() const { return state.getRawParameterValue("mix")->load(std::memory_order_relaxed); }

void LTAMicrowaveAudioProcessor::setParameter(const juce::String& id, float value)
{
    if (auto* param = state.getParameter(id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(value));
        param->endChangeGesture();
    }
}
void LTAMicrowaveAudioProcessor::setEffect(int index, bool enabled)
{
    if (index >= 0 && index < effectCount) setParameter(ids[(size_t) index], enabled ? 1.0f : 0.0f);
}
void LTAMicrowaveAudioProcessor::setRunning(bool enabled) { setParameter("running", enabled ? 1.0f : 0.0f); }
void LTAMicrowaveAudioProcessor::setPower(float p) { setParameter("power", juce::jlimit(0.0f, 1.0f, p)); }
void LTAMicrowaveAudioProcessor::setMix(float m) { setParameter("mix", juce::jlimit(0.0f, 1.0f, m)); }
void LTAMicrowaveAudioProcessor::clearAllEffects()
{
    for (int i = 0; i < effectCount; ++i) setEffect(i, false);
    setRunning(false);
}

bool LTAMicrowaveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

void LTAMicrowaveAudioProcessor::ChannelMemory::clear()
{
    for (auto* line : { &warpLine, &glitchLine, &echoLine, &reverbOne, &reverbTwo })
        std::fill(line->begin(), line->end(), 0.0f);
    filter = bassFilter = 0.0f;
}

void LTAMicrowaveAudioProcessor::prepareToPlay(double sampleRate, int)
{
    rate = juce::jmax(8000.0, sampleRate);
    // Allocate only during preparation, never in the real-time processing loop.
    const int length = juce::jmax(32768, (int) std::ceil(rate * 2.1));
    for (auto& channel : memory)
    {
        channel.warpLine.assign((size_t) length, 0.0f);
        channel.glitchLine.assign((size_t) length, 0.0f);
        channel.echoLine.assign((size_t) length, 0.0f);
        channel.reverbOne.assign((size_t) length, 0.0f);
        channel.reverbTwo.assign((size_t) length, 0.0f);
    }
    absoluteSample = 0;
    circularIndex = 0;
    scopeWrite.store(0);
    scopeDecimation = 0;
    compressorEnvelope = visualSmooth = 0.0f;
    visualMeter.store(0.0f);
    for (auto& s : scope) s.store(0.0f);
}
void LTAMicrowaveAudioProcessor::releaseResources()
{
    for (auto& c : memory) c.clear();
}

void LTAMicrowaveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || memory[0].warpLine.empty()) return;

    std::array<bool, effectCount> enabled {};
    bool hasEffects = false;
    for (int i = 0; i < effectCount; ++i)
    {
        enabled[(size_t) i] = effectEnabled(i);
        hasEffects |= enabled[(size_t) i];
    }
    const bool active = isRunning() && hasEffects;
    const float power = getPower();
    const float wet = getMix();
    const int len = (int) memory[0].warpLine.size();
    const float pitch = (float) rate;
    auto* left = buffer.getWritePointer(0);
    auto* right = numCh == 2 ? buffer.getWritePointer(1) : nullptr;

    // All effect memory remains prepared so switching presets doesn't allocate.
    // A strict bypass preserves the source audio byte-for-byte.
    for (int n = 0; n < numSamples; ++n)
    {
        float original[2] { left[n], right ? right[n] : 0.0f };
        float working[2] { original[0], original[1] };
        const float mod = (float) absoluteSample / pitch;
        const float strength = 0.1f + power * 0.9f;
        const int idx = circularIndex;

        if (active && wet > 0.00001f)
        {
            for (int c = 0; c < numCh; ++c)
            {
                auto& m = memory[(size_t)c];
                float x = working[c];

                if (enabled[defrost])
                {
                    const float cutoff = 6500.0f - 4500.0f * power;
                    const float alpha = 1.0f - std::exp(-2.0f * pi * cutoff / pitch);
                    m.filter += alpha * (x - m.filter);
                    const float flutter = 1.0f - 0.025f * power * (1.0f + std::sin(2.0f * pi * 3.8f * mod));
                    x = juce::jmap(strength * 0.9f, x, m.filter) * flutter;
                }
                if (enabled[warp])
                {
                    m.warpLine[(size_t) idx] = x;
                    const float sweep = std::sin(2.0f * pi * (0.36f + power * 0.54f) * mod + c * 1.6f);
                    const int delay = (int) ((0.012f + (0.007f + 0.014f * power) * (0.5f + 0.5f * sweep)) * pitch);
                    const float shifted = readDelay(m.warpLine, idx, delay);
                    x = x * (0.62f - power * 0.15f) + shifted * (0.38f + power * 0.15f);
                }
                if (enabled[glitch])
                {
                    m.glitchLine[(size_t) idx] = x;
                    const int period = juce::jmax(128, (int) (pitch * (0.43f - 0.27f * power)));
                    const int phase = (int) (absoluteSample % (uint64_t) period);
                    const int burst = juce::jmax(128, period / 4);
                    if (phase < burst)
                    {
                        const int slice = juce::jmax(32, burst / 5);
                        const int repeat = phase % slice;
                        const int lag = slice * 3 - repeat;
                        x = readDelay(m.glitchLine, idx, lag);
                        if (phase % slice < 10) x *= 0.7f;
                    }
                }
                if (enabled[bass])
                {
                    const float alpha = 1.0f - std::exp(-2.0f * pi * 145.0f / pitch);
                    m.bassFilter += alpha * (x - m.bassFilter);
                    x += m.bassFilter * (0.28f + power * 0.67f);
                }
                if (enabled[distort])
                {
                    const float gain = 1.0f + power * 9.0f;
                    x = softClip(clampAudio(x * gain)) / juce::jmax(1.0f, gain * 0.24f);
                }
                if (enabled[overcook])
                {
                    const float gain = 1.5f + power * 14.0f;
                    const float clipped = juce::jlimit(-0.82f, 0.82f, x * gain) / 0.82f;
                    const float levels = std::round(64.0f - 56.0f * power);
                    x = std::round(clipped * levels) / levels * 0.80f;
                }
                working[c] = clampAudio(x);
            }

            if (enabled[compress])
            {
                float peak = std::abs(working[0]);
                if (numCh == 2) peak = juce::jmax(peak, std::abs(working[1]));
                const float attack = std::exp(-1.0f / (pitch * 0.009f));
                const float release = std::exp(-1.0f / (pitch * 0.11f));
                compressorEnvelope = (peak > compressorEnvelope ? attack : release) * compressorEnvelope
                                   + (1.0f - (peak > compressorEnvelope ? attack : release)) * peak;
                const float threshold = 0.50f - power * 0.33f;
                const float gain = compressorEnvelope > threshold
                    ? std::pow(threshold / juce::jmax(threshold, compressorEnvelope), 0.4f + 0.45f * power) : 1.0f;
                for (int c = 0; c < numCh; ++c) working[c] *= gain * (1.04f + 0.28f * power);
            }

            for (int c = 0; c < numCh; ++c)
            {
                auto& m = memory[(size_t)c];
                float x = working[c];
                if (enabled[reheat])
                {
                    const int delay = (int) (pitch * (0.17f + 0.12f * power));
                    const float delayed = readDelay(m.echoLine, idx, delay);
                    m.echoLine[(size_t) idx] = clampAudio(x + delayed * (0.32f + power * 0.31f));
                    x += delayed * (0.17f + 0.29f * power);
                }
                if (enabled[steam])
                {
                    const int delayA = (int) (pitch * (c == 0 ? 0.041f : 0.049f));
                    const int delayB = (int) (pitch * (c == 0 ? 0.067f : 0.073f));
                    const float a = readDelay(m.reverbOne, idx, delayA);
                    const float b = readDelay(m.reverbTwo, idx, delayB);
                    m.reverbOne[(size_t) idx] = softClip(x * 0.28f + (a * 0.60f + b * 0.13f) * (0.72f + power * 0.25f));
                    m.reverbTwo[(size_t) idx] = softClip(x * 0.24f + (b * 0.63f - a * 0.11f) * (0.72f + power * 0.25f));
                    x += (a + b) * (0.22f + power * 0.36f);
                }
                working[c] = x;
            }
            for (int c = 0; c < numCh; ++c)
                working[c] = original[c] * (1.0f - wet) + clampAudio(working[c]) * wet;
            // Gentle safety limiter when FX are on. Bypass does not colour the input.
            for (int c = 0; c < numCh; ++c)
                working[c] = juce::jlimit(-1.0f, 1.0f, working[c]);
        }

        left[n] = working[0];
        if (right) right[n] = working[1];

        const float scopeValue = 0.5f * (working[0] + (right ? working[1] : working[0]));
        if (++scopeDecimation >= 48)
        {
            scopeDecimation = 0;
            const int pos = scopeWrite.load(std::memory_order_relaxed);
            scope[(size_t) (pos & (scopeSize - 1))].store(scopeValue, std::memory_order_relaxed);
            scopeWrite.store((pos + 1) & (scopeSize - 1), std::memory_order_relaxed);
        }
        const float level = juce::jmin(1.0f, std::abs(scopeValue));
        visualSmooth = juce::jmax(level, visualSmooth * 0.9990f);
        ++absoluteSample;
        circularIndex = (circularIndex + 1) % len;
    }
    visualMeter.store(visualSmooth, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* LTAMicrowaveAudioProcessor::createEditor()
{
    return new LTAMicrowaveAudioProcessorEditor(*this);
}
void LTAMicrowaveAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    const auto snapshot = state.copyState();
    if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, dest);
}
void LTAMicrowaveAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LTAMicrowaveAudioProcessor();
}
