#pragma once

#include "Biquad.h"
#include "Effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vox
{
// Makes everything sound as if it were sung into an old microphone and
// recorded on the media of its decade: the mic's narrow, coloured tone, the
// grit of tubes or tape, and the record's hiss, crackle and wobble.
//
// Signal order, like a real old recording:
//   squash (compression) -> mic tone -> saturation -> + surface noise
//   -> wow and flutter -> the medium's bandwidth -> bit depth -> level
class Vintage
{
public:
    struct Era
    {
        const char* name;
        float lowCut, highCut;   // Hz: the bandwidth of mic plus medium
        bool steep;              // 24 dB/oct edges instead of 12, for the narrow old media
        float peakHz, peakDb, peakQ;
        float peak2Hz, peak2Db, peak2Q;
        float drive, bias;       // saturation, and its asymmetry (tubes and carbon mics)
        float squash;            // compression, 0..1
        float hiss, crackle, surface, hum; // noise levels (crackle in pops per second)
        float wowPercent, wowHz, flutterPercent; // pitch wobble from uneven speed
        int bits;                // 0 = full resolution
        float sampleRate;        // 0 = full rate (the sampler era's grainy rates)
        bool mono;
        float makeupDb;
    };

    static constexpr int numEras = 11;

    // name, lowCut, highCut, steep, peak, peak2, drive, bias, squash, hiss, crackle, surface, hum, wow, wowHz, flutter, bits, rate, mono, makeup
    static constexpr Era eras[numEras] = {
        { "1900s Wax Cylinder", 350, 2600, true, 900, 8, 2.0f, 1900, 5, 3.0f, 0.5f, 0.10f, 0.6f, 0.020f, 25, 0.015f, 0, 0.60f, 0.6f, 0.25f, 0, 0, true, 12 },
        { "1920s Gramophone", 250, 3800, true, 1100, 6, 1.5f, 2500, 3, 2.0f, 0.35f, 0.05f, 0.5f, 0.008f, 14, 0.012f, 0, 0.35f, 0.55f, 0.12f, 0, 0, true, 10 },
        { "1930s Radio", 200, 4500, true, 1500, 5, 1.0f, 3000, 2, 1.5f, 0.6f, 0.15f, 0.7f, 0.012f, 3, 0.004f, 0.006f, 0.05f, 0.5f, 0.0f, 0, 0, true, 11.5 },
        { "1940s Ribbon", 70, 7000, false, 200, 3, 0.7f, 4000, -3, 0.8f, 0.2f, 0.02f, 0.4f, 0.004f, 6, 0.006f, 0.002f, 0.12f, 0.5f, 0.05f, 0, 0, true, 3 },
        { "1950s Tube", 50, 11000, false, 150, 2, 0.7f, 5000, 3, 0.8f, 0.35f, 0.12f, 0.45f, 0.004f, 2.5f, 0.003f, 0.001f, 0.08f, 0.5f, 0.03f, 0, 0, true, 3.5 },
        { "1960s Tape", 40, 14000, false, 120, 2.5f, 0.8f, 2500, 2, 0.7f, 0.45f, 0.04f, 0.55f, 0.005f, 0, 0, 0, 0.10f, 0.8f, 0.06f, 0, 0, false, 4.5 },
        { "1970s Studio", 30, 17000, false, 250, 1.5f, 0.6f, 8000, 2, 0.6f, 0.25f, 0.03f, 0.5f, 0.002f, 0, 0, 0, 0.04f, 0.6f, 0.02f, 0, 0, false, 4 },
        { "1980s Cassette", 70, 9000, true, 100, 2, 0.9f, 3500, 1.5f, 0.7f, 0.3f, 0.02f, 0.5f, 0.012f, 0, 0, 0, 0.25f, 1.1f, 0.12f, 0, 0, false, 4.5 },
        { "1990s Sampler", 40, 11000, true, 100, 3, 0.7f, 6000, 0, 1.0f, 0.25f, 0.0f, 0.3f, 0.0f, 0, 0, 0, 0.0f, 0.5f, 0.0f, 12, 26040, false, 2 },
        { "Telephone", 400, 3200, true, 1800, 6, 1.2f, 3000, 0, 1.0f, 0.55f, 0.10f, 0.6f, 0.004f, 1, 0.002f, 0, 0.0f, 0.5f, 0.0f, 0, 0, true, 14.5 },
        { "Megaphone", 500, 4000, true, 1500, 9, 2.0f, 3000, 4, 2.0f, 0.8f, 0.0f, 0.5f, 0.0f, 0, 0, 0, 0.0f, 0.5f, 0.0f, 0, 0, true, 16 },
    };

    void prepare (double newSampleRate)
    {
        rate = newSampleRate;
        for (auto& line : lines)
            line.prepare ((int) (rate * 0.05));
        follower = 0.0f;
        smoothCoef = (float) std::exp (-1.0 / (0.05 * rate));
        attackCoef = (float) std::exp (-1.0 / (0.005 * rate));
        releaseCoef = (float) std::exp (-1.0 / (0.12 * rate));
        surfaceCoef = (float) std::exp (-2.0 * pi * 400.0 / rate);
        crackleDecay = (float) std::exp (-1.0 / (0.0004 * rate));
        currentEra = -1;
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            line.reset();
        for (auto& ch : channels)
            ch = {};
        follower = surfaceState = crackleLevel = 0.0f;
        wowPhase = flutterPhase = humPhase = 0.0;
        holdCount = 0.0f;
        currentEra = -1;
    }

    // era: index into eras. wear 0..1: how worn the record is (0.5 = typical). amount 0..1: blend with the clean sound.
    void set (int era, float newWear, float newAmount)
    {
        era = std::clamp (era, 0, numEras - 1);
        wear = std::clamp (newWear, 0.0f, 1.0f) * 2.0f;
        amount = std::clamp (newAmount, 0.0f, 1.0f);
        if (era == currentEra)
            return;
        currentEra = era;
        e = eras[era];

        for (auto& ch : channels)
        {
            ch.lowCut[0].setHighPass (rate, e.lowCut, 0.707);
            ch.lowCut[1].setHighPass (rate, e.lowCut, 0.707);
            const double top = std::min ((double) e.highCut, rate * 0.45);
            ch.highCut[0].setLowPass (rate, top, 0.707);
            ch.highCut[1].setLowPass (rate, top, 0.707);
            ch.peak.setPeak (rate, e.peakHz, e.peakQ, e.peakDb);
            if (e.peak2Hz > 0.0f && std::abs (e.peak2Db) > 0.01f)
                ch.peak2.setPeak (rate, std::min ((double) e.peak2Hz, rate * 0.4), e.peak2Q, e.peak2Db);
            else
                ch.peak2.setPeak (rate, 1000.0, 1.0, 0.0);
        }
        driveAmount = 1.0f + e.drive * 8.0f;
        biasOffset = std::tanh (e.bias);
        biasSlope = 1.0f - biasOffset * biasOffset;
        makeup = std::pow (10.0f, e.makeupDb / 20.0f);
        holdStep = e.sampleRate > 0.0f ? (float) (e.sampleRate / rate) : 1.0f;
        levels = e.bits > 0 ? std::pow (2.0f, (float) (e.bits - 1)) : 0.0f;
    }

    void process (float& left, float& right)
    {
        if (currentEra < 0 || amount <= 0.0f)
            return;

        const float dryL = left, dryR = right;
        float in[2] = { left, right };
        if (e.mono)
            in[0] = in[1] = 0.5f * (left + right);

        // Squash: an envelope on the louder channel turns loud parts down, like the limiters of the day.
        const float level = std::max (std::abs (in[0]), std::abs (in[1]));
        follower = level + (level > follower ? attackCoef : releaseCoef) * (follower - level);
        const float squashGain = follower > 0.05f ? std::pow (0.05f / follower, e.squash * 0.6f) : 1.0f;

        // The record's own noise, shared by both sides.
        const float noise = surfaceNoise();

        // Wow (slow) and flutter (fast) wobble the pitch by sweeping a short delay.
        const double twoPi = 2.0 * pi;
        wowPhase += e.wowHz / rate;
        flutterPhase += 7.3 / rate;
        wowPhase -= std::floor (wowPhase);
        flutterPhase -= std::floor (flutterPhase);
        const float wowDepth = (float) (e.wowPercent * 0.01 * wear / (twoPi * e.wowHz) * rate);
        const float flutterDepth = (float) (e.flutterPercent * 0.01 * wear / (twoPi * 7.3) * rate);
        const float targetDelay = 2.0f + wowDepth + flutterDepth;
        baseDelay = targetDelay + smoothCoef * (baseDelay - targetDelay);
        const float delay = baseDelay + wowDepth * (float) std::sin (twoPi * wowPhase)
                            + flutterDepth * (float) std::sin (twoPi * flutterPhase);

        // Grainy sample rate (the 90s sampler): hold each sample for a while.
        holdCount += holdStep;
        const bool takeSample = holdCount >= 1.0f;
        if (takeSample)
            holdCount -= 1.0f;

        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            auto& ch = channels[(size_t) c];
            float x = in[c] * squashGain;
            x = ch.peak2.process (ch.peak.process (x));
            x = (std::tanh (driveAmount * x + e.bias) - biasOffset) / (driveAmount * biasSlope);
            x += noise;

            lines[(size_t) c].push (x);
            x = lines[(size_t) c].read (delay);

            x = ch.lowCut[0].process (x);
            x = ch.highCut[0].process (x);
            if (e.steep)
            {
                x = ch.lowCut[1].process (x);
                x = ch.highCut[1].process (x);
            }

            if (holdStep < 1.0f)
            {
                if (takeSample)
                    ch.held = x;
                x = ch.held;
            }
            if (levels > 0.0f)
                x = std::round (x * levels) / levels;

            out[c] = x * makeup;
        }

        left = dryL + (out[0] - dryL) * amount;
        right = dryR + (out[1] - dryR) * amount;
    }

    float getAmount() const { return amount; }

private:
    float random() // -1..1
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) / 8388608.0f - 1.0f;
    }

    float surfaceNoise()
    {
        float n = random() * e.hiss * wear;

        // Rumble and grit of the surface: low-passed noise.
        surfaceState = random() * (1.0f - surfaceCoef) + surfaceCoef * surfaceState;
        n += surfaceState * e.surface * wear * 6.0f;

        // Crackle: random pops of random size.
        if (e.crackle > 0.0f && (random() * 0.5f + 0.5f) < e.crackle * wear / (float) rate)
        {
            const float size = random() * 0.5f + 0.5f;
            crackleLevel = 0.25f * size * size * size * (random() > 0.0f ? 1.0f : -1.0f);
        }
        n += crackleLevel * random();
        crackleLevel *= crackleDecay;

        // Mains hum from old radios.
        if (e.hum > 0.0f)
        {
            humPhase += 60.0 / rate;
            humPhase -= std::floor (humPhase);
            const double a = 2.0 * pi * humPhase;
            n += e.hum * wear * (float) (std::sin (a) + 0.5 * std::sin (3.0 * a));
        }
        return n;
    }

    struct Channel
    {
        Biquad lowCut[2], highCut[2], peak, peak2;
        float held = 0.0f;
    };

    double rate = 48000.0;
    Era e = eras[0];
    int currentEra = -1;
    float wear = 1.0f, amount = 0.0f;
    float driveAmount = 1.0f, biasOffset = 0.0f, biasSlope = 1.0f, makeup = 1.0f, levels = 0.0f;
    float follower = 0.0f, attackCoef = 0.0f, releaseCoef = 0.0f, smoothCoef = 0.999f, baseDelay = 2.0f;
    float surfaceState = 0.0f, surfaceCoef = 0.9f, crackleLevel = 0.0f, crackleDecay = 0.9f;
    float holdStep = 1.0f, holdCount = 0.0f;
    double wowPhase = 0.0, flutterPhase = 0.0, humPhase = 0.0;
    uint32_t seed = 12345u;
    std::array<Channel, 2> channels;
    std::array<DelayLine, 2> lines;
};
} // namespace vox
