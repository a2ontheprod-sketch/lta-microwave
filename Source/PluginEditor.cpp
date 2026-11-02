#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
constexpr float artW = 1672.0f;
constexpr float artH = 941.0f;
constexpr float pi = juce::MathConstants<float>::pi;
const juce::Colour lcd { 0xffd9ee76 };
const juce::Colour orange { 0xffff532e };
const juce::Colour icy { 0xff78b7f0 };
const juce::Colour panel { 0xff0e1710 };
const juce::Colour dark { 0xff090906 };

// Rectangles line up with the actual physical buttons in the photographed microwave.
const std::array<juce::Rectangle<float>, 9> buttons {
    juce::Rectangle<float>{1151, 361, 141, 77},
    juce::Rectangle<float>{1295, 361, 151, 77},
    juce::Rectangle<float>{1455, 361, 141, 77},
    juce::Rectangle<float>{1151, 446, 141, 82},
    juce::Rectangle<float>{1295, 446, 151, 82},
    juce::Rectangle<float>{1455, 446, 141, 82},
    juce::Rectangle<float>{1151, 534, 141, 79},
    juce::Rectangle<float>{1295, 534, 151, 79},
    juce::Rectangle<float>{1455, 534, 141, 79}
};
const juce::Rectangle<float> startRect { 1407, 650, 199, 89 };
const juce::Rectangle<float> stopRect { 1407, 746, 199, 83 };
const juce::Rectangle<float> lcdRect { 1174, 138, 394, 196 };
const juce::Rectangle<float> mixRect { 1397, 284, 169, 44 };
const juce::Point<float> knobCentre { 1274.0f, 727.0f };

bool anyActive(const LTAMicrowaveAudioProcessor& proc)
{
    for (int i = 0; i < LTAMicrowaveAudioProcessor::effectCount; ++i)
        if (proc.effectEnabled(i)) return true;
    return false;
}

void neonLine(juce::Graphics& g, const juce::Path& p, juce::Colour colour, float opacity)
{
    g.setColour(colour.withMultipliedAlpha(opacity * 0.13f));
    g.strokePath(p, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colour.withMultipliedAlpha(opacity * 0.45f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colour.withMultipliedAlpha(opacity));
    g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

LTAMicrowaveAudioProcessorEditor::LTAMicrowaveAudioProcessorEditor(LTAMicrowaveAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    microwaveBackground = juce::ImageFileFormat::loadFrom(BinaryData::microwave_bg_png,
                                                           BinaryData::microwave_bg_pngSize);
    setOpaque(true);
    setSize(1170, 658);
    setResizable(true, true);
    setResizeLimits(840, 473, 1672, 941);
    getConstrainer()->setFixedAspectRatio((double) artW / (double) artH);
    startTimerHz(30);
}

void LTAMicrowaveAudioProcessorEditor::resized() {}

juce::Point<float> LTAMicrowaveAudioProcessorEditor::toArtSpace(juce::Point<float> point) const
{
    return { point.x * artW / (float) getWidth(), point.y * artH / (float) getHeight() };
}

int LTAMicrowaveAudioProcessorEditor::effectAt(juce::Point<float> p) const
{
    for (int i = 0; i < (int) buttons.size(); ++i)
        if (buttons[(size_t) i].contains(p)) return i;
    return -1;
}

void LTAMicrowaveAudioProcessorEditor::flashEffect(int index)
{
    lastEffect = index;
    animPulse = 1.0f;
    lastPressTime = juce::Time::getMillisecondCounterHiRes();
    if (index >= 0 && index < LTAMicrowaveAudioProcessor::effectCount)
        flash[(size_t) index] = 1.0f;
}

void LTAMicrowaveAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto p = toArtSpace(e.position);
    pressedEffect = effectAt(p);
    if (pressedEffect >= 0)
    {
        processor.setEffect(pressedEffect, !processor.effectEnabled(pressedEffect));
        flashEffect(pressedEffect);
    }
    else if (startRect.contains(p))
    {
        processor.setRunning(true);
        flashEffect(-1);
    }
    else if (stopRect.contains(p))
    {
        processor.setRunning(false);
        flashEffect(-1);
    }
    else if (p.getDistanceFrom(knobCentre) < 85.0f)
    {
        draggingKnob = true;
        draggingMix = e.mods.isShiftDown();
        startDragY = p.y;
        startDragValue = draggingMix ? processor.getMix() : processor.getPower();
    }
    else if (mixRect.contains(p))
    {
        draggingKnob = true;
        draggingMix = true;
        startDragY = p.y;
        startDragValue = processor.getMix();
    }
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::adjustKnob(float y, bool mix)
{
    const float newValue = juce::jlimit(0.0f, 1.0f, startDragValue + (startDragY - y) / 260.0f);
    if (mix) processor.setMix(newValue);
    else processor.setPower(newValue);
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingKnob) adjustKnob(toArtSpace(e.position).y, draggingMix);
}

void LTAMicrowaveAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    pressedEffect = -1;
    draggingKnob = false;
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e,
                                                       const juce::MouseWheelDetails& d)
{
    const auto p = toArtSpace(e.position);
    const float delta = (d.deltaY > 0 ? 0.035f : -0.035f);
    if (mixRect.contains(p) || (p.getDistanceFrom(knobCentre) < 85.0f && e.mods.isShiftDown()))
        processor.setMix(processor.getMix() + delta);
    else if (p.getDistanceFrom(knobCentre) < 95.0f)
        processor.setPower(processor.getPower() + delta);
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::timerCallback()
{
    ++tick;
    if (processor.isRunning() && anyActive(processor)) cookSeconds += 1.0f / 30.0f;
    for (auto& v : flash) v *= 0.87f;
    animPulse *= 0.88f;
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::renderScreen(juce::Graphics& g, bool running, int count)
{
    // Replace the static text in the art with an actual responsive LCD display.
    g.setColour(panel.withAlpha(0.98f));
    g.fillRoundedRectangle(lcdRect, 15.0f);
    g.setColour(lcd.withAlpha(0.15f));
    g.drawRoundedRectangle(lcdRect.reduced(2.0f), 13.0f, 2.0f);

    const bool cooking = running && count > 0;
    const juce::String mode = cooking ? "COOKING" : running ? "READY" : "STOPPED";
    const int minutes = ((int) cookSeconds / 60) % 100;
    const int seconds = (int) cookSeconds % 60;
    const juce::String time = juce::String::formatted("%02d:%02d", minutes, seconds);

    g.setColour(lcd.withAlpha(0.78f));
    g.setFont(juce::Font("Consolas", 20.0f, juce::Font::bold));
    g.drawText("LTA / MULTI-FX", 1194, 154, 325, 26, juce::Justification::centredLeft);
    g.setFont(juce::Font("Consolas", 27.0f, juce::Font::bold));
    g.drawText(mode, 1189, 192, 365, 39, juce::Justification::centred);

    g.setFont(juce::Font("Consolas", 39.0f, juce::Font::bold));
    g.setColour(cooking ? lcd : lcd.withAlpha(0.68f));
    g.drawText(time, 1202, 235, 332, 47, juce::Justification::centred);

    g.setColour(lcd.withAlpha(0.95f));
    g.setFont(juce::Font("Consolas", 17.0f, juce::Font::bold));
    g.drawText("POWER " + juce::String((int) std::round(processor.getPower() * 100.0f)) + "%",
               1190, 286, 214, 30, juce::Justification::centredLeft);
