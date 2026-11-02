#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vox
{
// Finds the pitch of a monophonic voice with the YIN method, run on a 2x
// downsampled copy of the input every 128 downsampled samples.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate / 2.0;
        window = std::max (64, (int) std::round (rate * 0.024)); // 24 ms: two periods of 80 Hz
        minLag = std::max (2, (int) (rate / 1000.0));             // up to 1 kHz
        maxLag = std::min (window - 2, (int) (rate / 70.0));       // down to 70 Hz
        hop = std::max (32, (int) std::round (rate * 0.004));
        buffer.assign ((size_t) (window + maxLag + 1), 0.0f);
        diff.assign ((size_t) maxLag + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        sinceHop = 0;
        half = false;
        pending = 0.0f;
        frequency = 0.0f;
        voiced = false;
    }

    // Feed one input sample. Returns true when a new estimate is ready.
    bool process (float x)
    {
        // Average pairs of samples: a crude low-pass that's plenty for finding pitch.
        if (! half)
        {
            pending = x;
            half = true;
            return false;
        }
        half = false;
        buffer[(size_t) writePos] = 0.5f * (pending + x);
        writePos = (writePos + 1) % (int) buffer.size();

        if (++sinceHop < hop)
            return false;
        sinceHop = 0;
        analyse();
        return true;
    }

    bool isVoiced() const { return voiced; }
    float getFrequency() const { return frequency; } // Hz, valid while voiced

private:
    float at (int i) const // i = 0 is the oldest sample in the analysis frame
    {
        return buffer[(size_t) ((writePos + i) % (int) buffer.size())];
    }

    void analyse()
    {
        float energy = 0.0f;
        for (int i = 0; i < window; ++i)
            energy += at (i) * at (i);
        if (energy / (float) window < 1.0e-6f) // about -60 dBFS
        {
            voiced = false;
            return;
        }

        // Cumulative mean normalised difference function.
        float running = 0.0f;
        diff[0] = 1.0f;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            float d = 0.0f;
            for (int i = 0; i < window; ++i)
            {
                const float delta = at (i) - at (i + lag);
                d += delta * delta;
            }
            running += d;
            diff[(size_t) lag] = running > 0.0f ? d * (float) lag / running : 1.0f;
        }

        int best = -1;
        for (int lag = minLag; lag < maxLag; ++lag)
            if (diff[(size_t) lag] < threshold)
            {
                while (lag + 1 < maxLag && diff[(size_t) lag + 1] < diff[(size_t) lag])
                    ++lag;
                best = lag;
                break;
            }

        if (best < 0)
        {
            voiced = false;
            return;
        }

        // Parabolic interpolation for a fractional lag.
        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float denom = a - 2.0f * b + c;
        const float shift = std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f;
        frequency = (float) (rate / ((double) best + std::clamp (shift, -0.5f, 0.5f)));
        voiced = true;
    }

    static constexpr float threshold = 0.15f;
    double rate = 22050.0;
    int window = 512, minLag = 22, maxLag = 300, hop = 96;
    std::vector<float> buffer, diff;
    int writePos = 0, sinceHop = 0;
    bool half = false;
    float pending = 0.0f, frequency = 0.0f;
    bool voiced = false;
};

// Musical scales for snapping, as 12-bit masks (bit 0 = the key's root).
struct Scales
{
    static constexpr int count = 6;
    static constexpr int masks[count] = {
        0b111111111111, // Chromatic
        0b101010110101, // Major
        0b010110101101, // Minor (natural)
        0b001010010101, // Major pentatonic
        0b010010101001, // Minor pentatonic
        0b010011101001, // Blues
    };

    static bool contains (int scale, int key, int note)
    {
        const int degree = ((note - key) % 12 + 12) % 12;
        return (masks[std::clamp (scale, 0, count - 1)] >> degree) & 1;
    }

    // Nearest note in the scale to a fractional MIDI note.
    static int nearest (float midi, int key, int scale)
    {
        const int centre = (int) std::lround (midi);
        int best = centre;
        float bestDistance = 1.0e9f;
        for (int n = centre - 6; n <= centre + 6; ++n)
            if (contains (scale, key, n) && std::abs ((float) n - midi) < bestDistance)
            {
                bestDistance = std::abs ((float) n - midi);
                best = n;
            }
        return best;
    }

    // The note a diatonic step (a third = 2 steps, a fifth = 4) above `note` in the scale.
    static int stepsAbove (int note, int steps, int key, int scale)
    {
        int n = note;
        for (int s = 0; s < steps; ++s)
            do
                ++n;
            while (! contains (scale, key, n) && n < note + 24);
        return n;
    }
};

