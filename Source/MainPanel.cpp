#include "MainPanel.h"
#include "Parameters.h"

namespace
{
constexpr int captionHeight = 15;
constexpr int optionHeight = 44;
constexpr int switchHeight = 28;

void styleCaption (juce::Label& label, const juce::String& text)
{
    // Engraved, in capitals, like the lettering on old gear.
    label.setText (text.toUpperCase(), juce::dontSendNotification);
    label.setFont (Gear::engraved (10.5f));
    label.setColour (juce::Label::textColourId, Palette::dim);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip)
    : attachment (state, id, slider)
{
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    // Short, readable values: "1.2k Hz", "15 ms", "0.45".
    auto* param = state.getParameter (id);
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (param))
    {
        const auto unit = floatParam->getLabel();
        slider.textFromValueFunction = [unit] (double v)
        {
            const double a = std::abs (v);
            juce::String text = a >= 1000.0 ? juce::String (v / 1000.0, a >= 10000.0 ? 1 : 2) + "k"
                              : a >= 100.0  ? juce::String (juce::roundToInt (v))
                              : a >= 10.0   ? juce::String (v, 1)
                                            : juce::String (v, 2);
            return unit.isEmpty() ? text : text + " " + unit;
        };
        slider.updateText();
    }
    slider.setTooltip (tip);
    slider.setWantsKeyboardFocus (false);
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (id)->convertFrom0to1 (
                                                state.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);

    styleCaption (label, name);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Palette::text);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (captionHeight + 2));
    slider.setBounds (area);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip)
    : attachment ((box.addItemList (state.getParameter (id)->getAllValueStrings(), 1), state), id, box)
{
    box.setTooltip (tip);
    box.setWantsKeyboardFocus (false);
    addAndMakeVisible (box);
    styleCaption (label, name);
    addAndMakeVisible (label);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (captionHeight));
    box.setBounds (area.removeFromTop (juce::jmin (26, area.getHeight())).reduced (0, 1));
}

//==============================================================================
Switch::Switch (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip)
    : attachment (state, id, button)
{
    button.setButtonText (name);
    button.setTooltip (tip);
    button.setWantsKeyboardFocus (false);
    addAndMakeVisible (button);
}

void Switch::resized()
{
    button.setBounds (getLocalBounds());
}

//==============================================================================
TunerDisplay::TunerDisplay (VocoderProcessor& p) : processor (p)
{
    setTooltip ("The note you're singing. The bar shows how far off it is before auto-tune corrects it.");
    startTimerHz (30);
}

void TunerDisplay::timerCallback()
{
    const float newPitch = processor.getSungPitch();
    const int newTarget = processor.getTargetNote();
    if (newPitch >= 0.0f && newTarget >= 0)
        shown += 0.4f * ((newPitch - (float) newTarget) - shown);
    if (std::abs (newPitch - pitch) > 0.01f || newTarget != target)
    {
        pitch = newPitch;
        target = newTarget;
        repaint();
    }
}

void TunerDisplay::paint (juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::background.withAlpha (0.6f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const bool singing = pitch >= 0.0f && target >= 0;
    auto noteArea = bounds.removeFromLeft (52.0f);
    g.setColour (singing ? Palette::tune : Palette::dim);
    g.setFont (FontOptions (18.0f, Font::bold));
    g.drawText (singing ? MidiMessage::getMidiNoteName (target, true, true, 3) : String ("--"), noteArea,
                Justification::centred);

    // Cents meter: centre line is in tune, the dot is where your voice is.
    auto meter = bounds.reduced (8.0f, bounds.getHeight() * 0.5f - 2.0f);
    g.setColour (Palette::control.brighter (0.15f));
    g.fillRoundedRectangle (meter, 2.0f);
    g.setColour (Palette::dim);
    g.fillRect (Rectangle<float> (2.0f, 12.0f).withCentre (meter.getCentre()));
    if (singing)
    {
        const float offset = jlimit (-0.5f, 0.5f, shown);
        const float x = meter.getCentreX() + offset * meter.getWidth();
        g.setColour (std::abs (offset) < 0.1f ? Palette::fx : Palette::tune);
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre ({ x, meter.getCentreY() }));
    }
}

//==============================================================================
Section::Section (const juce::String& t, juce::Colour a) : title (t), accent (a)
{
    VocoderLook::setAccent (*this, accent);
}

