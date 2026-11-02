#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vox
{
enum class FilterMode
{
    off = 0,
    lowPass,
    highPass,
    bandPass,
    notch
};

// Zero-delay-feedback state variable filter (Cytomic / Simper). Stays stable
// while the cutoff is swept every sample, which is what LFO and envelope
// modulation need.
class StateVariableFilter
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() { ic1 = ic2 = 0.0f; }

    void setMode (FilterMode m) { mode = m; }

    // resonance 0..1 maps to Q 0.5..20.
    void setCutoff (float hz, float resonance)
    {
        const double fc = std::clamp ((double) hz, 20.0, sampleRate * 0.45);
        const double g = std::tan (pi * fc / sampleRate);
        k = (float) (1.0 / (0.5 * std::pow (40.0, std::clamp ((double) resonance, 0.0, 1.0))));
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) g * a1;
        a3 = (float) g * a2;
    }

    float process (float v0)
    {
        if (mode == FilterMode::off)
            return v0;

        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        switch (mode)
        {
            case FilterMode::lowPass: return v2;
            case FilterMode::highPass: return v0 - k * v1 - v2;
            case FilterMode::bandPass: return v1;
            case FilterMode::notch: return v0 - k * v1;
            case FilterMode::off: break;
        }
        return v0;
    }

private:
    double sampleRate = 44100.0;
    FilterMode mode = FilterMode::lowPass;
    float k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

enum class LfoShape
{
    sine = 0,
    triangle,
    sawDown,
    square,
    sampleAndHold
};

class Lfo
{
public:
    void setShape (LfoShape s) { shape = s; }
    void setIncrement (double cyclesPerSample) { increment = cyclesPerSample; }
    void syncTo (double cycles) { phase = cycles - std::floor (cycles); }

    // Returns -1..1.
    float process()
    {
        const double p = phase;
        phase += increment;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            randomState = randomState * 1664525u + 1013904223u;
            held = (float) (randomState >> 8) / 8388608.0f - 1.0f;
        }

        switch (shape)
        {
            case LfoShape::sine: return (float) std::sin (2.0 * pi * p);
            case LfoShape::triangle: return (float) (1.0 - 4.0 * std::abs (p - 0.5));
            case LfoShape::sawDown: return (float) (1.0 - 2.0 * p);
            case LfoShape::square: return p < 0.5 ? 1.0f : -1.0f;
            case LfoShape::sampleAndHold: return held;
        }
        return 0.0f;
    }

private:
    LfoShape shape = LfoShape::sine;
    double phase = 0.0, increment = 0.0;
    float held = 0.0f;
    uint32_t randomState = 987654u;
};
} // namespace vox
