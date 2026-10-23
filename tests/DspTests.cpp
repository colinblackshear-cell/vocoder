// Plain C++ tests for the DSP code. No JUCE needed, so they build in seconds.
#include "../Source/dsp/CarrierSynth.h"
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

    for (auto wave : { vox::Waveform::square, vox::Waveform::supersaw })
    {
        s.waveform = wave;
        synth.setSettings (s);
        const double level = rms ([&] { return synth.process(); }, 4800);
        check (level > 0.1 && level < 1.5, "every waveform plays at a sane level");
    }
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
} // namespace

int main()
{
    testBandPass();
    testSynth();
    testVocoder();
    testGate();
    std::printf ("\n%s\n", failures == 0 ? "All tests passed" : "Some tests failed");
    return failures == 0 ? 0 : 1;
}