// Shifts the pitch of a voice by a ratio, with two crossfading read heads on a
// delay line. The head length follows the voice's period, which keeps the
// crossfades in phase and the sound clean for the small shifts auto-tune makes.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate;
        buffer.assign ((size_t) std::ceil (sampleRate * 0.1), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase = 0.0;
        length = rate * 0.03;
        nextLength = length;
    }

    // Period of the input in samples (0 if unknown); the head length becomes a multiple of it.
    void setPeriod (double samples)
    {
        const double minimum = rate * 0.025;
        if (samples > 1.0)
            nextLength = 2.0 * std::ceil (minimum / (2.0 * samples)) * samples; // even, so the heads (half a length apart) stay in phase
        else
            nextLength = minimum;
        nextLength = std::min (nextLength, (double) buffer.size() * 0.45);
    }

    float process (float x, float ratio)
    {
        const int size = (int) buffer.size();
        buffer[(size_t) writePos] = x;

        // phase runs 0..1 over one head length; the delay shrinks for ratio > 1.
        const double step = (1.0 - ratio) / length;
        const double before = phase;
        phase += step;
        if (phase >= 1.0 || phase < 0.0)
            phase -= std::floor (phase);

        // Each time a head passes its silent point (head 0 at phase 0, head 1 at
        // phase 0.5), take up a new length, keeping the other head's delay where
        // it was. Small steps keep the silent head's jump inaudible.
        const bool head0Silent = (step > 0.0 && phase < before) || (step < 0.0 && phase > before);
        const bool head1Silent = (before < 0.5) != (phase < 0.5) && ! head0Silent;
        if ((head0Silent || head1Silent) && std::abs (nextLength - length) > 0.5)
        {
            const double newLength = std::clamp (nextLength, length * 0.88, length * 1.12);
            // The audible head sits at 0.5 of its cycle; keep its delay (0.5 * length) unchanged.
            const double audible = 0.5 * length / newLength;
            phase = head0Silent ? audible + (step > 0.0 ? -0.5 : 0.5) : audible;
            phase = std::clamp (phase, 0.0, 0.999999);
            length = newLength;
        }

        float out = 0.0f;
        for (int head = 0; head < 2; ++head)
        {
            double p = phase + 0.5 * head;
            p -= std::floor (p);
            const double delay = 2.0 + p * length;
            const float gain = (float) std::sin (pi * p); // sin^2 windows, half a cycle apart, sum to 1
            double readPos = (double) writePos - delay;
            while (readPos < 0.0)
                readPos += size;
            const int i0 = (int) readPos;
            const float frac = (float) (readPos - i0);
            const float a = buffer[(size_t) i0], b = buffer[(size_t) ((i0 + 1) % size)];
            out += gain * gain * (a + frac * (b - a));
        }

        writePos = (writePos + 1) % size;
        return out;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double rate = 48000.0, phase = 0.0, length = 1440.0, nextLength = 1440.0;
    std::vector<float> buffer;
    int writePos = 0;
};

// Pitch correction plus the note the voice is singing, for "sing to play".
class AutoTune
{
public:
    struct Settings
    {
        bool correct = false;   // shift the voice onto the scale
        int key = 0;            // 0 = C
        int scale = 0;          // index into Scales
        float speedMs = 20.0f;  // 0 = instant (the hard robotic effect)
        float amount = 1.0f;    // 0..1 of the correction
    };

    void prepare (double sampleRate)
    {
        rate = sampleRate;
        tracker.prepare (sampleRate);
        shifter.prepare (sampleRate);
        reset();
    }

    void reset()
    {
        tracker.reset();
        shifter.reset();
        correction = 0.0f;
        targetCorrection = 0.0f;
        heldNote = candidate = -1;
        candidateTime = unvoicedTime = 0;
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        speedCoef = s.speedMs <= 0.5f ? 0.0f : (float) std::exp (-1.0 / (s.speedMs * 0.001 * rate));
    }

    float process (float x)
    {
        if (tracker.process (x))
            update();

        correction = targetCorrection + speedCoef * (correction - targetCorrection);
        if (! settings.correct)
            return x;
        const float ratio = std::exp2 (correction * settings.amount / 12.0f);
        return shifter.process (x, ratio);
    }

    // The scale note being sung, or -1 when silent; changes only once a new note holds for 40 ms.
    int getSungNote() const { return heldNote; }
    bool isVoiced() const { return tracker.isVoiced(); }
    float getDetectedMidi() const { return detectedMidi; }

private:
    void update()
    {
        const int hopSamples = (int) (rate * 0.004);
        if (! tracker.isVoiced())
        {
            shifter.setPeriod (0.0);
            targetCorrection = 0.0f; // let unvoiced sounds (s, t, breaths) through untouched
            unvoicedTime += hopSamples;
            if (unvoicedTime > (int) (rate * 0.12))
                heldNote = candidate = -1;
            return;
        }
        unvoicedTime = 0;

        const float f = tracker.getFrequency();
        detectedMidi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        shifter.setPeriod (rate / f);

        // Stick with the current note until the voice is clearly nearer another (hysteresis).
        int target = Scales::nearest (detectedMidi, settings.key, settings.scale);
        if (heldNote >= 0 && target != heldNote && std::abs (detectedMidi - (float) heldNote) < 0.65f
            && Scales::contains (settings.scale, settings.key, heldNote))
            target = heldNote;
        targetCorrection = std::clamp ((float) target - detectedMidi, -6.0f, 6.0f);

        if (target != candidate)
        {
            candidate = target;
            candidateTime = 0;
        }
        candidateTime += hopSamples;
        if (candidateTime >= (int) (rate * 0.04) || heldNote < 0)
            heldNote = candidate;
    }

    Settings settings;
    double rate = 48000.0;
    PitchTracker tracker;
    PitchShifter shifter;
    float speedCoef = 0.0f, correction = 0.0f, targetCorrection = 0.0f, detectedMidi = 0.0f;
    int heldNote = -1, candidate = -1, candidateTime = 0, unvoicedTime = 0;
};
} // namespace vox
