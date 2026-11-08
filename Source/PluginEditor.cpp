#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "dsp/Chords.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
constexpr int liveBarHeight = 76;
constexpr int keyboardHeight = 90;
constexpr int keyboardToolsHeight = 28;
constexpr int chordPadHeight = 82;
constexpr int numWhiteKeys = 36; // C2 to C7
constexpr int presetBarHeight = 48;
constexpr int cheekWidth = 14; // walnut side panels
const auto background = Palette::background;
const auto recordRed = Palette::record;
} // namespace

//==============================================================================
LiveBar::LiveBar (VocoderProcessor& p, juce::AudioDeviceManager& dm, juce::Value& mute)
    : processor (p), deviceManager (dm),
      tempoAttachment (p.params, ParamIDs::tempo, tempoSlider),
      metronomeAttachment (p.params, ParamIDs::metronome, metronomeButton)
{
    muteInput.referTo (mute);
    muteInput.addListener (this);

    for (auto* label : { &inputLabel, &outputLabel, &midiLabel, &tempoLabel })
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

    tempoSlider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 26);
    tempoSlider.setNumDecimalPlacesToDisplay (0);
    tempoSlider.setWantsKeyboardFocus (false);
    addAndMakeVisible (tempoSlider);
    metronomeButton.setTooltip ("Metronome. It's never recorded; Record always counts in one bar");
    metronomeButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (metronomeButton);

    recordTime.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    recordTime.setColour (juce::Label::textColourId, recordRed);
    addAndMakeVisible (recordTime);

    deviceManager.addChangeListener (this);
    refreshAudioDevices();
    refreshMidiDevices();
    valueChanged (muteInput);
    startTimer (50);
}

LiveBar::~LiveBar()
{
    deviceManager.removeChangeListener (this);
    muteInput.removeListener (this);
}

void LiveBar::paint (juce::Graphics& g)
{
    Gear::drawFaceplate (g, getLocalBounds().toFloat().reduced (1.0f), Palette::record, {}, false);
}

void LiveBar::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto labels = area.removeFromTop (18);
    area.removeFromTop (2);
    auto row = area.removeFromTop (30);

    // Right side: tempo, click, mic, record.
    auto right = row.removeFromRight (470);
    auto rightLabels = labels.removeFromRight (470);
    tempoLabel.setBounds (rightLabels.removeFromLeft (100));
    tempoSlider.setBounds (right.removeFromLeft (100));
    right.removeFromLeft (8);
    metronomeButton.setBounds (right.removeFromLeft (70));
    micOnButton.setBounds (right.removeFromLeft (82));
    recordButton.setBounds (right.removeFromLeft (70).reduced (0, 1));
    showFilesButton.setBounds (right.removeFromRight (56).reduced (0, 1));
    recordTime.setBounds (right.reduced (4, 0));

    const int column = row.getWidth() / 3;
    for (auto [label, box] : { std::pair { &inputLabel, &inputBox }, { &outputLabel, &outputBox }, { &midiLabel, &midiBox } })
    {
        label->setBounds (labels.removeFromLeft (column));
        box->setBounds (row.removeFromLeft (column).withTrimmedRight (8));
    }
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
    // MIDI keyboards can be plugged in at any time; check every couple of seconds.
    if (++ticks % 40 == 0 && juce::MidiInput::getAvailableDevices().size() != midiIds.size())
        refreshMidiDevices();

    auto& recorder = processor.recorder;
    if (recorder.finishIfDone())
        recordTime.setText ("Saved", juce::dontSendNotification);

    const auto state = recorder.getState();
    const double position = processor.getClockPosition();
    const double start = processor.getRecordStartPosition();
    juce::String button = "Record", status = recordTime.getText();

    switch (state)
    {
        case Recorder::State::countIn:
            button = "Cancel";
            status = "Count-in " + juce::String (juce::jlimit (1, 8, (int) std::ceil (start - position)));
            break;
        case Recorder::State::recording:
            button = "Stop";
            status = "Bar " + juce::String ((int) std::floor (juce::jmax (0.0, position - start) / 4.0) + 1);
            break;
        case Recorder::State::stopping:
        case Recorder::State::finished:
            button = "Stop";
            status = "Ending bar";
            break;
        case Recorder::State::idle:
            break;
    }

    if (recordButton.getButtonText() != button)
    {
        recordButton.setButtonText (button);
        if (state == Recorder::State::idle)
            recordButton.removeColour (juce::TextButton::buttonColourId);
        else
            recordButton.setColour (juce::TextButton::buttonColourId, recordRed);
    }
    recordButton.setEnabled (state != Recorder::State::stopping && state != Recorder::State::finished);
    recordTime.setText (status, juce::dontSendNotification);
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
    if (recorder.getState() != Recorder::State::idle)
    {
        recorder.requestStop(); // ends at the bar line, or cancels the count-in
        if (recorder.getState() == Recorder::State::idle)
            recordTime.setText ({}, juce::dontSendNotification);
        return;
    }

    const int channels = juce::jmax (1, processor.getTotalNumOutputChannels());
    if (! recorder.arm (processor.getSampleRate(), channels))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't start recording",
                                                "Check that " + Recorder::getFolder().getFullPathName() + " can be written to.");
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
namespace
{
juce::ComboBox& filled (juce::ComboBox& box, juce::AudioProcessorValueTreeState& state, const char* id)
{
    box.addItemList (state.getParameter (id)->getAllValueStrings(), 1);
    return box;
}
} // namespace

