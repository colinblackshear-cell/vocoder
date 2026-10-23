#pragma once

#include <cmath>

namespace vox
{
// Peak follower with separate attack and release times.
class EnvelopeFollower
{
public:
    void setTimes (double sampleRate, double attackMs, double releaseMs)
    {
        attackCoef = coefFor (sampleRate, attackMs);
        releaseCoef = coefFor (sampleRate, releaseMs);
    }

    void reset() { level = 0.0f; }

    float process (float x)
    {
        const float rectified = std::abs (x);
        const float coef = rectified > level ? attackCoef : releaseCoef;
        level = rectified + coef * (level - rectified);
        return level;
    }

    float getLevel() const { return level; }

private:
    static float coefFor (double sampleRate, double ms)
    {
        const double samples = std::fmax (ms, 0.01) * 0.001 * sampleRate;
        return (float) std::exp (-1.0 / samples);
    }

    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float level = 0.0f;
};
} // namespace vox
