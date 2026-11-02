#pragma once

#include "dsp/Arpeggiator.h"
#include "dsp/CarrierSynth.h"
#include "dsp/Effects.h"
#include "dsp/Filter.h"
#include "dsp/Vocoder.h"
#include "Recorder.h"

#include <juce_audio_utils/juce_audio_utils.h>

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
    juce::MidiKeyboardState keyboardState; // the on-screen keyboard
    Recorder recorder;

private:
    struct Timing
    {
        double bpm = 120.0;
        double ppq = 0.0;
        bool hostPlaying = false;
    };

    Timing readTiming();
    void updateSettings (const Timing& timing);
    void handleMidi (const juce::MidiMessage& message);

    double sampleRate = 44100.0;

    vox::Vocoder vocoder;
    vox::CarrierSynth synth;
    vox::Arpeggiator arp;
    vox::NoiseGate gate;

    vox::StateVariableFilter filter;
    vox::Lfo lfo;
    vox::EnvelopeFollower voiceLevel;
    vox::Distortion distortion;
    vox::Chorus chorus;
    vox::StereoDelay delay;
    juce::Reverb reverb;

    juce::SmoothedValue<float> inputGain, outputGain, mix, resonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff;
    float filterDrive = 0.0f, voiceFollow = 0.0f, lfoDepth = 0.0f, reverbMix = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderProcessor)
};