ChordPad::ChordPad (VocoderProcessor& p, LatchKeyboard& k)
    : processor (p), keyboard (k),
      keyAttachment (p.params, ParamIDs::chordKey, filled (keyBox, p.params, ParamIDs::chordKey)),
      scaleAttachment (p.params, ParamIDs::chordScale, filled (scaleBox, p.params, ParamIDs::chordScale)),
      flavourAttachment (p.params, ParamIDs::chordFlavour, filled (flavourBox, p.params, ParamIDs::chordFlavour))
{
    VocoderLook::setAccent (*this, Palette::filter);

    for (auto* label : { &keyLabel, &scaleLabel, &flavourLabel })
    {
        label->setFont (juce::FontOptions (11.5f));
        label->setColour (juce::Label::textColourId, Palette::dim);
        addAndMakeVisible (*label);
    }

    keyBox.setTooltip ("The song's key");
    scaleBox.setTooltip ("Major sounds happy, Minor sad, Dorian and Mixolydian funky, Lydian dreamy, Phrygian dark");
    flavourBox.setTooltip ("Simple is three notes. Seventh and Ninth sound jazzy, Sus floaty, Power rocky, Big wide");
    for (auto* box : { &keyBox, &scaleBox, &flavourBox })
    {
        box->setWantsKeyboardFocus (false);
        box->onChange = [this] { settingsChanged(); };
        addAndMakeVisible (*box);
    }

    for (int i = 0; i < numChords; ++i)
    {
        auto& b = chordButtons[i];
        b.setClickingTogglesState (false);
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::filter.withAlpha (0.85f));
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

void ChordPad::paint (juce::Graphics& g)
{
    Gear::drawFaceplate (g, getLocalBounds().toFloat().reduced (1.0f), Palette::filter, {}, false);
}

void ChordPad::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    // Top row: settings on the left, Hold and Stop on the right.
    auto top = area.removeFromTop (24);
    stopButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (6);
    holdButton.setBounds (top.removeFromRight (100));
    for (auto [label, box, width] : { std::tuple { &keyLabel, &keyBox, 70 }, { &scaleLabel, &scaleBox, 140 },
                                      { &flavourLabel, &flavourBox, 110 } })
    {
        label->setBounds (top.removeFromLeft (label == &flavourLabel ? 72 : 40));
        box->setBounds (top.removeFromLeft (width));
        top.removeFromLeft (10);
    }

    // Bottom row: the seven chords of the scale, big enough to hit easily.
    area.removeFromTop (6);
    const int width = area.getWidth() / numChords;
    for (auto& b : chordButtons)
        b.setBounds (area.removeFromLeft (width).reduced (3, 0));
}

juce::Array<int> ChordPad::notesFor (int index) const
{
    auto value = [this] (const char* id) { return (int) processor.params.getRawParameterValue (id)->load(); };
    juce::Array<int> notes;
    for (int n : vox::Chords::build (value (ParamIDs::chordKey), value (ParamIDs::chordScale), index,
                                     value (ParamIDs::chordFlavour)))
        notes.add (n);
    return notes;
}

void ChordPad::relabel()
{
    auto value = [this] (const char* id) { return (int) processor.params.getRawParameterValue (id)->load(); };
    for (int i = 0; i < numChords; ++i)
    {
        chordButtons[i].setButtonText (vox::Chords::name (value (ParamIDs::chordKey), value (ParamIDs::chordScale), i,
                                                          value (ParamIDs::chordFlavour)));
        chordButtons[i].setToggleState (i == activeChord, juce::dontSendNotification);
    }
}

