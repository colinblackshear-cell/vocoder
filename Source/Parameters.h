#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
inline constexpr auto inputGain = "inputGain";
inline constexpr auto gateThreshold = "gateThreshold";

inline constexpr auto bands = "bands";
inline constexpr auto sharpness = "sharpness";
inline constexpr auto attack = "attack";
inline constexpr auto release = "release";
inline constexpr auto formant = "formant";
inline constexpr auto sibilance = "sibilance";

inline constexpr auto waveform = "waveform";
inline constexpr auto octave = "octave";
inline constexpr auto detune = "detune";
inline constexpr auto noise = "noise";
inline constexpr auto carrierAttack = "carrierAttack";
inline constexpr auto carrierRelease = "carrierRelease";
inline constexpr auto drone = "drone";
inline constexpr auto droneNote = "droneNote";

inline constexpr auto arpMode = "arpMode";
inline constexpr auto arpRate = "arpRate";
inline constexpr auto arpPattern = "arpPattern";
inline constexpr auto arpOctaves = "arpOctaves";
inline constexpr auto arpGate = "arpGate";
inline constexpr auto arpSwing = "arpSwing";
inline constexpr auto gatePattern = "gatePattern";
inline constexpr auto tempo = "tempo";

inline constexpr auto filterMode = "filterMode";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto resonance = "resonance";
inline constexpr auto filterDrive = "filterDrive";
inline constexpr auto voiceFollow = "voiceFollow";
inline constexpr auto lfoShape = "lfoShape";
inline constexpr auto lfoSync = "lfoSync";
inline constexpr auto lfoRate = "lfoRate";
inline constexpr auto lfoDepth = "lfoDepth";

inline constexpr auto drive = "drive";
inline constexpr auto crush = "crush";
inline constexpr auto chorusMix = "chorusMix";
inline constexpr auto chorusRate = "chorusRate";
inline constexpr auto chorusDepth = "chorusDepth";
inline constexpr auto delayMix = "delayMix";
inline constexpr auto delayTime = "delayTime";
inline constexpr auto delayFeedback = "delayFeedback";
inline constexpr auto delayPingPong = "delayPingPong";
inline constexpr auto reverbMix = "reverbMix";
inline constexpr auto reverbSize = "reverbSize";
inline constexpr auto reverbDamping = "reverbDamping";

inline constexpr auto mix = "mix";
inline constexpr auto outputGain = "outputGain";
} // namespace ParamIDs

// Musical note lengths, in beats, shared by the arp rate, LFO sync and delay time.
namespace NoteLengths
{
inline const juce::StringArray arpNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
inline constexpr double arpBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

inline const juce::StringArray lfoNames { "Free (Hz)", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
inline constexpr double lfoBeats[] = { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };

inline const juce::StringArray delayNames { "1/2", "1/4 dotted", "1/4", "1/8 dotted", "1/8", "1/16" };
inline constexpr double delayBeats[] = { 2.0, 1.5, 1.0, 0.75, 0.5, 0.25 };
} // namespace NoteLengths

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using Group = AudioProcessorParameterGroup;

    auto skewed = [] (float min, float max, float centre)
    {
        NormalisableRange<float> range (min, max);
        range.setSkewForCentre (centre);
        return range;
    };

