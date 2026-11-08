#include "EasyPanel.h"

namespace
{
const juce::Colour stepColours[] = { Palette::voice, Palette::filter, Palette::arp, Palette::fx, Palette::tune, Palette::vintage };
} // namespace

//==============================================================================
Tile::Tile (const juce::String& name, const juce::String& d, juce::Colour a)
    : Button (name), description (d), accent (a)
{
    setClickingTogglesState (false);
    setWantsKeyboardFocus (false);
    setTooltip (d);
}

void Tile::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace juce;
    // A big selector key with a pilot lamp, like the voice buttons on an old organ.
    auto bounds = getLocalBounds().toFloat().reduced (5.0f, 4.0f).withTrimmedBottom (2.0f);
    const bool on = getToggleState();
    Gear::drawKey (g, bounds, on ? Palette::control.interpolatedWith (accent, 0.28f) : Palette::control, on, highlighted, down, 6.0f);
    if (down)
        bounds = bounds.translated (0.0f, 1.0f);

    auto content = bounds.reduced (14.0f, 9.0f);
    Gear::drawLamp (g, { content.getX() + 7.0f, content.getY() + 11.0f }, 6.0f, accent, on ? 1.0f : 0.0f);

    g.setColour (Palette::text);
    g.setFont (Gear::engraved (jmin (18.0f, bounds.getHeight() * 0.28f)));
    g.drawText (getButtonText().toUpperCase(), content.withTrimmedLeft (24.0f).removeFromTop (22.0f), Justification::centredLeft);

    g.setColour (on ? Palette::text.withAlpha (0.85f) : Palette::dim);
    g.setFont (FontOptions (13.0f));
    g.drawFittedText (description, content.withTrimmedTop (27.0f).toNearestInt(), Justification::topLeft, 2);
}

//==============================================================================
EasyPanel::EasyPanel (VocoderProcessor& p) : processor (p)
{
    const auto& steps = EasySteps::all();
    for (int i = 0; i < (int) steps.size(); ++i)
    {
        auto* b = stepButtons.add (new juce::TextButton (juce::String (i + 1) + "  " + steps[(size_t) i].title));
        b->setWantsKeyboardFocus (false);
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, stepColours[i].withAlpha (0.85f));
        b->setColour (juce::TextButton::textColourOnId, Palette::background);
        b->onClick = [this, i] { showStep (i); };
        addAndMakeVisible (b);
    }

    backButton.onClick = [this] { showStep (step - 1); };
    nextButton.onClick = [this] { showStep (step + 1); };
    for (auto* b : { &backButton, &nextButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    guide.setFont (juce::FontOptions (15.0f));
    guide.setColour (juce::Label::textColourId, Palette::text);
    guide.setJustificationType (juce::Justification::centredLeft);
    guide.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (guide);

    showStep (0);
    startTimerHz (5); // tiles follow changes made in Studio, presets or the DAW
}

void EasyPanel::showStep (int index)
{
    step = juce::jlimit (0, (int) EasySteps::all().size() - 1, index);
    buildStep();
}

void EasyPanel::buildStep()
{
    const auto& s = EasySteps::all()[(size_t) step];
    const auto accent = stepColours[step];
    VocoderLook::setAccent (*this, accent);

    for (int i = 0; i < stepButtons.size(); ++i)
        stepButtons[i]->setToggleState (i == step, juce::dontSendNotification);
    backButton.setEnabled (step > 0);
    nextButton.setEnabled (step < stepButtons.size() - 1);
    nextButton.setButtonText (step < stepButtons.size() - 1
                                  ? juce::String ("Next: ") + EasySteps::all()[(size_t) step + 1].title
                                  : juce::String ("Done"));

    guide.setText (juce::String (step + 1) + ". " + s.guide, juce::dontSendNotification);

    tiles.clear();
    for (size_t t = 0; t < s.tiles.size(); ++t)
    {
        auto* tile = tiles.add (new Tile (s.tiles[t].name, s.tiles[t].description, accent));
        tile->onClick = [this, t]
        {
            const auto& st = EasySteps::all()[(size_t) step];
            // In a pick-one step, clicking the lit tile again switches it off (back to the first tile).
            if (! st.switches && ! st.group.empty() && t > 0 && EasySteps::isActive (processor.params, st, st.tiles[t]))
                EasySteps::apply (processor.params, st, st.tiles.front());
            else
                EasySteps::apply (processor.params, st, st.tiles[t]);
            if (juce::String (st.title) == "Voice")
                processor.soundPicked();
            timerCallback();
        };
        addAndMakeVisible (tile);
    }

    // A few knobs to fine-tune the step.
    extras.clear();
    auto& state = processor.params;
    using namespace ParamIDs;
    auto knob = [&] (const char* id, const char* name, const char* tip) { extras.push_back (std::make_unique<Knob> (state, id, name, tip)); };
    switch (step)
    {
        case 0:
            knob (formant, "Character", "Smaller and squeakier, or bigger and deeper");
            knob (octave, "Octave", "Plays the synth higher or lower");
            knob (sibilance, "Clarity", "Lets your s and t sounds through so words stay clear");
            break;
        case 1:
            extras.push_back (std::make_unique<Choice> (state, chordKey, "Key", "The song's key"));
            extras.push_back (std::make_unique<Choice> (state, chordScale, "Scale",
                                                        "Major sounds happy, Minor sad, Dorian funky, Lydian dreamy"));
            extras.push_back (std::make_unique<Choice> (state, chordTiming, "Chords change",
                                                        "Next Beat keeps chord changes in time even if you click a little early or late"));
            break;
        case 2:
            extras.push_back (std::make_unique<Choice> (state, arpRate, "Speed", "How fast the steps go, locked to the tempo"));
            knob (arpGate, "Length", "Short and choppy, or long and smooth");
            knob (arpSwing, "Swing", "Pushes every other step late for a groove");
            break;
        case 3:
            knob (delayMix, "Echo", "How loud the echoes are");
            knob (reverbMix, "Space", "How big the room is");
            knob (chorusMix, "Wide", "How wide the sound is");
            knob (drive, "Crunch", "How gritty");
            knob (voiceLevel, "Your voice", "How loud your own voice is");
            break;
        case 4:
            extras.push_back (std::make_unique<TunerDisplay> (processor));
            knob (tuneSpeed, "Speed", "0 is the hard robot snap, higher sounds natural");
            knob (voiceLevel, "Your voice", "How loud your own tuned voice is");
            knob (robotLevel, "Robot", "How loud the robot is");
            break;
        default:
            knob (vintageWear, "Wear", "How worn the record is: hiss, crackle and wobble");
            knob (vintageAmount, "Amount", "Blend between today's sound and the old record");
            knob (voiceLevel, "Your voice", "How loud your own voice is");
            break;
    }
    for (auto& e : extras)
        addAndMakeVisible (*e);

    timerCallback();
    resized();
    repaint();
}

void EasyPanel::timerCallback()
{
    const auto& s = EasySteps::all()[(size_t) step];
    bool anyOn = false;
    for (int t = 0; t < tiles.size(); ++t)
    {
        // In pick-one steps only the first matching tile lights up.
        const bool on = EasySteps::isActive (processor.params, s, s.tiles[(size_t) t]) && (s.switches || ! anyOn);
        anyOn = anyOn || on;
        if (tiles[t]->getToggleState() != on)
            tiles[t]->setToggleState (on, juce::dontSendNotification);
    }
}

void EasyPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
    Gear::drawFaceplate (g, getLocalBounds().toFloat().reduced (4.0f, 3.0f), stepColours[step]);
}

void EasyPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    // Step buttons across the top.
    auto steps = area.removeFromTop (36);
    const int w = steps.getWidth() / juce::jmax (1, stepButtons.size());
    for (auto* b : stepButtons)
        b->setBounds (steps.removeFromLeft (w).reduced (3, 0));
    area.removeFromTop (6);

    // Guide text, with Back / Next on the right.
    auto guideRow = area.removeFromTop (40);
    nextButton.setBounds (guideRow.removeFromRight (150).reduced (0, 5));
    guideRow.removeFromRight (6);
    backButton.setBounds (guideRow.removeFromRight (64).reduced (0, 5));
    guide.setBounds (guideRow.withTrimmedRight (8));

    // Fine-tune controls along the bottom.
    auto extraRow = area.removeFromBottom (juce::jmin (110, area.getHeight() / 3));
    if (! extras.empty())
    {
        const bool tuner = dynamic_cast<TunerDisplay*> (extras.front().get()) != nullptr;
        if (tuner)
        {
            extras.front()->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2).withSizeKeepingCentre (
                extraRow.getWidth() / 2 - 20, 34));
        }
        const int count = (int) extras.size() - (tuner ? 1 : 0);
        const int cell = juce::jmin (150, extraRow.getWidth() / juce::jmax (1, count));
        auto row = extraRow.withSizeKeepingCentre (cell * count, extraRow.getHeight());
        for (size_t i = tuner ? 1 : 0; i < extras.size(); ++i)
        {
            auto bounds = row.removeFromLeft (cell).reduced (8, 0);
            if (dynamic_cast<Choice*> (extras[i].get()) != nullptr)
                bounds = bounds.withSizeKeepingCentre (bounds.getWidth(), 44);
            extras[i]->setBounds (bounds);
        }
    }
    area.removeFromBottom (6);

    // Tiles: four across, in as many rows as needed.
    const int columns = 4;
    const int rows = (tiles.size() + columns - 1) / columns;
    if (rows == 0)
        return;
    const int tileHeight = juce::jmin (112, area.getHeight() / rows);
    auto grid = area.removeFromTop (tileHeight * rows);
    const int tileWidth = grid.getWidth() / columns;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (grid.getX() + (i % columns) * tileWidth, grid.getY() + (i / columns) * tileHeight, tileWidth,
                             tileHeight);
}
