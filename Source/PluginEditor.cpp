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
    setTooltip("LTA MICROWAVE — Click any FX button to stack effects. Drag POWER. Drag MIX text on the LCD.");
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
    const auto mix = "MIX " + juce::String((int) std::round(processor.getMix() * 100.0f)) + "%";
    g.drawText(mix, 1395, 286, 168, 30, juce::Justification::centredRight);

    g.setColour(lcd.withAlpha(0.24f));
    for (int i = 0; i < 24; ++i)
    {
        const float x = 1188.0f + i * 15.4f;
        const float v = processor.meter();
        if (i < (int)(v * 25.0f))
        {
            g.setColour(lcd.withAlpha(0.65f));
            g.fillRect(x, 183.0f, 9.0f, 4.0f);
        }
    }
}

void LTAMicrowaveAudioProcessorEditor::renderWindow(juce::Graphics& g, bool running, int count)
{
    const juce::Rectangle<float> chamber { 118.0f, 268.0f, 895.0f, 458.0f };
    const bool cooking = running && count > 0;
    const float meter = juce::jlimit(0.0f, 1.0f, processor.meter());
    const float intensity = cooking ? (0.25f + 0.75f * processor.getPower()) : 0.0f;

    // A transparent dark layer keeps the real-world microwave photography, but allows
    // the waveform and every preset animation to be visibly live rather than baked in.
    g.setColour(dark.withAlpha(cooking ? 0.75f : 0.91f));
    g.fillRoundedRectangle(chamber, 24.0f);

    if (!cooking)
    {
        g.setColour(juce::Colour(0xffa2bdb3).withAlpha(0.26f));
        g.setFont(juce::Font("Consolas", 23.0f, juce::Font::bold));
        g.drawText("INSERT AUDIO / SELECT FX / START", 195, 447, 745, 55, juce::Justification::centred);
        return;
    }

    const bool distorted = processor.effectEnabled(LTAMicrowaveAudioProcessor::distort);
    const bool overcooked = processor.effectEnabled(LTAMicrowaveAudioProcessor::overcook);
    const bool warped = processor.effectEnabled(LTAMicrowaveAudioProcessor::warp);
    const bool glitched = processor.effectEnabled(LTAMicrowaveAudioProcessor::glitch);
    const bool defrosted = processor.effectEnabled(LTAMicrowaveAudioProcessor::defrost);
    const bool boosted = processor.effectEnabled(LTAMicrowaveAudioProcessor::bass);
    const bool echoing = processor.effectEnabled(LTAMicrowaveAudioProcessor::reheat);
    const bool steamy = processor.effectEnabled(LTAMicrowaveAudioProcessor::steam);
    const bool compressed = processor.effectEnabled(LTAMicrowaveAudioProcessor::compress);

    const juce::Colour mainColour = defrosted ? icy : (overcooked ? juce::Colours::yellow : orange);
    const float animationTime = (float) tick * 0.033333f;
    const float cy = 505.0f;
    const int head = processor.scopeHead();
    const float height = (75.0f + meter * 135.0f) * intensity;

    // Heating coils: intensity changes with the FX and actual input level.
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<float> heater { 260.0f + i * 193.0f, 314.0f, 130.0f, 14.0f };
        g.setColour(mainColour.withAlpha(0.10f + meter * 0.18f));
        g.fillRoundedRectangle(heater.expanded(10.0f), 14.0f);
        g.setColour(mainColour.withAlpha(0.4f + meter * 0.46f));
        g.fillRoundedRectangle(heater, 7.0f);
    }

    // Platter rotates while audio is running.
    g.setColour(mainColour.withAlpha(0.15f));
    g.fillEllipse(285.0f, 635.0f, 575.0f, 51.0f);
    const float turn = animationTime * (1.2f + processor.getPower() * 1.6f);
    for (int i = 0; i < 4; ++i)
    {
        const float a = turn + i * pi / 2.0f;
        g.setColour(mainColour.withAlpha(0.13f));
        g.drawLine(570.0f, 658.0f, 570.0f + std::cos(a) * 210.0f,
                   658.0f + std::sin(a) * 18.0f, 5.0f);
    }

    // STEAM: rising hazy particles. DEFROST: cold blue fog rather than orange.
    if (steamy || defrosted)
    {
        for (int i = 0; i < 17; ++i)
        {
            const float t = std::fmod((animationTime * (steamy ? 0.24f : 0.09f)) + i * 0.117f, 1.0f);
            const float x = 190.0f + i * 46.0f + 18.0f * std::sin(t * 13.0f + i);
            const float y = 665.0f - 315.0f * t;
            const float size = 9.0f + 35.0f * t;
            g.setColour((defrosted ? icy : juce::Colours::whitesmoke).withAlpha(0.09f * (1.0f - t)));
            g.fillEllipse(x, y, size, size);
        }
    }

    // Bass: low-frequency concentric pulses under the waveform.
    if (boosted)
    {
        const float pulse = std::sin(animationTime * 8.0f) * 0.5f + 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            const float r = 90.0f + i * 58.0f + pulse * 45.0f;
            g.setColour(orange.withAlpha((0.20f - i * 0.045f) * (0.3f + meter)));
            g.drawEllipse(562.0f - r, cy - r * 0.45f, r * 2.0f, r * 0.9f, 3.0f);
        }
    }

    // Real incoming samples from the processor's lock-free oscilloscope ring.
    juce::Path waveform;
    const int points = 170;
    for (int i = 0; i < points; ++i)
    {
        const float x = 180.0f + i * 4.63f;
        float sample = processor.scopeAt(head - (points - i) * 2);
        float y = cy - sample * height * 1.8f;
        if (warped) y += 26.0f * intensity * std::sin(animationTime * 5.0f + i * 0.115f);
        if (distorted) y = cy + (y < cy ? -1.0f : 1.0f) * juce::jmin(85.0f, std::abs(y - cy) * 1.6f);
        if (overcooked) y = std::round(y / 14.0f) * 14.0f;
        if (compressed) y = cy + (y - cy) * 0.48f;
        if (glitched && ((tick / 4) % 4 == 0) && i > 60 && i < 124)
            y += ((i / 11) % 2 ? 43.0f : -30.0f);
        if (i == 0) waveform.startNewSubPath(x, y);
        else waveform.lineTo(x, y);
    }
    if (echoing)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(-17.0f, 28.0f));
        neonLine(g, waveform, mainColour, 0.22f);
        g.restoreState();
    }
    neonLine(g, waveform, mainColour, 0.67f + 0.24f * meter);

    // GLITCH: twitchy horizontal data bands. OVERCOOK: an occasional flash.
    if (glitched && tick % 17 < 4)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float y = 370.0f + i * 42.0f;
            g.setColour(mainColour.withAlpha(0.17f));
            g.fillRect(300.0f + (float)(tick * 31 + i * 113) - 300.0f * std::floor((tick * 31 + i * 113) / 380.0f), y,
                       125.0f + (i % 3) * 54.0f, 3.0f);
        }
    }
    if (overcooked && (tick % 43 < 2))
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f * intensity));
        g.fillRoundedRectangle(chamber.reduced(11.0f), 17.0f);
    }
    if (animPulse > 0.015f)
    {
        g.setColour(mainColour.withAlpha(0.15f * animPulse));
        g.fillRoundedRectangle(chamber.reduced(2.0f), 24.0f);
    }

    g.setColour(mainColour.withAlpha(0.8f));
    g.setFont(juce::Font("Consolas", 17.0f, juce::Font::bold));
    g.drawText(juce::String(count) + " EFFECT" + (count == 1 ? "" : "S") + " ACTIVE",
               176, 691, 330, 22, juce::Justification::centredLeft);
}

