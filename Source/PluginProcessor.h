#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class LTAMicrowaveAudioProcessor final : public juce::AudioProcessor
{
public:
    enum Effect : int { distort, overcook, warp, glitch, defrost, bass, reheat, steam, compress, effectCount };

    LTAMicrowaveAudioProcessor();
    ~LTAMicrowaveAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LTA MICROWAVE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;

    static juce::StringArray effectNames();
    static const char* effectID(int index);
    bool effectEnabled(int index) const;
    bool isRunning() const;
    float getPower() const;
    float getMix() const;
    void setEffect(int index, bool enabled);
    void setRunning(bool running);
    void setPower(float value);
    void setMix(float value);
    void clearAllEffects();

    static constexpr int scopeSize = 512;
    float scopeAt(int index) const noexcept { return scope[(size_t) (index & (scopeSize - 1))].load(std::memory_order_relaxed); }
    int scopeHead() const noexcept { return scopeWrite.load(std::memory_order_relaxed); }
    float meter() const noexcept { return visualMeter.load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void setParameter(const juce::String& id, float rawValue);

    struct ChannelMemory
    {
        std::vector<float> warpLine, glitchLine, echoLine, reverbOne, reverbTwo;
        float filter = 0.0f;
        float bassFilter = 0.0f;
        void clear();
    };
    std::array<ChannelMemory, 2> memory;
    double rate = 44100.0;
    uint64_t absoluteSample = 0;
    int circularIndex = 0;
    float compressorEnvelope = 0.0f;
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> visualMeter { 0.0f };
    float visualSmooth = 0.0f;
    int scopeDecimation = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LTAMicrowaveAudioProcessor)
};
