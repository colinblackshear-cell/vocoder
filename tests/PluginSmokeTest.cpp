// Runs the whole plugin offline with every section switched on, checking the
// output stays finite, audible and under full scale.
#include "../Source/PluginProcessor.h"

#include <cstdio>

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

    // The on-screen keyboard and chord buttons feed notes through keyboardState,
    // and the Record button writes the output to a WAV.
    {
        set ("arpMode", 0.0f);
        set ("delayMix", 0.0f);
        set ("reverbMix", 0.0f);
        processor.keyboardState.allNotesOff (1);

        const bool started = processor.recorder.start (sampleRate, 2);
        for (int note : { 48, 51, 55, 60 })
            processor.keyboardState.noteOn (1, note, 0.8f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;
        float peak = 0.0f;
        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float voice = 0.3f * (float) std::sin (phase);
                phase += 2.0 * juce::MathConstants<double>::pi * 140.0 / sampleRate;
                buffer.setSample (0, i, voice);
                buffer.setSample (1, i, voice);
            }
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, blockSize));
        }
        processor.recorder.stop();

        const auto file = processor.recorder.getLastFile();
        const bool played = peak > 0.05f;
        const bool recorded = started && file.getSize() > 200 * blockSize * 2 * 3;
        std::printf ("%s  on-screen notes play the vocoder: peak %.3f\n", played ? "PASS" : "FAIL", peak);
        std::printf ("%s  recording saved: %s (%lld bytes)\n", recorded ? "PASS" : "FAIL",
                     file.getFileName().toRawUTF8(), (long long) file.getSize());
        file.deleteFile();
        failures += (played ? 0 : 1) + (recorded ? 0 : 1);
    }

    return failures == 0 ? 0 : 1;
}
