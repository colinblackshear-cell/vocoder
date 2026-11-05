#pragma once

#include "Parameters.h"

#include <utility>
#include <vector>

// The building blocks of Easy mode. Each step owns a group of settings, and
// each tile in a step sets some of them. Picking a tile in a "pick one" step
// first resets the step's group, so tiles never pile up on each other. In the
// Effects step every tile is its own switch.
namespace EasySteps
{
using Values = std::vector<std::pair<const char*, float>>;

struct Tile
{
    const char* name;
    const char* description;
    Values values;
};

struct Step
{
    const char* title;
    const char* guide;
    std::vector<const char*> group; // reset before a tile is applied (pick-one steps)
    std::vector<Tile> tiles;
    bool switches = false;          // Effects: each tile toggles on and off
};

inline const std::vector<Step>& all()
{
    using namespace ParamIDs;
    static const std::vector<Step> steps {
        { "Voice",
          "Start with your raw voice, or pick a robot. Robots need notes: click a chord at the bottom.",
          { robotOn, bands, sharpness, attack, release, formant, sibilance, waveform, octave, detune, noise, carrierAttack,
            carrierRelease, pulseWidth, pwmDepth, osc2Wave, osc2Semitones, osc2Fine, osc2Level, subLevel, glide,
            vibratoRate, vibratoDepth },
          {
              { "Raw", "Just your own voice, no robot", { { robotOn, 0 } } },
              { "Robot", "The classic talking machine", { { robotOn, 1 }, { bands, 16 }, { sharpness, 1.6f }, { release, 40 } } },
              { "Daft", "Smooth French disco robot",
                { { robotOn, 1 }, { waveform, 2 }, { detune, 14 }, { bands, 24 }, { attack, 3 }, { release, 50 }, { osc2Semitones, 12 },
                  { osc2Level, 0.3f }, { glide, 40 } } },
              { "Choir", "A huge, soft group of singers",
                { { robotOn, 1 }, { waveform, 2 }, { detune, 30 }, { bands, 32 }, { sharpness, 2 }, { attack, 15 }, { release, 250 },
                  { carrierAttack, 300 }, { carrierRelease, 1500 }, { vibratoRate, 4.5f }, { vibratoDepth, 12 } } },
              { "Whisper", "Breathy and spooky",
                { { robotOn, 1 }, { waveform, 4 }, { noise, 0.8f }, { sibilance, 0.8f }, { formant, 3 }, { bands, 28 } } },
              { "Monster", "Deep, big and scary",
                { { robotOn, 1 }, { octave, -1 }, { formant, -7 }, { osc2Wave, 1 }, { osc2Semitones, -12 }, { osc2Level, 0.5f },
                  { subLevel, 0.8f } } },
              { "Chipmunk", "Tiny and squeaky",
                { { robotOn, 1 }, { waveform, 3 }, { pulseWidth, 0.25f }, { octave, 1 }, { formant, 7 }, { glide, 60 },
                  { vibratoRate, 7 }, { vibratoDepth, 25 } } },
              { "Alien", "Wobbly visitor from space",
                { { robotOn, 1 }, { waveform, 3 }, { pulseWidth, 0.2f }, { pwmDepth, 0.6f }, { formant, 4 }, { osc2Semitones, 6 },
                  { osc2Level, 0.4f }, { vibratoRate, 8 }, { vibratoDepth, 40 } } },
              { "Retro Game", "8-bit video game sound",
                { { robotOn, 1 }, { waveform, 1 }, { bands, 12 }, { octave, 1 }, { release, 20 }, { osc2Wave, 1 }, { osc2Semitones, 12 },
                  { osc2Level, 0.3f } } },
          } },

        { "Chords",
          "Pick a chord type, then click the chord buttons at the bottom. Try a few in a row to make a song.",
          {},
          {
              { "Simple", "Three notes, clear and strong", { { chordFlavour, 0 } } },
              { "Dreamy", "Seventh chords, soft and smooth", { { chordFlavour, 1 } } },
              { "Jazzy", "Ninth chords, rich and fancy", { { chordFlavour, 2 } } },
              { "Floaty", "Sus2 chords that hang in the air", { { chordFlavour, 3 } } },
              { "Tense", "Sus4 chords that want to move", { { chordFlavour, 4 } } },
              { "Sparkly", "Add9 chords with a bright top", { { chordFlavour, 5 } } },
              { "Rock", "Power chords, just root and fifth", { { chordFlavour, 7 } } },
              { "Huge", "Spread wide across the keyboard", { { chordFlavour, 8 } } },
          } },

        { "Rhythm",
          "Add movement. Gates chop your voice into a beat, even without the robot; arps play the robot's chord one note at a time.",
          { arpOn, arpMode, arpRate, arpPattern, arpOctaves, arpGate, arpSwing, gatePattern },
          {
              { "Off", "Just your voice and the chord", {} },
              { "Climb", "Notes go up, over and over", { { arpOn, 1 }, { arpMode, 1 }, { arpRate, 3 }, { arpGate, 0.6f } } },
              { "Bounce", "Up and down across two octaves",
                { { arpOn, 1 }, { arpMode, 1 }, { arpRate, 3 }, { arpPattern, 2 }, { arpOctaves, 2 }, { arpGate, 0.5f } } },
              { "Sparkle", "Random notes, fast and twinkly",
                { { arpOn, 1 }, { arpMode, 1 }, { arpRate, 4 }, { arpPattern, 3 }, { arpOctaves, 2 }, { arpGate, 0.4f } } },
              { "Swing", "Laid-back eighth notes",
                { { arpOn, 1 }, { arpMode, 1 }, { arpRate, 1 }, { arpPattern, 2 }, { arpSwing, 0.5f }, { arpGate, 0.7f } } },
              { "Trance", "The classic dance-floor chop", { { arpOn, 1 }, { arpMode, 2 }, { gatePattern, 3 }, { arpRate, 3 }, { arpGate, 0.5f } } },
              { "Stutter", "Glitchy stops and starts", { { arpOn, 1 }, { arpMode, 2 }, { gatePattern, 5 }, { arpRate, 3 }, { arpGate, 0.7f } } },
              { "Gallop", "Like a running horse", { { arpOn, 1 }, { arpMode, 2 }, { gatePattern, 4 }, { arpRate, 3 }, { arpGate, 0.6f } } },
          } },

        { "Effects",
          "Turn on as many as you like. Fine-tune them in Studio.",
          {},
          {
              { "Echo", "Repeats in time with the beat", { { delayMix, 0.35f }, { delayTime, 3 }, { delayFeedback, 0.45f } } },
              { "Space", "A big hall around you", { { reverbMix, 0.4f }, { reverbSize, 0.8f } } },
              { "Wide", "Spreads the sound left and right", { { chorusMix, 0.5f } } },
              { "Wobble", "Wub-wub filter sweep", { { lfoDepth, 2.0f }, { lfoSync, 6 }, { cutoff, 2500 }, { resonance, 0.5f } } },
              { "Wah", "Opens up as you get louder", { { voiceFollow, 3 }, { cutoff, 400 }, { resonance, 0.7f } } },
              { "Crunch", "Gritty distortion", { { drive, 0.5f }, { crush, 0.2f } } },
          },
          true },

        { "Auto-Tune",
          "Auto-tune fixes the pitch of your own voice (Your voice, in the bar at the bottom). Sing to Play lets your singing play the robot.",
          { tuneOn, tuneSpeed, tuneAmount, singToPlay },
          {
              { "Off", "Your voice as it is", {} },
              { "Natural", "Gentle correction, sounds human", { { tuneOn, 1 }, { tuneSpeed, 50 } } },
              { "Robot Tune", "The hard, jumpy pop effect", { { tuneOn, 1 }, { tuneSpeed, 0 } } },
              { "Sing to Play", "Your melody plays the synth", { { tuneOn, 1 }, { tuneSpeed, 10 }, { singToPlay, 1 } } },
              { "Harmony", "Sing one note, hear a chord", { { tuneOn, 1 }, { tuneSpeed, 20 }, { singToPlay, 3 } } },
          } },

        { "Vintage",
          "Sing into a microphone from another time. Everything you hear goes onto an old record.",
          { vintageOn, vintageEra },
          {
              { "Today", "Clean and modern", {} },
              { "1900s", "Wax cylinder: tiny, crackly and wobbly", { { vintageOn, 1 }, { vintageEra, 0 } } },
              { "1920s", "Gramophone record, played on a horn", { { vintageOn, 1 }, { vintageEra, 1 } } },
              { "1930s", "Old radio broadcast", { { vintageOn, 1 }, { vintageEra, 2 } } },
              { "1940s", "Ribbon mic: the warm crooner sound", { { vintageOn, 1 }, { vintageEra, 3 } } },
              { "1950s", "Tube mic on a vinyl single", { { vintageOn, 1 }, { vintageEra, 4 } } },
              { "1960s", "Warm tape, like the classic studios", { { vintageOn, 1 }, { vintageEra, 5 } } },
              { "1970s", "Smooth analog studio", { { vintageOn, 1 }, { vintageEra, 6 } } },
              { "1980s", "Cassette tape with hiss and wobble", { { vintageOn, 1 }, { vintageEra, 7 } } },
              { "1990s", "Gritty 12-bit beat sampler", { { vintageOn, 1 }, { vintageEra, 8 } } },
              { "Telephone", "Calling from far away", { { vintageOn, 1 }, { vintageEra, 9 } } },
              { "Megaphone", "Loud and bossy", { { vintageOn, 1 }, { vintageEra, 10 } } },
          } },
    };
    return steps;
}

inline void set (juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

inline void setDefault (juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->getDefaultValue());
}

inline float value (juce::AudioProcessorValueTreeState& state, const char* id)
{
    return state.getRawParameterValue (id)->load();
}

// Whether a tile's settings are what's currently set.
inline bool isActive (juce::AudioProcessorValueTreeState& state, const Step& step, const Tile& tile)
{
    auto matches = [&state] (const char* id, float v)
    {
        auto* p = state.getParameter (id);
        const float range = p->getNormalisableRange().getRange().getLength();
        return std::abs (value (state, id) - v) <= range * 0.002f + 1.0e-4f;
    };

    if (step.switches)
        return matches (tile.values.front().first, tile.values.front().second);

    for (const auto& [id, v] : tile.values)
        if (! matches (id, v))
            return false;

    // A tile with fewer settings (like "Off") only matches if the rest of the group is at its defaults.
    for (const char* id : step.group)
    {
        const bool setByTile = std::any_of (tile.values.begin(), tile.values.end(),
                                            [id] (const auto& kv) { return juce::String (kv.first) == id; });
        if (! setByTile)
        {
            auto* p = state.getParameter (id);
            if (! matches (id, p->convertFrom0to1 (p->getDefaultValue())))
                return false;
        }
    }
    return true;
}

inline void apply (juce::AudioProcessorValueTreeState& state, const Step& step, const Tile& tile)
{
    if (step.switches)
    {
        const bool on = ! isActive (state, step, tile);
        for (const auto& [id, v] : tile.values)
            on ? set (state, id, v) : setDefault (state, id);
        if (on)
            set (state, ParamIDs::fxOn, 1.0f);
        return;
    }

    for (const char* id : step.group)
        setDefault (state, id);
    for (const auto& [id, v] : tile.values)
        set (state, id, v);

    // Auto-tune follows the chord buttons' key and scale, so your voice and chords agree.
    if (juce::String (step.title) == "Auto-Tune" && ! tile.values.empty())
    {
        set (state, ParamIDs::tuneKey, value (state, ParamIDs::chordKey));
        const int scale = juce::jlimit (0, 6, (int) value (state, ParamIDs::chordScale));
        set (state, ParamIDs::tuneScale, (float) ChordNames::tuneScales[scale]);

        // Correcting your voice only makes sense if you can hear it; Sing to Play needs the robot.
        if (value (state, ParamIDs::singToPlay) < 0.5f)
            set (state, ParamIDs::voiceOn, 1.0f);
        else
            set (state, ParamIDs::robotOn, 1.0f);
    }
}
} // namespace EasySteps
