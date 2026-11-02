// Plain C++ tests for the DSP code. No JUCE needed, so they build in seconds.
#include "../Source/dsp/Arpeggiator.h"
#include "../Source/dsp/AutoTune.h"
#include "../Source/dsp/CarrierSynth.h"
#include "../Source/dsp/Effects.h"
#include "../Source/dsp/Filter.h"
#include "../Source/dsp/Vocoder.h"

#include <cstdio>
#include <functional>
#include <vector>

namespace
{
constexpr double sr = 48000.0;
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

double rms (const std::function<float()>& next, int samples)
{
    double sum = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const double x = next();
        sum += x * x;
    }
    return std::sqrt (sum / samples);
}

float sine (double& phase, double hz)
{
    const float out = (float) std::sin (phase);
    phase += 2.0 * vox::pi * hz / sr;
    return out;
}

void testBandPass()
{
    auto response = [] (double hz)
    {
        vox::Biquad f;
        f.setBandPass (sr, 1000.0, 4.0);
        double phase = 0.0;
        for (int i = 0; i < 4800; ++i) f.process (sine (phase, hz));
        return rms ([&] { return f.process (sine (phase, hz)); }, 48000) * std::sqrt (2.0);
    };
    check (std::abs (response (1000.0) - 1.0) < 0.05, "band pass has unity gain at its centre");
    check (response (100.0) < 0.05, "band pass rejects a tone far below its centre");
}

void testSynth()
{
    vox::CarrierSynth synth;
    synth.prepare (sr);
    vox::CarrierSynth::Settings s;
    s.noise = 0.0f;
    s.releaseMs = 20.0f;
    synth.setSettings (s);

    check (rms ([&] { return synth.process(); }, 4800) < 1.0e-6, "synth is silent with no notes");

    synth.noteOn (60, 1.0f);
    check (rms ([&] { return synth.process(); }, 4800) > 0.2, "note on plays the carrier");

    synth.noteOff (60);
    rms ([&] { return synth.process(); }, 24000);
    check (rms ([&] { return synth.process(); }, 4800) < 1.0e-3, "note off releases to silence");

    s.drone = true;
    synth.setSettings (s);
    check (rms ([&] { return synth.process(); }, 4800) > 0.2, "drone plays with no keys held");

    for (auto wave : { vox::Waveform::square, vox::Waveform::supersaw, vox::Waveform::pulse, vox::Waveform::triangle })
    {
        s.waveform = wave;
        synth.setSettings (s);
        const double level = rms ([&] { return synth.process(); }, 4800);
        check (level > 0.1 && level < 1.5, "every waveform plays at a sane level");
    }
}

void testSynthExtras()
{
    vox::CarrierSynth synth;
    synth.prepare (sr);
    vox::CarrierSynth::Settings s;
    s.noise = 0.0f;
    s.waveform = vox::Waveform::pulse;
    s.pulseWidth = 0.1f;
    s.pwmDepth = 1.0f;
    s.osc2Level = 1.0f;
    s.osc2Wave = vox::Waveform::triangle;
    s.subLevel = 1.0f;
    s.vibratoCents = 50.0f;
    s.glideMs = 200.0f;
    synth.setSettings (s);

    bool finite = true;
    double level = 0.0;
    for (int note : { 48, 55, 60, 67 })
    {
        synth.noteOn (note, 1.0f);
        level = std::max (level, rms ([&] {
            const float y = synth.process();
            finite = finite && std::isfinite (y);
            return y;
        }, 9600));
    }
    check (finite && level > 0.2 && level < 2.0, "second oscillator, sub, PWM, glide and vibrato stay at a sane level");

    // Glide: right after a jump the pitch is still near the old note.
    vox::CarrierSynth g;
    g.prepare (sr);
    vox::CarrierSynth::Settings gs;
    gs.noise = 0.0f;
    gs.glideMs = 300.0f;
    g.setSettings (gs);
    auto crossings = [&] (int samples)
    {
        int count = 0;
        float last = g.process();
        for (int i = 0; i < samples; ++i)
        {
            const float y = g.process();
            if (last < 0.0f && y >= 0.0f)
                ++count;
            last = y;
        }
        return count;
    };
    g.noteOn (45, 1.0f); // A2, 110 Hz
    crossings (24000);
    g.noteOff (45);
    g.noteOn (69, 1.0f); // A4, 440 Hz
    const int early = crossings (2400);   // first 50 ms
    crossings (48000);
    const int late = crossings (2400);
    check (early < late * 3 / 4, "glide slides up instead of jumping");
}

