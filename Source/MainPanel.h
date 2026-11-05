#pragma once

#include "Look.h"
#include "PluginProcessor.h"

// The simple view: the controls that matter most, as big knobs in coloured
// sections. Everything else lives in the Advanced view.

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip);
    void resized() override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip);
    void resized() override;

private:
    juce::Label label;
    juce::ComboBox box;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
};

class Switch : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& name, const juce::String& tip);
    void resized() override;

private:
    juce::ToggleButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

// Shows the note you're singing and how far off it is.
class TunerDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit TunerDisplay (VocoderProcessor& processor);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    VocoderProcessor& processor;
    float pitch = -1.0f, shown = 0.0f;
    int target = -1;
};

class Section : public juce::Component
{
public:
    Section (const juce::String& title, juce::Colour accent);

    // Choices and switches stack in a column on the left; knobs fill the rest.
    void addOption (std::unique_ptr<juce::Component> c);
    void addKnob (std::unique_ptr<juce::Component> c);
    void setExtra (std::unique_ptr<juce::Component> c) { extra = std::move (c); addAndMakeVisible (*extra); }
    void setOptionsOnTop (bool onTop) { optionsOnTop = onTop; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    juce::Colour accent;
    std::vector<std::unique_ptr<juce::Component>> options, knobs;
    std::unique_ptr<juce::Component> extra;
    bool optionsOnTop = false;
};

class MainPanel : public juce::Component
{
public:
    explicit MainPanel (VocoderProcessor& processor);
    void resized() override;

private:
    Section voice { "VOICE", Palette::voice }, synth { "SYNTH", Palette::synth }, tune { "AUTO-TUNE", Palette::tune },
        arp { "ARP", Palette::arp }, filter { "FILTER", Palette::filter }, fx { "EFFECTS", Palette::fx },
        vintage { "VINTAGE", Palette::vintage }, mix { "OUTPUT", Palette::mix };
};

// Your sound as a chain of modules: Your voice, Auto-Tune, Robot, Rhythm,
// Effects, Vintage. Each switches on and off on its own, so you can start from your
// raw voice and add only what you want. Shown in every view.
class ListenBar : public juce::Component
{
public:
    explicit ListenBar (VocoderProcessor& processor);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    static constexpr int numParts = 6;

    struct Part
    {
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    juce::Label title { {}, "Your sound" };
    Part parts[numParts];
    juce::Slider robotLevel, voiceLevel;
    juce::AudioProcessorValueTreeState::SliderAttachment robotAttachment, voiceAttachment;
};
