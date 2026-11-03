#pragma once

#include "EasySteps.h"
#include "MainPanel.h"

// Easy mode: build a sound one step at a time (Voice, Chords, Rhythm,
// Effects, Auto-Tune) by clicking big tiles. Studio and Advanced have the
// same settings in more detail.

class Tile : public juce::Button
{
public:
    Tile (const juce::String& name, const juce::String& description, juce::Colour accent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String description;
    juce::Colour accent;
};

class EasyPanel : public juce::Component, private juce::Timer
{
public:
    explicit EasyPanel (VocoderProcessor& processor);
    void resized() override;
    void paint (juce::Graphics&) override;

    void showStep (int index);

private:
    void timerCallback() override;
    void buildStep();

    VocoderProcessor& processor;
    int step = 0;

    juce::OwnedArray<juce::TextButton> stepButtons;
    juce::TextButton backButton { "Back" }, nextButton { "Next" };
    juce::Label guide;
    juce::OwnedArray<Tile> tiles;
    std::vector<std::unique_ptr<juce::Component>> extras; // the fine-tune knobs for this step
};
