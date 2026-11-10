#include "PluginEditor.h"

namespace
{
    juce::String noteName (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int pc = ((midi % 12) + 12) % 12;
        return juce::String (names[pc]) + juce::String (midi / 12 - 1);
    }

    // soft neon glow: a few fat low-alpha strokes under the real one
    void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float core)
    {
        g.setColour (c.withAlpha (0.07f));
        g.strokePath (p, juce::PathStrokeType (core * 7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.15f));
        g.strokePath (p, juce::PathStrokeType (core * 4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (core * 2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (core, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ============================================================================
//  Look and feel
// ============================================================================
SonyLookAndFeel::SonyLookAndFeel()
{
    setColour (juce::ComboBox::textColourId,       sty::text);
    setColour (juce::PopupMenu::backgroundColourId,            sty::panel);
    setColour (juce::PopupMenu::textColourId,                  sty::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, sty::accent);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour (juce::TextButton::textColourOffId,  sty::text.withAlpha (0.75f));
    setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId,    sty::cyan);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            sty::text);
}

void SonyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (8.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float arcR   = radius - 3.f;

    // dark brushed body
    const float bodyR = radius - 14.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1748), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff0b0516), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.f, bodyR * 2.f);
    g.setColour (sty::accent.withAlpha (0.35f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.f, bodyR * 2.f, 1.2f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff241440));
    g.strokePath (track, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: purple -> pink, with glow
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, angle, true);
    g.setColour (sty::neon.withAlpha (0.18f));
    g.strokePath (value, juce::PathStrokeType (13.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (sty::accent, centre.x - arcR, centre.y,
                                             sty::neon,   centre.x + arcR, centre.y, false));
    g.strokePath (value, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer dot
    const float dotR = bodyR - 7.f;
    const float dx = centre.x + std::sin (angle) * dotR;
    const float dy = centre.y - std::cos (angle) * dotR;
    g.setColour (sty::cyan.withAlpha (0.3f));
    g.fillEllipse (dx - 6.f, dy - 6.f, 12.f, 12.f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (dx - 2.5f, dy - 2.5f, 5.f, 5.f);
}

void SonyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();

    if (on)
    {
        g.setColour (sty::neon.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.f), 11.f);
        g.setGradientFill (juce::ColourGradient (sty::accent, r.getX(), r.getY(),
                                                 sty::neon,   r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 9.f);
    }
    else
    {
        g.setColour (sty::panel.brighter (down ? 0.25f : (highlighted ? 0.12f : 0.f)));
        g.fillRoundedRectangle (r, 9.f);
        g.setColour (sty::accent.withAlpha (highlighted ? 0.8f : 0.4f));
        g.drawRoundedRectangle (r, 9.f, 1.2f);
    }

    // LED
    const auto led = juce::Rectangle<float> (r.getX() + 11.f, r.getCentreY() - 4.f, 8.f, 8.f);
    if (on)
    {
        g.setColour (sty::cyan.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.f));
        g.setColour (juce::Colours::white);
    }
    else
        g.setColour (sty::accent.withAlpha (0.35f));
    g.fillEllipse (led);
}

void SonyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height).reduced (1.5f);
    g.setColour (sty::panel);
    g.fillRoundedRectangle (r, 9.f);
    g.setColour (sty::accent.withAlpha (box.isPopupActive() ? 1.f : 0.5f));
    g.drawRoundedRectangle (r, 9.f, 1.2f);

    juce::Path arrow;
    const float cx = (float) width - 18.f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 5.f, cy - 3.f, cx + 5.f, cy - 3.f, cx, cy + 4.f);
    g.setColour (sty::cyan);
    g.fillPath (arrow);
}

