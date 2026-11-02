#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace vox
{
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        buffer.assign ((size_t) maxSamples + 2, 0.0f);
        writeIndex = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) writeIndex] = x;
        if (++writeIndex >= (int) buffer.size())
            writeIndex = 0;
    }

    // Reads the sample written `delay` samples ago, with linear interpolation.
    float read (float delay) const
    {
        const int size = (int) buffer.size();
        delay = std::clamp (delay, 1.0f, (float) (size - 2));
        float pos = (float) writeIndex - delay;
        if (pos < 0.0f)
            pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float t = pos - (float) i0;
        return buffer[(size_t) i0] + t * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

private:
    std::vector<float> buffer;
    int writeIndex = 0;
};

// Drive (tanh saturation) followed by a bit crusher.
class Distortion
{
public:
    // drive and crush 0..1, where 0 is off.
    void set (float newDrive, float newCrush)
    {
        drive = newDrive;
        crush = newCrush;
        levels = std::pow (2.0f, 16.0f - 13.0f * crush);
        holdSamples = 1 + (int) (crush * 7.0f);
    }

    float process (float x)
    {
        if (drive > 0.0f)
        {
            const float amount = 1.0f + drive * 20.0f;
            x = std::tanh (x * amount) / std::tanh (amount) * (1.0f - 0.4f * drive);
        }

        if (crush > 0.0f)
        {
            if (++holdCount >= holdSamples)
            {
                holdCount = 0;
                held = std::round (x * levels) / levels;
            }
            x = held;
        }
        return x;
    }

private:
    float drive = 0.0f, crush = 0.0f, levels = 65536.0f, held = 0.0f;
    int holdSamples = 1, holdCount = 0;
};

// Stereo chorus: two modulated delays, their LFOs a quarter cycle apart.
class Chorus
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        left.prepare ((int) (0.05 * sampleRate));
        right.prepare ((int) (0.05 * sampleRate));
    }

    void reset()
    {
        left.reset();
        right.reset();
    }

    void set (float rateHz, float depth, float newMix)
    {
        increment = rateHz / sampleRate;
        depthSamples = (float) (depth * 0.007 * sampleRate);
        mix = newMix;
    }

    void process (float& l, float& r)
    {
        if (mix <= 0.0f)
            return;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        const float base = (float) (0.012 * sampleRate);
        const float modL = (float) std::sin (2.0 * pi * phase);
        const float modR = (float) std::cos (2.0 * pi * phase);
        left.push (l);
        right.push (r);
        const float wetL = left.read (base + depthSamples * (0.5f + 0.5f * modL));
        const float wetR = right.read (base + depthSamples * (0.5f + 0.5f * modR));
        l += (wetL - l) * mix * 0.5f;
        r += (wetR - r) * mix * 0.5f;
    }

private:
    double sampleRate = 44100.0, phase = 0.0, increment = 0.0;
    float depthSamples = 0.0f, mix = 0.0f;
    DelayLine left, right;
};

// Tempo-synced stereo delay, optionally bouncing between left and right.
class StereoDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        left.prepare ((int) (2.5 * sampleRate));
        right.prepare ((int) (2.5 * sampleRate));
        smoothCoef = (float) std::exp (-1.0 / (0.05 * sampleRate));
        tone.setHighPass (sampleRate, 120.0, 0.707);
    }

    void reset()
    {
        left.reset();
        right.reset();
    }

    void set (double seconds, float newFeedback, float newMix, bool newPingPong)
    {
        targetDelay = (float) std::clamp (seconds * sampleRate, 1.0, 2.4 * sampleRate);
        if (currentDelay <= 0.0f)
            currentDelay = targetDelay;
        feedback = std::clamp (newFeedback, 0.0f, 0.95f);
        mix = newMix;
        pingPong = newPingPong;
    }

    void process (float& l, float& r)
    {
        if (mix <= 0.0f)
            return;

        currentDelay = targetDelay + smoothCoef * (currentDelay - targetDelay);
        const float echoL = left.read (currentDelay);
        const float echoR = right.read (currentDelay);

        if (pingPong)
        {
            // Mono input goes into the left line only, then each echo crosses sides.
            left.push (tone.process ((l + r) * 0.5f) + echoR * feedback);
            right.push (echoL * feedback);
        }
        else
        {
            left.push (l + echoL * feedback);
            right.push (r + echoR * feedback);
        }

        l += echoL * mix;
        r += echoR * mix;
    }

private:
    double sampleRate = 44100.0;
    float targetDelay = 0.0f, currentDelay = 0.0f, smoothCoef = 0.999f;
    float feedback = 0.4f, mix = 0.0f;
    bool pingPong = true;
    Biquad tone;
    DelayLine left, right;
};
} // namespace vox
