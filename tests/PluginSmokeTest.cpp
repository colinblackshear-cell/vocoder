// Runs the whole plugin offline with every section switched on, checking the
// output stays finite, audible and under full scale.
#include "../Source/PluginProcessor.h"

#include <cstdio>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    VocoderProcessor processor;

    const double sampleRate = 48000.0;
    const int blockSize = 256;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    auto set = [&] (const char* id, float value)
    {
        auto* p = processor.params.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    int failures = 0;
    for (int arpMode = 0; arpMode < 3; ++arpMode)
    {
        set ("arpMode", (float) arpMode);
        set ("waveform", 2.0f);
        set ("lfoDepth", 2.0f);
        set ("cutoff", 2000.0f);
        set ("resonance", 0.8f);
        set ("voiceFollow", 2.0f);
        set ("drive", 0.5f);
        set ("crush", 0.3f);
        set ("chorusMix", 0.7f);
        set ("delayMix", 0.5f);
        set ("delayFeedback", 0.8f);
        set ("reverbMix", 0.5f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;
        float peak = 0.0f;
        bool finite = true;

        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (int note : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == 300)
                for (int note : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

            for (int i = 0; i < blockSize; ++i)
            {
                const float voice = 0.3f * (float) std::sin (phase) * (float) (1.0 + std::sin (phase * 0.013));
                phase += 2.0 * juce::MathConstants<double>::pi * 140.0 / sampleRate;
                buffer.setSample (0, i, voice);
                buffer.setSample (1, i, voice);
            }

            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float y = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (y);
                    peak = std::max (peak, std::abs (y));
                }
        }

        const bool ok = finite && peak > 0.05f && peak <= 1.0f;
        std::printf ("%s  arp mode %d: peak %.3f\n", ok ? "PASS" : "FAIL", arpMode, peak);
        if (! ok)
            ++failures;
    }

    // Feeds a sung-ish tone through a processor for some blocks; returns the peak.
    auto run = [&] (VocoderProcessor& proc, int blocks, bool& finite)
    {
        static double phase = 0.0;
        juce::AudioBuffer<float> buffer (2, blockSize);
        float peak = 0.0f;
        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float voice = 0.3f * (float) std::sin (phase) * (float) (1.0 + 0.5 * std::sin (phase * 0.013));
                phase += 2.0 * juce::MathConstants<double>::pi * 140.0 / sampleRate;
                buffer.setSample (0, i, voice);
                buffer.setSample (1, i, voice);
            }
            juce::MidiBuffer midi;
            proc.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    finite = finite && std::isfinite (buffer.getSample (ch, i));
            peak = std::max (peak, buffer.getMagnitude (0, blockSize));
        }
        return peak;
    };
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        failures += ok ? 0 : 1;
    };

    // Every factory preset makes sound, stays finite and under full scale.
    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        proc.setCurrentProgram (program);
        proc.requestChord ({ 48, 51, 55, 60 }, true);
        bool finite = true;
        const float peak = run (proc, 400, finite);
        check (finite && peak > 0.01f && peak <= 1.0f && proc.getCurrentProgram() == program,
               "preset " + proc.getProgramName (program) + ": peak " + juce::String (peak, 3));
    }

    // Presets leave performance settings alone and reset the rest.
    {
        VocoderProcessor proc;
        auto value = [&] (const char* id) { return proc.params.getRawParameterValue (id)->load(); };
        auto setIn = [&] (const char* id, float v)
        {
            auto* p = proc.params.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
        setIn ("inputGain", 6.0f);
        setIn ("tempo", 95.0f);
        setIn ("reverbMix", 0.9f);
        proc.setCurrentProgram (1); // Classic Robot: no reverb
        check (std::abs (value ("inputGain") - 6.0f) < 0.01f && std::abs (value ("tempo") - 95.0f) < 0.1f
                   && value ("reverbMix") < 0.01f && std::abs (value ("bands") - 16.0f) < 0.5f,
               "presets keep input gain and tempo, reset everything else");
    }

    // With Chord Timing on Next Bar, a chord clicked mid-bar waits for the bar line.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        auto* timing = proc.params.getParameter ("chordTiming");
        timing->setValueNotifyingHost (timing->convertTo0to1 (2.0f));
        bool finite = true;
        run (proc, 100, finite); // about a beat in at 120 BPM
        proc.requestChord ({ 48, 51, 55, 60 });
        const int barBlock = 96000 / blockSize; // next bar line, at beat 4
        const float before = run (proc, barBlock - 100 - 5, finite);
        run (proc, 10, finite);
        const float after = run (proc, 100, finite);
        check (finite && before < 0.01f && after > 0.05f,
               "chord waits for the next bar: before " + juce::String (before, 4) + ", after " + juce::String (after, 3));
    }

    // Record: a 1-bar count-in, then the take starts on the downbeat and stops
    // at a bar line, so the file is a whole number of bars.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        proc.requestChord ({ 48, 51, 55, 60 }, true);
        bool finite = true;
        run (proc, 37, finite); // start somewhere mid-bar

        const bool armed = proc.recorder.arm (sampleRate, 2);
        run (proc, 2, finite);
        const bool countingIn = proc.recorder.getState() == Recorder::State::countIn;
        run (proc, 375 + 225, finite); // count-in bar, then most of bar 1
        const bool recording = proc.recorder.getState() == Recorder::State::recording;
        proc.recorder.requestStop();

        bool saved = false;
        for (int i = 0; i < 1000 && ! saved; ++i)
        {
            run (proc, 1, finite);
            saved = proc.recorder.finishIfDone();
        }

        const auto file = proc.recorder.getLastFile();
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        juce::int64 length = -1;
        float peak = 0.0f;
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)))
        {
            length = reader->lengthInSamples;
            juce::AudioBuffer<float> take (2, (int) length);
            reader->read (&take, 0, (int) length, 0, true, true);
            peak = take.getMagnitude (0, (int) length);
        }
        check (armed && countingIn && recording && saved, "record button: count-in, recording, saved");
        check (length == 96000 && peak > 0.05f,
               "take is exactly one bar: " + juce::String (length) + " samples, peak " + juce::String (peak, 3));
        file.deleteFile();

        // Stop during the count-in cancels and leaves no file behind.
        proc.recorder.arm (sampleRate, 2);
        const auto cancelled = proc.recorder.getLastFile();
        run (proc, 10, finite);
        proc.recorder.requestStop();
        check (proc.recorder.getState() == Recorder::State::idle && ! cancelled.existsAsFile() && finite,
               "stopping during the count-in cancels the take");
    }

    return failures == 0 ? 0 : 1;
}
