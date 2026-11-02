
#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <array>

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

const juce::Rectangle<float> startRect { 1407, 650, 199, 89 };
const juce::Rectangle<float> stopRect { 1407, 746, 199, 83 };
const juce::Rectangle<float> lcdRect { 1174, 138, 394, 196 };
const juce::Rectangle<float> mixRect { 1397, 284, 169, 44 };
const juce::Point<float> knobCentre { 1274.0f, 727.0f };

bool anyActive(const LTAMicrowaveAudioProcessor& proc)
{
    for (int i = 0; i < LTAMicrowaveAudioProcessor::effectCount; ++i)
        if (proc.effectEnabled(i))
            return true;

    return false;
}

void neonLine(juce::Graphics& g,
              const juce::Path& path,
              juce::Colour colour,
              float opacity)
{
    g.setColour(colour.withMultipliedAlpha(opacity * 0.13f));
    g.strokePath(path, juce::PathStrokeType(10.0f));

    g.setColour(colour.withMultipliedAlpha(opacity * 0.45f));
    g.strokePath(path, juce::PathStrokeType(4.0f));

    g.setColour(colour.withMultipliedAlpha(opacity));
    g.strokePath(path, juce::PathStrokeType(1.8f));
}
}

LTAMicrowaveAudioProcessorEditor::LTAMicrowaveAudioProcessorEditor(
    LTAMicrowaveAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p)
{
    microwaveBackground =
        juce::ImageFileFormat::loadFrom(
            BinaryData::microwave_bg_png,
            BinaryData::microwave_bg_pngSize);

    setOpaque(true);
    setSize(1170, 658);
    setResizable(true, true);
    setResizeLimits(840, 473, 1672, 941);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(
            static_cast<double>(artW) / artH);

    startTimerHz(30);
}

void LTAMicrowaveAudioProcessorEditor::resized()
{
}

juce::Point<float>
LTAMicrowaveAudioProcessorEditor::toArtSpace(
    juce::Point<float> point) const
{
    return {
        point.x * artW / static_cast<float>(getWidth()),
        point.y * artH / static_cast<float>(getHeight())
    };
}

int LTAMicrowaveAudioProcessorEditor::effectAt(
    juce::Point<float> point) const
{
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i)
    {
        if (buttons[static_cast<size_t>(i)].contains(point))
            return i;
    }

    return -1;
}

void LTAMicrowaveAudioProcessorEditor::flashEffect(int index)
{
    lastEffect = index;
    animPulse = 1.0f;
    lastPressTime = juce::Time::getMillisecondCounterHiRes();

    if (index >= 0 &&
        index < LTAMicrowaveAudioProcessor::effectCount)
    {
        flash[static_cast<size_t>(index)] = 1.0f;
    }
}

