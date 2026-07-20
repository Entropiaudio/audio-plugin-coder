#if INCLUDE_MOONBASE_UI
#include "LAFs.h"
#include "../../SupressMSVCWarnings.h"

#include "Fonts.h"

MBDrawableButtonLAF::MBDrawableButtonLAF (std::unique_ptr <juce::Drawable> offDisabled_,
                                      std::unique_ptr <juce::Drawable> offNormal_,
                                      std::unique_ptr <juce::Drawable> offHover_,
                                      std::unique_ptr <juce::Drawable> offClick_,
                                      std::unique_ptr <juce::Drawable> onDisabled_,
                                      std::unique_ptr <juce::Drawable> onNormal_,
                                      std::unique_ptr <juce::Drawable> onHover_,
                                      std::unique_ptr <juce::Drawable> onClick_)
{
    offStates [0] = std::move (offDisabled_);
    offStates [1] = std::move (offNormal_);
    offStates [2] = std::move (offHover_);
    offStates [3] = std::move (offClick_);
    onStates  [0] = std::move (onDisabled_);
    onStates  [1] = std::move (onNormal_);
    onStates  [2] = std::move (onHover_);
    onStates  [3] = std::move (onClick_);
}

float MBDrawableButtonLAF::getRatioWoH ()
{
    juce::Drawable* d = nullptr;
    for (int i = 0; i < 4; i++)
    {
        if (offStates[i] != nullptr)
        {
            d = offStates[i].get();
            break;
        }
    }

    if (d == nullptr) return 2.0f;
    return (static_cast<float> (d->getWidth()) / static_cast<float> (d->getHeight()));
}

float MBDrawableButtonLAF::getRatioHoW ()
{
    juce::Drawable* d = nullptr;
    for (int i = 0; i < 4; i++)
    {
        if (offStates[i] != nullptr)
        {
            d = offStates[i].get();
            break;
        }
    }

    if (d == nullptr) return 0.5f;

    return (static_cast<float> (d->getHeight()) / static_cast<float> (d->getWidth()));
}

void MBDrawableButtonLAF::setDisabledAlpha (const float alpha)
{
    disabledAlpha = alpha;
}

void MBDrawableButtonLAF::setRectanglePlacement (const juce::RectanglePlacement& r)
{
    rectPlacement = r;
}

void MBDrawableButtonLAF::setImage (int index, std::unique_ptr <juce::Drawable> img)
{
    jassert (index >= 0 && index < 8);
    bool isOnState = index >= 4;
    index = index % 4;
    if (isOnState)
        onStates[index].reset (img.release ());
    else offStates[index].reset (img.release ());
}

void MBDrawableButtonLAF::setDrawFallbackRect (const bool draw)
{
    drawFallbackRect = draw;
}

void MBDrawableButtonLAF::drawToggleButton  (juce::Graphics &g, juce::ToggleButton &b, bool over, bool down)
{
    const auto enabled   { b.isEnabled () };
    const auto state     { b.getToggleState () };
    const auto area      { b.getLocalBounds ().toFloat () };
    const int mouseState { enabled ? (down ? 3 : over ? 2 : 1) : 0 };

    auto* drawable { state ? onStates[mouseState].get() : offStates[mouseState].get() };
    if (drawable)
    {
        drawable->drawWithin (g, area, rectPlacement, 1.0f);

        drawAdditionalStuff (g, b, over, down, area);
    }
    else if (!drawable && mouseState == 0)
    {
        auto d = state ? onStates[0].get() : offStates[0].get();
        if (d) d->drawWithin (g, area, rectPlacement, disabledAlpha);
        drawAdditionalStuff (g, b, over, down, area);
    }
    else if (drawFallbackRect)
    {
        g.setColour (juce::Colours::white.withAlpha (enabled ? state ? 1.f : 0.5f : 0.3f));
        g.drawRect (area);
        g.setFont (area.getHeight() * 0.13f);
        g.drawText (b.getName(), area,  juce::Justification::centred);
    }

}


