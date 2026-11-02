#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Records the plugin's output to a 24-bit WAV. The message thread opens and
// closes files; the audio thread decides the exact sample a take starts and
// stops on (the bar lines) and copies samples into a FIFO that a background
// thread writes to disk.
class Recorder
{
public:
    enum class State
    {
        idle,
        countIn,   // armed, waiting for the first bar
        recording,
        stopping,  // finishing the current bar
        finished   // audio thread is done; message thread closes the file
    };

    Recorder() { thread.startThread(); }
    ~Recorder() { closeFile(); }

    static juce::File getFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Vocoder Recordings");
    }

    //==============================================================================
    // Message thread

    // Opens a new take; recording begins when the audio thread reaches the start bar.
    bool arm (double sampleRate, int numChannels)
    {
        if (state != State::idle || sampleRate <= 0.0 || numChannels <= 0 || ! getFolder().createDirectory())
            return false;

        const auto name = "Vocoder " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        auto file = getFolder().getNonexistentChildFile (name, ".wav");

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
        state = State::countIn;
        return true;
    }

    // Stops at the end of the current bar, or cancels a take still counting in.
    void requestStop()
    {
        for (int attempt = 0; attempt < 2; ++attempt) // the take may start between the two checks
        {
            auto expected = State::recording;
            if (state.compare_exchange_strong (expected, State::stopping))
                return;

            expected = State::countIn;
            if (state.compare_exchange_strong (expected, State::idle))
            {
                closeFile();
                lastFile.deleteFile();
                return;
            }
        }
    }

    // Call regularly; returns true once when a take has just been saved.
    bool finishIfDone()
    {
        if (state != State::finished)
            return false;
        closeFile();
        state = State::idle;
        return true;
    }

    State getState() const { return state; }
    juce::File getLastFile() const { return lastFile; }

    //==============================================================================
    // Audio thread

    void beginTake()
    {
        const juce::ScopedLock sl (lock);
        auto expected = State::countIn;
        if (state.compare_exchange_strong (expected, State::recording)) // unless it was just cancelled
            active = threaded.get();
    }

    void endTake()
    {
        const juce::ScopedLock sl (lock);
        active = nullptr;
        state = State::finished;
    }

    // Writes samples [start, start + num) of the given channels.
    void write (const float* const* channels, int numChannels, int start, int num)
    {
        if (num <= 0)
            return;

        const float* offset[8] {};
        numChannels = juce::jmin (numChannels, 8);
        for (int ch = 0; ch < numChannels; ++ch)
            offset[ch] = channels[ch] + start;

        const juce::ScopedLock sl (lock);
        if (active != nullptr)
            active->write (offset, num);
    }

private:
    void closeFile()
    {
        {
            const juce::ScopedLock sl (lock);
            active = nullptr;
        }
        threaded.reset();
    }

    juce::TimeSliceThread thread { "Vocoder recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::CriticalSection lock;
    juce::AudioFormatWriter::ThreadedWriter* active = nullptr;
    std::atomic<State> state { State::idle };
    juce::File lastFile;
};