    auto real = [] (const char* id, const char* name, NormalisableRange<float> range, float def, const char* unit)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                      AudioParameterFloatAttributes().withLabel (unit));
    };
    auto integer = [] (const char* id, const char* name, int min, int max, int def)
    {
        return std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, min, max, def);
    };
    auto choice = [] (const char* id, const char* name, const StringArray& options, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, options, def);
    };
    auto toggle = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Group> (
        "input", "Input", "|",
        real (ParamIDs::inputGain, "Input Gain", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB"),
        real (ParamIDs::gateThreshold, "Gate Threshold", { -80.0f, -20.0f, 0.1f }, -60.0f, "dB")));

    layout.add (std::make_unique<Group> (
        "vocoder", "Vocoder", "|",
        integer (ParamIDs::bands, "Bands", 8, 40, 20),
        real (ParamIDs::sharpness, "Band Sharpness", { 0.5f, 4.0f, 0.01f }, 1.0f, "x"),
        real (ParamIDs::attack, "Attack", skewed (0.5f, 50.0f, 8.0f), 5.0f, "ms"),
        real (ParamIDs::release, "Release", skewed (5.0f, 500.0f, 80.0f), 60.0f, "ms"),
        real (ParamIDs::formant, "Formant Shift", { -12.0f, 12.0f, 0.01f }, 0.0f, "st"),
        real (ParamIDs::sibilance, "Sibilance", { 0.0f, 1.0f, 0.01f }, 0.3f, "")));

    layout.add (std::make_unique<Group> (
        "carrier", "Carrier Synth", "|",
        choice (ParamIDs::waveform, "Waveform", { "Saw", "Square", "Supersaw" }, 0),
        integer (ParamIDs::octave, "Octave", -2, 2, 0),
        real (ParamIDs::detune, "Supersaw Detune", { 0.0f, 50.0f, 0.1f }, 20.0f, "cents"),
        real (ParamIDs::noise, "Noise", { 0.0f, 1.0f, 0.01f }, 0.05f, ""),
        real (ParamIDs::carrierAttack, "Carrier Attack", skewed (0.5f, 2000.0f, 100.0f), 5.0f, "ms"),
        real (ParamIDs::carrierRelease, "Carrier Release", skewed (5.0f, 3000.0f, 300.0f), 150.0f, "ms"),
        toggle (ParamIDs::drone, "Drone (no keys)", false),
        std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::droneNote, 1 }, "Drone Note", 24, 84, 48,
                                             AudioParameterIntAttributes().withStringFromValueFunction (
                                                 [] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 3); }))));

    layout.add (std::make_unique<Group> (
        "arp", "Arpeggiator", "|",
        choice (ParamIDs::arpMode, "Arp Mode", { "Off", "Note Arp", "Rhythm Gate" }, 0),
        choice (ParamIDs::arpRate, "Arp Rate", NoteLengths::arpNames, 3),
        choice (ParamIDs::arpPattern, "Arp Pattern", { "Up", "Down", "Up/Down", "Random", "As Played" }, 0),
        integer (ParamIDs::arpOctaves, "Arp Octaves", 1, 3, 1),
        real (ParamIDs::arpGate, "Arp Gate Length", { 0.05f, 1.0f, 0.01f }, 0.6f, ""),
        real (ParamIDs::arpSwing, "Arp Swing", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        choice (ParamIDs::gatePattern, "Gate Pattern",
                { "Straight", "Eighths", "Offbeats", "Trance", "Gallop", "Stutter", "Sparse" }, 3),
        real (ParamIDs::tempo, "Tempo (standalone)", { 60.0f, 200.0f, 0.1f }, 120.0f, "BPM")));

    layout.add (std::make_unique<Group> (
        "filter", "Filter", "|",
        choice (ParamIDs::filterMode, "Filter Type", { "Off", "Low Pass", "High Pass", "Band Pass", "Notch" }, 1),
        real (ParamIDs::cutoff, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, "Hz"),
        real (ParamIDs::resonance, "Resonance", { 0.0f, 1.0f, 0.01f }, 0.2f, ""),
        real (ParamIDs::filterDrive, "Filter Drive", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        real (ParamIDs::voiceFollow, "Voice Follow", { -4.0f, 4.0f, 0.01f }, 0.0f, "oct"),
        choice (ParamIDs::lfoShape, "LFO Shape", { "Sine", "Triangle", "Saw", "Square", "Random" }, 0),
        choice (ParamIDs::lfoSync, "LFO Sync", NoteLengths::lfoNames, 0),
        real (ParamIDs::lfoRate, "LFO Rate", skewed (0.05f, 20.0f, 2.0f), 1.0f, "Hz"),
        real (ParamIDs::lfoDepth, "LFO to Cutoff", { 0.0f, 4.0f, 0.01f }, 0.0f, "oct")));

    layout.add (std::make_unique<Group> (
        "fx", "Effects", "|",
        real (ParamIDs::drive, "Drive", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        real (ParamIDs::crush, "Bit Crush", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        real (ParamIDs::chorusMix, "Chorus Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        real (ParamIDs::chorusRate, "Chorus Rate", skewed (0.05f, 5.0f, 1.0f), 0.8f, "Hz"),
        real (ParamIDs::chorusDepth, "Chorus Depth", { 0.0f, 1.0f, 0.01f }, 0.5f, ""),
        real (ParamIDs::delayMix, "Delay Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        choice (ParamIDs::delayTime, "Delay Time", NoteLengths::delayNames, 3),
        real (ParamIDs::delayFeedback, "Delay Feedback", { 0.0f, 0.95f, 0.01f }, 0.4f, ""),
        toggle (ParamIDs::delayPingPong, "Ping-Pong", true),
        real (ParamIDs::reverbMix, "Reverb Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, ""),
        real (ParamIDs::reverbSize, "Reverb Size", { 0.0f, 1.0f, 0.01f }, 0.6f, ""),
        real (ParamIDs::reverbDamping, "Reverb Damping", { 0.0f, 1.0f, 0.01f }, 0.5f, "")));

    layout.add (std::make_unique<Group> (
        "output", "Output", "|",
        real (ParamIDs::mix, "Dry/Wet", { 0.0f, 1.0f, 0.01f }, 1.0f, ""),
        real (ParamIDs::outputGain, "Output Gain", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB")));

    return layout;
}
