#pragma once

#include "PluginProcessor.h"

#include "Brand.h"

// Live pitch screen: grey = what you sang, glowing pink = what SonyTune outputs
class PitchView : public juce::Component, private juce::Timer
{
public:
    explicit PitchView (SonyTuneAudioProcessor& p);
    ~PitchView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { phase += 0.04f; repaint(); }

    SonyTuneAudioProcessor& proc;
    float lo = 48.f, hi = 72.f;
    float phase = 0.f;
    float glow = 0.f;
};

class SonyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SonyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

class SonyTuneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SonyTuneAudioProcessorEditor (SonyTuneAudioProcessor&);
    ~SonyTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& title, const juce::String& paramId, const juce::String& suffix);

    SonyTuneAudioProcessor& proc;
    SonyLookAndFeel laf;
    PitchView pitchView;

    juce::ComboBox keyBox, scaleBox;
    juce::TextButton fastButton { "FAST FLOW" }, hqButton { "HQ MODE" };
    Knob speed, amount, humanize, formant, mix;

    float phase = 0.f, level = 0.f, levelOut = 0.f;
    juce::Rectangle<int> orbArea;

    // attachments last so they are destroyed before the widgets they point at
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   fastAtt, hqAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonyTuneAudioProcessorEditor)
};