// ============================================================================
//  Pitch view
// ============================================================================
PitchView::PitchView (SonyTuneAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void PitchView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // ---- screen background, brightens a little with the voice
    const float lvl = juce::jlimit (0.f, 1.f, proc.outLevel.load (std::memory_order_relaxed) * 1.6f);
    glow += (lvl - glow) * 0.25f;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c0a34), b.getX(), b.getY(),
                                             juce::Colour (0xff050210), b.getRight(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 14.f);
    g.setGradientFill (juce::ColourGradient (sty::neon.withAlpha (0.05f + 0.12f * glow), b.getCentreX(), b.getBottom(),
                                             sty::neon.withAlpha (0.f), b.getCentreX(), b.getY(), false));
    g.fillRoundedRectangle (b, 14.f);

    // faint scanlines
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (float yy = b.getY() + 2.f; yy < b.getBottom(); yy += 4.f)
        g.drawHorizontalLine ((int) yy, b.getX() + 8.f, b.getRight() - 8.f);

    const auto area = b.reduced (6.f).withTrimmedLeft (44.f).reduced (0.f, 10.f);

    // ---- copy the history out of the audio engine (oldest -> newest)
    constexpr int N = sonytune::Engine::kVizSize;
    float raw[N], out[N];
    const int w = proc.engine.vizWrite.load (std::memory_order_acquire);
    for (int i = 0; i < N; ++i)
    {
        const int src = (((w - N + i) % N) + N) % N;
        raw[i] = proc.engine.vizRaw[(size_t) src].load (std::memory_order_relaxed);
        out[i] = proc.engine.vizOut[(size_t) src].load (std::memory_order_relaxed);
    }

    // ---- vertical range follows the singer
    float mn = 1.0e9f, mx = -1.0e9f;
    for (int i = 0; i < N; ++i)
    {
        if (raw[i] >= 0.f) { mn = juce::jmin (mn, raw[i], out[i]); mx = juce::jmax (mx, raw[i], out[i]); }
    }
    if (mx >= mn)
    {
        float tLo = mn - 3.f, tHi = mx + 3.f;
        if (tHi - tLo < 16.f) { const float c = 0.5f * (tHi + tLo); tLo = c - 8.f; tHi = c + 8.f; }
        lo += (tLo - lo) * 0.08f;
        hi += (tHi - hi) * 0.08f;
    }

    auto toY = [&] (float m) { return area.getBottom() - (m - lo) / (hi - lo) * area.getHeight(); };

    // ---- highlight the lane the output is locked to right now
    if (out[N - 1] >= 0.f)
    {
        const float lane = std::round (out[N - 1]);
        const float y0 = toY (lane + 0.5f), y1 = toY (lane - 0.5f);
        g.setGradientFill (juce::ColourGradient (sty::neon.withAlpha (0.0f), area.getX(), 0.f,
                                                 sty::neon.withAlpha (0.16f + 0.1f * glow), area.getRight(), 0.f, false));
        g.fillRect (area.getX(), y0, area.getWidth(), y1 - y0);
    }

    // ---- note grid: notes that are IN the chosen key/scale are bright
    const int key  = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int mask = sonytune::scaleMask ((int) proc.apvts.getRawParameterValue ("scale")->load());
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    for (int m = (int) std::ceil (lo); m <= (int) std::floor (hi); ++m)
    {
        const bool allowed = sonytune::isAllowedNote (m, key, mask);
        const float y = toY ((float) m);
        g.setColour (sty::accent.withAlpha (allowed ? 0.40f : 0.06f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        if (allowed)
        {
            g.setColour (sty::cyan.withAlpha (0.75f));
            g.drawText (noteName (m), (int) b.getX() + 4, (int) y - 8, 38, 16, juce::Justification::centredRight);
        }
    }

    // ---- traces
    auto buildPath = [&] (const float* v)
    {
        juce::Path p;
        bool pen = false;
        for (int i = 0; i < N; ++i)
        {
            if (v[i] < 0.f) { pen = false; continue; }
            const float x = area.getX() + area.getWidth() * (float) i / (float) (N - 1);
            if (! pen) { p.startNewSubPath (x, toY (v[i])); pen = true; }
            else         p.lineTo (x, toY (v[i]));
        }
        return p;
    };

    const auto rawPath = buildPath (raw);
    const auto outPath = buildPath (out);

    g.setColour (sty::rawLine.withAlpha (0.6f));
    g.strokePath (rawPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    glowStroke (g, outPath, sty::neon, 2.3f);

    // ---- "lock" indicator at the head of the line (pulses with the voice)
    if (out[N - 1] >= 0.f)
    {
        const float y = toY (out[N - 1]);
        const float pulse = 1.f + 0.25f * std::sin (phase * 5.f) + 0.9f * glow;
        const float r1 = 9.f * pulse;
        g.setColour (sty::neon.withAlpha (0.28f));
        g.fillEllipse (area.getRight() - r1 + 2.f, y - r1, r1 * 2.f, r1 * 2.f);
        g.setColour (sty::cyan.withAlpha (0.6f));
        g.drawEllipse (area.getRight() - r1 + 2.f, y - r1, r1 * 2.f, r1 * 2.f, 1.2f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (area.getRight() - 2.f, y - 4.f, 8.f, 8.f);
        g.setColour (sty::text);
        g.setFont (juce::Font (juce::FontOptions (14.f, juce::Font::bold)));
        g.drawText (noteName ((int) std::lround (out[N - 1])), (int) area.getRight() - 76, (int) y - 30, 60, 16,
                    juce::Justification::centredRight);
    }

    // ---- latency read-out
    const double sr = juce::jmax (1.0, proc.getSampleRate());
    g.setColour (sty::text.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    g.drawText ("LATENCY " + juce::String (1000.0 * proc.engine.getLatencySamples() / sr, 1) + " ms",
                (int) b.getRight() - 150, (int) b.getY() + 6, 140, 14, juce::Justification::centredRight);

    g.setColour (sty::accent.withAlpha (0.45f));
    g.drawRoundedRectangle (b.reduced (0.5f), 14.f, 1.2f);
}

// ============================================================================
//  Editor
// ============================================================================
SonyTuneAudioProcessorEditor::SonyTuneAudioProcessorEditor (SonyTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pitchView (p)
{
    setLookAndFeel (&laf);
    setSize (900, 600);

    addAndMakeVisible (pitchView);

    keyBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (int i = 0; i < sonytune::kNumScales; ++i) scaleBox.addItem (sonytune::scaleName (i), i + 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "key",   keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);

    for (auto* btn : { &fastButton, &hqButton })
    {
        btn->setClickingTogglesState (true);
        addAndMakeVisible (*btn);
    }
    fastAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "fast", fastButton);
    hqAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "hq",   hqButton);

    setupKnob (speed,    "SPEED",    "speed",    " ms");
    setupKnob (amount,   "AMOUNT",   "amount",   " %");
    setupKnob (humanize, "HUMANIZE", "humanize", " %");
    setupKnob (formant,  "FORMANT",  "formant",  " st");
    setupKnob (mix,      "MIX",      "mix",      " %");

    startTimerHz (30);
}

SonyTuneAudioProcessorEditor::~SonyTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SonyTuneAudioProcessorEditor::timerCallback()
{
    phase += 0.03f;
    const float li = juce::jlimit (0.f, 1.f, proc.inLevel.load (std::memory_order_relaxed)  * 1.6f);
    const float lq = juce::jlimit (0.f, 1.f, proc.outLevel.load (std::memory_order_relaxed) * 1.6f);
    level    += (li - level)    * 0.3f;
    levelOut += (lq - levelOut) * 0.3f;
    repaint();
}

void SonyTuneAudioProcessorEditor::setupKnob (Knob& k, const juce::String& title, const juce::String& paramId,
                                              const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, sty::text.withAlpha (0.9f));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void SonyTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // ---- background
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0420), 0.f, 0.f,
                                             sty::bg, 0.f, full.getBottom(), false));
    g.fillAll();

    // slow drifting colour blobs, swell with the voice
    const float bx = full.getCentreX() + std::sin (phase) * 260.f;
    g.setGradientFill (juce::ColourGradient (sty::accent.withAlpha (0.14f + 0.10f * level), bx, 90.f,
                                             sty::accent.withAlpha (0.f), bx, 340.f, true));
    g.fillRect (full);
    const float bx2 = full.getCentreX() - std::sin (phase * 0.7f) * 300.f;
    g.setGradientFill (juce::ColourGradient (sty::neon.withAlpha (0.07f + 0.08f * levelOut), bx2, full.getBottom() - 40.f,
                                             sty::neon.withAlpha (0.f), bx2, full.getBottom() - 330.f, true));
    g.fillRect (full);

    // faint grid
    g.setColour (sty::accent.withAlpha (0.05f));
    for (int x = 0; x < getWidth(); x += 30) g.drawVerticalLine (x, 0.f, full.getBottom());
    for (int y = 0; y < getHeight(); y += 30) g.drawHorizontalLine (y, 0.f, full.getRight());

    // ---- logo with glow
    const juce::Rectangle<int> logoArea (24, 8, 420, 46);
    const juce::String logo = juce::String (JucePlugin_Name).toUpperCase();
    g.setFont (juce::Font (juce::FontOptions (40.f, juce::Font::bold)));
    g.setColour (sty::neon.withAlpha (0.10f + 0.12f * levelOut));
    for (int dx = -2; dx <= 2; ++dx)
        for (int dy = -2; dy <= 2; ++dy)
            g.drawText (logo, logoArea.translated (dx, dy), juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (sty::text, 24.f, 8.f, sty::neon, 260.f, 54.f, false));
    g.drawText (logo, logoArea, juce::Justification::centredLeft);

    g.setColour (sty::cyan);
    g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
    g.drawText (sty::tagline(), 27, 52, 400, 16, juce::Justification::centredLeft);

    // ---- voice orb (top right): rings pulse with input, core glows with output
    const auto c = orbArea.toFloat().getCentre();
    for (int i = 0; i < 3; ++i)
    {
        const float t = std::fmod (phase * 0.6f + (float) i / 3.f, 1.f);
        const float r = 14.f + t * (14.f + 22.f * level);
        g.setColour (sty::accent.withAlpha ((1.f - t) * (0.15f + 0.5f * level)));
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.6f);
    }
    const float coreR = 11.f + 9.f * levelOut;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, c.y,
                                             sty::neon, c.x + coreR, c.y + coreR, true));
    g.fillEllipse (c.x - coreR, c.y - coreR, coreR * 2.f, coreR * 2.f);
    g.setColour (sty::neon.withAlpha (0.25f + 0.3f * levelOut));
    g.fillEllipse (c.x - coreR * 1.8f, c.y - coreR * 1.8f, coreR * 3.6f, coreR * 3.6f);

    // ---- footer
    g.setColour (sty::text.withAlpha (0.3f));
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    g.drawText (juce::String (JucePlugin_Name) + "  v" + JucePlugin_VersionString, 24, getHeight() - 22, 300, 16, juce::Justification::centredLeft);
}

void SonyTuneAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (20);
    auto top = r.removeFromTop (60);                        // title area
    orbArea = top.removeFromRight (76).withSizeKeepingCentre (76, 76);
    pitchView.setBounds (r.removeFromTop (260));
    r.removeFromTop (14);

    auto row = r.removeFromTop (36);
    const int gap = 10, wBox = (row.getWidth() - 3 * gap) / 4;
    keyBox.setBounds    (row.removeFromLeft (wBox));  row.removeFromLeft (gap);
    scaleBox.setBounds  (row.removeFromLeft (wBox));  row.removeFromLeft (gap);
    fastButton.setBounds (row.removeFromLeft (wBox)); row.removeFromLeft (gap);
    hqButton.setBounds  (row);

    r.removeFromTop (10);
    r.removeFromBottom (20);                                // footer
    const int kw = r.getWidth() / 5;
    Knob* knobs[5] = { &speed, &amount, &humanize, &formant, &mix };
    for (auto* k : knobs)
    {
        auto col = r.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (20));
        k->slider.setBounds (col);
    }
}
