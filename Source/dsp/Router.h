#pragma once

#include <algorithm>
#include <cmath>

namespace vox
{
// Keeps one signal about as loud as another. It follows the loudness (mean square)
// of both and works out the gain that would make them match, within limits.
// It only learns while both are actually sounding, so silence never makes it
// turn a signal right up.
class LoudnessMatch
{
public:
    void prepare (double sampleRate, double followMs, float minimumGain, float maximumGain)
    {
        followCoef = coefFor (sampleRate, followMs);
        gainCoef = coefFor (sampleRate, followMs * 0.7);
        minGain = minimumGain;
        maxGain = maximumGain;
        reset();
    }

    void reset()
    {
        referenceLevel = signalLevel = 0.0f;
        gain = targetGain = 1.0f;
    }

    // Feed one sample of each; returns the gain to multiply `signal` by.
    // With `learn` false the gain holds where it is (say, while the signal is only a fading tail).
    float process (float reference, float signal, bool learn = true)
    {
        referenceLevel = reference * reference + followCoef * (referenceLevel - reference * reference);
        signalLevel = signal * signal + followCoef * (signalLevel - signal * signal);

        if (learn && referenceLevel > floor && signalLevel > floor)
            targetGain = std::clamp (std::sqrt (referenceLevel / signalLevel), minGain, maxGain);

        gain = targetGain + gainCoef * (gain - targetGain);
        return gain;
    }

    float getGain() const { return gain; }

private:
    static float coefFor (double sampleRate, double ms) { return (float) std::exp (-1.0 / (std::max (ms, 0.1) * 0.001 * sampleRate)); }

    static constexpr float floor = 1.0e-7f; // about -70 dBFS
    float followCoef = 0.999f, gainCoef = 0.999f;
    float minGain = 0.25f, maxGain = 4.0f;
    float referenceLevel = 0.0f, signalLevel = 0.0f;
    float gain = 1.0f, targetGain = 1.0f;
};

// Decides what you hear from the two sources: the robot (vocoder) and your own voice.
//
// The rules, so any combination of switches makes sense:
//  - The robot is matched in loudness to your voice, so every robot sound
//    (Monster, Choir, a note arp...) comes out about as loud as you sing.
//  - Your voice switch does what it says while the robot is making sound.
//  - When nothing else would be heard (the robot is off, or it's on but has no
//    notes to play), your voice stands in, so a switch never leaves you in silence.
//    It waits a moment before coming in, so a quick gap between chords doesn't let it through.
class SmartMix
{
public:
    void prepare (double sampleRate)
    {
        match.prepare (sampleRate, 350.0, 0.25f, 6.0f);
        holdSamples = (int) (0.6 * sampleRate);
        fadeInCoef = (float) std::exp (-1.0 / (0.12 * sampleRate));
        fadeOutCoef = (float) std::exp (-1.0 / (0.02 * sampleRate));
        reset();
    }

    void reset()
    {
        match.reset();
        silentFor = holdSamples;
        standIn = 0.0f;
    }

    struct Switches
    {
        bool voiceOn = true;
        bool robotOn = false;
        bool robotHasNotes = false; // chord, keys, drone or Sing to Play
        bool fillGaps = true;       // let your voice stand in while the robot has no notes
    };

    // `voice` is your voice as it goes into the vocoder; `robot` is the vocoder's raw output.
    // Returns the gain for the robot (before its level knob); `voiceStandIn()` is then 0..1.
    float process (float voice, float robot, const Switches& s)
    {
        const bool robotSounding = s.robotOn && s.robotHasNotes;
        const float robotGain = match.process (voice, robot, s.robotHasNotes);

        silentFor = robotSounding ? 0 : std::min (silentFor + 1, holdSamples);

        bool wanted = false;
        if (! s.voiceOn)
        {
            if (! s.robotOn)
                wanted = true; // everything switched off: back to your raw voice
            else if (s.fillGaps && silentFor >= holdSamples)
                wanted = true; // robot on, but no notes for it to play
        }

        const float target = wanted ? 1.0f : 0.0f;
        const float coef = target > standIn ? fadeInCoef : fadeOutCoef;
        standIn = target + coef * (standIn - target);
        return robotGain;
    }

    float voiceStandIn() const { return standIn; }
    bool isStandingIn() const { return standIn > 0.5f; }

private:
    LoudnessMatch match;
    int holdSamples = 1, silentFor = 0;
    float standIn = 0.0f, fadeInCoef = 0.99f, fadeOutCoef = 0.9f;
};
} // namespace vox
