#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Records the plugin's output to a 24-bit WAV. The audio thread only copies
// samples into a FIFO; a background thread writes them to disk.
class Recorder
{
public:
    Recorder() { thread.startThread(); }
    ~Recorder() { stop(); }

    static juce::File getFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vocoder Recordings");
    }

    // Starts a new take in the recordings folder. Call from the message thread.
    bool start (double sampleRate, int numChannels)
    {
        stop();
        if (sampleRate <= 0.0 || numChannels <= 0 || ! getFolder().createDirectory())
            return false;

        const auto name = "Vocoder " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav";
        auto file = getFolder().getNonexistentChildFile (name.upToLastOccurrenceOf (".", false, false), ".wav");

        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                                          .withSampleRate (sampleRate)
                                                                          .withNumChannels (numChannels)
                                                                          .withBitsPerSample (24));
        if (writer == nullptr)
            return false;

        threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), thread, 1 << 16);
        lastFile = file;

        const juce::ScopedLock sl (lock);
        active = threaded.get();
        return true;
    }

    // Stops and finishes the file. Call from the message thread.
    void stop()
    {
        {
            const juce::ScopedLock sl (lock);
            active = nullptr;
        }
        threaded.reset();
    }

    bool isRecording() const { return active != nullptr; }
    juce::File getLastFile() const { return lastFile; }

    // Called from the audio thread with the finished output.
    void write (const float* const* channels, int numSamples)
    {
        const juce::ScopedLock sl (lock);
        if (auto* writer = active.load())
            writer->write (channels, numSamples);
    }

private:
    juce::TimeSliceThread thread { "Vocoder recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::CriticalSection lock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> active { nullptr };
    juce::File lastFile;
};
