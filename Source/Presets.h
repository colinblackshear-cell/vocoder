#pragma once

#include "Parameters.h"

#include <utility>
#include <vector>

// Factory sounds. Each preset starts from the default settings and changes
// only what it lists (values in the parameter's own units; choices by index).
// Performance settings such as input gain, gate, tempo and the drone are left
// alone, so switching sounds mid-set never changes your levels or tempo.
namespace Presets
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

// Choice indexes, to keep the table readable.
namespace Wave { inline constexpr float saw = 0, square = 1, supersaw = 2, pulse = 3, triangle = 4; }
namespace Osc2 { inline constexpr float saw = 0, square = 1, pulse = 2, triangle = 3; }
namespace Arp { inline constexpr float off = 0, notes = 1, gate = 2; }
namespace Filter { inline constexpr float off = 0, lowPass = 1, highPass = 2, bandPass = 3; }

inline const std::vector<Preset>& all()
{
    using namespace ParamIDs;
    static const std::vector<Preset> presets {
        { "Init", {} },

        { "Classic Robot",
          { { waveform, Wave::saw }, { bands, 16 }, { sharpness, 1.6f }, { release, 40 }, { noise, 0.05f },
            { cutoff, 8000 }, { chorusMix, 0.15f } } },

        { "French Touch",
          { { waveform, Wave::supersaw }, { detune, 14 }, { bands, 24 }, { attack, 3 }, { release, 50 },
            { osc2Wave, Osc2::saw }, { osc2Semitones, 12 }, { osc2Level, 0.3f }, { glide, 40 },
            { cutoff, 6000 }, { resonance, 0.35f }, { drive, 0.15f },
            { chorusMix, 0.5f }, { chorusRate, 0.6f }, { reverbMix, 0.15f } } },

        { "Choir Pad",
          { { waveform, Wave::supersaw }, { detune, 30 }, { bands, 32 }, { sharpness, 2.0f }, { attack, 15 },
            { release, 250 }, { carrierAttack, 300 }, { carrierRelease, 1500 }, { vibratoRate, 4.5f },
            { vibratoDepth, 12 }, { cutoff, 5000 }, { chorusMix, 0.6f }, { reverbMix, 0.5f }, { reverbSize, 0.85f } } },

        { "Whisper Ghost",
          { { waveform, Wave::triangle }, { noise, 0.8f }, { sibilance, 0.8f }, { formant, 3 }, { bands, 28 },
            { filterMode, Filter::highPass }, { cutoff, 300 }, { delayMix, 0.3f }, { delayTime, 2 },
            { reverbMix, 0.55f }, { reverbSize, 0.9f } } },

        { "Trance Gate",
          { { waveform, Wave::supersaw }, { detune, 22 }, { arpMode, Arp::gate }, { gatePattern, 3 }, { arpRate, 3 },
            { arpGate, 0.5f }, { cutoff, 7000 }, { delayMix, 0.3f }, { delayTime, 1 }, { reverbMix, 0.3f } } },

        { "Stutter Chop",
          { { waveform, Wave::square }, { arpMode, Arp::gate }, { gatePattern, 5 }, { arpRate, 3 }, { arpGate, 0.7f },
            { cutoff, 4000 }, { resonance, 0.4f }, { crush, 0.25f } } },

        { "Arp Robot",
          { { waveform, Wave::pulse }, { pulseWidth, 0.3f }, { pwmDepth, 0.4f }, { arpMode, Arp::notes }, { arpRate, 3 },
            { arpPattern, 2 }, { arpOctaves, 2 }, { arpGate, 0.5f }, { cutoff, 6000 },
            { delayMix, 0.35f }, { delayTime, 3 }, { delayFeedback, 0.45f } } },

        { "Acid Wobble",
          { { waveform, Wave::saw }, { subLevel, 0.5f }, { cutoff, 600 }, { resonance, 0.8f }, { filterDrive, 0.5f },
            { lfoShape, 0 }, { lfoSync, 6 }, { lfoDepth, 2.5f }, { drive, 0.3f } } },

        { "Auto-Wah Funk",
          { { waveform, Wave::square }, { subLevel, 0.3f }, { cutoff, 400 }, { resonance, 0.75f }, { voiceFollow, 3 },
            { chorusMix, 0.2f } } },

        { "Lo-Fi Radio",
          { { waveform, Wave::square }, { bands, 12 }, { noise, 0.15f }, { filterMode, Filter::bandPass }, { cutoff, 1500 },
            { resonance, 0.3f }, { crush, 0.5f }, { drive, 0.4f } } },

        { "Monster",
          { { waveform, Wave::saw }, { octave, -1 }, { formant, -7 }, { osc2Wave, Osc2::square }, { osc2Semitones, -12 },
            { osc2Level, 0.5f }, { subLevel, 0.8f }, { cutoff, 3000 }, { drive, 0.5f }, { reverbMix, 0.25f },
            { reverbSize, 0.7f } } },

        { "Chipmunk Bot",
          { { waveform, Wave::pulse }, { pulseWidth, 0.25f }, { octave, 1 }, { formant, 7 }, { glide, 60 },
            { vibratoRate, 7 }, { vibratoDepth, 25 }, { crush, 0.2f } } },

        { "Space Echo",
          { { waveform, Wave::supersaw }, { detune, 25 }, { cutoff, 3000 }, { lfoSync, 1 }, { lfoDepth, 1.5f },
            { chorusMix, 0.4f }, { delayMix, 0.5f }, { delayTime, 3 }, { delayFeedback, 0.7f },
            { reverbMix, 0.5f }, { reverbSize, 0.9f }, { reverbDamping, 0.3f } } },
    };
    return presets;
}

// Settings that belong to the performance, not the sound.
inline bool keepsValue (const juce::String& id)
{
    using namespace ParamIDs;
    for (auto* kept : { inputGain, gateThreshold, tempo, metronome, chordTiming, drone, droneNote, outputGain })
        if (id == kept)
            return true;
    return false;
}

// Puts every sound parameter back to its default, then applies the preset.
inline void apply (juce::AudioProcessorValueTreeState& state, const Preset& preset)
{
    for (auto* param : state.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (! keepsValue (p->getParameterID()))
                p->setValueNotifyingHost (p->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}
} // namespace Presets