//==============================================================================
//==============================================================================
CloseButtonLAF::CloseButtonLAF ()
:
MBDrawableButtonLAF (
    nullptr,
    MB_LOAD_SVG (MoonbaseBinary::Close_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Close_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Close_Click_svg),
    nullptr,
    MB_LOAD_SVG (MoonbaseBinary::Close_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Close_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Close_Click_svg)
) {}

//==============================================================================
//==============================================================================
BackButtonLAF::BackButtonLAF ()
:
MBDrawableButtonLAF (
    nullptr,
    MB_LOAD_SVG (MoonbaseBinary::Back_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Back_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Back_Click_svg),
    nullptr,
    MB_LOAD_SVG (MoonbaseBinary::Back_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Back_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Back_Click_svg)
) {}

//==============================================================================
//==============================================================================
WideTextButtonLAF::WideTextButtonLAF ()
:
MBDrawableButtonLAF (
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Click_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Long_Click_svg)
) {}

void WideTextButtonLAF::setRed ()
{
    setImage (0, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Normal_svg));
    setImage (1, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Normal_svg));
    setImage (2, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Over_svg));
    setImage (3, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Click_svg));
    setImage (4, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Normal_svg));
    setImage (5, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Normal_svg));
    setImage (6, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Over_svg));
    setImage (7, MB_LOAD_SVG (MoonbaseBinary::Button_Red_Long_Click_svg));
}

static inline void MB_ReduceClipRegion  (juce::Graphics&g, const  juce::Rectangle<float>& area,
                                     const juce::Path& path, bool zeroWinding, float rotation)
{
    auto transform { path.getTransformToScaleToFit(area, false).rotated (rotation, area.getCentreX(), area.getCentreY()) };
    auto transformedMask { path };
    transformedMask.applyTransform(transform);
    transformedMask.setUsingNonZeroWinding(zeroWinding);
    g.reduceClipRegion (transformedMask);
}

#define MB_IMPLEMENT_TEXT_BUTTON \
    const auto text = b.getButtonText (); \
     juce::Font font (Roboto::Regular ().withHeight (static_cast<float> (b.getHeight ()) * textHeightNormal)); \
    g.setColour ( juce::Colour (0xFFF0F0F0)); \
    g.setFont (font); \
    g.drawText (text, area,  juce::Justification::centred); \
    const auto enabled = b.isEnabled (); \
    if (!enabled) \
    { \
       MB_ReduceClipRegion (g, area, offStates[0]->getOutlineAsPath (), false, 0.0f); \
       g.fillAll (juce::Colours::white.withAlpha (0.3f)); \
    }

#define MB_IMPLEMENT_TEXT_BUTTON_W_FOR_H \
    auto& drawable = offStates[1]; \
    return (int)(static_cast<float> (height) * (static_cast<float> (drawable->getWidth ()) / static_cast<float> (drawable->getHeight ())));

void WideTextButtonLAF::drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area)
{
    juce::ignoreUnused (over, down);
    MB_IMPLEMENT_TEXT_BUTTON
}

int WideTextButtonLAF::getWidthForHeight (int height)
{
    MB_IMPLEMENT_TEXT_BUTTON_W_FOR_H
}

//==============================================================================
//==============================================================================
NarrowTextButtonLAF::NarrowTextButtonLAF ()
:
MBDrawableButtonLAF (
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Click_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Blue_Short_Click_svg)
) {}

void NarrowTextButtonLAF::drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area)
{
    juce::ignoreUnused (over, down);
    MB_IMPLEMENT_TEXT_BUTTON
}

int NarrowTextButtonLAF::getWidthForHeight (int height)
{
    MB_IMPLEMENT_TEXT_BUTTON_W_FOR_H
}

//==============================================================================
//==============================================================================
SolidTextButtonLAF::SolidTextButtonLAF ()
:
MBDrawableButtonLAF (
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Click_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Normal_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Over_svg),
    MB_LOAD_SVG (MoonbaseBinary::Button_Solid_Click_svg)
) {}

