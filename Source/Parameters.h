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
inline constexpr auto mix = "mix";
inline constexpr auto outputGain = "outputGain";
} // namespace ParamIDs

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto ms = [] (float min, float max, float def, float skewCentre)
    {
        NormalisableRange<float> range (min, max);
        range.setSkewForCentre (skewCentre);
        return std::make_pair (range, def);
    };

    auto add = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (unit)));
    };

    // Input
    add (ParamIDs::inputGain, "Input Gain", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB");
    add (ParamIDs::gateThreshold, "Gate Threshold", { -80.0f, -20.0f, 0.1f }, -60.0f, "dB");

    // Vocoder
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::bands, 1 }, "Bands", 8, 40, 20));
    add (ParamIDs::sharpness, "Band Sharpness", { 0.5f, 4.0f, 0.01f }, 1.0f, "x");
    auto [attackRange, attackDef] = ms (0.5f, 50.0f, 5.0f, 8.0f);
    add (ParamIDs::attack, "Attack", attackRange, attackDef, "ms");
    auto [releaseRange, releaseDef] = ms (5.0f, 500.0f, 60.0f, 80.0f);
    add (ParamIDs::release, "Release", releaseRange, releaseDef, "ms");
    add (ParamIDs::formant, "Formant Shift", { -12.0f, 12.0f, 0.01f }, 0.0f, "st");
    add (ParamIDs::sibilance, "Sibilance", { 0.0f, 1.0f, 0.01f }, 0.3f, "");

    // Carrier synth
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::waveform, 1 }, "Waveform",
                                                        StringArray { "Saw", "Square", "Supersaw" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::octave, 1 }, "Octave", -2, 2, 0));
    add (ParamIDs::detune, "Supersaw Detune", { 0.0f, 50.0f, 0.1f }, 20.0f, "cents");
    add (ParamIDs::noise, "Noise", { 0.0f, 1.0f, 0.01f }, 0.05f, "");
    auto [carAttackRange, carAttackDef] = ms (0.5f, 2000.0f, 5.0f, 100.0f);
    add (ParamIDs::carrierAttack, "Carrier Attack", carAttackRange, carAttackDef, "ms");
    auto [carReleaseRange, carReleaseDef] = ms (5.0f, 3000.0f, 150.0f, 300.0f);
    add (ParamIDs::carrierRelease, "Carrier Release", carReleaseRange, carReleaseDef, "ms");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::drone, 1 }, "Drone (no keys)", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::droneNote, 1 }, "Drone Note", 24, 84, 48,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (
                                                         [] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 3); })));

    // Output
    add (ParamIDs::mix, "Dry/Wet", { 0.0f, 1.0f, 0.01f }, 1.0f, "");
    add (ParamIDs::outputGain, "Output Gain", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB");

    return layout;
}
