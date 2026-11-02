#include "PluginEditor.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
constexpr int liveBarHeight = 76;
constexpr int keyboardHeight = 90;
constexpr int keyboardToolsHeight = 28;
constexpr int chordPadHeight = 36;
constexpr int numWhiteKeys = 36; // C2 to C7
const auto background = juce::Colour (0xff1d1f24);
const auto recordRed = juce::Colour (0xffd23c3c);
} // namespace

//==============================================================================
LiveBar::LiveBar (VocoderProcessor& p, juce::AudioDeviceManager& dm, juce::Value& mute)
    : processor (p), deviceManager (dm)
{
    muteInput.referTo (mute);
    muteInput.addListener (this);

    for (auto* label : { &inputLabel, &outputLabel, &midiLabel })
    {
        label->setFont (juce::FontOptions (12.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addAndMakeVisible (*label);
    }

    for (auto* box : { &inputBox, &outputBox, &midiBox })
        addAndMakeVisible (*box);

    inputBox.onChange = [this] { chooseAudioDevice (true); };
    outputBox.onChange = [this] { chooseAudioDevice (false); };
    midiBox.onChange = [this] { chooseMidiDevice(); };

    micOnButton.setTooltip ("Use headphones when the mic is on, or the speakers will feed back");
    micOnButton.onClick = [this] { muteInput.setValue (! micOnButton.getToggleState()); };
    addAndMakeVisible (micOnButton);

    recordButton.setClickingTogglesState (false);
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible (recordButton);

    showFilesButton.onClick = [this]
    {
        auto last = processor.recorder.getLastFile();
        if (last.existsAsFile())
            last.revealToUser();
        else if (Recorder::getFolder().createDirectory())
            Recorder::getFolder().startAsProcess();
    };
    addAndMakeVisible (showFilesButton);

    recordTime.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    recordTime.setColour (juce::Label::textColourId, recordRed);
    addAndMakeVisible (recordTime);

    deviceManager.addChangeListener (this);
    refreshAudioDevices();
    refreshMidiDevices();
    valueChanged (muteInput);
    startTimer (500);
}

LiveBar::~LiveBar()
{
    deviceManager.removeChangeListener (this);
    muteInput.removeListener (this);
}

void LiveBar::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (getLocalBounds());
}

void LiveBar::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto labels = area.removeFromTop (18);
    area.removeFromTop (2);
    auto row = area.removeFromTop (30);

    auto right = row.removeFromRight (300);
    labels.removeFromRight (300);

    const int column = row.getWidth() / 3;
    for (auto [label, box] : { std::pair { &inputLabel, &inputBox }, { &outputLabel, &outputBox }, { &midiLabel, &midiBox } })
    {
        label->setBounds (labels.removeFromLeft (column));
        box->setBounds (row.removeFromLeft (column).reduced (0, 0).withTrimmedRight (8));
    }

    micOnButton.setBounds (right.removeFromLeft (76));
    recordButton.setBounds (right.removeFromLeft (80).reduced (0, 1));
    recordTime.setBounds (right.removeFromLeft (54));
    showFilesButton.setBounds (right.reduced (0, 1));
}

void LiveBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAudioDevices();
}

void LiveBar::valueChanged (juce::Value&)
{
    micOnButton.setToggleState (! (bool) muteInput.getValue(), juce::dontSendNotification);
}

void LiveBar::timerCallback()
{
    // MIDI keyboards can be plugged in at any time.
    if (juce::MidiInput::getAvailableDevices().size() != midiIds.size())
        refreshMidiDevices();

    if (processor.recorder.isRecording())
    {
        const auto seconds = (int) ((juce::Time::currentTimeMillis() - recordStartMs) / 1000);
        recordTime.setText (juce::String::formatted ("%d:%02d", seconds / 60, seconds % 60), juce::dontSendNotification);
    }
}

void LiveBar::refreshAudioDevices()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr)
        return;

    const auto setup = deviceManager.getAudioDeviceSetup();
    auto fill = [] (juce::ComboBox& box, const juce::StringArray& names, const juce::String& current)
    {
        box.clear (juce::dontSendNotification);
        box.addItemList (names, 1);
        box.setSelectedItemIndex (names.indexOf (current), juce::dontSendNotification);
    };

    fill (inputBox, type->getDeviceNames (true), setup.inputDeviceName);
    fill (outputBox, type->getDeviceNames (false), setup.outputDeviceName);
}

void LiveBar::chooseAudioDevice (bool isInput)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    auto& box = isInput ? inputBox : outputBox;
    if (box.getSelectedItemIndex() < 0)
        return;

    if (isInput)
    {
        setup.inputDeviceName = box.getText();
        setup.useDefaultInputChannels = true;
    }
    else
    {
        setup.outputDeviceName = box.getText();
        setup.useDefaultOutputChannels = true;
    }

    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open that device", error);
}

