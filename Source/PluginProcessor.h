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

    // Factory presets (Presets.h), also listed in the DAW's preset menu.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState params;
    juce::MidiKeyboardState keyboardState; // the on-screen keyboard
    Recorder recorder;

    // Chord buttons: up to 4 notes that replace the previous chord at the next
    // beat or bar (per Chord Timing). An empty chord stops. Message thread only.
    void requestChord (const juce::Array<int>& notes, bool immediate = false);

    // For the UI: where the beat clock is, and where a take will start.
    double getClockPosition() const { return uiPosition.load(); }
    double getRecordStartPosition() const { return uiRecordStart.load(); }

private:
    struct Timing
    {
        double bpm = 120.0;
        double ppq = 0.0;       // beats; from the host while it plays, else our own clock
        double beatsPerBar = 4.0;
        bool hostPlaying = false;
    };

    Timing readTiming();
    void updateSettings (const Timing& timing);
    void handleMidi (const juce::MidiMessage& message);
    void scheduleChord (uint64_t packed, const Timing& timing);
    void applyChord();
    void scheduleRecording (Timing& timing);
    void addMetronome (juce::AudioBuffer<float>& out, const Timing& timing, double beatsPerSample);

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

    // Beat clock used when no host transport is running (standalone, or Logic stopped).
    double clockPosition = 0.0;
    std::atomic<double> uiPosition { 0.0 }, uiRecordStart { 0.0 };

    std::atomic<uint64_t> pendingChord { 0 };
    uint64_t queuedChord = 0;
    double chordApplyAt = 0.0;
    std::array<int, 4> chordNotes {};
    int chordCount = 0;

    bool takeScheduled = false, stopScheduled = false;
    double recordStart = 0.0, recordStop = 0.0;

    float clickLevel = 0.0f, clickPhase = 0.0f, clickStep = 0.0f, clickDecay = 0.999f;
    juce::int64 lastClickBeat = std::numeric_limits<juce::int64>::min();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderProcessor)
};