void ChordPad::settingsChanged()
{
    // A chord that's playing switches straight to the new key, scale or type.
    if (activeChord >= 0)
        processor.requestChord (notesFor (activeChord));
    relabel();
}

void ChordPad::toggleChord (int index)
{
    // The processor snaps the change to the next beat or bar (Chord Timing).
    const bool wasActive = activeChord == index;
    activeChord = wasActive ? -1 : index;

    // Chords are played by the robot, so switch it on to hear them.
    auto& s = processor.params;
    if (! wasActive && s.getRawParameterValue (ParamIDs::robotOn)->load() < 0.5f)
        if (auto* robot = s.getParameter (ParamIDs::robotOn))
            robot->setValueNotifyingHost (1.0f);
    processor.requestChord (wasActive ? juce::Array<int>() : notesFor (index));
    relabel();
}

void ChordPad::playIfSilent()
{
    if (activeChord >= 0)
        return;
    for (int note = 0; note < 128; ++note)
        if (processor.keyboardState.isNoteOnForChannels (0xffff, note))
            return;
    toggleChord (0);
}

void ChordPad::stopAll()
{
    activeChord = -1;
    processor.requestChord ({}, true);
    processor.keyboardState.allNotesOff (1);
    relabel();
}

//==============================================================================
PresetBar::PresetBar (VocoderProcessor& p) : processor (p), meter (p)
{
    addAndMakeVisible (meter);
    // Grouped by category, in the order categories first appear.
    const auto& presets = Presets::all();
    juce::StringArray categories;
    for (const auto& preset : presets)
        categories.addIfNotAlreadyThere (preset.category);
    for (const auto& category : categories)
    {
        presetBox.addSectionHeading (category);
        for (size_t i = 0; i < presets.size(); ++i)
            if (category == presets[i].category)
                presetBox.addItem (presets[i].name, (int) i + 1);
    }
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { choose (presetBox.getSelectedId() - 1); };
    prevButton.onClick = [this] { choose (processor.getCurrentProgram() - 1); };
    nextButton.onClick = [this] { choose (processor.getCurrentProgram() + 1); };

    title.setFont (Gear::engraved (11.0f));
    title.setColour (juce::Label::textColourId, Palette::dim);
    title.setJustificationType (juce::Justification::centredRight);
    const char* viewTips[] = { "Build a sound step by step with big buttons", "The main controls as knobs",
                               "Every setting, as a list" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = viewButtons[i];
        b.setClickingTogglesState (false);
        b.setTooltip (viewTips[i]);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::arp);
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i]
        {
            if (onViewChosen)
                onViewChosen (i);
        };
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &title, &presetBox, &prevButton, &nextButton })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
    startTimerHz (4); // follow preset changes made from the DAW
}

void PresetBar::choose (int index)
{
    const int count = processor.getNumPrograms();
    index = (index % count + count) % count; // wrap around at either end
    processor.setCurrentProgram (index);
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
}