void Section::addOption (std::unique_ptr<juce::Component> c)
{
    addAndMakeVisible (*c);
    options.push_back (std::move (c));
}

void Section::addKnob (std::unique_ptr<juce::Component> c)
{
    addAndMakeVisible (*c);
    knobs.push_back (std::move (c));
}

void Section::paint (juce::Graphics& g)
{
    // Each section is its own piece of gear.
    Gear::drawFaceplate (g, getLocalBounds().toFloat().reduced (3.0f), accent, title);
}

void Section::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    area.removeFromTop (22);

    if (extra != nullptr)
    {
        extra->setBounds (area.removeFromBottom (30));
        area.removeFromBottom (6);
    }

    if (optionsOnTop && ! options.empty())
    {
        // A row of choices across the top, knobs underneath.
        auto strip = area.removeFromTop (optionHeight);
        area.removeFromTop (4);
        const int width = strip.getWidth() / (int) options.size();
        for (auto& o : options)
            o->setBounds (strip.removeFromLeft (width).withTrimmedRight (6));
    }
    else if (! options.empty())
    {
        const int columnWidth = knobs.empty() ? area.getWidth() : juce::jmin (118, area.getWidth() / 2);
        auto column = area.removeFromLeft (columnWidth);
        area.removeFromLeft (8);
        for (auto& o : options)
        {
            const bool isSwitch = dynamic_cast<Switch*> (o.get()) != nullptr;
            o->setBounds (column.removeFromTop (isSwitch ? switchHeight : optionHeight));
            column.removeFromTop (2);
        }
    }

    if (knobs.empty())
        return;

    // Knobs share one size everywhere, centred in the space left.
    const int count = (int) knobs.size();
    const int cellWidth = juce::jmin (72, area.getWidth() / count);
    const int cellHeight = juce::jmin (area.getHeight(), cellWidth + 34);
    auto row = area.withSizeKeepingCentre (cellWidth * count, cellHeight);
    for (auto& k : knobs)
        k->setBounds (row.removeFromLeft (cellWidth).reduced (2, 0));
}

//==============================================================================
MainPanel::MainPanel (VocoderProcessor& p)
{
    auto& s = p.params;
    using namespace ParamIDs;
    auto knob = [&s] (const char* id, const char* name, const char* tip) { return std::make_unique<Knob> (s, id, name, tip); };
    auto choice = [&s] (const char* id, const char* name, const char* tip) { return std::make_unique<Choice> (s, id, name, tip); };
    auto toggle = [&s] (const char* id, const char* name, const char* tip) { return std::make_unique<Switch> (s, id, name, tip); };

    voice.addKnob (knob (formant, "Character", "Moves your voice's character up (smaller, chipmunk) or down (bigger, monster)"));
    voice.addKnob (knob (bands, "Bands", "Fewer bands sound more robotic, more sound clearer"));
    voice.addKnob (knob (sibilance, "Clarity", "Lets your s and t sounds through so words stay clear"));

    synth.setOptionsOnTop (true);
    synth.addOption (choice (waveform, "Wave", "The synth's tone: Saw is bright, Supersaw is wide, Triangle is soft"));
    synth.addKnob (knob (octave, "Octave", "Plays the synth higher or lower"));
    synth.addKnob (knob (osc2Level, "Osc 2", "Blends in a second oscillator, tuned in the Advanced view (default a fifth up)"));
    synth.addKnob (knob (osc2Semitones, "Osc 2 Tune", "Second oscillator's pitch in semitones: 7 is a fifth, 12 an octave"));
    synth.addKnob (knob (glide, "Glide", "Slides between notes"));

    tune.addOption (toggle (tuneOn, "Auto-Tune", "Snaps your voice to the nearest note in the key"));
    tune.addOption (choice (tuneKey, "Key", "The song's key"));
    tune.addOption (choice (tuneScale, "Scale", "Which notes are allowed. Chromatic works in any key"));
    tune.addOption (choice (singToPlay, "Sing to Play", "Your sung melody plays the synth, with harmony if you like. No keys needed"));
    tune.addKnob (knob (tuneSpeed, "Speed", "How fast notes snap. 0 is the hard robotic effect, higher sounds natural"));
    tune.addKnob (knob (tuneAmount, "Amount", "How much of the correction to apply"));
    tune.setExtra (std::make_unique<TunerDisplay> (p));

    arp.addOption (choice (arpMode, "Mode", "Note Arp plays your chord one note at a time. Rhythm Gate chops the sound"));
    arp.addOption (choice (arpRate, "Rate", "Step length, locked to the tempo"));
    arp.addOption (choice (arpPattern, "Pattern", "Note order for the note arp"));
    arp.addOption (choice (gatePattern, "Gate", "Rhythm for the rhythm gate"));

    filter.addKnob (knob (cutoff, "Cutoff", "Darker to brighter"));
    filter.addKnob (knob (resonance, "Resonance", "Adds a whistling peak at the cutoff"));
    filter.addKnob (knob (lfoDepth, "Wobble", "Sweeps the cutoff up and down (speed in Advanced: LFO Sync)"));
    filter.addKnob (knob (voiceFollow, "Wah", "Opens the filter as you sing louder"));

    fx.addKnob (knob (drive, "Drive", "Distortion"));
    fx.addKnob (knob (chorusMix, "Chorus", "Widens the sound"));
    fx.addKnob (knob (delayMix, "Delay", "Tempo-synced echoes"));
    fx.addKnob (knob (reverbMix, "Reverb", "Space"));

    vintage.setOptionsOnTop (true);
    vintage.addOption (choice (vintageEra, "Era", "Which old microphone and record you sing through"));
    vintage.addKnob (knob (vintageWear, "Wear", "How worn the record is: hiss, crackle and wobble"));
    vintage.addKnob (knob (vintageAmount, "Amount", "Blend between today's sound and the old record"));

    mix.addKnob (knob (outputGain, "Level", "Output level"));

    for (auto* section : { &voice, &synth, &tune, &arp, &filter, &fx, &vintage, &mix })
        addAndMakeVisible (section);
}

void MainPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    auto top = area.removeFromTop (area.getHeight() / 2);
    auto bottom = area;

    // Widths in proportion to what each section holds.
    auto lay = [] (juce::Rectangle<int> row, std::initializer_list<std::pair<Section*, float>> parts)
    {
        float total = 0.0f;
        for (auto& [s, w] : parts)
            total += w;
        const float unit = (float) row.getWidth() / total;
        for (auto& [s, w] : parts)
            s->setBounds (row.removeFromLeft (juce::roundToInt (w * unit)));
    };
    lay (top, { { &voice, 3.3f }, { &synth, 4.4f }, { &tune, 4.3f } });
    lay (bottom, { { &arp, 2.0f }, { &filter, 3.6f }, { &fx, 3.6f }, { &vintage, 2.35f }, { &mix, 1.45f } });
}

//==============================================================================
ListenBar::ListenBar (VocoderProcessor& p)
    : processor (p),
      robotAttachment (p.params, ParamIDs::robotLevel, robotLevel),
      voiceAttachment (p.params, ParamIDs::voiceLevel, voiceLevel)
{
    // One click back to where you started: just your raw voice.
    rawButton.setTooltip ("Back to just your raw voice: switches every module off and stops the chords");
    rawButton.setWantsKeyboardFocus (false);
    rawButton.setColour (juce::TextButton::buttonColourId, Palette::tune.darker (0.9f));
    rawButton.onClick = [this]
    {
        processor.setCurrentProgram (0);
        if (onBackToRaw)
            onBackToRaw();
    };
    addAndMakeVisible (rawButton);

    struct Info { const char* id; const char* name; const char* tip; juce::Colour colour; };
    const Info info[numParts] = {
        { ParamIDs::voiceOn, "Your voice",
          "Your own voice, straight from the mic (auto-tuned when Auto-Tune is on). When nothing else would be heard, it comes in on its own and its lamp pulses",
          Palette::tune },
        { ParamIDs::tuneOn, "Auto-Tune", "Snaps your voice to the notes of the key", Palette::tune.interpolatedWith (Palette::synth, 0.5f) },
        { ParamIDs::robotOn, "Robot", "The vocoder: your words played by the synth, on the notes of your chord", Palette::voice },
        { ParamIDs::arpOn, "Rhythm", "The rhythm gate chops your sound into a beat; the note arp plays the robot's chord", Palette::arp },
        { ParamIDs::fxOn, "Effects", "Echo, reverb, chorus and distortion on everything", Palette::fx },
        { ParamIDs::vintageOn, "Vintage", "Puts everything on an old record, sung into a mic from another decade", Palette::vintage },
    };
    for (int i = 0; i < numParts; ++i)
    {
        auto& b = parts[i].button;
        b.setButtonText (info[i].name);
        b.setTooltip (info[i].tip);
        b.setClickingTogglesState (true);
        b.setWantsKeyboardFocus (false);
        b.accent = info[i].colour;
        parts[i].attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.params, info[i].id, b);
        addAndMakeVisible (b);
    }

    for (auto* s : { &robotLevel, &voiceLevel })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setWantsKeyboardFocus (false);
        addAndMakeVisible (*s);
    }
    voiceLevel.setTooltip ("Your voice level");
    robotLevel.setTooltip ("Robot level");
    VocoderLook::setAccent (voiceLevel, Palette::tune);
    VocoderLook::setAccent (robotLevel, Palette::voice);
    voiceLevel.setColour (juce::Slider::trackColourId, Palette::tune);
    robotLevel.setColour (juce::Slider::trackColourId, Palette::voice);

    // Switching a part on should always be heard, so fill in what it needs:
    // auto-tune needs your voice, rhythm needs a mode (the editor gives the robot notes),
    // and effects need at least one effect turned up.
    auto& state = p.params;
    auto value = [&state] (const char* id) { return state.getRawParameterValue (id)->load(); };
    auto set = [&state] (const char* id, float v)
    {
        if (auto* param = state.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    parts[1].button.onClick = [this, value, set]
    {
        if (parts[1].button.getToggleState() && value (ParamIDs::voiceOn) < 0.5f && value (ParamIDs::singToPlay) < 0.5f)
            set (ParamIDs::voiceOn, 1.0f);
    };
    parts[3].button.onClick = [this, value, set]
    {
        if (parts[3].button.getToggleState() && value (ParamIDs::arpMode) < 0.5f)
            set (ParamIDs::arpMode, 2.0f); // Rhythm Gate: works on your voice and the robot
    };
    parts[4].button.onClick = [this, value, set]
    {
        if (! parts[4].button.getToggleState())
            return;
        bool any = false;
        for (auto* id : { ParamIDs::drive, ParamIDs::crush, ParamIDs::chorusMix, ParamIDs::delayMix, ParamIDs::reverbMix })
            any = any || value (id) > 0.001f;
        if (! any)
            set (ParamIDs::reverbMix, 0.3f);
    };

    // A part's level slider fades while the part is switched off.
    parts[0].button.onStateChange = [this] { voiceLevel.setAlpha (parts[0].button.getToggleState() ? 1.0f : 0.35f); };
    parts[2].button.onStateChange = [this] { robotLevel.setAlpha (parts[2].button.getToggleState() ? 1.0f : 0.35f); };
    parts[0].button.onStateChange();
    parts[2].button.onStateChange();
    startTimerHz (15);
}

void ListenBar::timerCallback()
{
    auto& voice = parts[0].button;
    const bool standingIn = processor.isVoiceStandingIn() && ! voice.getToggleState();
    if (standingIn || voice.standingIn)
    {
        voice.standingIn = standingIn;
        voice.pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.004f);
        voice.repaint();
    }
    voiceLevel.setAlpha (voice.getToggleState() || standingIn ? 1.0f : 0.35f);
}

