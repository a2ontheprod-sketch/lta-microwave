
#include "PluginEditor.h"
#include <BinaryData.h>
#include <array>
#include <cmath>

namespace
{
constexpr float W = 1672.0f;
constexpr float H = 941.0f;
constexpr float pi = juce::MathConstants<float>::pi;

const juce::Colour orange(0xffff532e);
const juce::Colour lcd(0xffd9ee76);
const juce::Colour icy(0xff78b7f0);

const std::array<juce::Rectangle<float>, 9> buttons {{
    {1151, 361, 141, 77},
    {1295, 361, 151, 77},
    {1455, 361, 141, 77},
    {1151, 446, 141, 82},
    {1295, 446, 151, 82},
    {1455, 446, 141, 82},
    {1151, 534, 141, 79},
    {1295, 534, 151, 79},
    {1455, 534, 141, 79}
}};

const std::array<const char*, 9> names {{
    "DISTORT", "OVERCOOK", "WARP",
    "GLITCH", "DEFROST", "BASS",
    "REHEAT", "STEAM", "COMPRESS"
}};

const juce::Rectangle<float> startRect {1407,650,199,89};
const juce::Rectangle<float> stopRect {1407,746,199,83};
const juce::Rectangle<float> lcdRect {1174,138,394,196};
const juce::Rectangle<float> mixRect {1397,284,169,44};
const juce::Point<float> knobCentre {1274,727};

bool anyActive(const LTAMicrowaveAudioProcessor& p)
{
    for (int i = 0; i < LTAMicrowaveAudioProcessor::effectCount; ++i)
        if (p.effectEnabled(i))
            return true;

    return false;
}

void glowLine(juce::Graphics& g,
              const juce::Path& path,
              juce::Colour colour,
              float alpha)
{
    g.setColour(colour.withMultipliedAlpha(alpha * 0.13f));
    g.strokePath(path, juce::PathStrokeType(10.0f));

    g.setColour(colour.withMultipliedAlpha(alpha * 0.45f));
    g.strokePath(path, juce::PathStrokeType(4.0f));

    g.setColour(colour.withMultipliedAlpha(alpha));
    g.strokePath(path, juce::PathStrokeType(1.8f));
}
}

LTAMicrowaveAudioProcessorEditor::
LTAMicrowaveAudioProcessorEditor(LTAMicrowaveAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p)
{
    microwaveBackground = juce::ImageFileFormat::loadFrom(
        BinaryData::microwave_bg_png,
        BinaryData::microwave_bg_pngSize);

    setOpaque(true);
    setSize(1170, 658);
    setResizable(true, true);
    setResizeLimits(840, 473, 1672, 941);

    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double) W / (double) H);

    startTimerHz(60);
}

void LTAMicrowaveAudioProcessorEditor::resized()
{
}

juce::Point<float>
LTAMicrowaveAudioProcessorEditor::toArtSpace(
    juce::Point<float> p) const
{
    return {
        p.x * W / (float) getWidth(),
        p.y * H / (float) getHeight()
    };
}

int LTAMicrowaveAudioProcessorEditor::effectAt(
    juce::Point<float> p) const
{
    for (int i = 0; i < (int) buttons.size(); ++i)
        if (buttons[(size_t) i].contains(p))
            return i;

    return -1;
}

void LTAMicrowaveAudioProcessorEditor::flashEffect(int index)
{
    lastEffect = index;
    animPulse = 1.0f;
    lastPressTime = juce::Time::getMillisecondCounterHiRes();

    if (index >= 0 &&
        index < LTAMicrowaveAudioProcessor::effectCount)
        flash[(size_t) index] = 1.0f;
}

