#include "MainPanel.h"
#include "Parameters.h"

namespace
{
constexpr int captionHeight = 15;
constexpr int optionHeight = 44;
constexpr int switchHeight = 28;

void styleCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
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
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

    // A short accent bar and the title.
    g.setColour (accent);
    g.fillRoundedRectangle (bounds.getX() + 12.0f, bounds.getY() + 11.0f, 14.0f, 4.0f, 2.0f);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (title, bounds.withTrimmedLeft (32.0f).withHeight (26.0f), juce::Justification::centredLeft);
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

    mix.addKnob (knob (robotLevel, "Robot", "How loud the robot (vocoder) is"));
    mix.addKnob (knob (voiceLevel, "Voice", "How loud your own voice is, auto-tuned if Auto-Tune is on"));
    mix.addKnob (knob (outputGain, "Level", "Output level"));

    for (auto* section : { &voice, &synth, &tune, &arp, &filter, &fx, &mix })
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
    lay (bottom, { { &arp, 2.0f }, { &filter, 3.6f }, { &fx, 3.6f }, { &mix, 3.0f } });
}

//==============================================================================
ListenBar::ListenBar (VocoderProcessor& p)
    : robotAttachment (p.params, ParamIDs::robotLevel, robotLevel),
      voiceAttachment (p.params, ParamIDs::voiceLevel, voiceLevel)
{
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Palette::dim);
    addAndMakeVisible (title);

    struct Info { const char* id; const char* name; const char* tip; juce::Colour colour; };
    const Info info[numParts] = {
        { ParamIDs::voiceOn, "Your voice", "Your own voice, straight from the mic (auto-tuned when Auto-Tune is on)", Palette::tune },
        { ParamIDs::tuneOn, "Auto-Tune", "Snaps your voice to the notes of the key", Palette::tune.interpolatedWith (Palette::synth, 0.5f) },
        { ParamIDs::robotOn, "Robot", "The vocoder: your words played by the synth, on the notes of your chord", Palette::voice },
        { ParamIDs::arpOn, "Rhythm", "The rhythm gate chops your sound into a beat; the note arp plays the robot's chord", Palette::arp },
        { ParamIDs::fxOn, "Effects", "Echo, reverb, chorus and distortion on everything", Palette::fx },
    };
    for (int i = 0; i < numParts; ++i)
    {
        auto& b = parts[i].button;
        b.setButtonText (info[i].name);
        b.setTooltip (info[i].tip);
        b.setClickingTogglesState (true);
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonOnColourId, info[i].colour.withAlpha (0.85f));
        b.setColour (juce::TextButton::textColourOnId, Palette::background);
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

    // A part's level slider fades while the part is switched off.
    parts[0].button.onStateChange = [this] { voiceLevel.setAlpha (parts[0].button.getToggleState() ? 1.0f : 0.35f); };
    parts[2].button.onStateChange = [this] { robotLevel.setAlpha (parts[2].button.getToggleState() ? 1.0f : 0.35f); };
    parts[0].button.onStateChange();
    parts[2].button.onStateChange();
}

void ListenBar::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
    g.setColour (Palette::panelEdge);
    g.fillRect (0, 0, getWidth(), 1);

    // Arrows between the modules, showing the order the sound flows through them.
    g.setColour (Palette::dim);
    for (int i = 0; i + 1 < numParts; ++i)
    {
        auto from = parts[i].button.getBounds();
        if (i == 0)
            from = from.getUnion (voiceLevel.getBounds());
        if (i == 2)
            from = from.getUnion (robotLevel.getBounds());
        const float x0 = (float) from.getRight() + 3.0f, x1 = (float) parts[i + 1].button.getX() - 3.0f;
        const float y = (float) from.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath (x0, y);
        arrow.lineTo (x1, y);
        g.strokePath (arrow, juce::PathStrokeType (1.5f));
        juce::Path head;
        head.addTriangle (x1, y, x1 - 5.0f, y - 3.5f, x1 - 5.0f, y + 3.5f);
        g.fillPath (head);
    }
}

void ListenBar::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    title.setBounds (area.removeFromLeft (80));

    const int gap = 22;
    const int sliderWidth = juce::jlimit (50, 100, (area.getWidth() - 5 * 92 - 4 * gap) / 2);
    const int buttonWidth = juce::jmin (100, (area.getWidth() - 2 * sliderWidth - 4 * gap) / numParts);
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