//==============================================================================
void ModuleButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const bool on = getToggleState();
    Gear::drawKey (g, r, on ? Palette::control.interpolatedWith (accent, 0.18f) : Palette::control.darker (0.2f), on, highlighted,
                   down, 3.0f);
    if (down)
        r = r.translated (0.0f, 1.0f);

    const float glow = on ? 1.0f : standingIn ? 0.35f + 0.45f * pulse : 0.0f;
    Gear::drawLamp (g, { r.getX() + 11.0f, r.getCentreY() }, 4.0f, accent, glow);
    g.setColour (on || standingIn ? Palette::text : Palette::dim);
    g.setFont (Gear::engraved (jmin (13.5f, r.getHeight() * 0.4f)));
    g.drawFittedText (standingIn ? getButtonText() + " (auto)" : getButtonText(), r.withTrimmedLeft (20.0f).toNearestInt(),
                      Justification::centred, 1, 0.8f);
}

//==============================================================================
VuMeter::VuMeter (VocoderProcessor& p) : processor (p)
{
    setTooltip ("Output level");
    startTimerHz (30);
}

void VuMeter::timerCallback()
{
    // Rough VU ballistics: about 300 ms to rise and fall.
    const float db = juce::Decibels::gainToDecibels (processor.getOutputLevel(), -40.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 33.0f); // -30 dB to +3 dB
    const float moved = needle + 0.2f * (target - needle);
    if (std::abs (moved - needle) > 0.001f)
    {
        needle = moved;
        repaint();
    }
}

void VuMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    auto face = r.reduced (2.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff6e6b8), face.getCentreX(), face.getBottom(), Colour (0xffd9c08a), face.getCentreX(),
                                       face.getY(), true));
    g.fillRoundedRectangle (face, 2.0f);

    const auto pivot = Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
    const float radius = face.getHeight() * 1.05f;
    const float start = -0.75f, end = 0.75f, red = start + (end - start) * (30.0f / 33.0f);
    Path scale, redZone;
    scale.addCentredArc (pivot.x, pivot.y, radius * 0.82f, radius * 0.82f, 0.0f, start, red, true);
    redZone.addCentredArc (pivot.x, pivot.y, radius * 0.82f, radius * 0.82f, 0.0f, red, end, true);
    g.setColour (Colour (0xff2a2520));
    g.strokePath (scale, PathStrokeType (1.0f));
    g.setColour (Colour (0xffc8322a));
    g.strokePath (redZone, PathStrokeType (2.5f));
    for (int i = 0; i <= 6; ++i)
    {
        const float a = start + (end - start) * (float) i / 6.0f;
        g.setColour (a > red ? Colour (0xffc8322a) : Colour (0xff2a2520));
        g.drawLine (Line<float> (pivot.getPointOnCircumference (radius * 0.82f, a), pivot.getPointOnCircumference (radius * 0.92f, a)), 1.0f);
    }
    g.setColour (Colour (0xff2a2520));
    g.setFont (Gear::engraved (8.0f));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.45f), Justification::centred);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    const float a = start + (end - start) * needle;
    g.setColour (Colour (0xff1a1612));
    g.drawLine (Line<float> (pivot.getPointOnCircumference (radius * 0.3f, a), pivot.getPointOnCircumference (radius * 0.97f, a)), 1.2f);
    g.restoreState();

    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.25f), face.getX(), face.getY(), Colours::transparentWhite, face.getX(),
                                       face.getCentreY(), false));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 2.0f);
}

void ListenBar::paint (juce::Graphics& g)
{
    using namespace juce;
    // A rack rail the modules sit in.
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff2e2a25), 0.0f, r.getY(), Colour (0xff141210), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (0, 0.0f, r.getRight());
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, r.getRight());

    // Patch cables between the modules, in the order the sound flows through them.
    for (int i = 0; i + 1 < numParts; ++i)
    {
        auto from = parts[i].button.getBounds();
        if (i == 0)
            from = from.getUnion (voiceLevel.getBounds());
        if (i == 2)
            from = from.getUnion (robotLevel.getBounds());
        const float x0 = (float) from.getRight() + 1.0f, x1 = (float) parts[i + 1].button.getX() - 1.0f;
        const float y = (float) from.getCentreY() + 2.0f;
        Path cable;
        cable.startNewSubPath (x0, y);
        cable.cubicTo (x0 + 6.0f, y + 9.0f, x1 - 6.0f, y + 9.0f, x1, y);
        g.setColour (Colour (0xff0a0908));
        g.strokePath (cable, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::record.darker (0.6f));
        g.strokePath (cable, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::metal);
        g.fillRoundedRectangle (x0 - 1.0f, y - 2.5f, 4.0f, 5.0f, 1.0f);
        g.fillRoundedRectangle (x1 - 3.0f, y - 2.5f, 4.0f, 5.0f, 1.0f);
    }
}

void ListenBar::resized()
{
    auto area = getLocalBounds().reduced (10, 5);
    rawButton.setBounds (area.removeFromLeft (92));
    area.removeFromLeft (12);

    const int gap = 22;
    const int sliderWidth = juce::jlimit (50, 100, (area.getWidth() - numParts * 88 - (numParts - 1) * gap) / 2);
    const int buttonWidth = juce::jmin (100, (area.getWidth() - 2 * sliderWidth - (numParts - 1) * gap) / numParts);
    for (int i = 0; i < numParts; ++i)
    {
        parts[i].button.setBounds (area.removeFromLeft (buttonWidth));
        if (i == 0)
            voiceLevel.setBounds (area.removeFromLeft (sliderWidth).reduced (4, 0));
        if (i == 2)
            robotLevel.setBounds (area.removeFromLeft (sliderWidth).reduced (4, 0));
        area.removeFromLeft (gap);
    }
}
