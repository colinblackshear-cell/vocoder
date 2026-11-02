#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

// Device pickers, mic monitoring and the Record button. Only shown in the
// standalone app, where there's no DAW to do these jobs.
class LiveBar : public juce::Component,
                private juce::ChangeListener,
                private juce::Value::Listener,
                private juce::Timer
{
public:
    LiveBar (VocoderProcessor& processor, juce::AudioDeviceManager& deviceManager, juce::Value& muteInput);
    ~LiveBar() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void valueChanged (juce::Value&) override;
    void timerCallback() override;

    void refreshAudioDevices();
    void refreshMidiDevices();
    void chooseAudioDevice (bool isInput);
    void chooseMidiDevice();
    void toggleRecording();

    VocoderProcessor& processor;
    juce::AudioDeviceManager& deviceManager;
    juce::Value muteInput;

    juce::Label inputLabel { {}, "Mic" }, outputLabel { {}, "Output" }, midiLabel { {}, "Keyboard" };
    juce::ComboBox inputBox, outputBox, midiBox;
    juce::ToggleButton micOnButton { "Mic on" };
    juce::TextButton recordButton { "Record" }, showFilesButton { "Show recordings" };
    juce::Label recordTime;

    juce::StringArray midiIds;
    juce::int64 recordStartMs = 0;
};

// On-screen keyboard that can latch: with Hold on, a click turns a note on
// and the next click turns it off, so you can sing over it hands-free.
class LatchKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit LatchKeyboard (juce::MidiKeyboardState& s)
        : MidiKeyboardComponent (s, horizontalKeyboard), keyState (s) {}

    bool latch = false;

protected:
    bool mouseDownOnKey (int note, const juce::MouseEvent& e) override;
    bool mouseDraggedToKey (int note, const juce::MouseEvent& e) override;

private:
    juce::MidiKeyboardState& keyState;
};

// One-click chords in a chosen key, for playing without a MIDI keyboard.
class ChordPad : public juce::Component
{
public:
    explicit ChordPad (juce::MidiKeyboardState& state, LatchKeyboard& keyboard);
    void resized() override;
    void stopAll();

private:
    static constexpr int numChords = 6;

    void relabel();
    void toggleChord (int index);
    juce::Array<int> notesFor (int index) const;

    juce::MidiKeyboardState& state;
    LatchKeyboard& keyboard;
    juce::ComboBox keyBox, scaleBox;
    juce::TextButton chordButtons[numChords];
    juce::ToggleButton holdButton { "Hold keys" };
    juce::TextButton stopButton { "Stop all" };
    int activeChord = -1;
    juce::Array<int> sounding;
};

class VocoderEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocoderEditor (VocoderProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    void setOctave (int octave);
    void parentHierarchyChanged() override;

    VocoderProcessor& processor;
    std::unique_ptr<LiveBar> liveBar;
    juce::GenericAudioProcessorEditor controls;
    LatchKeyboard keyboard;
    ChordPad chordPad;
    juce::TextButton octaveDown { "Oct -" }, octaveUp { "Oct +" };
    juce::Label keyHint;
    int keyOctave = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderEditor)
};