void LTAMicrowaveAudioProcessorEditor::mouseDown(
    const juce::MouseEvent& e)
{
    const auto p = toArtSpace(e.position);
    pressedEffect = effectAt(p);

    if (pressedEffect >= 0)
    {
        const bool wasSelected =
            processor.effectEnabled(pressedEffect);

        // SINGLE SELECTION:
        // Turn every mode off before selecting the new one.
        for (int i = 0;
             i < LTAMicrowaveAudioProcessor::effectCount;
             ++i)
        {
            processor.setEffect(i, false);
        }

        // Clicking the selected mode again switches it off.
        if (!wasSelected)
            processor.setEffect(pressedEffect, true);

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

        startDragValue = draggingMix
            ? processor.getMix()
            : processor.getPower();
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

void LTAMicrowaveAudioProcessorEditor::adjustKnob(
    float y, bool mix)
{
    const float value = juce::jlimit(
        0.0f, 1.0f,
        startDragValue + (startDragY - y) / 260.0f);

    if (mix)
        processor.setMix(value);
    else
        processor.setPower(value);

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::mouseDrag(
    const juce::MouseEvent& e)
{
    if (draggingKnob)
        adjustKnob(toArtSpace(e.position).y, draggingMix);
}

void LTAMicrowaveAudioProcessorEditor::mouseUp(
    const juce::MouseEvent&)
{
    pressedEffect = -1;
    draggingKnob = false;
    repaint();
}

void LTAMicrowaveAudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& e,
    const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f)
        return;

    const auto p = toArtSpace(e.position);
    const float delta = wheel.deltaY > 0 ? 0.035f : -0.035f;

    if (mixRect.contains(p) ||
        (p.getDistanceFrom(knobCentre) < 85.0f &&
         e.mods.isShiftDown()))
    {
        processor.setMix(processor.getMix() + delta);
    }
    else if (p.getDistanceFrom(knobCentre) < 95.0f)
    {
        processor.setPower(processor.getPower() + delta);
    }

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::timerCallback()
{
    ++tick;

    if (processor.isRunning() && anyActive(processor))
        cookSeconds += 1.0f / 60.0f;

    for (auto& f : flash)
        f *= 0.91f;

    animPulse *= 0.90f;

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::renderScreen(
    juce::Graphics& g, bool running, int count)
{
    g.setColour(juce::Colour(0xff0e1710));
    g.fillRoundedRectangle(lcdRect, 15.0f);

    g.setColour(lcd.withAlpha(0.17f));
    g.drawRoundedRectangle(lcdRect.reduced(2), 13.0f, 2.0f);

    const bool cooking = running && count > 0;

    const juce::String mode =
        cooking ? "COOKING" : running ? "READY" : "STOPPED";

    const int minutes = ((int) cookSeconds / 60) % 100;
    const int seconds = (int) cookSeconds % 60;

    const juce::String time =
        juce::String::formatted("%02d:%02d", minutes, seconds);

    g.setColour(lcd);
    g.setFont(juce::Font("Consolas", 20.0f, juce::Font::bold));
    g.drawText("LTA / MULTI-FX",
               1194,154,325,26,
               juce::Justification::centredLeft);

    g.setFont(juce::Font("Consolas", 27.0f, juce::Font::bold));
    g.drawText(mode,1189,192,365,39,
               juce::Justification::centred);

    g.setFont(juce::Font("Consolas", 39.0f, juce::Font::bold));
    g.drawText(time,1202,235,332,47,
               juce::Justification::centred);

    g.setFont(juce::Font("Consolas", 17.0f, juce::Font::bold));

    g.drawText(
        "POWER " + juce::String(
            (int) std::round(processor.getPower() * 100)) + "%",
        1190,286,214,30,
        juce::Justification::centredLeft);

    g.drawText(
        "MIX " + juce::String(
            (int) std::round(processor.getMix() * 100)) + "%",
        1395,286,168,30,
        juce::Justification::centredRight);

    const float level = juce::jlimit(
        0.0f, 1.0f, processor.meter());

    for (int i = 0; i < 24; ++i)
    {
        if (i < (int)(level * 25.0f))
        {
            g.setColour(lcd.withAlpha(0.75f));
            g.fillRect(1188.0f + i * 15.4f,
                       183.0f,9.0f,4.0f);
        }
    }
}

void LTAMicrowaveAudioProcessorEditor::renderWindow(
    juce::Graphics& g, bool running, int count)
{
    const juce::Rectangle<float> chamber {
        118,268,895,458
    };

    const bool cooking = running && count > 0;

    const float meter = juce::jlimit(
        0.0f,1.0f,processor.meter());

    const float intensity = cooking
        ? 0.25f + 0.75f * processor.getPower()
        : 0.0f;

    g.setColour(juce::Colours::black.withAlpha(
        cooking ? 0.75f : 0.91f));

    g.fillRoundedRectangle(chamber,24.0f);

    if (!cooking)
    {
        g.setColour(juce::Colour(0xffa2bdb3).withAlpha(0.4f));
        g.setFont(juce::Font("Consolas",23.0f,juce::Font::bold));

        g.drawText(
            "SELECT MODE / PRESS START",
            195,447,745,55,
            juce::Justification::centred);

        return;
    }

    const bool distorted = processor.effectEnabled(0);
    const bool overcooked = processor.effectEnabled(1);
    const bool warped = processor.effectEnabled(2);
    const bool glitched = processor.effectEnabled(3);
    const bool defrosted = processor.effectEnabled(4);
    const bool boosted = processor.effectEnabled(5);
    const bool echoing = processor.effectEnabled(6);
    const bool steamy = processor.effectEnabled(7);
    const bool compressed = processor.effectEnabled(8);

    const auto colour = defrosted
        ? icy : overcooked ? juce::Colours::yellow : orange;

    const float t = tick / 60.0f;
    const float cy = 505.0f;
    const int head = processor.scopeHead();

    const float height =
        (75.0f + meter * 135.0f) * intensity;

    // Heating elements.
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<float> heater {
            260.0f + i * 193.0f,
            314.0f,130.0f,14.0f
        };

        g.setColour(colour.withAlpha(0.10f + meter * 0.18f));
        g.fillRoundedRectangle(heater.expanded(10),14.0f);

        g.setColour(colour.withAlpha(0.4f + meter * 0.46f));
        g.fillRoundedRectangle(heater,7.0f);
    }

    // Rotating microwave platter.
    g.setColour(colour.withAlpha(0.15f));
    g.fillEllipse(285,635,575,51);

    const float turn =
        t * (1.2f + processor.getPower() * 1.6f);

    for (int i = 0; i < 4; ++i)
    {
        const float a = turn + i * pi / 2.0f;

        g.setColour(colour.withAlpha(0.13f));

        g.drawLine(
            570,658,
            570 + std::cos(a) * 210,
            658 + std::sin(a) * 18,
            5.0f);
    }

    // Steam and defrost animation.
    if (steamy || defrosted)
    {
        for (int i = 0; i < 17; ++i)
        {
            const float v = std::fmod(
                t * (steamy ? 0.24f : 0.09f) +
                i * 0.117f,1.0f);

            const float x =
                190 + i * 46 + 18 * std::sin(v * 13 + i);

            const float y = 665 - 315 * v;
            const float size = 9 + 35 * v;

            g.setColour(
                (defrosted ? icy : juce::Colours::whitesmoke)
                    .withAlpha(0.09f * (1.0f - v)));

            g.fillEllipse(x,y,size,size);
        }
    }

    // Bass pulse.
    if (boosted)
    {
        const float pulse =
            std::sin(t * 8.0f) * 0.5f + 0.5f;

        for (int i = 0; i < 3; ++i)
        {
            const float r =
                90 + i * 58 + pulse * 45;

            g.setColour(orange.withAlpha(
                (0.20f - i * 0.045f) * (0.3f + meter)));

            g.drawEllipse(
                562 - r,cy - r * 0.45f,
                r * 2,r * 0.9f,3);
        }
    }

    // Live waveform from the audio processor.
    juce::Path waveform;

    for (int i = 0; i < 170; ++i)
    {
        const float x = 180 + i * 4.63f;
        const float sample =
            processor.scopeAt(head - (170 - i) * 2);

        float y = cy - sample * height * 1.8f;

        if (warped)
            y += 26 * intensity *
                std::sin(t * 5 + i * 0.115f);

        if (distorted)
            y = cy + (y < cy ? -1.0f : 1.0f) *
                juce::jmin(85.0f,std::abs(y - cy) * 1.6f);

        if (overcooked)
            y = std::round(y / 14.0f) * 14.0f;

        if (compressed)
            y = cy + (y - cy) * 0.48f;

        if (glitched && (tick / 8) % 4 == 0 &&
            i > 60 && i < 124)
            y += (i / 11) % 2 ? 43.0f : -30.0f;

        if (i == 0)
            waveform.startNewSubPath(x,y);
        else
            waveform.lineTo(x,y);
    }

    if (echoing)
    {
        g.saveState();

        g.addTransform(
            juce::AffineTransform::translation(-17,28));

        glowLine(g,waveform,colour,0.22f);
        g.restoreState();
    }

    glowLine(g,waveform,colour,0.67f + 0.24f * meter);

    // Glitch interference lines.
    if (glitched && tick % 34 < 8)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float y = 370 + i * 42;

            const float x = 300 + std::fmod(
                (float)(tick * 31 + i * 113),380.0f);

            g.setColour(colour.withAlpha(0.17f));
            g.fillRect(x,y,125.0f + (i % 3) * 54.0f,3.0f);
        }
    }

    if (overcooked && tick % 86 < 4)
    {
        g.setColour(juce::Colours::white.withAlpha(
            0.08f * intensity));

        g.fillRoundedRectangle(
            chamber.reduced(11),17.0f);
    }

    if (animPulse > 0.015f)
    {
        g.setColour(colour.withAlpha(
            0.15f * animPulse));

        g.fillRoundedRectangle(chamber,24.0f);
    }

    g.setColour(colour.withAlpha(0.8f));
    g.setFont(juce::Font("Consolas",17.0f,juce::Font::bold));

    g.drawText(
        juce::String(count) +
        (count == 1 ? " EFFECT ACTIVE" : " EFFECTS ACTIVE"),
        176,691,330,22,
        juce::Justification::centredLeft);
}

