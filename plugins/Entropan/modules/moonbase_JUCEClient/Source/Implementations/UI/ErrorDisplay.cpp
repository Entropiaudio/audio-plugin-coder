#if INCLUDE_MOONBASE_UI
#include "ErrorDisplay.h"
#include "../../SupressMSVCWarnings.h"

using namespace Moonbase::JUCEClient;
//==============================================================================
//==============================================================================
ErrorDisplay::ErrorDisplay (API& api_)
:
api (api_)
{
    setInterceptsMouseClicks (false, true);

    addChildComponent (errorText);
    errorText.setReadOnly (true);
    errorText.setMultiLine (true);
    errorText.setScrollbarsShown (true);
    errorText.setColour (juce::TextEditor::ColourIds::backgroundColourId,  juce::Colour (0x00000000));
    errorText.setColour (juce::TextEditor::ColourIds::outlineColourId,  juce::Colour (0x00000000));
    errorText.setColour (juce::TextEditor::ColourIds::textColourId,  juce::Colour (0xFFFFA700));
    errorText.setColour (juce::TextEditor::ColourIds::highlightColourId, juce::Colours::white.withAlpha (0.3f));
    errorText.setColour (juce::TextEditor::ColourIds::highlightedTextColourId,  juce::Colour (0xFFFFA700));

    update ();
}

void ErrorDisplay::update ()
{
    auto error = api.getLastError ();

    // const juce::String fbError { "This is an examplary error, that tells us something incredibly important!" };
    // if (error.isEmpty ())
    //     error = fbError;

    errorText.setText (error);

    if (error.isNotEmpty ())
    {
        warningIcon = MB_LOAD_SVG (MoonbaseBinary::ErrorSign_svg);
        errorText.setVisible (true);
        errorText.applyFontToAllText (errorText.getFont ());
    }
    else
    {
        warningIcon.reset ();
        errorText.setVisible (false);
    }
}


void ErrorDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.originalComponent != & errorText)
        return;

    // funny thing, viewport only scrolls if the mouse event is originated by a child of it, which texteditor is not. So lets fake it!
    if (auto viewport = dynamic_cast<juce::Viewport*> (errorText.getChildComponent (0)))
    {
        if (auto content = viewport->getViewedComponent ())
        {
            juce::MouseEvent fakeEvent{
                e.source,
                e.position,
                e.mods,
                e.pressure,
                e.orientation, e.rotation,
                e.tiltX, e.tiltY,

                // replace event and originalComponent with a child of the viewport
                content,
                content,

                e.eventTime,
                e.mouseDownPosition,
                e.mouseDownTime,
                e.getNumberOfClicks(),
                e.mouseWasDraggedSinceMouseDown()
            };

            errorText.mouseWheelMove (fakeEvent, wheel);
        }
    }
}

void ErrorDisplay::paint  (juce::Graphics &g)
{   
    if (warningIcon != nullptr)
        warningIcon->drawWithin (g, warningIconArea, juce::RectanglePlacement::centred, 1.0f);
}

void ErrorDisplay::resized ()
{
    auto area = getLocalBounds ();

    const auto iconSize = static_cast<float> (area.getHeight ()) * (0.8f);
    warningIconArea = juce::Rectangle<float> (iconSize, iconSize);
    warningIconArea.setCentre ((float)area.getTopLeft ().getX () + iconSize * 0.5f,
                               (float)area.getCentreY ());

    update ();
    
    #if JUCE_VERSION >= 0x080002
        errorText.setFont (juce::FontOptions (static_cast<float> (getHeight ()) * (0.35f)));
    #else
        errorText.setFont (juce::Font (static_cast<float> (getHeight ()) * (0.35f)));
    #endif

    errorText.applyFontToAllText (errorText.getFont ());

    area.removeFromLeft ((int)(warningIconArea.getRight () + static_cast<float> (getWidth()) * (0.02f)));
    errorText.setBounds (area);
}

#endif
