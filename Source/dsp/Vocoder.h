#pragma once

#include "Biquad.h"
#include "EnvelopeFollower.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace vox
{
// Channel vocoder: splits the modulator (voice) into bands, follows each band's
// loudness, and applies it to the same bands of the carrier (synth).
class Vocoder
{
public:
    static constexpr int maxBands = 40;
    static constexpr double lowestHz = 80.0;
    static constexpr double highestHz = 12000.0;

    struct Settings
    {
        int bands = 20;
        float sharpness = 1.0f;     // scales each band's Q
        float attackMs = 5.0f;
        float releaseMs = 60.0f;
        float formantSemitones = 0.0f; // shifts carrier bands against the voice bands
        float sibilance = 0.3f;     // passes the voice's high "s" and "t" sounds through
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        configured = false;
        setSettings (settings);
        reset();
    }

    void reset()
    {
        for (int i = 0; i < maxBands; ++i)
        {
            for (auto& f : modFilters[(size_t) i]) f.reset();
            for (auto& f : carFilters[(size_t) i]) f.reset();
            followers[(size_t) i].reset();
        }
        sibilanceFilter.reset();
    }

    void setSettings (const Settings& s)
    {
        auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f; };
        const bool bandsChanged = ! configured || s.bands != settings.bands || differs (s.sharpness, settings.sharpness)
                                  || differs (s.formantSemitones, settings.formantSemitones);
        settings = s;
        settings.bands = std::clamp (s.bands, 4, maxBands);

        for (auto& f : followers)
            f.setTimes (sampleRate, settings.attackMs, settings.releaseMs);

        if (! bandsChanged)
            return;

        const int n = settings.bands;
        const double octaves = std::log2 (highestHz / lowestHz);
        const double bandwidth = std::pow (2.0, octaves / n);
        const double q = std::sqrt (bandwidth) / (bandwidth - 1.0) * settings.sharpness;
        const double shift = std::pow (2.0, settings.formantSemitones / 12.0);
        const double nyquistGuard = sampleRate * 0.45;

        for (int i = 0; i < n; ++i)
        {
            const double centre = lowestHz * std::pow (2.0, octaves * (i + 0.5) / n);
            const double carrierCentre = std::min (centre * shift, nyquistGuard);
            for (auto& f : modFilters[(size_t) i]) f.setBandPass (sampleRate, std::min (centre, nyquistGuard), q);
            for (auto& f : carFilters[(size_t) i]) f.setBandPass (sampleRate, carrierCentre, q);
        }

        // Narrower bands pass less energy, so make up the difference.
        makeup = (float) (2.5 * std::sqrt (n / 20.0) * std::sqrt (settings.sharpness));
        sibilanceFilter.setHighPass (sampleRate, 5000.0, 0.707);
        configured = true;
    }

    float process (float modulator, float carrier)
    {
        float out = 0.0f;
        for (int i = 0; i < settings.bands; ++i)
        {
            float m = modulator, c = carrier;
            for (auto& f : modFilters[(size_t) i]) m = f.process (m);
            for (auto& f : carFilters[(size_t) i]) c = f.process (c);
            out += c * followers[(size_t) i].process (m);
        }
        return out * makeup + sibilanceFilter.process (modulator) * settings.sibilance;
    }

private:
    double sampleRate = 44100.0;
    Settings settings;
    bool configured = false;
    float makeup = 1.0f;
    std::array<std::array<Biquad, 2>, maxBands> modFilters {};
    std::array<std::array<Biquad, 2>, maxBands> carFilters {};
    std::array<EnvelopeFollower, maxBands> followers {};
    Biquad sibilanceFilter;
};

// Mutes the voice between phrases so the vocoder doesn't hum on room noise.
class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        detector.setTimes (sampleRate, 1.0, 80.0);
        smoothCoef = (float) std::exp (-1.0 / (0.005 * sampleRate));
        reset();
    }

    void reset()
    {
        detector.reset();
        gain = 0.0f;
    }

    void setThresholdDb (float db) { threshold = std::pow (10.0f, db / 20.0f); }

    float process (float x)
    {
        const float target = detector.process (x) > threshold ? 1.0f : 0.0f;
        gain = target + smoothCoef * (gain - target);
        return x * gain;
    }

private:
    EnvelopeFollower detector;
    float threshold = 0.001f;
    float smoothCoef = 0.99f;
    float gain = 0.0f;
};
} // namespace vox
