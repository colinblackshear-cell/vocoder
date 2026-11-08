#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Colours and drawing for the whole window: old studio gear. Warm black
// faceplates with cream lettering, walnut side cheeks, knurled knobs with
// aluminium caps, jewel pilot lamps and amber display windows.
namespace Palette
{
inline const juce::Colour background { 0xff17130f }; // the rack behind the gear
inline const juce::Colour panel { 0xff26221d };      // a faceplate
inline const juce::Colour panelEdge { 0xff433a2f };
inline const juce::Colour control { 0xff332d26 };
inline const juce::Colour text { 0xfff2e7cf };       // cream silkscreen
inline const juce::Colour dim { 0xffaa9b82 };

// Each module's colour, in the faded tones of old gear.
inline const juce::Colour voice { 0xff5cbfae };   // robot: teal
inline const juce::Colour synth { 0xffb594dc };   // synth: lilac
inline const juce::Colour tune { 0xffec7a5f };    // your voice / auto-tune: coral
inline const juce::Colour arp { 0xffeab34c };     // rhythm: mustard
inline const juce::Colour filter { 0xff72aedb };  // chords / filter: sky
inline const juce::Colour fx { 0xffa9c860 };      // effects: avocado
inline const juce::Colour vintage { 0xffdcab70 }; // vintage: tan
inline const juce::Colour mix { 0xffe6dccb };
inline const juce::Colour record { 0xffe0503f };

inline const juce::Colour display { 0xff0f0c09 };     // display windows
inline const juce::Colour displayText { 0xffffc66e }; // amber
inline const juce::Colour woodDark { 0xff3a2213 };
inline const juce::Colour woodLight { 0xff6e4425 };
inline const juce::Colour metal { 0xffc9c4ba };
} // namespace Palette

// Drawing for the hardware parts, shared by the look and the panels.
namespace Gear
{
inline juce::Font engraved (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.08f);
}

// Walnut side panel, with a little grain.
inline void drawWood (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Palette::woodLight, r.getX(), r.getY(), Palette::woodDark, r.getRight(), r.getY(), false));
    g.fillRect (r);
    Random random (7);
    for (int i = 0; i < (int) (r.getWidth() * 0.9f); ++i)
    {
        const float x = r.getX() + random.nextFloat() * r.getWidth();
        g.setColour (Colours::black.withAlpha (0.06f + random.nextFloat() * 0.12f));
        Path grain;
        grain.startNewSubPath (x, r.getY());
        for (float y = r.getY(); y < r.getBottom(); y += 40.0f)
            grain.lineTo (x + std::sin ((y + x * 13.0f) * 0.01f) * 1.5f, y + 40.0f);
        g.strokePath (grain, PathStrokeType (0.6f + random.nextFloat()));
    }
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRect (r, 1.0f);
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float radius)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Palette::metal, c.x - radius, c.y - radius, Palette::metal.darker (1.2f), c.x + radius,
                                       c.y + radius, false));
    g.fillEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), 0.7f);
    g.drawLine (c.x - radius * 0.65f, c.y + radius * 0.3f, c.x + radius * 0.65f, c.y - radius * 0.3f, 1.1f);
}

// A jewel pilot lamp. `glow` 0 is off, 1 fully lit.
inline void drawLamp (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float glow)
{
    using namespace juce;
    if (glow > 0.01f)
    {
        g.setGradientFill (ColourGradient (colour.withAlpha (0.45f * glow), c.x, c.y, colour.withAlpha (0.0f), c.x + radius * 3.0f,
                                           c.y, true));
        g.fillEllipse (Rectangle<float> (radius * 6.0f, radius * 6.0f).withCentre (c));
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (Rectangle<float> (radius * 2.0f + 3.0f, radius * 2.0f + 3.0f).withCentre (c));
    const auto lens = colour.darker (2.2f).interpolatedWith (colour.brighter (0.25f), glow);
    g.setGradientFill (ColourGradient (lens.brighter (0.6f), c.x - radius * 0.4f, c.y - radius * 0.5f, lens.darker (0.6f),
                                       c.x + radius, c.y + radius, true));
    g.fillEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.25f + 0.4f * glow));
    g.fillEllipse (Rectangle<float> (radius * 0.7f, radius * 0.5f).withCentre ({ c.x - radius * 0.3f, c.y - radius * 0.4f }));
}