void LTAMicrowaveAudioProcessorEditor::renderButtons(juce::Graphics& g, bool running)
{
    for (int i = 0; i < LTAMicrowaveAudioProcessor::effectCount; ++i)
    {
        const auto r = buttons[(size_t) i];
        const bool on = processor.effectEnabled(i);
        const float pulse = flash[(size_t) i];
        if (on || pulse > 0.05f)
        {
            g.setColour(orange.withAlpha((on ? 0.18f : 0.0f) + pulse * 0.22f));
            g.fillRoundedRectangle(r.reduced(7.0f), 10.0f);
        }
        g.setColour((on ? juce::Colour(0xffff5d43) : juce::Colour(0xff34342b)));
        g.fillRoundedRectangle(r.getX() + 17, r.getY() + 6, 19.0f, 5.0f, 2.5f);
        if (on)
        {
            g.setColour(orange.withAlpha(0.34f + pulse * 0.4f));
            g.fillEllipse(r.getX() + 17, r.getY() + 3, 21.0f, 10.0f);
        }
        if (pressedEffect == i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.15f));
            g.fillRoundedRectangle(r.reduced(3.0f), 11.0f);
        }
    }

    // Real start/stop state illumination. Both buttons live ON the actual artwork.
    if (running)
    {
        g.setColour(juce::Colour(0xffb6f267).withAlpha(0.20f));
        g.fillRoundedRectangle(startRect.reduced(11.0f), 11.0f);
    }
    else
    {
        g.setColour(orange.withAlpha(0.22f));
        g.fillRoundedRectangle(stopRect.reduced(11.0f), 11.0f);
    }
}

void LTAMicrowaveAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171716));
    g.saveState();
    g.addTransform(juce::AffineTransform::scale((float)getWidth() / artW,
                                                (float)getHeight() / artH));
    if (microwaveBackground.isValid())
        g.drawImage(microwaveBackground, 0, 0, (int) artW, (int) artH,
                    0, 0, microwaveBackground.getWidth(), microwaveBackground.getHeight());

    int activeCount = 0;
    for (int i = 0; i < LTAMicrowaveAudioProcessor::effectCount; ++i)
        if (processor.effectEnabled(i)) ++activeCount;
    const bool running = processor.isRunning();
    renderWindow(g, running, activeCount);
    renderScreen(g, running, activeCount);
    renderButtons(g, running);

    // The knob is a real draggable parameter, not just a photo.
    const float power = processor.getPower();
    const float angle = (-145.0f + power * 290.0f) * pi / 180.0f;
    juce::Path arc;
    arc.addCentredArc(knobCentre.x, knobCentre.y, 87.0f, 87.0f, 0.0f,
                     -145.0f * pi / 180.0f, angle, true);
    g.setColour(orange.withAlpha(0.75f));
    g.strokePath(arc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    const float x = knobCentre.x + std::sin(angle) * 54.0f;
    const float y = knobCentre.y - std::cos(angle) * 54.0f;
    g.setColour(juce::Colour(0xffecddd0));
    g.fillEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font("Consolas", 15.0f, juce::Font::bold));
    g.drawText("v1.0", 1519, 870, 85, 23, juce::Justification::centredRight);

    g.restoreState();
}
