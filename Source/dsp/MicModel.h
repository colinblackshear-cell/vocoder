#pragma once

#include "Biquad.h"
#include "Router.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace vox
{
// The Mic locker: makes your voice sound as if it went through a classic studio
// microphone instead of the laptop, phone or headset mic you're really using.
//
// It can't add detail the real mic never picked up. What it does is the tone and feel:
//   1. Your mic: the opposite of a typical laptop / phone / headset curve, so you start near flat.
//   2. The mic: the tone shape that studio mic family is known for, and the gentle
//      saturation of its tubes or transformer.
//   3. The singer: Distance (close-up bass boost, or a little room when far), Warmth,
//      Air, and Polish (rumble cut, a de-esser and a smooth leveler).
// The result is kept about as loud as what came in, so switching mics changes the tone,
// not the level.
//
// Pure C++ with no JUCE, so the vocoder, every Kaleidobox pedalboard and Pocket can share it.
class MicModel
{
public:
    enum Family { clean, tube, ribbon, dynamic };

    struct Band { float hz, db, q; };

    struct Mic
    {
        const char* name;
        Family family;
        const char* about;
        float highPass;          // Hz
        float lowShelfHz, lowShelfDb;
        Band peaks[3];           // db 0 = unused
        float highShelfHz, highShelfDb;
        float lowPass;           // Hz, 0 = none
        float drive;             // saturation, 0..1 at Warmth 50%
        float bias;              // asymmetry: tubes and transformers add even harmonics
        float proximity;         // how much closeness boosts the bass (omni pencils barely, ribbons a lot)
    };

    static constexpr int numMics = 12;

    static constexpr Mic mics[numMics] = {
        { "Natural", clean, "Only removes your mic's stock sound. Honest, nothing added",
          60, 0, 0, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 0, 0, 0, 0.0f, 0.0f, 0.4f },
        { "Studio Standard", clean, "The do-everything studio condenser. Even, clear, a touch of presence",
          80, 0, 0, { { 220, -1, 1 }, { 5000, 2, 0.8f }, { 0, 0, 1 } }, 12000, 1.5f, 0, 0.05f, 0.1f, 1.0f },
        { "Pop Air", clean, "Bright, glossy top for modern pop and R&B vocals",
          100, 0, 0, { { 300, -2, 1 }, { 3500, 1.5f, 1 }, { 0, 0, 1 } }, 10000, 5, 0, 0.05f, 0.1f, 1.0f },
        { "Pencil Clean", clean, "Small diaphragm. Flat, detailed, very natural",
          90, 150, -1.5f, { { 7000, 1, 2 }, { 0, 0, 1 }, { 0, 0, 1 } }, 9000, 1.5f, 0, 0.0f, 0.0f, 0.3f },
        { "Golden Tube", tube, "Warm and thick, smooth top, tube bloom. The 1950s legend sound",
          50, 180, 2.5f, { { 400, 1.5f, 0.9f }, { 3000, 1.5f, 1.2f }, { 9000, -1, 1 } }, 14000, -2, 0, 0.35f, 0.35f, 1.0f },
        { "Silk Tube", tube, "Open, airy top with tube warmth. Intimate and expensive",
          60, 150, 1, { { 1200, -1, 1 }, { 6000, 2, 0.8f }, { 0, 0, 1 } }, 11000, 3, 0, 0.25f, 0.3f, 1.0f },
        { "Velvet Tube", tube, "Darker and forgiving, rich lows. Kind to harsh voices",
          50, 200, 2, { { 2500, -1, 1 }, { 5000, 1, 1.5f }, { 0, 0, 1 } }, 10000, -2, 0, 0.3f, 0.35f, 1.0f },
        { "Classic Ribbon", ribbon, "Big round lows, soft dark top. The crooner and old radio studio",
          40, 150, 3, { { 700, 1, 0.8f }, { 0, 0, 1 }, { 0, 0, 1 } }, 6000, -6, 14000, 0.15f, 0.2f, 1.3f },
        { "Modern Ribbon", ribbon, "Smooth like a ribbon but more open. Great close up",
          45, 120, 2, { { 3000, 1, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, 8000, -3, 0, 0.1f, 0.15f, 1.2f },
        { "Broadcast", dynamic, "Radio, podcast and rap vocal. Full, dense, tamed highs",
          60, 200, 2, { { 2000, 1, 1 }, { 5500, 2, 1 }, { 0, 0, 1 } }, 9000, -4, 15000, 0.15f, 0.2f, 1.1f },
        { "Stage", dynamic, "The live vocal workhorse. Punchy mids, cuts through a band",
          100, 120, -2, { { 350, -1, 1 }, { 5000, 5, 1.2f }, { 0, 0, 1 } }, 10000, -5, 14000, 0.1f, 0.15f, 1.2f },
        { "Bullet", dynamic, "Old harmonica mic. Mid honk and grit for lo-fi hooks",
          300, 0, 0, { { 1500, 6, 0.9f }, { 0, 0, 1 }, { 0, 0, 1 } }, 5000, -8, 7000, 0.55f, 0.4f, 0.6f },
    };

    struct Source
    {
        const char* name;
        float lowShelfHz, lowShelfDb;
        Band peak;
        float highShelfHz, highShelfDb;
    };

    static constexpr int numSources = 5;

    // What you're singing into, and the opposite of its usual curve.
    static constexpr Source sources[numSources] = {
        { "Mac built-in", 200, 3, { 4000, -2, 1 }, 0, 0 },
        { "iPhone", 180, 2, { 2500, -2, 1 }, 9000, 1 },
        { "AirPods / headset", 250, 4, { 0, 0, 1 }, 6000, 4 },
        { "USB mic", 0, 0, { 6000, -1.5f, 1 }, 0, 0 },
        { "Pro mic / interface", 0, 0, { 0, 0, 1 }, 0, 0 },
    };

    static const char* familyName (Family f)
    {
        switch (f)
        {
            case clean: return "Clean and natural";
            case tube: return "Vintage tube";
            case ribbon: return "Ribbon";
            case dynamic: return "Dynamic";
        }
        return "";
    }

    struct Settings
    {
        int mic = 1;            // Studio Standard
        int source = 0;         // Mac built-in
        float distance = 0.4f;  // 0 = lips on the grille, 0.5 = a normal singing distance, 1 = across the room
        float warmth = 0.5f;
        float air = 0.5f;       // 0.5 = as the mic is
        float polish = 0.6f;
        float amount = 1.0f;

        bool operator== (const Settings& o) const
        {
            const auto same = [] (float a, float b) { return ! (a < b) && ! (b < a); };
            return mic == o.mic && source == o.source && same (distance, o.distance) && same (warmth, o.warmth)
                && same (air, o.air) && same (polish, o.polish);
        }
    };

    void prepare (double newSampleRate)
    {
        rate = newSampleRate;
        attack = coef (0.010);
        release = coef (0.150);
        essAttack = coef (0.002);
        essRelease = coef (0.060);
        dcCoef = (float) std::exp (-2.0 * pi * 15.0 / rate);
        for (auto& m : match)
            m.prepare (rate, 400.0, 0.3f, 3.0f);
        roomLength = std::min ((int) roomSize, (int) (rate * 0.045) + 1);
        configured = false;
        reset();
    }

    void reset()
    {
        for (auto& ch : channels)
            ch.reset();
        for (auto& m : match)
            m.reset();
        for (auto& r : room)
            r.fill (0.0f);
        roomPos = roomPosRight = 0;
    }

    void set (const Settings& s)
    {
        amount = std::clamp (s.amount, 0.0f, 1.0f);
        if (configured && s == current)
            return;
        current = s;
        configured = true;

        const auto& mic = mics[std::clamp (s.mic, 0, numMics - 1)];
        const auto& src = sources[std::clamp (s.source, 0, numSources - 1)];
        auto& f = design;

        f[0].setHighPass (rate, safe (mic.highPass), 0.707);
        shelf (f[1], src.lowShelfHz, src.lowShelfDb, false);
        peak (f[2], src.peak);
        shelf (f[3], src.highShelfHz, src.highShelfDb, true);
        shelf (f[4], mic.lowShelfHz, mic.lowShelfDb, false);
        peak (f[5], mic.peaks[0]);
        peak (f[6], mic.peaks[1]);
        peak (f[7], mic.peaks[2]);
        shelf (f[8], mic.highShelfHz, mic.highShelfDb, true);
        if (mic.lowPass > 0.0f && mic.lowPass < rate * 0.45)
            f[9].setLowPass (rate, mic.lowPass, 0.707);
        else
            f[9].setFlat();

        // Close up, a directional mic's bass swells; far away, the lows thin out.
        const float d = std::clamp (s.distance, 0.0f, 1.0f);
        const float proximityDb = (d < 0.5f ? (0.5f - d) * 14.0f : -(d - 0.5f) * 6.0f) * mic.proximity;
        shelf (f[10], 160.0f, proximityDb, false);
        shelf (f[11], 11000.0f, (std::clamp (s.air, 0.0f, 1.0f) - 0.5f) * 12.0f, true);
        const float p = std::clamp (s.polish, 0.0f, 1.0f);
        f[12].setHighPass (rate, 40.0 + p * 50.0, 0.707);

        roomMix = d > 0.5f ? (d - 0.5f) * 0.5f : 0.0f;
        driveAmount = mic.drive * std::clamp (s.warmth, 0.0f, 1.0f) * 2.0f;
        bias = mic.bias;
        polish = p;
        threshold = std::pow (10.0f, (-14.0f - p * 16.0f) / 20.0f);
        ratio = 1.0f + p * 3.0f;
        essDepth = p * 0.6f;
        essBand.setBandPass (rate, safe (7000.0f), 1.2);

        for (auto& ch : channels)
        {
            for (size_t i = 0; i < numFilters; ++i)
                ch.filters[i].copyCoefficients (design[i]);
            ch.ess.copyCoefficients (essBand);
        }
    }

    // One sample on one channel (0 or 1).
    float process (float x, int channel = 0)
    {
        auto& ch = channels[(size_t) std::clamp (channel, 0, 1)];
        float y = x;
        for (auto& f : ch.filters)
            y = f.process (y);

        // A little room when the singer stands back: a few early reflections.
        if (roomMix > 0.0f && channel == 0)
        {
            auto& line = room[0];
            line[(size_t) roomPos] = y;
            float reflections = 0.0f;
            for (int t = 0; t < 4; ++t)
            {
                const int back = (int) (roomLength * roomTaps[t]);
                reflections += line[(size_t) ((roomPos - back + roomLength) % roomLength)] * roomGains[t];
            }
            roomPos = (roomPos + 1) % roomLength;
            y += roomMix * reflections;
        }
        else if (roomMix > 0.0f)
        {
            auto& line = room[1];
            line[(size_t) roomPosRight] = y;
            float reflections = 0.0f;
            for (int t = 0; t < 4; ++t)
            {
                const int back = (int) (roomLength * roomTaps[3 - t]);
                reflections += line[(size_t) ((roomPosRight - back + roomLength) % roomLength)] * roomGains[t];
            }
            roomPosRight = (roomPosRight + 1) % roomLength;
            y += roomMix * reflections;
        }

        // Tube / transformer colour: soft, slightly lopsided saturation (the lopsidedness adds
        // the even harmonics), with the DC it creates taken straight back out.
        if (driveAmount > 0.001f)
        {
            const float k = 1.0f + driveAmount * 6.0f;
            const float b = bias * driveAmount * 0.3f;
            const float shaped = (std::tanh (k * y + b) - std::tanh (b)) / std::tanh (k);
            ch.dc = shaped + dcCoef * (ch.dc - shaped);
            const float lowered = shaped - ch.dc;
            y = y + std::min (1.0f, driveAmount * 1.5f) * (lowered - y);
        }

        if (polish > 0.001f)
        {
            // De-esser: when the "s" band gets loud, pull that band down.
            const float s = ch.ess.process (y);
            const float sLevel = std::abs (s);
            ch.essEnv = sLevel + (sLevel > ch.essEnv ? essAttack : essRelease) * (ch.essEnv - sLevel);
            const float bodyLevel = std::max (1.0e-5f, ch.env);
            const float over = std::clamp ((ch.essEnv / bodyLevel - 0.35f) * 2.0f, 0.0f, 1.0f);
            y -= s * over * essDepth;

            // Leveler: a gentle compressor that evens out loud and soft words.
            const float level = std::abs (y);
            ch.env = level + (level > ch.env ? attack : release) * (ch.env - level);
            if (ch.env > threshold)
            {
                const float overDb = 20.0f * std::log10 (ch.env / threshold);
                y *= std::pow (10.0f, -overDb * (1.0f - 1.0f / ratio) / 20.0f);
            }
        }
        else
        {
            const float level = std::abs (y);
            ch.env = level + (level > ch.env ? attack : release) * (ch.env - level);
        }

        // Same loudness as what came in, so a mic changes the tone, not the level.
        y *= match[(size_t) std::clamp (channel, 0, 1)].process (x, y);

        return x + amount * (y - x);
    }

    void process (float& left, float& right)
    {
        left = process (left, 0);
        right = process (right, 1);
    }

    const Settings& getSettings() const { return current; }

private:
    static constexpr size_t numFilters = 13;
    static constexpr int roomSize = 4096;
    static constexpr float roomTaps[4] = { 0.23f, 0.41f, 0.67f, 0.97f };
    static constexpr float roomGains[4] = { 0.5f, 0.38f, 0.28f, 0.2f };

    // Biquad keeps its coefficients private; this tiny wrapper copies them between banks.
    struct Filter : Biquad
    {
        void copyCoefficients (const Filter& other)
        {
            const float zz1 = z1, zz2 = z2;
            *this = other;
            z1 = zz1;
            z2 = zz2;
        }
    };

    struct Channel
    {
        std::array<Filter, numFilters> filters;
        Filter ess;
        float env = 0.0f, essEnv = 0.0f, dc = 0.0f;
        void reset()
        {
            for (auto& f : filters)
                f.reset();
            ess.reset();
            env = essEnv = dc = 0.0f;
        }
    };

    float coef (double seconds) const { return (float) std::exp (-1.0 / (seconds * rate)); }
    float safe (float hz) const { return std::clamp (hz, 10.0f, (float) (rate * 0.45)); }

    void shelf (Filter& f, float hz, float db, bool high) const
    {
        if (hz <= 0.0f || std::abs (db) < 0.01f)
            f.setFlat();
        else if (high)
            f.setHighShelf (rate, safe (hz), db);
        else
            f.setLowShelf (rate, safe (hz), db);
    }

    void peak (Filter& f, const Band& b) const
    {
        if (b.hz <= 0.0f || std::abs (b.db) < 0.01f)
            f.setFlat();
        else
            f.setPeak (rate, safe (b.hz), b.q, b.db);
    }

    double rate = 48000.0;
    Settings current;
    bool configured = false;
    float amount = 1.0f;
    std::array<Filter, numFilters> design;
    Filter essBand;
    std::array<Channel, 2> channels;
    std::array<LoudnessMatch, 2> match;
    std::array<std::array<float, roomSize>, 2> room {};
    int roomLength = 1, roomPos = 0, roomPosRight = 0;
    float roomMix = 0.0f, driveAmount = 0.0f, bias = 0.0f, polish = 0.0f;
    float threshold = 0.1f, ratio = 1.0f, essDepth = 0.0f;
    float attack = 0.99f, release = 0.999f, essAttack = 0.9f, essRelease = 0.999f, dcCoef = 0.999f;
};
} // namespace vox