// A faceplate: a warm black panel with a lighter top edge, a coloured stripe under
// the engraved title, and a screw in each corner.
inline void drawFaceplate (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, const juce::String& title = {},
                           bool screws = true)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 5.0f);
    g.setGradientFill (ColourGradient (Palette::panel.brighter (0.12f), r.getX(), r.getY(), Palette::panel.darker (0.25f), r.getX(),
                                       r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    if (screws && r.getWidth() > 60.0f && r.getHeight() > 40.0f)
        for (auto c : { Point<float> (r.getX() + 8.0f, r.getY() + 8.0f), Point<float> (r.getRight() - 8.0f, r.getY() + 8.0f),
                        Point<float> (r.getX() + 8.0f, r.getBottom() - 8.0f), Point<float> (r.getRight() - 8.0f, r.getBottom() - 8.0f) })
            drawScrew (g, c, 3.2f);

    if (title.isNotEmpty())
    {
        auto band = Rectangle<float> (r.getX() + 18.0f, r.getY() + 5.0f, r.getWidth() - 36.0f, 18.0f);
        g.setColour (Palette::text);
        g.setFont (engraved (12.0f));
        g.drawText (title, band, Justification::centredLeft);
        const float textWidth = GlyphArrangement::getStringWidth (engraved (12.0f), title);
        g.setColour (accent);
        g.fillRect (band.getX() + textWidth + 8.0f, band.getCentreY() - 1.0f, band.getWidth() - textWidth - 8.0f, 2.5f);
    }
}

// The knob: a scale of ticks (lit up to the value in the module's colour), a knurled
// black skirt and an aluminium cap with a pointer line.
inline void drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, float startAngle, float endAngle,
                      juce::Colour accent, bool bipolar, bool hover, bool enabled = true)
{
    using namespace juce;
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    // Scale.
    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const bool lit = enabled && (a >= jmin (from, angle) - 0.01f && a <= jmax (from, angle) + 0.01f);
        const bool major = i == 0 || i == ticks - 1 || i == ticks / 2;
        g.setColour (lit ? accent : Palette::dim.withAlpha (0.55f));
        g.drawLine (Line<float> (centre.getPointOnCircumference (radius * (major ? 0.78f : 0.84f), a),
                                 centre.getPointOnCircumference (radius * 0.98f, a)),
                    major ? 2.0f : 1.4f);
    }

    // Skirt, with a soft shadow.
    const float skirt = radius * 0.74f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (skirt * 2.0f, skirt * 2.0f).withCentre (centre.translated (0.0f, radius * 0.06f)));
    g.setGradientFill (ColourGradient (Colour (0xff3b3631), centre.x, centre.y - skirt, Colour (0xff0e0c0a), centre.x,
                                       centre.y + skirt, false));
    g.fillEllipse (Rectangle<float> (skirt * 2.0f, skirt * 2.0f).withCentre (centre));
    g.setColour (Colours::white.withAlpha (0.07f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 36.0f;
        g.drawLine (Line<float> (centre.getPointOnCircumference (skirt * 0.8f, a), centre.getPointOnCircumference (skirt * 0.98f, a)),
                    1.0f);
    }

    // Aluminium cap.
    const float cap = skirt * 0.66f;
    g.setGradientFill (ColourGradient (Palette::metal.brighter (hover ? 0.35f : 0.2f), centre.x - cap * 0.6f, centre.y - cap,
                                       Palette::metal.darker (0.9f), centre.x + cap * 0.6f, centre.y + cap, false));
    g.fillEllipse (Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre), 0.8f);

    // Pointer: cream on the skirt, black on the cap.
    g.setColour (enabled ? Palette::text : Palette::dim);
    g.drawLine (Line<float> (centre.getPointOnCircumference (cap * 1.05f, angle), centre.getPointOnCircumference (skirt * 0.97f, angle)),
                jmax (2.0f, radius * 0.07f));
    g.setColour (Colour (0xff1a1714));
    g.drawLine (Line<float> (centre.getPointOnCircumference (cap * 0.25f, angle), centre.getPointOnCircumference (cap * 0.92f, angle)),
                jmax (1.6f, radius * 0.05f));
}