void LTAMicrowaveAudioProcessorEditor::renderButtons(
    juce::Graphics& g, bool running)
{
    for (int i = 0;
         i < LTAMicrowaveAudioProcessor::effectCount;
         ++i)
    {
        const auto r = buttons[(size_t) i];
        const bool selected = processor.effectEnabled(i);
        const bool held = pressedEffect == i;

        const float pulse = flash[(size_t) i];

        // Button moves down 5 pixels when physically pressed.
        const float depression = held
            ? 5.0f
            : pulse * 4.0f;

        const auto face = r.reduced(4.0f)
                           .translated(0.0f,depression);

        // Deep recess.
        g.setColour(juce::Colour(0xff090807));
        g.fillRoundedRectangle(
            r.reduced(2.0f).translated(0,5),
            12.0f);

        // Bottom shadow disappears when depressed.
        g.setColour(juce::Colours::black.withAlpha(
            0.56f * (1.0f - depression / 6.0f)));

        g.fillRoundedRectangle(
            face.translated(0,6.0f),
            10.0f);

        // Physical dark plastic surface.
        g.setColour(selected
            ? juce::Colour(0xff57271e)
            : juce::Colour(0xff292725));

        g.fillRoundedRectangle(face,9.0f);

        // Raised top bevel.
        g.setColour(juce::Colour(0xffb5aaa0).withAlpha(
            selected ? 0.28f : 0.44f));

        g.drawLine(
            face.getX() + 11,
            face.getY() + 3,
            face.getRight() - 11,
            face.getY() + 3,
            held ? 1.0f : 2.5f);

        // Lower bevel.
        g.setColour(juce::Colours::black.withAlpha(0.70f));

        g.drawLine(
            face.getX() + 10,
            face.getBottom() - 3,
            face.getRight() - 10,
            face.getBottom() - 3,
            held ? 1.0f : 3.0f);

        // LED lamp.
        g.setColour(selected
            ? orange
            : juce::Colour(0xff504944));

        g.fillRoundedRectangle(
            face.getX() + 13,
            face.getY() + 7,
            23,4,2);

        // Light emitted by selected effect.
        if (selected)
        {
            g.setColour(orange.withAlpha(
                0.12f + pulse * 0.25f));

            g.fillRoundedRectangle(
                face.reduced(3.0f),7.0f);
        }

        // Repaint text so it moves with the button.
        g.setColour(selected
            ? juce::Colour(0xffffc1b0)
            : juce::Colour(0xffe7dfd5));

        g.setFont(juce::Font(
            "Arial",18.0f,juce::Font::bold));

        g.drawFittedText(
            names[(size_t) i],
            face.toNearestInt().reduced(8,10),
            juce::Justification::centred,
            1);

        g.setColour(selected
            ? orange.withAlpha(0.65f)
            : juce::Colour(0xff514b47));

        g.drawRoundedRectangle(face,9.0f,1.5f);
    }

    // START and STOP illumination.
    if (running)
    {
        g.setColour(
            juce::Colour(0xffb6f267).withAlpha(0.20f));

        g.fillRoundedRectangle(
            startRect.reduced(11),11.0f);
    }
    else
    {
        g.setColour(orange.withAlpha(0.22f));
        g.fillRoundedRectangle(
            stopRect.reduced(11),11.0f);
    }
}

