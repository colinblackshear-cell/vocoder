#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Colours and drawing for the whole window.
namespace Palette
{
inline const juce::Colour background { 0xff121419 };
inline const juce::Colour panel { 0xff1b1f27 };
inline const juce::Colour panelEdge { 0xff2a303b };
inline const juce::Colour control { 0xff262b35 };
inline const juce::Colour text { 0xffe9ecf2 };
inline const juce::Colour dim { 0xff8b93a3 };

inline const juce::Colour voice { 0xff3fd0bd };
inline const juce::Colour synth { 0xff9a7cff };
inline const juce::Colour tune { 0xffff5d9e };
inline const juce::Colour arp { 0xffffb547 };
inline const juce::Colour filter { 0xff47a8ff };
inline const juce::Colour fx { 0xff7fdc8a };
inline const juce::Colour vintage { 0xffd2a679 };
inline const juce::Colour mix { 0xffd8dde6 };
inline const juce::Colour record { 0xffe5484d };
} // namespace Palette

class VocoderLook : public juce::LookAndFeel_V4
{
public:
    // Components can set this property to pick their accent colour.
    static constexpr const char* accentProperty = "accent";

    VocoderLook()
    {
        using namespace juce;
        setColourScheme ({ Palette::background, Palette::panel, Palette::control, Palette::panelEdge, Palette::text,
                           Palette::voice, Colours::white, Palette::control, Palette::text });

        setColour (ResizableWindow::backgroundColourId, Palette::background);
        setColour (ComboBox::backgroundColourId, Palette::control);
        setColour (ComboBox::outlineColourId, Palette::panelEdge);
        setColour (ComboBox::arrowColourId, Palette::dim);
        setColour (PopupMenu::backgroundColourId, Palette::panel);
        setColour (PopupMenu::highlightedBackgroundColourId, Palette::control.brighter (0.2f));
        setColour (TextButton::buttonColourId, Palette::control);
        setColour (TextButton::buttonOnColourId, Palette::voice.withAlpha (0.8f));
        setColour (TextButton::textColourOffId, Palette::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxTextColourId, Palette::dim);
        setColour (Label::textColourId, Palette::text);
        setColour (ToggleButton::tickColourId, Palette::voice);
        setColour (ToggleButton::tickDisabledColourId, Palette::dim);
        setColour (TooltipWindow::backgroundColourId, Palette::panel.brighter (0.1f));
        setColour (TooltipWindow::outlineColourId, Palette::panelEdge);
        setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::voice.withAlpha (0.7f));
        setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::voice.withAlpha (0.3f));
    }

    static juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = Palette::voice)
    {
        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
            if (auto v = p->getProperties()[accentProperty]; ! v.isVoid())
                return juce::Colour ((juce::uint32) (juce::int64) v);
        return fallback;
    }

    static void setAccent (juce::Component& c, juce::Colour colour)
    {
        c.getProperties().set (accentProperty, (juce::int64) colour.getARGB());
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float startAngle,
                           float endAngle, juce::Slider& slider) override
    {
        using namespace juce;
        const auto accent = accentOf (slider);
        auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float ring = jmax (3.0f, radius * 0.14f);
        const float arcRadius = radius - ring * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (Palette::control.brighter (0.15f));
        g.strokePath (track, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));

        // Bipolar controls (formant, osc 2 tune...) fill from the centre.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (slider.isEnabled() ? accent : Palette::dim);
        g.strokePath (value, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));

        const float capRadius = arcRadius - ring * 1.4f;
        g.setColour (Palette::control.brighter (slider.isMouseOverOrDragging() ? 0.25f : 0.1f));
        g.fillEllipse (Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre));

        const auto tip = centre.getPointOnCircumference (capRadius * 0.8f, angle);
        const auto inner = centre.getPointOnCircumference (capRadius * 0.3f, angle);
        g.setColour (Palette::text);
        g.drawLine ({ inner, tip }, jmax (2.0f, ring * 0.6f));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        using namespace juce;
        const auto accent = accentOf (button);
        auto bounds = button.getLocalBounds().toFloat();
        const float h = jmin (18.0f, bounds.getHeight() - 4.0f);
        auto pill = Rectangle<float> (h * 1.8f, h).withCentre ({ bounds.getX() + h * 0.9f + 2.0f, bounds.getCentreY() });

        const bool on = button.getToggleState();
        g.setColour (on ? accent : Palette::control.brighter (highlighted ? 0.25f : 0.12f));
        g.fillRoundedRectangle (pill, h * 0.5f);
        const float knob = h - 4.0f;
        g.setColour (on ? Palette::background : Palette::dim);
        g.fillEllipse (Rectangle<float> (knob, knob).withCentre ({ on ? pill.getRight() - h * 0.5f : pill.getX() + h * 0.5f,
                                                                   pill.getCentreY() }));

        g.setColour (button.isEnabled() ? Palette::text : Palette::dim);
        g.setFont (FontOptions (13.0f));
        g.drawFittedText (button.getButtonText(), bounds.withLeft (pill.getRight() + 6.0f).toNearestInt(),
                          Justification::centredLeft, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour, bool highlighted,
                               bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto c = colour;
        if (down)
            c = c.brighter (0.2f);
        else if (highlighted)
            c = c.brighter (0.1f);
        g.setColour (c);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        Path arrow;
        const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (accentOf (box, Palette::dim));
        g.strokePath (arrow, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::FontOptions (juce::jmin (14.0f, (float) box.getHeight() * 0.55f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions (11.5f));
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::textColourId, slider.findColour (juce::Slider::textBoxTextColourId));
        return label;
    }
};