void SolidTextButtonLAF::drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area)
{
    juce::ignoreUnused (over, down);
    MB_IMPLEMENT_TEXT_BUTTON
}

int SolidTextButtonLAF::getWidthForHeight (int height)
{
    MB_IMPLEMENT_TEXT_BUTTON_W_FOR_H
}

//==============================================================================
//==============================================================================
TextEditorLAF::TextEditorLAF ()
{
    textEditorOutline = MB_LOAD_SVG (MoonbaseBinary::Email_Box_svg);
}

void TextEditorLAF::drawTextEditorOutline  (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    juce::ignoreUnused (te);
    if (textEditorOutline != nullptr)
        textEditorOutline->drawWithin (g,  juce::Rectangle<float> ((float)width, (float)height), juce::RectanglePlacement::centred, 1.0f);
}

int TextEditorLAF::getWidthForHeight (int height)
{
    auto& drawable = textEditorOutline;
    jassert (drawable != nullptr);
    return (int) (static_cast<float> (height) * (static_cast<float> (drawable->getWidth ()) / static_cast<float> (drawable->getHeight ())));
}

//==============================================================================
//==============================================================================
TextLinkLAF::TextLinkLAF ()
{
    colours [0] =  juce::Colour (0xFFD0D0D0);
    colours [1] =  juce::Colour (0xFF747474);
}

void TextLinkLAF::setUnderlined (bool underline)
{
    underlined = underline;
}

void TextLinkLAF::setColours (const  juce::Colour& normal, const juce::Colour& over)
{
    colours[0] = normal;
    colours[1] = over;
}

void TextLinkLAF::drawToggleButton  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down)
{
    juce::ignoreUnused (down);
    const auto text = b.getName ();
    const float height = (float)b.getHeight ();

    juce::Font f { Roboto::Regular ().withHeight (height) };
    f.setUnderline (underlined);

    g.setFont (f);
    g.setColour (colours[over]);
    g.drawText (text, b.getLocalBounds (), juce::Justification::centred);

}

//==============================================================================
//==============================================================================
RadioButtonLAF::RadioButtonLAF ()
{
    buttonIcon [0][0] = MB_LOAD_SVG (MoonbaseBinary::Selector_Off_Normal_svg);
    buttonIcon [0][1] = MB_LOAD_SVG (MoonbaseBinary::Selector_Off_Over_svg);
    buttonIcon [0][2] = MB_LOAD_SVG (MoonbaseBinary::Selector_Off_Click_svg);
    buttonIcon [1][0] = MB_LOAD_SVG (MoonbaseBinary::Selector_On_Normal_svg);
    buttonIcon [1][1] = MB_LOAD_SVG (MoonbaseBinary::Selector_On_Over_svg);
    buttonIcon [1][2] = MB_LOAD_SVG (MoonbaseBinary::Selector_On_Click_svg);
}

void RadioButtonLAF::drawToggleButton  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down)
{
    const auto text = b.getName ();
    const auto state = b.getToggleState ();

    auto area = b.getLocalBounds ().toFloat ();

    juce::Font font = Roboto::Regular ();
    const auto iconSize = static_cast<float> (area.getHeight ()) * (0.89f);
    const auto iconArea = area.removeFromLeft (iconSize).removeFromTop (iconSize);

    const auto iconToTextPadding = static_cast<float> (area.getWidth ()) * (0.08f);
    area.removeFromLeft (iconToTextPadding);

    const auto fontHeight = static_cast<float> (area.getHeight ()) * (0.85f);
    g.setFont (font.withHeight (fontHeight));
    g.setColour ( juce::Colour (0xFFD0D0D0));
    g.drawText (text, area, juce::Justification::centredLeft);

    if (auto& icon = buttonIcon [state][down ? 2 : over])
        icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);
}

#endif