void LTAMicrowaveAudioProcessorEditor::paint(
    juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171716));

    g.saveState();

    g.addTransform(juce::AffineTransform::scale(
        (float)getWidth() / W,
        (float)getHeight() / H));

    if (microwaveBackground.isValid())
    {
        g.drawImage(
            microwaveBackground,
            0,0,(int)W,(int)H,
            0,0,
            microwaveBackground.getWidth(),
            microwaveBackground.getHeight());
    }

    int activeCount = 0;

    for (int i = 0;
         i < LTAMicrowaveAudioProcessor::effectCount;
         ++i)
    {
        if (processor.effectEnabled(i))
            ++activeCount;
    }

    const bool running = processor.isRunning();

    renderWindow(g,running,activeCount);
    renderScreen(g,running,activeCount);
    renderButtons(g,running);

    // Power dial progress ring.
    const float power = processor.getPower();

    const float angle =
        (-145.0f + power * 290.0f) * pi / 180.0f;

    juce::Path arc;

    arc.addCentredArc(
        knobCentre.x,knobCentre.y,
        87,87,0,
        -145.0f * pi / 180.0f,
        angle,true);

    g.setColour(orange.withAlpha(0.75f));
    g.strokePath(arc,juce::PathStrokeType(6.0f));

    const float x =
        knobCentre.x + std::sin(angle) * 54;

    const float y =
        knobCentre.y - std::cos(angle) * 54;

    g.setColour(juce::Colour(0xffecddd0));
    g.fillEllipse(x - 5,y - 5,10,10);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font("Consolas",15.0f,juce::Font::bold));

    g.drawText(
        "v1.1",
        1519,870,85,23,
        juce::Justification::centredRight);

    g.restoreState();
}