void testVocoder()
{
    vox::Vocoder vocoder;
    vocoder.prepare (sr);
    vox::Vocoder::Settings vs;
    vs.sibilance = 0.0f;
    vocoder.setSettings (vs);

    vox::CarrierSynth synth;
    synth.prepare (sr);
    vox::CarrierSynth::Settings cs;
    cs.drone = true;
    synth.setSettings (cs);

    check (rms ([&] { return vocoder.process (0.0f, synth.process()); }, 9600) < 1.0e-4,
           "no voice means no output, even with the carrier playing");

    // A rough "vowel": a 150 Hz buzz with a few harmonics.
    double phase = 0.0;
    auto voice = [&]
    {
        const float a = sine (phase, 150.0);
        return 0.3f * (a + 0.5f * a * a * a);
    };
    const double level = rms ([&] { return vocoder.process (voice(), synth.process()); }, 48000);
    check (level > 0.02 && level < 1.5, "voice plus carrier gives a usable output level");

    for (int bands : { 8, 32, 40 })
    {
        vs.bands = bands;
        vs.formantSemitones = 7.0f;
        vocoder.setSettings (vs);
        const double l = rms ([&] { return vocoder.process (voice(), synth.process()); }, 24000);
        check (l > 0.02 && l < 1.5, "level stays usable across band counts and formant shift");
    }
}

void testGate()
{
    vox::NoiseGate gate;
    gate.prepare (sr);
    gate.setThresholdDb (-40.0f);
    double phase = 0.0;
    check (rms ([&] { return gate.process (0.001f * sine (phase, 200.0)); }, 9600) < 1.0e-5,
           "gate mutes quiet room noise");
    check (rms ([&] { return gate.process (0.5f * sine (phase, 200.0)); }, 9600) > 0.3, "gate opens for the voice");
}
void testArpeggiator()
{
    vox::Arpeggiator::Settings s;
    s.mode = vox::ArpMode::notes;
    s.pattern = vox::ArpPattern::up;
    s.octaves = 2;

    vox::CarrierSynth synth;
    synth.prepare (sr);
    synth.setSettings ({});
    vox::Arpeggiator arp;
    arp.prepare (sr);
    arp.setSettings (s, synth);
    arp.setIncrement (1.0 / 1000.0);

    // Play the chord in the "wrong" order; the up pattern should sort it.
    arp.noteOn (64, 1.0f, synth);
    arp.noteOn (60, 1.0f, synth);
    arp.noteOn (67, 1.0f, synth);
    check (synth.getHeldNoteCount() == 0, "note arp holds chord notes back from the synth");

    std::vector<int> seen;
    int lastHeld = 0;
    for (int i = 0; i < 7 * 1000; ++i)
    {
        arp.process (synth);
        const int held = synth.getHeldNoteCount();
        if (held > lastHeld)
            seen.push_back (i / 1000);
        lastHeld = held;
    }
    check (seen.size() == 7, "note arp plays one note per step");
    check (synth.getHeldNoteCount() <= 1, "note arp never stacks notes");

    // Pattern order: up over two octaves, then up/down.
    auto sequence = [] (vox::ArpPattern pattern, int steps)
    {
        vox::Arpeggiator::Settings ps;
        ps.mode = vox::ArpMode::notes;
        ps.pattern = pattern;
        ps.octaves = 2;
        vox::CarrierSynth probe;
        probe.prepare (sr);
        probe.setSettings ({});
        vox::Arpeggiator a;
        a.prepare (sr);
        a.setSettings (ps, probe);
        a.setIncrement (1.0 / 100.0);
        for (int n : { 64, 60, 67 })
            a.noteOn (n, 1.0f, probe);
        std::vector<int> notes;
        for (int i = 0; i < steps * 100; ++i)
        {
            a.process (probe);
            if (i % 100 == 10)
                notes.push_back (a.getPlayingNote());
        }
        return notes;
    };
    check (sequence (vox::ArpPattern::up, 7) == std::vector<int> { 60, 64, 67, 72, 76, 79, 60 },
           "up pattern climbs the sorted chord across octaves");
    check (sequence (vox::ArpPattern::upDown, 8) == std::vector<int> { 60, 64, 67, 72, 76, 79, 76, 72 },
           "up/down pattern turns around at the top");
    check (sequence (vox::ArpPattern::down, 3) == std::vector<int> { 79, 76, 72 }, "down pattern starts at the top");

    arp.noteOff (60, synth);
    arp.noteOff (64, synth);
    arp.noteOff (67, synth);
    for (int i = 0; i < 2000; ++i)
        arp.process (synth);
    check (synth.getHeldNoteCount() == 0, "note arp stops when keys are released");

    // Switching the arp off hands held keys straight to the synth.
    arp.noteOn (60, 1.0f, synth);
    s.mode = vox::ArpMode::off;
    arp.setSettings (s, synth);
    check (synth.getHeldNoteCount() == 1, "turning the arp off plays held keys normally");

    // Rhythm gate: "Eighths" is on, off, on, off...
    s.mode = vox::ArpMode::gate;
    s.gatePattern = 1;
    s.gate = 0.9f;
    arp.setSettings (s, synth);
    arp.syncTo (0.0);
    std::vector<float> gains;
    for (int i = 0; i < 4 * 1000; ++i)
    {
        const float g = arp.process (synth);
        if (i % 1000 == 500)
            gains.push_back (g);
    }
    check (gains.size() == 4 && gains[0] > 0.99f && gains[1] < 0.01f && gains[2] > 0.99f && gains[3] < 0.01f,
           "rhythm gate follows its pattern");
}