void LTAMicrowaveAudioProcessorEditor::mouseDown(
    const juce::MouseEvent& e)
{
    const auto point = toArtSpace(e.position);
    pressedEffect = effectAt(point);

    if (pressedEffect >= 0)
    {
        processor.setEffect(
            pressedEffect,
            !processor.effectEnabled(pressedEffect));

        flashEffect(pressedEffect);
    }
    else if (startRect.contains(point))
    {
        processor.setRunning(true);
        flashEffect(-1);
    }
    else if (stopRect.contains(point))
    {
        processor.setRunning(false);
        flashEffect(-1);
    }
    else if (point.getDistanceFrom(knobCentre) < 85.0f)
    {
        draggingKnob = true;
        draggingMix = e.mods.isShiftDown();
        startDragY = point.y;

        startDragValue = draggingMix
            ? processor.getMix()
            : processor.getPower();
    }
    else if (mixRect.contains(point))
    {
        draggingKnob = true;
        draggingMix = true;
        startDragY = point.y;
        startDragValue = processor.getMix();
    }

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::adjustKnob(
    float y, bool mix)
{
    const float value = juce::jlimit(
        0.0f,
        1.0f,
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
    const auto point = toArtSpace(e.position);

    if (wheel.deltaY == 0.0f)
        return;

    const float delta = wheel.deltaY > 0.0f
        ? 0.035f
        : -0.035f;

    if (mixRect.contains(point) ||
        (point.getDistanceFrom(knobCentre) < 85.0f &&
         e.mods.isShiftDown()))
    {
        processor.setMix(
            juce::jlimit(0.0f, 1.0f, processor.getMix() + delta));
    }
    else if (point.getDistanceFrom(knobCentre) < 95.0f)
    {
        processor.setPower(
            juce::jlimit(0.0f, 1.0f, processor.getPower() + delta));
    }

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::timerCallback()
{
    ++tick;

    if (processor.isRunning() && anyActive(processor))
        cookSeconds += 1.0f / 30.0f;

    for (auto& value : flash)
        value *= 0.87f;

    animPulse *= 0.88f;

    repaint();
}

void LTAMicrowaveAudioProcessorEditor::renderScreen(
    juce::Graphics& g,
    bool running,
    int count)
{
    g.setColour(panel.withAlpha(0.98f));
    g.fillRoundedRectangle(lcdRect, 15.0f);

    g.setColour(lcd.withAlpha(0.15f));
    g.drawRoundedRectangle(
        lcdRect.reduced(2.0f), 13.0f, 2.0f);

    const bool cooking = running && count > 0;

    const juce::String mode = cooking
        ? "COOKING"
        : running ? "READY" : "STOPPED";

    const int minutes =
        (static_cast<int>(cookSeconds) / 60) % 100;

    const int seconds =
        static_cast<int>(cookSeconds) % 60;

    const juce::String time =
        juce::String::formatted(
            "%02d:%02d", minutes, seconds);

    g.setColour(lcd.withAlpha(0.78f));
    g.setFont(juce::Font("Consolas", 20.0f,
                          juce::Font::bold));

    g.drawText(
        "LTA / MULTI-FX",
        1194, 154, 325, 26,
        juce::Justification::centredLeft);

    g.setFont(juce::Font("Consolas", 27.0f,
                          juce::Font::bold));

    g.drawText(
        mode,
        1189, 192, 365, 39,
        juce::Justification::centred);

    g.setFont(juce::Font("Consolas", 39.0f,
                          juce::Font::bold));

    g.setColour(cooking ? lcd : lcd.withAlpha(0.68f));

    g.drawText(
        time,
        1202, 235, 332, 47,
        juce::Justification::centred);

    g.setColour(lcd.withAlpha(0.95f));
    g.setFont(juce::Font("Consolas", 17.0f,
                          juce::Font::bold));

    const int power =
        static_cast<int>(std::round(
            processor.getPower() * 100.0f));

    const int mix =
        static_cast<int>(std::round(
            processor.getMix() * 100.0f));

    g.drawText(
        "POWER " + juce::String(power) + "%",
        1190, 286, 214, 30,
        juce::Justification::centredLeft);

    g.drawText(
        "MIX " + juce::String(mix) + "%",
        1395, 286, 168, 30,
        juce::Justification::centredRight);

    const float level = juce::jlimit(
        0.0f, 1.0f, processor.meter());

    g.setColour(lcd.withAlpha(0.65f));

    for (int i = 0; i < 24; ++i)
    {
        if (i < static_cast<int>(level * 25.0f))
        {
            const float x = 1188.0f + i * 15.4f;
            g.fillRect(x, 183.0f, 9.0f, 4.0f);
        }
    }
}

void LTAMicrowaveAudioProcessorEditor::renderWindow(
    juce::Graphics& g,
    bool running,
    int count)
{
    const juce::Rectangle<float> chamber {
        118.0f, 268.0f, 895.0f, 458.0f
    };

    const bool cooking = running && count > 0;

    const float meter = juce::jlimit(
        0.0f, 1.0f, processor.meter());

    const float intensity = cooking
        ? 0.25f + 0.75f * processor.getPower()
        : 0.0f;

    g.setColour(dark.withAlpha(
        cooking ? 0.75f : 0.91f));

    g.fillRoundedRectangle(chamber, 24.0f);

    if (!cooking)
    {
        g.setColour(
            juce::Colour(0xffa2bdb3).withAlpha(0.26f));

        g.setFont(
            juce::Font("Consolas", 23.0f,
                       juce::Font::bold));

        g.drawText(
            "INSERT AUDIO / SELECT FX / START",
            195, 447, 745, 55,
            juce::Justification::centred);

        return;
    }

    const bool distorted =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::distort);

    const bool overcooked =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::overcook);

    const bool warped =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::warp);

    const bool glitched =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::glitch);

    const bool defrosted =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::defrost);

    const bool boosted =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::bass);

    const bool echoing =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::reheat);

    const bool steamy =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::steam);

    const bool compressed =
        processor.effectEnabled(
            LTAMicrowaveAudioProcessor::compress);

    const juce::Colour mainColour =
        defrosted ? icy :
        overcooked ? juce::Colours::yellow :
        orange;

    const float animationTime =
        static_cast<float>(tick) / 30.0f;

    const float centreY = 505.0f;
    const int head = processor.scopeHead();

    const float height =
        (75.0f + meter * 135.0f) * intensity;

    // Animated heating elements
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<float> heater {
            260.0f + i * 193.0f,
            314.0f,
            130.0f,
            14.0f
        };

        g.setColour(mainColour.withAlpha(
            0.10f + meter * 0.18f));

        g.fillRoundedRectangle(
            heater.expanded(10.0f), 14.0f);

        g.setColour(mainColour.withAlpha(
            0.4f + meter * 0.46f));

        g.fillRoundedRectangle(heater, 7.0f);
    }

    // Rotating microwave plate
    g.setColour(mainColour.withAlpha(0.15f));

    g.fillEllipse(
        285.0f, 635.0f, 575.0f, 51.0f);

    const float turn =
        animationTime *
        (1.2f + processor.getPower() * 1.6f);

    for (int i = 0; i < 4; ++i)
    {
        const float angle =
            turn + i * pi / 2.0f;

        g.setColour(mainColour.withAlpha(0.13f));

        g.drawLine(
            570.0f,
            658.0f,
            570.0f + std::cos(angle) * 210.0f,
            658.0f + std::sin(angle) * 18.0f,
            5.0f);
    }

    // Steam and defrost particles
    if (steamy || defrosted)
    {
        for (int i = 0; i < 17; ++i)
        {
            const float speed =
                steamy ? 0.24f : 0.09f;

            const float t = std::fmod(
                animationTime * speed +
                i * 0.117f,
                1.0f);

            const float x =
                190.0f + i * 46.0f +
                18.0f * std::sin(t * 13.0f + i);

            const float y =
                665.0f - 315.0f * t;

            const float size =
                9.0f + 35.0f * t;

            const auto colour = defrosted
                ? icy
                : juce::Colours::whitesmoke;

            g.setColour(colour.withAlpha(
                0.09f * (1.0f - t)));

            g.fillEllipse(x, y, size, size);
        }
    }

    // Bass pulse animation
    if (boosted)
    {
        const float pulse =
            std::sin(animationTime * 8.0f) *
            0.5f + 0.5f;

        for (int i = 0; i < 3; ++i)
        {
            const float radius =
                90.0f + i * 58.0f +
                pulse * 45.0f;

            g.setColour(orange.withAlpha(
                (0.20f - i * 0.045f) *
                (0.3f + meter)));

            g.drawEllipse(
                562.0f - radius,
                centreY - radius * 0.45f,
                radius * 2.0f,
                radius * 0.9f,
                3.0f);
        }
    }

    // Real incoming audio waveform
    juce::Path waveform;
    constexpr int points = 170;

    for (int i = 0; i < points; ++i)
    {
        const float x =
            180.0f + i * 4.63f;

        const float sample =
            processor.scopeAt(
                head - (points - i) * 2);

        float y =
            centreY - sample * height * 1.8f;

        if (warped)
        {
            y += 26.0f * intensity *
                std::sin(
                    animationTime * 5.0f +
                    i * 0.115f);
        }

        if (distorted)
        {
            const float direction =
                y < centreY ? -1.0f : 1.0f;

            y = centreY + direction *
                juce::jmin(
                    85.0f,
                    std::abs(y - centreY) * 1.6f);
        }

        if (overcooked)
            y = std::round(y / 14.0f) * 14.0f;

        if (compressed)
            y = centreY + (y - centreY) * 0.48f;

        if (glitched &&
            (tick / 4) % 4 == 0 &&
            i > 60 && i < 124)
        {
            y += (i / 11) % 2
                ? 43.0f
                : -30.0f;
        }

        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    // Delay echo visual trail
    if (echoing)
    {
        g.saveState();

        g.addTransform(
            juce::AffineTransform::translation(
                -17.0f, 28.0f));

        neonLine(
            g, waveform,
            mainColour, 0.22f);

        g.restoreState();
    }

    neonLine(
        g, waveform, mainColour,
        0.67f + 0.24f * meter);

    // Glitch interference
    if (glitched && tick % 17 < 4)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float y =
                370.0f + i * 42.0f;

            const float x =
                300.0f +
                std::fmod(
                    static_cast<float>(
                        tick * 31 + i * 113),
                    380.0f);

            g.setColour(
                mainColour.withAlpha(0.17f));

            g.fillRect(
                x, y,
                125.0f + (i % 3) * 54.0f,
                3.0f);
        }
    }

    // Overcook flashing
    if (overcooked && tick % 43 < 2)
    {
        g.setColour(
            juce::Colours::white.withAlpha(
                0.08f * intensity));

        g.fillRoundedRectangle(
            chamber.reduced(11.0f), 17.0f);
    }

    // Button press visual feedback
    if (animPulse > 0.015f)
    {
        g.setColour(
            mainColour.withAlpha(
                0.15f * animPulse));

        g.fillRoundedRectangle(
            chamber.reduced(2.0f), 24.0f);
    }

    g.setColour(
        mainColour.withAlpha(0.8f));

    g.setFont(
        juce::Font("Consolas", 17.0f,
                   juce::Font::bold));

    g.drawText(
        juce::String(count) +
        (count == 1
            ? " EFFECT ACTIVE"
            : " EFFECTS ACTIVE"),
        176, 691, 330, 22,
        juce::Justification::centredLeft);
}