void LiveBar::refreshMidiDevices()
{
    const auto devices = juce::MidiInput::getAvailableDevices();
    midiIds.clear();
    midiBox.clear (juce::dontSendNotification);
    midiBox.addItem ("All keyboards", 1);

    int enabledCount = 0, enabledIndex = -1;
    for (int i = 0; i < devices.size(); ++i)
    {
        midiIds.add (devices[i].identifier);
        midiBox.addItem (devices[i].name, i + 2);
        if (deviceManager.isMidiInputDeviceEnabled (devices[i].identifier))
        {
            ++enabledCount;
            enabledIndex = i;
        }
    }

    // Nothing enabled yet (first launch, or a new keyboard): listen to everything.
    if (enabledCount == 0)
    {
        for (auto& id : midiIds)
            deviceManager.setMidiInputDeviceEnabled (id, true);
        enabledCount = midiIds.size();
    }

    midiBox.setSelectedId (enabledCount == 1 && midiIds.size() > 1 ? enabledIndex + 2 : 1, juce::dontSendNotification);
}

void LiveBar::chooseMidiDevice()
{
    const int chosen = midiBox.getSelectedId() - 2; // -1 means all
    for (int i = 0; i < midiIds.size(); ++i)
        deviceManager.setMidiInputDeviceEnabled (midiIds[i], chosen < 0 || chosen == i);
}

void LiveBar::toggleRecording()
{
    auto& recorder = processor.recorder;
    if (recorder.isRecording())
    {
        recorder.stop();
        recordButton.setButtonText ("Record");
        recordButton.removeColour (juce::TextButton::buttonColourId);
        recordTime.setText ("saved", juce::dontSendNotification);
        return;
    }

    const int channels = juce::jmax (1, processor.getTotalNumOutputChannels());
    if (recorder.start (processor.getSampleRate(), channels))
    {
        recordStartMs = juce::Time::currentTimeMillis();
        recordButton.setButtonText ("Stop");
        recordButton.setColour (juce::TextButton::buttonColourId, recordRed);
        recordTime.setText ("0:00", juce::dontSendNotification);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't start recording",
                                                "Check that " + Recorder::getFolder().getFullPathName() + " can be written to.");
    }
}

//==============================================================================
bool LatchKeyboard::mouseDownOnKey (int note, const juce::MouseEvent& e)
{
    if (! latch)
        return MidiKeyboardComponent::mouseDownOnKey (note, e);

    if (keyState.isNoteOn (getMidiChannel(), note))
        keyState.noteOff (getMidiChannel(), note, 0.0f);
    else
        keyState.noteOn (getMidiChannel(), note, 0.8f);
    return false;
}

bool LatchKeyboard::mouseDraggedToKey (int note, const juce::MouseEvent& e)
{
    return latch ? false : MidiKeyboardComponent::mouseDraggedToKey (note, e);
}

//==============================================================================
ChordPad::ChordPad (juce::MidiKeyboardState& s, LatchKeyboard& k) : state (s), keyboard (k)
{
    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keyBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.addItemList ({ "Major", "Minor" }, 1);
    scaleBox.setSelectedId (2, juce::dontSendNotification);

    for (auto* box : { &keyBox, &scaleBox })
    {
        box->setWantsKeyboardFocus (false);
        box->onChange = [this]
        {
            stopAll();
            relabel();
        };
        addAndMakeVisible (*box);
    }

    for (int i = 0; i < numChords; ++i)
    {
        auto& b = chordButtons[i];
        b.setClickingTogglesState (false);
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3d8bd8));
        b.onClick = [this, i] { toggleChord (i); };
        addAndMakeVisible (b);
    }

    holdButton.setWantsKeyboardFocus (false);
    holdButton.setTooltip ("Clicked keys keep playing until you click them again");
    holdButton.onClick = [this] { keyboard.latch = holdButton.getToggleState(); };
    addAndMakeVisible (holdButton);

    stopButton.setWantsKeyboardFocus (false);
    stopButton.onClick = [this] { stopAll(); };
    addAndMakeVisible (stopButton);

    relabel();
}

void ChordPad::resized()
{
    auto area = getLocalBounds().reduced (8, 4);
    keyBox.setBounds (area.removeFromLeft (60));
    area.removeFromLeft (4);
    scaleBox.setBounds (area.removeFromLeft (80));
    area.removeFromLeft (10);

    stopButton.setBounds (area.removeFromRight (72));
    area.removeFromRight (4);
    holdButton.setBounds (area.removeFromRight (92));
    area.removeFromRight (6);

    const int width = area.getWidth() / numChords;
    for (auto& b : chordButtons)
        b.setBounds (area.removeFromLeft (width).reduced (2, 0));
}

juce::Array<int> ChordPad::notesFor (int index) const
{
    // Diatonic triads: degree offsets from the key, and whether each is minor.
    static constexpr int majorRoots[] = { 0, 2, 4, 5, 7, 9 };
    static constexpr bool majorMinor[] = { false, true, true, false, false, true };
    static constexpr int minorRoots[] = { 0, 3, 5, 7, 8, 10 };
    static constexpr bool minorMinor[] = { true, false, true, true, false, false };

    const bool isMajor = scaleBox.getSelectedId() == 1;
    const int key = keyBox.getSelectedId() - 1;
    const int root = 48 + (key + (isMajor ? majorRoots[index] : minorRoots[index])) % 12;
    const bool minor = isMajor ? majorMinor[index] : minorMinor[index];

    // Root, third, fifth and the octave: a full voicing for the vocoder.
    return { root, root + (minor ? 3 : 4), root + 7, root + 12 };
}