void testFilter()
{
    auto level = [] (vox::FilterMode mode, double hz)
    {
        vox::StateVariableFilter f;
        f.prepare (sr);
        f.setMode (mode);
        f.setCutoff (1000.0f, 0.0f);
        double phase = 0.0;
        for (int i = 0; i < 4800; ++i) f.process (sine (phase, hz));
        return rms ([&] { return f.process (sine (phase, hz)); }, 24000) * std::sqrt (2.0);
    };
    check (level (vox::FilterMode::lowPass, 100.0) > 0.9 && level (vox::FilterMode::lowPass, 8000.0) < 0.05,
           "low pass keeps lows and cuts highs");
    check (level (vox::FilterMode::highPass, 100.0) < 0.05 && level (vox::FilterMode::highPass, 8000.0) > 0.9,
           "high pass keeps highs and cuts lows");

    // Full resonance with the cutoff swept every sample must stay stable.
    vox::StateVariableFilter f;
    f.prepare (sr);
    f.setMode (vox::FilterMode::lowPass);
    vox::Lfo lfo;
    lfo.setIncrement (5.0 / sr);
    double phase = 0.0;
    bool finite = true;
    float peak = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        f.setCutoff (1000.0f * std::exp2 (3.0f * lfo.process()), 1.0f);
        const float y = f.process (0.3f * sine (phase, 220.0));
        finite = finite && std::isfinite (y);
        peak = std::max (peak, std::abs (y));
    }
    check (finite && peak < 50.0f, "resonant filter stays stable while swept");
}

void testEffects()
{
    vox::Chorus chorus;
    chorus.prepare (sr);
    chorus.set (0.8f, 0.5f, 1.0f);
    vox::StereoDelay delay;
    delay.prepare (sr);
    delay.set (0.25, 0.9f, 0.5f, true);
    vox::Distortion dist;
    dist.set (1.0f, 1.0f);

    double phase = 0.0;
    bool finite = true;
    float peak = 0.0f;
    for (int i = 0; i < 96000 * 2; ++i)
    {
        float x = dist.process (i < 4800 ? 0.5f * sine (phase, 330.0) : 0.0f);
        float l = x, r = x;
        chorus.process (l, r);
        delay.process (l, r);
        finite = finite && std::isfinite (l) && std::isfinite (r);
        peak = std::max ({ peak, std::abs (l), std::abs (r) });
    }
    check (finite && peak < 4.0f, "drive, crush, chorus and high-feedback delay stay bounded");

    // Ping-pong: the first echo lands on the left, the next on the right.
    vox::StereoDelay pp;
    pp.prepare (sr);
    pp.set (0.1, 0.5f, 1.0f, true);
    const int echo = (int) (0.1 * sr);
    float firstL = 0.0f, firstR = 0.0f, secondR = 0.0f;
    for (int i = 0; i < echo * 3; ++i)
    {
        float l = (i == 0) ? 1.0f : 0.0f, r = l;
        pp.process (l, r);
        if (i > echo / 2 && i < echo * 3 / 2)
        {
            firstL = std::max (firstL, std::abs (l));
            firstR = std::max (firstR, std::abs (r));
        }
        if (i > echo * 3 / 2 && i < echo * 5 / 2)
            secondR = std::max (secondR, std::abs (r));
    }
    check (firstL > 0.3f && firstR < 0.01f && secondR > 0.1f, "ping-pong delay bounces left then right");
}
// A voice-like test tone: a few harmonics, falling off like a sung vowel.
float voiceTone (double& phase, double hz)
{
    const float out = 0.5f * (float) (std::sin (phase) + 0.6 * std::sin (2 * phase) + 0.4 * std::sin (3 * phase)
                                      + 0.2 * std::sin (5 * phase));
    phase += 2.0 * vox::pi * hz / sr;
    return out;
}

// Average pitch, in Hz, that a fresh tracker finds in a signal.
double measurePitch (const std::vector<float>& signal)
{
    vox::PitchTracker t;
    t.prepare (sr);
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < signal.size(); ++i)
        if (t.process (signal[i]) && t.isVoiced() && i > signal.size() / 3)
        {
            sum += t.getFrequency();
            ++count;
        }
    return count > 0 ? sum / count : 0.0;
}

