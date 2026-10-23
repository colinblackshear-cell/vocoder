#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace vox
{
enum class Waveform
{
    saw = 0,
    square,
    supersaw
};

// Band-limited step correction for naive saw and square oscillators.
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float midiToHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

struct Voice
{
    static constexpr int numSaws = 7;

    int note = -1;
    bool held = false;
    float level = 0.0f;
    float velocity = 0.0f;
    uint64_t age = 0;
    std::array<float, numSaws> phases {};

    bool isActive() const { return held || level > 1.0e-4f; }
};

// Polyphonic carrier synth. The vocoder imposes the voice's spectrum onto its output.
class CarrierSynth
{
public:
    static constexpr int maxVoices = 8;

    struct Settings
    {
        Waveform waveform = Waveform::saw;
        int octave = 0;
        float detuneCents = 20.0f;
        float noise = 0.05f;
        float attackMs = 5.0f;
        float releaseMs = 150.0f;
        bool drone = false;
        int droneNote = 48;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v = Voice {};
        droneVoice = Voice {};
        seedPhases (droneVoice);
        heldNotes = 0;
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        attackStep = (float) (1.0 / (std::fmax (s.attackMs, 0.1) * 0.001 * sampleRate));
        releaseCoef = (float) std::exp (-1.0 / (std::fmax (s.releaseMs, 1.0) * 0.001 * sampleRate));

        for (int i = 0; i < Voice::numSaws; ++i)
        {
            const float spread = (float) i / (float) (Voice::numSaws - 1) * 2.0f - 1.0f;
            detuneRatios[(size_t) i] = std::pow (2.0f, spread * s.detuneCents / 1200.0f);
        }
    }

    void noteOn (int note, float velocity)
    {
        Voice* target = nullptr;
        for (auto& v : voices)
            if (v.note == note)
                target = &v;

        if (target == nullptr)
            for (auto& v : voices)
                if (! v.isActive())
                {
                    target = &v;
                    break;
                }

        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices)
                if (v.age < target->age)
                    target = &v;
        }

        if (! target->isActive())
            seedPhases (*target);

        if (! target->held)
            ++heldNotes;

        target->note = note;
        target->held = true;
        target->velocity = velocity;
        target->age = ++clock;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.note == note && v.held)
            {
                v.held = false;
                --heldNotes;
            }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            v.held = false;
        heldNotes = 0;
    }

    int getHeldNoteCount() const { return heldNotes; }

    float process()
    {
        float out = 0.0f;
        for (auto& v : voices)
            if (v.isActive())
                out += renderVoice (v);

        droneVoice.note = settings.droneNote;
        droneVoice.velocity = 1.0f;
        droneVoice.held = settings.drone && heldNotes == 0;
        if (droneVoice.isActive())
            out += renderVoice (droneVoice);

        noiseState = noiseState * 1664525u + 1013904223u;
        const float white = (float) (noiseState >> 8) / 8388608.0f - 1.0f;
        return out + white * settings.noise;
    }

private:
    float renderVoice (Voice& v)
    {
        if (v.held)
            v.level = std::fmin (1.0f, v.level + attackStep);
        else
            v.level *= releaseCoef;

        const float baseHz = midiToHz ((float) (v.note + 12 * settings.octave));
        float sample = 0.0f;

        if (settings.waveform == Waveform::supersaw)
        {
            for (size_t i = 0; i < Voice::numSaws; ++i)
                sample += saw (v.phases[i], baseHz * detuneRatios[i]);
            sample *= 0.4f;
        }
        else if (settings.waveform == Waveform::square)
        {
            sample = square (v.phases[0], baseHz);
        }
        else
        {
            sample = saw (v.phases[0], baseHz);
        }

        return sample * v.level * (0.3f + 0.7f * v.velocity);
    }

    float saw (float& phase, float hz)
    {
        const float dt = std::fmin (hz / (float) sampleRate, 0.5f);
        const float out = 2.0f * phase - 1.0f - polyBlep (phase, dt);
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
        return out;
    }

    float square (float& phase, float hz)
    {
        const float dt = std::fmin (hz / (float) sampleRate, 0.5f);
        float half = phase + 0.5f;
        if (half >= 1.0f)
            half -= 1.0f;
        const float out = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep (phase, dt) - polyBlep (half, dt);
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
        return out;
    }

    void seedPhases (Voice& v)
    {
        for (auto& p : v.phases)
        {
            noiseState = noiseState * 1664525u + 1013904223u;
            p = (float) (noiseState >> 8) / 16777216.0f;
        }
    }

    double sampleRate = 44100.0;
    Settings settings;
    float attackStep = 0.01f, releaseCoef = 0.999f;
    std::array<float, Voice::numSaws> detuneRatios {};
    std::array<Voice, maxVoices> voices {};
    Voice droneVoice;
    int heldNotes = 0;
    uint64_t clock = 0;
    uint32_t noiseState = 22222u;
};
} // namespace vox