// A chunky push button cap. `lit` tints it with the lamp colour.
inline void drawKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, bool lit, bool highlighted, bool down,
                     float corner = 4.0f)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 2.0f), corner);
    auto cap = down ? r.translated (0.0f, 1.0f) : r;
    auto top = base.brighter (highlighted ? 0.3f : 0.18f), bottom = base.darker (0.35f);
    g.setGradientFill (ColourGradient (top, cap.getX(), cap.getY(), bottom, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, corner);
    g.setColour (Colours::white.withAlpha (lit ? 0.35f : 0.1f));
    g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + corner, cap.getRight() - corner);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (cap, corner, 1.0f);
}
} // namespace Gear

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
        setColour (ComboBox::backgroundColourId, Palette::display);
        setColour (ComboBox::textColourId, Palette::displayText);
        setColour (ComboBox::outlineColourId, Palette::panelEdge);
        setColour (ComboBox::arrowColourId, Palette::dim);
        setColour (PopupMenu::backgroundColourId, Palette::panel);
        setColour (PopupMenu::textColourId, Palette::text);
        setColour (PopupMenu::headerTextColourId, Palette::displayText);
        setColour (PopupMenu::highlightedBackgroundColourId, Palette::control.brighter (0.25f));
        setColour (PopupMenu::highlightedTextColourId, Palette::displayText);
        setColour (TextButton::buttonColourId, Palette::control);
        setColour (TextButton::buttonOnColourId, Palette::voice.withAlpha (0.85f));
        setColour (TextButton::textColourOffId, Palette::text);
        setColour (TextButton::textColourOnId, Palette::background);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxTextColourId, Palette::dim);
        setColour (Label::textColourId, Palette::text);
        setColour (ToggleButton::tickColourId, Palette::voice);
        setColour (ToggleButton::tickDisabledColourId, Palette::dim);
        setColour (TooltipWindow::backgroundColourId, Palette::panel.brighter (0.1f));
        setColour (TooltipWindow::outlineColourId, Palette::panelEdge);
        setColour (TooltipWindow::textColourId, Palette::text);
        setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::arp.withAlpha (0.7f));
        setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::arp.withAlpha (0.3f));
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
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        // Bipolar controls (formant, osc 2 tune...) light from the centre.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        Gear::drawKnob (g, bounds, pos, startAngle, endAngle, accentOf (slider), bipolar, slider.isMouseOverOrDragging(),
                        slider.isEnabled());
    }

    // Faders: a slot with a chrome cap.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        using namespace juce;
        if (style != Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }
        auto r = Rectangle<int> (x, y, width, height).toFloat();
        const float cy = r.getCentreY();
        auto slot = Rectangle<float> (r.getX(), cy - 2.5f, r.getWidth(), 5.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (slider.findColour (Slider::trackColourId).withAlpha (slider.getAlpha() > 0.5f ? 0.9f : 0.4f));
        g.fillRoundedRectangle (slot.withRight (sliderPos).reduced (0.0f, 1.0f), 2.0f);

        auto cap = Rectangle<float> (10.0f, jmin (20.0f, r.getHeight())).withCentre ({ sliderPos, cy });
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.0f);
        g.setGradientFill (ColourGradient (Palette::metal.brighter (0.3f), cap.getX(), cap.getY(), Palette::metal.darker (0.8f),
                                           cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (Colour (0xff1a1714));
        g.drawLine (cap.getCentreX(), cap.getY() + 3.0f, cap.getCentreX(), cap.getBottom() - 3.0f, 1.5f);
    }

    // On/off switches: a bat-handle toggle and a lamp.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        using namespace juce;
        const auto accent = accentOf (button);
        auto bounds = button.getLocalBounds().toFloat();
        const bool on = button.getToggleState();
        const float h = jmin (20.0f, bounds.getHeight() - 2.0f);

        auto plate = Rectangle<float> (h * 0.8f, h).withPosition (bounds.getX() + 2.0f, bounds.getCentreY() - h * 0.5f);
        g.setColour (Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (plate, 2.5f);
        const auto pivot = plate.getCentre();
        const auto tip = pivot.translated (0.0f, (on ? -1.0f : 1.0f) * h * 0.42f);
        g.setColour (Palette::metal.darker (highlighted ? 0.0f : 0.3f));
        g.drawLine (Line<float> (pivot, tip), 3.0f);
        g.setColour (Palette::metal.brighter (0.3f));
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (tip));

        Gear::drawLamp (g, { plate.getRight() + 9.0f, bounds.getCentreY() }, 3.5f, accent, on ? 1.0f : 0.0f);

        g.setColour (button.isEnabled() ? Palette::text : Palette::dim);
        g.setFont (FontOptions (13.0f));
        g.drawFittedText (button.getButtonText(), bounds.withLeft (plate.getRight() + 18.0f).toNearestInt(),
                          Justification::centredLeft, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour, bool highlighted,
                               bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (2.0f);
        const bool lit = button.getToggleState();
        Gear::drawKey (g, bounds, lit ? colour.withAlpha (1.0f) : colour, lit, highlighted, down);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down) override
    {
        auto bounds = button.getLocalBounds().reduced (4, 1).withTrimmedBottom (3).translated (0, down ? 1 : 0);
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        g.setFont (Gear::engraved (juce::jmin (14.0f, (float) button.getHeight() * 0.48f)));
        g.drawFittedText (button.getButtonText(), bounds, juce::Justification::centred, 1);
    }

    // Choices look like little amber display windows set into the panel.
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.06f : 0.0f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), 0.0f, 0.0f, Colours::transparentBlack, 0.0f,
                                           (float) height * 0.45f, false));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (height - 1, 3.0f, (float) width - 3.0f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        Path arrow;
        const float ax = (float) width - 13.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (accentOf (box, Palette::dim));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::FontOptions (juce::jmin (14.0f, (float) box.getHeight() * 0.55f), juce::Font::bold);
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
