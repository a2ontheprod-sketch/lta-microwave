#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class LTAMicrowaveAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit LTAMicrowaveAudioProcessorEditor(LTAMicrowaveAudioProcessor&);
    ~LTAMicrowaveAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    juce::Point<float> toArtSpace(juce::Point<float> mouse) const;
    int effectAt(juce::Point<float> point) const;
    void adjustKnob(float y, bool mix);
    void renderScreen(juce::Graphics&, bool running, int activeCount);
    void renderWindow(juce::Graphics&, bool running, int activeCount);
    void renderButtons(juce::Graphics&, bool running);
    void flashEffect(int index);

    LTAMicrowaveAudioProcessor& processor;
    juce::Image microwaveBackground;
    juce::TooltipWindow tips { this, 500 };
    int tick = 0;
    int lastEffect = -1;
    double lastPressTime = 0.0;
    float animPulse = 0.0f;
    bool draggingKnob = false;
    bool draggingMix = false;
    float startDragY = 0.0f;
    float startDragValue = 0.0f;
    int pressedEffect = -1;
    std::array<float, LTAMicrowaveAudioProcessor::effectCount> flash {};
    float cookSeconds = 0.0f;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LTAMicrowaveAudioProcessorEditor)
};