void ChordPad::relabel()
{
    static const juce::StringArray names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < numChords; ++i)
    {
        const auto notes = notesFor (i);
        const bool minor = notes[1] - notes[0] == 3;
        chordButtons[i].setButtonText (names[notes[0] % 12] + (minor ? "m" : ""));
        chordButtons[i].setToggleState (i == activeChord, juce::dontSendNotification);
    }
}

void ChordPad::toggleChord (int index)
{
    const bool wasActive = activeChord == index;
    for (int note : sounding)
        state.noteOff (1, note, 0.0f);
    sounding.clear();
    activeChord = -1;

    if (! wasActive)
    {
        sounding = notesFor (index);
        for (int note : sounding)
            state.noteOn (1, note, 0.8f);
        activeChord = index;
    }
    relabel();
}

void ChordPad::stopAll()
{
    sounding.clear();
    activeChord = -1;
    state.allNotesOff (1);
    relabel();
}

//==============================================================================
VocoderEditor::VocoderEditor (VocoderProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      controls (p),
      keyboard (p.keyboardState),
      chordPad (p.keyboardState, keyboard)
{
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            liveBar = std::make_unique<LiveBar> (p, holder->deviceManager, holder->getMuteInputValue());
            addAndMakeVisible (*liveBar);
        }

    addAndMakeVisible (controls);

    keyboard.setAvailableRange (36, 96);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    octaveDown.onClick = [this] { setOctave (keyOctave - 1); };
    octaveUp.onClick = [this] { setOctave (keyOctave + 1); };
    for (auto* b : { &octaveDown, &octaveUp })
        b->setWantsKeyboardFocus (false);

    addAndMakeVisible (chordPad);

    // Computer-key playing is for the standalone app; in a DAW the keys belong to the DAW.
    const bool standalone = liveBar != nullptr;
    keyHint.setFont (juce::FontOptions (12.0f));
    keyHint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    for (auto* c : std::initializer_list<juce::Component*> { &keyHint, &octaveDown, &octaveUp })
        addChildComponent (c);
    keyHint.setVisible (standalone);
    octaveDown.setVisible (standalone);
    octaveUp.setVisible (standalone);
    setOctave (keyOctave);

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (640, 420, 1600, 1400);
    setSize (820, liveBar != nullptr ? 760 : 680);
}

void VocoderEditor::setOctave (int octave)
{
    keyOctave = juce::jlimit (2, 8, octave);
    keyboard.setKeyPressBaseOctave (keyOctave);
    keyHint.setText ("Computer keys A to K play from " + juce::MidiMessage::getMidiNoteName (keyOctave * 12, true, true, 3)
                         + " (W, E, T, Y, U are the black keys)",
                     juce::dontSendNotification);
}

void VocoderEditor::parentHierarchyChanged()
{
    if (liveBar != nullptr)
        focusForComputerKeys (10);
}

void VocoderEditor::focusForComputerKeys (int attemptsLeft)
{
    // The window isn't on screen yet when the editor is created, so keep
    // trying briefly until it is and the computer keys reach the keyboard.
    if (isShowing())
        grabKeyboardFocus();

    if (! hasKeyboardFocus (true) && attemptsLeft > 0)
        juce::Timer::callAfterDelay (200, [safe = juce::Component::SafePointer<VocoderEditor> (this), attemptsLeft]
        {
            if (safe != nullptr)
                safe->focusForComputerKeys (attemptsLeft - 1);
        });
}

bool VocoderEditor::keyStateChanged (bool isKeyDown)
{
    // Keys typed anywhere in the window play the on-screen keyboard.
    return liveBar != nullptr && keyboard.keyStateChanged (isKeyDown);
}

bool VocoderEditor::keyPressed (const juce::KeyPress& key)
{
    // Claim the note keys. Unclaimed keys make macOS play its alert sound,
    // which repeats for as long as a key is held.
    const auto mods = key.getModifiers();
    if (liveBar == nullptr || mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false;

    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    return c != 0 && juce::String ("awsedftgyhujkolp;").containsChar (c);
}

void VocoderEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
}

void VocoderEditor::resized()
{
    auto area = getLocalBounds();
    if (liveBar != nullptr)
        liveBar->setBounds (area.removeFromTop (liveBarHeight));

    keyboard.setBounds (area.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / numWhiteKeys);
    chordPad.setBounds (area.removeFromBottom (chordPadHeight));
    auto tools = area.removeFromBottom (keyboardToolsHeight).reduced (8, 3);
    octaveDown.setBounds (tools.removeFromLeft (52));
    tools.removeFromLeft (4);
    octaveUp.setBounds (tools.removeFromLeft (52));
    tools.removeFromLeft (10);
    keyHint.setBounds (tools);

    controls.setBounds (area);
}