void LTAMicrowaveAudioProcessorEditor::renderButtons(
    juce::Graphics& g,
    bool running)
{
    for (int i = 0;
         i < LTAMicrowaveAudioProcessor::effectCount;
         ++i)
    {
        const auto rect =
            buttons[static_cast<size_t>(i)];

        const bool on =
            processor.effectEnabled(i);

        const float pulse =
            flash[static_cast<size_t>(i)];

        if (on || pulse > 0.05f)
        {
            const float alpha =
                (on ? 0.18f : 0.0f) +
                pulse * 0.22f;

            g.setColour(
                orange.withAlpha(alpha));

            g.fillRoundedRectangle(
                rect.reduced(7.0f), 10.0f);
        }

        g.setColour(
            on ? juce::Colour(0xffff5d43)
               : juce::Colour(0xff34342b));

        g.fillRoundedRectangle(
            rect.getX() + 17.0f,
            rect.getY() + 6.0f,
            19.0f, 5.0f, 2.5f);

        if (on)
        {
            g.setColour(
                orange.withAlpha(
                    0.34f + pulse * 0.4f));

            g.fillEllipse(
                rect.getX() + 17.0f,
                rect.getY() + 3.0f,
                21.0f, 10.0f);
        }

        if (pressedEffect == i)
        {
            g.setColour(
                juce::Colours::black.withAlpha(
                    0.15f));

            g.fillRoundedRectangle(
                rect.reduced(3.0f), 11.0f);
        }
    }

    // Start and stop indicators
    if (running)
    {
        g.setColour(
            juce::Colour(0xffb6f267)
                .withAlpha(0.20f));

        g.fillRoundedRectangle(
            startRect.reduced(11.0f), 11.0f);
    }
    else
    {
        g.setColour(
            orange.withAlpha(0.22f));

        g.fillRoundedRectangle(
            stopRect.reduced(11.0f), 11.0f);
    }
}

