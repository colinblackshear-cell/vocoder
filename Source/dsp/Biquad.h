#pragma once

#include <cmath>

namespace vox
{
constexpr double pi = 3.14159265358979323846;

// RBJ cookbook biquad, transposed direct form II.
class Biquad
{
public:
    void setBandPass (double sampleRate, double frequency, double q)
    {
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (alpha / a0);
        b1 = 0.0f;
        b2 = (float) (-alpha / a0);
        a1 = (float) (-2.0 * std::cos (w0) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighPass (double sampleRate, double frequency, double q)
    {
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 + cosw) / 2.0) / a0);
        b1 = (float) (-(1.0 + cosw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setLowPass (double sampleRate, double frequency, double q)
    {
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cosw) / 2.0) / a0);
        b1 = (float) ((1.0 - cosw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    // A bell boost or cut of `gainDb` around `frequency`.
    void setPeak (double sampleRate, double frequency, double q, double gainDb)
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha / a;
        b0 = (float) ((1.0 + alpha * a) / a0);
        b1 = (float) (-2.0 * cosw / a0);
        b2 = (float) ((1.0 - alpha * a) / a0);
        a1 = b1;
        a2 = (float) ((1.0 - alpha / a) / a0);
    }

    // A low shelf: everything below `frequency` boosted or cut by `gainDb` (slope 1).
    void setLowShelf (double sampleRate, double frequency, double gainDb)
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (a) * alpha;
        const double a0 = (a + 1.0) + (a - 1.0) * cosw + sq;
        b0 = (float) (a * ((a + 1.0) - (a - 1.0) * cosw + sq) / a0);
        b1 = (float) (2.0 * a * ((a - 1.0) - (a + 1.0) * cosw) / a0);
        b2 = (float) (a * ((a + 1.0) - (a - 1.0) * cosw - sq) / a0);
        a1 = (float) (-2.0 * ((a - 1.0) + (a + 1.0) * cosw) / a0);
        a2 = (float) (((a + 1.0) + (a - 1.0) * cosw - sq) / a0);
    }

    // A high shelf: everything above `frequency` boosted or cut by `gainDb` (slope 1).
    void setHighShelf (double sampleRate, double frequency, double gainDb)
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * frequency / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (a) * alpha;
        const double a0 = (a + 1.0) - (a - 1.0) * cosw + sq;
        b0 = (float) (a * ((a + 1.0) + (a - 1.0) * cosw + sq) / a0);
        b1 = (float) (-2.0 * a * ((a - 1.0) + (a + 1.0) * cosw) / a0);
        b2 = (float) (a * ((a + 1.0) + (a - 1.0) * cosw - sq) / a0);
        a1 = (float) (2.0 * ((a - 1.0) - (a + 1.0) * cosw) / a0);
        a2 = (float) (((a + 1.0) - (a - 1.0) * cosw - sq) / a0);
    }

    // Passes everything unchanged.
    void setFlat() { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

    void reset() { z1 = z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

protected:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};
} // namespace vox
