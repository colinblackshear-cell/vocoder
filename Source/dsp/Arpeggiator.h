#pragma once

#include "CarrierSynth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vox
{
enum class ArpMode
{
    off = 0,
    notes,  // plays the held chord one note at a time
    gate    // chops the vocoded sound into a rhythm
};

enum class ArpPattern
{
    up = 0,
    down,
    upDown,
    random,
    asPlayed
};

struct GatePatterns
{
    static constexpr int count = 7;

    // 16 steps each: x = sound, . = silence.
    static constexpr const char* steps[count] = {
        "xxxxxxxxxxxxxxxx", // Straight
        "x.x.x.x.x.x.x.x.", // Eighths
        ".x.x.x.x.x.x.x.x", // Offbeats
        "xx.xx.xx.x.xx.x.", // Trance
        "x.xxx.xxx.xxx.xx", // Gallop
        "xxxx....xx.x....", // Stutter
        "x...x..x..x.x...", // Sparse
    };

    static bool isOn (int pattern, int64_t step)
    {
        const auto* s = steps[std::clamp (pattern, 0, count - 1)];
        return s[((step % 16) + 16) % 16] == 'x';
    }
};

// Step sequencer for the carrier. Runs on a clock measured in steps, which the
// plugin locks to the host's beat position when the transport is playing.
class Arpeggiator
{
public:
    static constexpr int maxHeld = 16;

    struct Settings
    {
        ArpMode mode = ArpMode::off;
        ArpPattern pattern = ArpPattern::up;
        int octaves = 1;
        float gate = 0.6f;   // fraction of the step the note or gate stays open
        float swing = 0.0f;  // delays every second step by up to half a step
        int gatePattern = 0;
    };

    void prepare (double sampleRate)
    {
        smoothCoef = (float) std::exp (-1.0 / (0.002 * sampleRate));
        position = 0.0;
        lastStep = -1;
        gain = 1.0f;
    }

    void setSettings (const Settings& s, CarrierSynth& synth)
    {
        if (s.mode != settings.mode)
        {
            synth.allNotesOff();
            playingNote = -1;
            if (s.mode != ArpMode::notes)
                for (int i = 0; i < heldCount; ++i)
                    synth.noteOn (held[(size_t) i], heldVelocity);
        }
        settings = s;
        settings.octaves = std::clamp (s.octaves, 1, 4);
    }

    // Steps advanced per sample, from tempo and rate.
    void setIncrement (double stepsPerSample) { increment = stepsPerSample; }

    // Snap to the host's position (in steps) while its transport runs.
    void syncTo (double stepPosition) { position = stepPosition; }

    void noteOn (int note, float velocity, CarrierSynth& synth)
    {
        if (heldCount == 0)
            counter = 0;

        if (heldCount < maxHeld && std::find (held.begin(), held.begin() + heldCount, note) == held.begin() + heldCount)
            held[(size_t) heldCount++] = note;

        heldVelocity = velocity;
        if (settings.mode != ArpMode::notes)
            synth.noteOn (note, velocity);
    }

    void noteOff (int note, CarrierSynth& synth)
    {
        auto end = std::remove (held.begin(), held.begin() + heldCount, note);
        heldCount = (int) (end - held.begin());

        if (settings.mode != ArpMode::notes)
            synth.noteOff (note);
    }

    void allNotesOff (CarrierSynth& synth)
    {
        heldCount = 0;
        playingNote = -1;
        synth.allNotesOff();
    }

    // Advances one sample. Returns the rhythm gate's gain (1 when the gate is off).
    float process (CarrierSynth& synth)
    {
        const auto step = (int64_t) std::floor (position);
        const double frac = position - (double) step;
        position += increment;

        if (step != lastStep)
        {
            lastStep = step;
            triggered = false;
        }

        const double offset = (step % 2 != 0) ? settings.swing * 0.5 : 0.0;
        const double closeAt = offset + settings.gate * (1.0 - offset);
        const bool open = frac >= offset && frac < closeAt;

        if (settings.mode == ArpMode::notes)
        {
            if (playingNote >= 0 && (! open || heldCount == 0))
            {
                synth.noteOff (playingNote);
                playingNote = -1;
            }
            if (! triggered && open && heldCount > 0)
            {
                triggered = true;
                playingNote = nextNote();
                synth.noteOn (playingNote, heldVelocity);
            }
        }

        const float target = settings.mode == ArpMode::gate
                                 ? (open && GatePatterns::isOn (settings.gatePattern, step) ? 1.0f : 0.0f)
                                 : 1.0f;
        gain = target + smoothCoef * (gain - target);
        return gain;
    }

    int getPlayingNote() const { return playingNote; }

private:
    int nextNote()
    {
        std::array<int, maxHeld> order {};
        std::copy (held.begin(), held.begin() + heldCount, order.begin());
        if (settings.pattern != ArpPattern::asPlayed)
            std::sort (order.begin(), order.begin() + heldCount);

        const int length = heldCount * settings.octaves;
        int index = 0;

        switch (settings.pattern)
        {
            case ArpPattern::up:
            case ArpPattern::asPlayed:
                index = counter % length;
                break;
            case ArpPattern::down:
                index = length - 1 - counter % length;
                break;
            case ArpPattern::upDown:
            {
                const int period = std::max (1, 2 * length - 2);
                const int p = counter % period;
                index = p < length ? p : period - p;
                break;
            }
            case ArpPattern::random:
                randomState = randomState * 1664525u + 1013904223u;
                index = (int) ((randomState >> 8) % (uint32_t) length);
                break;
        }

        ++counter;
        return order[(size_t) (index % heldCount)] + 12 * (index / heldCount);
    }

    Settings settings;
    std::array<int, maxHeld> held {};
    int heldCount = 0;
    float heldVelocity = 1.0f;
    int playingNote = -1;
    int counter = 0;
    bool triggered = false;
    double position = 0.0, increment = 0.0;
    int64_t lastStep = -1;
    float gain = 1.0f, smoothCoef = 0.99f;
    uint32_t randomState = 12345u;
};
} // namespace vox
