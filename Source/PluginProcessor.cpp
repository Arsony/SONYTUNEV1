#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SonyTuneAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
                    StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    StringArray scales;
    for (int i = 0; i < sonytune::kNumScales; ++i) scales.add (sonytune::scaleName (i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    // SPEED: 0 ms = instant hard tune, higher = slower / more natural
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed",
                    NormalisableRange<float> (0.f, 500.f, 0.1f, 0.4f), 15.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                    NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
                    NormalisableRange<float> (0.f, 100.f, 0.1f), 30.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant",
                    NormalisableRange<float> (-5.f, 5.f, 0.01f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                    NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fast", 1 }, "Fast Flow", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hq", 1 }, "HQ Mode", false));
    return layout;
}

SonyTuneAudioProcessor::SonyTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pKey      = apvts.getRawParameterValue ("key");
    pScale    = apvts.getRawParameterValue ("scale");
    pSpeed    = apvts.getRawParameterValue ("speed");
    pAmount   = apvts.getRawParameterValue ("amount");
    pHumanize = apvts.getRawParameterValue ("humanize");
    pFormant  = apvts.getRawParameterValue ("formant");
    pMix      = apvts.getRawParameterValue ("mix");
    pFast     = apvts.getRawParameterValue ("fast");
    pHq       = apvts.getRawParameterValue ("hq");
}

void SonyTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    lastLatency = -1;
}

bool SonyTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SonyTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    sonytune::Engine::Params p;
    p.key          = (int) pKey->load();
    p.scale        = (int) pScale->load();
    p.speedMs      = pSpeed->load();
    p.amount       = pAmount->load() * 0.01f;
    p.humanize     = pHumanize->load() * 0.01f;
    p.formantSemis = pFormant->load();
    p.mix          = pMix->load() * 0.01f;
    p.fast         = pFast->load() > 0.5f;
    p.hq           = pHq->load() > 0.5f;
    engine.setParams (p);

    // tell the host about our delay so FL Studio lines the vocal up with the beat
    const int latency = engine.getLatencySamples();
    if (latency != lastLatency)
    {
        lastLatency = latency;
        setLatencySamples (latency);
    }

    const int numSamples = buffer.getNumSamples();
    const float inPeak = buffer.getNumChannels() > 0 ? buffer.getMagnitude (0, numSamples) : 0.f;

    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);

    const float outPeak = buffer.getNumChannels() > 0 ? buffer.getMagnitude (0, numSamples) : 0.f;
    // fast attack, slow release
    auto follow = [] (std::atomic<float>& env, float peak)
    {
        const float cur = env.load (std::memory_order_relaxed);
        env.store (peak > cur ? peak : cur * 0.9f + peak * 0.1f, std::memory_order_relaxed);
    };
    follow (inLevel, inPeak);
    follow (outLevel, outPeak);
}

juce::AudioProcessorEditor* SonyTuneAudioProcessor::createEditor()
{
    return new SonyTuneAudioProcessorEditor (*this);
}

void SonyTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SonyTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SonyTuneAudioProcessor();
}
