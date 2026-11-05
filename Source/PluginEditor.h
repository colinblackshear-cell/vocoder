#pragma once

#include "Look.h"
#include "EasyPanel.h"
#include "MainPanel.h"
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
    juce::TextButton recordButton { "Record" }, showFilesButton { "Files" };
    juce::Label recordTime;

    juce::Label tempoLabel { {}, "Tempo" };
    juce::Slider tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ToggleButton metronomeButton { "Click" };
    juce::AudioProcessorValueTreeState::SliderAttachment tempoAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment metronomeAttachment;

    juce::StringArray midiIds;
    int ticks = 0;
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

// One-click chords in a chosen key, scale and chord type, for playing
// without a MIDI keyboard.
class ChordPad : public juce::Component
{
public:
    ChordPad (VocoderProcessor& processor, LatchKeyboard& keyboard);
    void resized() override;
    void paint (juce::Graphics&) override;
    void stopAll();
    // Plays the key's own chord if nothing is playing, so a robot switched on is heard straight away.
    void playIfSilent();

private:
    static constexpr int numChords = 7;

    void relabel();
    void toggleChord (int index);
    void settingsChanged();
    juce::Array<int> notesFor (int index) const;

    VocoderProcessor& processor;
    LatchKeyboard& keyboard;
    juce::Label keyLabel { {}, "Key" }, scaleLabel { {}, "Scale" }, flavourLabel { {}, "Chord type" };
    juce::ComboBox keyBox, scaleBox, flavourBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment, scaleAttachment, flavourAttachment;
    juce::TextButton chordButtons[numChords];
    juce::ToggleButton holdButton { "Hold keys" };
    juce::TextButton stopButton { "Stop all" };
    int activeChord = -1;
};

// Factory preset menu with previous/next buttons.
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (VocoderProcessor& processor);
    void resized() override;
    void paint (juce::Graphics&) override;

    // Easy, Studio, Advanced.
    juce::TextButton viewButtons[3] { juce::TextButton ("Easy"), juce::TextButton ("Studio"), juce::TextButton ("Advanced") };
    std::function<void (int)> onViewChosen;

private:
    void timerCallback() override;
    void choose (int index);

    VocoderProcessor& processor;
    juce::Label title { {}, "Sound" };
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
};

class VocoderEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocoderEditor (VocoderProcessor&);
    ~VocoderEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyStateChanged (bool isKeyDown) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void setOctave (int octave);
    void parentHierarchyChanged() override;
    void focusForComputerKeys (int attemptsLeft);

    void showView (int view); // 0 Easy, 1 Studio, 2 Advanced

    VocoderLook look;
    juce::TooltipWindow tooltips { this, 600 };
    VocoderProcessor& processor;
    std::unique_ptr<LiveBar> liveBar;
    PresetBar presetBar;
    EasyPanel easyPanel;
    MainPanel mainPanel;
    juce::GenericAudioProcessorEditor controls;
    ListenBar listenBar;
    LatchKeyboard keyboard;
    ChordPad chordPad;
    void checkRobot();
    juce::TimedCallback robotWatch { [this] { checkRobot(); } };
    bool robotWasOn = false;
    juce::TextButton octaveDown { "Oct -" }, octaveUp { "Oct +" };
    juce::Label keyHint;
    int keyOctave = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderEditor)
};