void testAutoTune()
{
    for (double hz : { 82.0, 110.0, 196.0, 261.6, 440.0, 700.0 })
    {
        std::vector<float> tone;
        double phase = 0.0;
        for (int i = 0; i < (int) sr / 2; ++i)
            tone.push_back (voiceTone (phase, hz));
        const double found = measurePitch (tone);
        char what[96];
        std::snprintf (what, sizeof what, "pitch tracker finds %.1f Hz (got %.2f)", hz, found);
        check (std::abs (found - hz) / hz < 0.005, what);
    }

    {
        vox::PitchTracker t;
        t.prepare (sr);
        bool voiced = false;
        unsigned seed = 1;
        for (int i = 0; i < (int) sr / 2; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            if (t.process (((float) (seed >> 8) / 16777216.0f - 0.5f) * 0.5f) && i > 4800)
                voiced = voiced || t.isVoiced();
        }
        check (! voiced, "pitch tracker ignores noise");
    }

    for (double semis : { 1.0, -1.0, 3.0 })
    {
        vox::PitchShifter shifter;
        shifter.prepare (sr);
        shifter.setPeriod (sr / 220.0);
        std::vector<float> out;
        double phase = 0.0;
        const float ratio = (float) std::exp2 (semis / 12.0);
        for (int i = 0; i < (int) sr; ++i)
            out.push_back (shifter.process (voiceTone (phase, 220.0), ratio));
        const double expected = 220.0 * ratio, found = measurePitch (out);

        // Level stays steady once the heads lock to the voice's period (no phasey dips).
        double quietest = 1.0e9, loudest = 0.0;
        for (size_t start = (size_t) sr / 3; start + 480 < out.size(); start += 480)
        {
            double e = 0.0;
            for (size_t i = start; i < start + 480; ++i)
                e += out[i] * out[i];
            quietest = std::min (quietest, e);
            loudest = std::max (loudest, e);
        }
        const double steadiness = std::sqrt (quietest / loudest);

        char what[128];
        std::snprintf (what, sizeof what, "pitch shifter moves 220 Hz by %+.0f semitones (%.1f Hz, want %.1f), level steady (%.2f)",
                       semis, found, expected, steadiness);
        check (std::abs (found - expected) / expected < 0.01 && steadiness > 0.8, what);
    }

    // A slightly sharp A (226 Hz) in C major snaps to A3 = 220 Hz.
    {
        vox::AutoTune tune;
        tune.prepare (sr);
        tune.setSettings ({ true, 0, 1, 0.0f, 1.0f });
        std::vector<float> out;
        double phase = 0.0;
        for (int i = 0; i < (int) sr; ++i)
            out.push_back (tune.process (voiceTone (phase, 226.0)));
        const double found = measurePitch (out);
        char what[96];
        std::snprintf (what, sizeof what, "auto-tune pulls 226 Hz to 220 Hz (got %.1f), sung note A3 (got %d)", found, tune.getSungNote());
        check (std::abs (found - 220.0) < 1.5 && tune.getSungNote() == 57, what);
    }

    // C# isn't in C major: 280 Hz (just above C#4) goes to the nearer scale note, D4.
    {
        vox::AutoTune tune;
        tune.prepare (sr);
        tune.setSettings ({ true, 0, 1, 0.0f, 1.0f });
        std::vector<float> out;
        double phase = 0.0;
        for (int i = 0; i < (int) sr; ++i)
            out.push_back (tune.process (voiceTone (phase, 280.0))); // a bit above C#4
        const double found = measurePitch (out);
        check (std::abs (found - 293.66) < 2.0, "auto-tune moves an out-of-key note to the nearest scale note (D4)");
    }

    // Off: the voice passes through untouched.
    {
        vox::AutoTune tune;
        tune.prepare (sr);
        tune.setSettings ({ false, 0, 1, 0.0f, 1.0f });
        double phase = 0.0, phase2 = 0.0;
        float maxDiff = 0.0f;
        for (int i = 0; i < 4800; ++i)
            maxDiff = std::max (maxDiff, std::abs (tune.process (voiceTone (phase, 226.0)) - voiceTone (phase2, 226.0)));
        check (maxDiff < 1.0e-6f, "auto-tune off leaves the voice untouched");
    }

    check (vox::Scales::stepsAbove (60, 2, 0, 1) == 64 && vox::Scales::stepsAbove (62, 2, 0, 1) == 65
               && vox::Scales::stepsAbove (57, 4, 9, 2) == 64,
           "harmony steps follow the scale (C-E, D-F, A-E)");
}
} // namespace

int main()
{
    testBandPass();
    testSynth();
    testSynthExtras();
    testVocoder();
    testGate();
    testArpeggiator();
    testFilter();
    testEffects();
    testAutoTune();
    std::printf ("\n%s\n", failures == 0 ? "All tests passed" : "Some tests failed");
    return failures == 0 ? 0 : 1;
}
