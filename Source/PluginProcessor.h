#pragma once

#include "dsp/CarrierSynth.h"
#include "dsp/Vocoder.h"

#include <juce_audio_processors/juce_audio_processors.h>

class VocoderProcessor : public juce::AudioProcessor
{
public:
    VocoderProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState params;

private:
    void updateSettings();
    void handleMidi (const juce::MidiMessage& message);

    vox::Vocoder vocoder;
    vox::CarrierSynth synth;
    vox::NoiseGate gate;
    juce::SmoothedValue<float> inputGain, outputGain, mix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderProcessor)
};