void PresetBar::timerCallback()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PresetBar::paint (juce::Graphics& g)
{
    using namespace juce;
    Gear::drawFaceplate (g, getLocalBounds().toFloat().reduced (1.0f), Palette::arp, {}, false);

    // Name badge: a chrome plate with engraved letters, and a stripe of module colours.
    auto badge = Rectangle<float> (12.0f, 8.0f, 128.0f, (float) getHeight() - 16.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (badge.translated (0.0f, 1.5f), 3.0f);
    g.setGradientFill (ColourGradient (Palette::metal.brighter (0.3f), badge.getX(), badge.getY(), Palette::metal.darker (0.7f),
                                       badge.getX(), badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (badge, 3.0f, 0.8f);
    const Colour stripes[] = { Palette::tune, Palette::arp, Palette::fx, Palette::voice, Palette::filter };
    const float stripeWidth = 4.0f;
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (badge.getRight() - 8.0f - stripeWidth * (float) (5 - i), badge.getY() + 3.0f, stripeWidth - 1.0f, badge.getHeight() - 6.0f);
    }
    g.setColour (Colour (0xff1d1915));
    g.setFont (Gear::engraved (15.0f).withExtraKerningFactor (0.12f));
    g.drawText ("VOCODER", badge.withTrimmedLeft (9.0f).withTrimmedRight (stripeWidth * 5.0f + 10.0f), Justification::centredLeft);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced (12, 9);
    for (int i = 2; i >= 0; --i)
        viewButtons[i].setBounds (area.removeFromRight (i == 2 ? 88 : 68));
    area.removeFromLeft (136);
    meter.setBounds (area.removeFromLeft (64).withSizeKeepingCentre (60, 32));
    area.removeFromRight (8);

    auto centre = area.withSizeKeepingCentre (juce::jmin (400, area.getWidth()), area.getHeight());
    title.setBounds (centre.removeFromLeft (50));
    centre.removeFromLeft (6);
    prevButton.setBounds (centre.removeFromLeft (30));
    centre.removeFromLeft (4);
    nextButton.setBounds (centre.removeFromRight (30));
    centre.removeFromRight (4);
    presetBox.setBounds (centre);
}

//==============================================================================
VocoderEditor::VocoderEditor (VocoderProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      presetBar (p),
      easyPanel (p),
      mainPanel (p),
      controls (p),
      listenBar (p),
      keyboard (p.keyboardState),
      chordPad (p, keyboard)
{
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            liveBar = std::make_unique<LiveBar> (p, holder->deviceManager, holder->getMuteInputValue());
            addAndMakeVisible (*liveBar);
        }

    setLookAndFeel (&look);

    addAndMakeVisible (presetBar);
    addChildComponent (easyPanel);
    addChildComponent (mainPanel);
    addChildComponent (controls);
    addAndMakeVisible (listenBar);
    robotWasOn = p.params.getRawParameterValue (ParamIDs::robotOn)->load() > 0.5f;
    soundsSeen = p.getSoundChanges();
    listenBar.onBackToRaw = [this] { chordPad.stopAll(); };
    robotWatch.startTimer (100);
    presetBar.onViewChosen = [this] (int view) { showView (view); };
    showView ((int) p.params.state.getProperty ("view", 0)); // new instances start in Easy
    // Old ivory keys.
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff0e4c8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff17130f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff8f7f66));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colour (0x66000000));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff5a4e3e));

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
    setResizeLimits (760, 600, 1600, 1400);
    setSize (900, liveBar != nullptr ? 862 : 782);
}

VocoderEditor::~VocoderEditor()
{
    setLookAndFeel (nullptr);
}

void VocoderEditor::showView (int view)
{
    view = juce::jlimit (0, 2, view);
    easyPanel.setVisible (view == 0);
    mainPanel.setVisible (view == 1);
    controls.setVisible (view == 2);
    for (int i = 0; i < 3; ++i)
        presetBar.viewButtons[i].setToggleState (i == view, juce::dontSendNotification);
    processor.params.state.setProperty ("view", view, nullptr); // remembered with the song
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
    // Walnut cheeks either side, like the end panels on an old synth.
    Gear::drawWood (g, getLocalBounds().removeFromLeft (cheekWidth).toFloat());
    Gear::drawWood (g, getLocalBounds().removeFromRight (cheekWidth).toFloat());
}

// However the robot gets switched on or picked (the bar, a tile, a preset at the top),
// give it notes so it's heard. If you stop the chords, your voice fills in instead.
void VocoderEditor::checkRobot()
{
    auto& s = processor.params;
    const bool robot = s.getRawParameterValue (ParamIDs::robotOn)->load() > 0.5f;
    const bool singing = s.getRawParameterValue (ParamIDs::singToPlay)->load() > 0.5f;
    const bool picked = processor.getSoundChanges() != soundsSeen;
    soundsSeen = processor.getSoundChanges();
    if (robot && (! robotWasOn || picked) && ! singing)
        chordPad.playIfSilent();
    robotWasOn = robot;
}

void VocoderEditor::resized()
{
    auto area = getLocalBounds().reduced (cheekWidth, 0);
    if (liveBar != nullptr)
        liveBar->setBounds (area.removeFromTop (liveBarHeight));
    presetBar.setBounds (area.removeFromTop (presetBarHeight));

    keyboard.setBounds (area.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / numWhiteKeys);
    chordPad.setBounds (area.removeFromBottom (chordPadHeight));
    listenBar.setBounds (area.removeFromBottom (48));
    auto tools = area.removeFromBottom (keyboardToolsHeight).reduced (8, 3);
    octaveDown.setBounds (tools.removeFromLeft (52));
    tools.removeFromLeft (4);
    octaveUp.setBounds (tools.removeFromLeft (52));
    tools.removeFromLeft (10);
    keyHint.setBounds (tools);

    easyPanel.setBounds (area);
    mainPanel.setBounds (area);
    controls.setBounds (area);
}
