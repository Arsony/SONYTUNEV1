#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SonyTuneDSP.h"

class SonyTuneAudioProcessor : public juce::AudioProcessor
{
public:
    SonyTuneAudioProcessor();
    ~SonyTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    sonytune::Engine engine;   // the editor reads engine.vizRaw / vizOut / vizWrite

    // smoothed peak levels (0..1) so the interface can pulse with the voice
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };

private:
    std::atomic<float> *pKey, *pScale, *pSpeed, *pAmount, *pHumanize, *pFormant, *pMix, *pFast, *pHq;
    int lastLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonyTuneAudioProcessor)
};
