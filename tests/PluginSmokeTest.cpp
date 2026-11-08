// Runs the whole plugin offline with every section switched on, checking the
// output stays finite, audible and under full scale.
#include "../Source/PluginProcessor.h"
#include "../Source/EasySteps.h"
#include "../Source/dsp/Chords.h"

#include <cstdio>
#include <functional>
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

    // The robot on its own (a new instance starts as just your raw voice).
    auto robotOnly = [] (VocoderProcessor& proc)
    {
        // With gap filling off, so silence from the robot really is silence.
        for (auto [id, v] : { std::pair { "robotOn", 1.0f }, { "voiceOn", 0.0f }, { "arpOn", 1.0f }, { "fillGaps", 0.0f } })
        {
            auto* p = proc.params.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }
    };
    robotOnly (processor);

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

    // Easy mode: every tile in every step gives a sound that's audible, finite and under full scale,
    // and the tile shows as picked afterwards.
    for (const auto& step : EasySteps::all())
        for (const auto& tile : step.tiles)
        {
            VocoderProcessor proc;
            proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
            proc.prepareToPlay (sampleRate, blockSize);
            EasySteps::apply (proc.params, step, tile);
            const bool active = EasySteps::isActive (proc.params, step, tile);
            std::vector<int> chord = vox::Chords::build (0, 1, 0, (int) proc.params.getRawParameterValue ("chordFlavour")->load());
            juce::Array<int> notes;
            for (int n : chord)
                notes.add (n);
            proc.requestChord (notes, true);
            bool finite = true;
            const float peak = run (proc, 300, finite);
            check (finite && active && peak > 0.01f && peak <= 1.0f,
                   juce::String ("easy ") + step.title + " / " + tile.name + ": peak " + juce::String (peak, 3)
                       + (active ? "" : " (tile not lit)"));
        }

    // Hear switches: each part on its own.
    {
        auto make = [&] (bool robot, bool voice, bool fx, float& peak, bool fill = true)
        {
            VocoderProcessor proc;
            proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
            proc.prepareToPlay (sampleRate, blockSize);
            EasySteps::set (proc.params, "fillGaps", fill ? 1.0f : 0.0f);
            EasySteps::set (proc.params, "robotOn", robot ? 1.0f : 0.0f);
            EasySteps::set (proc.params, "voiceOn", voice ? 1.0f : 0.0f);
            EasySteps::set (proc.params, "fxOn", fx ? 1.0f : 0.0f);
            EasySteps::set (proc.params, "reverbMix", 0.5f);
            bool finite = true;
            run (proc, 200, finite); // let the level switches settle (your voice waits a moment to fill in)
            peak = run (proc, 300, finite);
            return finite;
        };
        float voiceOnly = 0, none = 0, noChordRobot = 0, noFill = 0;
        // No chord is playing: your voice fills in for the silent robot, and when everything is
        // switched off you're back to your raw voice. Only with gap filling off is the robot silent.
        bool ok = make (true, false, false, noChordRobot) && make (false, true, false, voiceOnly) && make (false, false, false, none)
                  && make (true, false, false, noFill, false);
        check (ok && noChordRobot > 0.2f && voiceOnly > 0.2f && none > 0.2f && noFill < 0.01f,
               "hear switches: robot alone (no chord) " + juce::String (noChordRobot, 3) + ", your voice alone "
                   + juce::String (voiceOnly, 3) + ", nothing on " + juce::String (none, 3) + ", no gap filling "
                   + juce::String (noFill, 4));
    }

    // Colin's switching sequence: raw voice, add an effect, add a rhythm, pick a robot preset at
    // the top, back to your voice, switch robots... Every step stays clearly audible and none
    // jumps far louder or quieter than your raw voice.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        auto val = [&] (const char* id) { return proc.params.getRawParameterValue (id)->load(); };
        bool chordPlaying = false, robotWas = false;
        int soundsSeen = proc.getSoundChanges();
        // What the editor does: a robot switched on or picked gets the key's chord if nothing is playing.
        auto editorWatch = [&]
        {
            const bool robot = val ("robotOn") > 0.5f;
            const bool picked = proc.getSoundChanges() != soundsSeen;
            soundsSeen = proc.getSoundChanges();
            if (robot && (! robotWas || picked) && val ("singToPlay") < 0.5f && ! chordPlaying)
            {
                proc.requestChord ({ 48, 52, 55 }, true);
                chordPlaying = true;
            }
            robotWas = robot;
        };
        double phase = 0.0, time = 0.0;
        juce::Random noise (1);
        auto level = [&] (double seconds)
        {
            juce::AudioBuffer<float> b (2, blockSize);
            double sum = 0.0;
            long count = 0;
            const int blocks = (int) (seconds * sampleRate / blockSize);
            for (int k = 0; k < blocks; ++k)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    // A buzzy, syllable-shaped "voice".
                    phase += 140.0 / sampleRate;
                    time += 1.0 / sampleRate;
                    const float saw = (float) (2.0 * (phase - std::floor (phase)) - 1.0);
                    const float env = (float) (0.5 + 0.5 * std::sin (time * juce::MathConstants<double>::twoPi * 3.0));
                    const float v = 0.3f * env * saw + 0.01f * (noise.nextFloat() * 2.0f - 1.0f);
                    b.setSample (0, i, v);
                    b.setSample (1, i, v);
                }
                juce::MidiBuffer m;
                proc.processBlock (b, m);
                if (k >= blocks / 2) // after the change has settled
                    for (int i = 0; i < blockSize; ++i, ++count)
                        sum += b.getSample (0, i) * b.getSample (0, i);
            }
            return std::sqrt (sum / (double) juce::jmax (1L, count));
        };
        auto findStep = [] (const char* title)
        {
            for (auto& st : EasySteps::all())
                if (juce::String (st.title) == title)
                    return &st;
            return (const EasySteps::Step*) nullptr;
        };
        auto tile = [&] (const char* stepTitle, const char* name)
        {
            const auto* st = findStep (stepTitle);
            for (auto& t : st->tiles)
                if (juce::String (t.name) == name)
                    EasySteps::apply (proc.params, *st, t);
            if (juce::String (stepTitle) == "Voice")
                proc.soundPicked();
            editorWatch();
        };
        auto preset = [&] (const char* name)
        {
            for (int i = 0; i < proc.getNumPrograms(); ++i)
                if (proc.getProgramName (i) == name)
                    proc.setCurrentProgram (i);
            editorWatch();
        };
        auto flip = [&] (const char* id)
        {
            EasySteps::set (proc.params, id, val (id) > 0.5f ? 0.0f : 1.0f);
            editorWatch();
        };
        auto stopChords = [&]
        {
            proc.requestChord ({}, true);
            chordPlaying = false;
        };

        const double raw = level (1.5);
        struct Step { const char* what; std::function<void()> action; double low, high; };
        const std::vector<Step> sequence {
            { "add Echo", [&] { tile ("Effects", "Echo"); }, 0.6, 1.6 },
            { "add Climb arp", [&] { tile ("Rhythm", "Climb"); }, 0.6, 2.0 },
            { "Monster preset at the top", [&] { preset ("Monster"); }, 0.5, 1.6 },
            { "back to Raw voice tile", [&] { tile ("Voice", "Raw"); }, 0.5, 1.6 },
            { "Monster voice tile", [&] { tile ("Voice", "Monster"); }, 0.5, 2.0 },
            { "Raw voice tile again", [&] { tile ("Voice", "Raw"); }, 0.5, 1.6 },
            { "Trance gate", [&] { tile ("Rhythm", "Trance"); }, 0.3, 1.6 },
            { "chain bar: robot on", [&] { flip ("robotOn"); }, 0.3, 2.0 },
            { "chain bar: your voice off", [&] { flip ("voiceOn"); }, 0.3, 1.6 },
            { "chain bar: robot off too", [&] { flip ("robotOn"); }, 0.3, 1.6 },
            { "Raw Voice preset", [&] { preset ("Raw Voice"); }, 0.8, 1.25 },
            { "Classic Robot preset", [&] { preset ("Classic Robot"); }, 0.5, 1.6 },
            { "Stop all chords", [&] { stopChords(); }, 0.5, 1.6 },
            { "Monster preset with nothing playing", [&] { preset ("Monster"); }, 0.5, 1.6 },
            { "Choir preset", [&] { preset ("Choir Pad"); }, 0.4, 1.6 },
        };
        for (const auto& step : sequence)
        {
            step.action();
            const double ratio = level (1.5) / raw;
            check (ratio > step.low && ratio < step.high,
                   juce::String ("switching: ") + step.what + " at " + juce::String (juce::Decibels::gainToDecibels (ratio), 1)
                       + " dB from your raw voice");
        }
    }

    // A new instance is just your raw voice, and the rhythm gate chops it with no robot.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        bool finite = true;
        const float raw = run (proc, 200, finite);
        EasySteps::set (proc.params, "arpOn", 1.0f);
        EasySteps::set (proc.params, "arpMode", 2.0f);
        EasySteps::set (proc.params, "gatePattern", 1.0f); // eighths: on, off, on, off
        // Over two bars of eighths, many blocks fall in the gaps and many in the open steps.
        int quiet = 0, loud = 0;
        run (proc, 10, finite);
        for (int i = 0; i < 2 * 96000 / blockSize; ++i)
        {
            const float level = run (proc, 1, finite);
            quiet += level < 0.02f ? 1 : 0;
            loud += level > 0.1f ? 1 : 0;
        }
        check (finite && raw > 0.2f && quiet > 150 && loud > 150,
               "raw voice by default (" + juce::String (raw, 3) + "), gate chops it with no robot: "
                   + juce::String (loud) + " loud / " + juce::String (quiet) + " quiet blocks");
    }

    // Effects tiles switch off again, back to the defaults.
    {
        VocoderProcessor proc;
        const auto& fx = EasySteps::all()[3];
        bool ok = true;
        for (const auto& tile : fx.tiles)
        {
            EasySteps::apply (proc.params, fx, tile);
            ok = ok && EasySteps::isActive (proc.params, fx, tile);
            EasySteps::apply (proc.params, fx, tile);
            ok = ok && ! EasySteps::isActive (proc.params, fx, tile);
        }
        check (ok, "easy effects tiles turn on and off");
    }

    // Switches never get stuck: an effect tile whose settings are there while Effects is
    // switched off shows as off, and clicking it switches Effects back on.
    {
        VocoderProcessor proc;
        const auto& fx = EasySteps::all()[3];
        const auto& echo = fx.tiles.front();
        EasySteps::apply (proc.params, fx, echo);
        EasySteps::set (proc.params, "fxOn", 0.0f);
        bool ok = ! EasySteps::isActive (proc.params, fx, echo);
        EasySteps::apply (proc.params, fx, echo);
        ok = ok && EasySteps::isActive (proc.params, fx, echo) && EasySteps::value (proc.params, "fxOn") > 0.5f;
        check (ok, "an effect tile switches Effects back on");

        // After a robot preset (your voice off), the Raw tile brings your voice back.
        proc.setCurrentProgram (1);
        const auto& voiceStep = EasySteps::all()[0];
        EasySteps::apply (proc.params, voiceStep, voiceStep.tiles.front());
        check (EasySteps::value (proc.params, "voiceOn") > 0.5f && EasySteps::value (proc.params, "robotOn") < 0.5f,
               "the Raw tile brings your voice back after a robot preset");
    }

    // From your raw voice, an arp tile or a chord type switches the robot on, since only the robot plays notes.
    {
        VocoderProcessor proc;
        const auto& rhythm = EasySteps::all()[2];
        const auto& chords = EasySteps::all()[1];
        bool ok = true;
        for (const auto& tile : rhythm.tiles)
        {
            proc.setCurrentProgram (0);
            EasySteps::apply (proc.params, rhythm, tile);
            const bool noteArp = EasySteps::value (proc.params, "arpMode") > 0.5f && EasySteps::value (proc.params, "arpMode") < 1.5f;
            ok = ok && (! noteArp || EasySteps::value (proc.params, "robotOn") > 0.5f);
        }
        proc.setCurrentProgram (0);
        EasySteps::apply (proc.params, chords, chords.tiles[1]);
        ok = ok && EasySteps::value (proc.params, "robotOn") > 0.5f;
        check (ok, "arp tiles and chord types switch the robot on");
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

    // Sing to Play: with no keys pressed, singing an A makes the vocoder play A3.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        auto setIn = [&] (const char* id, float v)
        {
            auto* p = proc.params.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
        robotOnly (proc);
        bool finite = true;
        run (proc, 10, finite); // let the level switches settle
        const float silent = run (proc, 100, finite); // no notes yet: the vocoder stays quiet
        setIn ("singToPlay", 1.0f);
        setIn ("tuneOn", 1.0f);
        run (proc, 50, finite);
        const float singing = run (proc, 200, finite);
        const int target = proc.getTargetNote();
        check (finite && silent < 0.01f && singing > 0.05f && target >= 0,
               "sing to play: quiet " + juce::String (silent, 4) + ", singing " + juce::String (singing, 3)
                   + ", note " + juce::MidiMessage::getMidiNoteName (target, true, true, 3));
    }

    // With Chord Timing on Next Bar, a chord clicked mid-bar waits for the bar line.
    {
        VocoderProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        robotOnly (proc);
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