void LTAMicrowaveAudioProcessorEditor::paint(
    juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171716));

    g.saveState();

    g.addTransform(
        juce::AffineTransform::scale(
            static_cast<float>(getWidth()) / artW,
            static_cast<float>(getHeight()) / artH));

    // Microwave artwork
    if (microwaveBackground.isValid())
    {
        g.drawImage(
            microwaveBackground,
            0, 0,
            static_cast<int>(artW),
            static_cast<int>(artH),
            0, 0,
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

    renderWindow(g, running, activeCount);
    renderScreen(g, running, activeCount);
    renderButtons(g, running);

    // Physical power dial indicator
    const float power = processor.getPower();

    const float startAngle =
        -145.0f * pi / 180.0f;

    const float angle =
        (-145.0f + power * 290.0f) *
        pi / 180.0f;

    juce::Path arc;

    arc.addCentredArc(
        knobCentre.x,
        knobCentre.y,
        87.0f, 87.0f,
        0.0f,
        startAngle,
        angle,
        true);

    g.setColour(
        orange.withAlpha(0.75f));

    g.strokePath(
        arc,
        juce::PathStrokeType(6.0f));

    const float x =
        knobCentre.x +
        std::sin(angle) * 54.0f;

    const float y =
        knobCentre.y -
        std::cos(angle) * 54.0f;

    g.setColour(
        juce::Colour(0xffecddd0));

    g.fillEllipse(
        x - 5.0f,
        y - 5.0f,
        10.0f, 10.0f);

    g.setColour(
        juce::Colours::white.withAlpha(0.5f));

    g.setFont(
        juce::Font("Consolas", 15.0f,
                   juce::Font::bold));

    g.drawText(
        "v1.0",
        1519, 870, 85, 23,
        juce::Justification::centredRight);

    g.restoreState();
}
