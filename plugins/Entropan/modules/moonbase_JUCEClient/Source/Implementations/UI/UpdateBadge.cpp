#if INCLUDE_MOONBASE_UI
#include "UpdateBadge.h"

#include <moonbase_JUCEClient/Assets/MoonbaseBinary.h>
#include "../../SupressMSVCWarnings.h"

using namespace Moonbase::JUCEClient;

UpdateBadge::UpdateBadge (API& api_, juce::Component& p, const UpdateBadge::Options& o)
:
api (api_),
parent (&p),
options (o),
#if JUCE_VERSION >= 0x080002
roboto (juce::Font (juce::FontOptions (
    juce::Typeface::createSystemTypefaceFor (
        MoonbaseBinary::RobotoRegular_ttf,
        MoonbaseBinary::RobotoRegular_ttfSize))
    .withHorizontalScale (1.0f)
    .withKerningFactor (0.05f)
))
#else
roboto (juce::Font (juce::Font (
    juce::Typeface::createSystemTypefaceFor (
        MoonbaseBinary::RobotoRegular_ttf,
        MoonbaseBinary::RobotoRegular_ttfSize))
    .withHorizontalScale (1.0f)
    .withExtraKerningFactor (0.05f)
))
#endif
{
    // The badge is supposed to sit in a corner of the parent component
    // You can change the inset from given borders using the Options argument
    jassert (options.justification == juce::Justification::topLeft
          || options.justification == juce::Justification::topRight
          || options.justification == juce::Justification::bottomLeft
          || options.justification == juce::Justification::bottomRight);

    setAlwaysOnTop (true);
    setVisible (false);

    parent->addChildComponent (this);
    parent->addComponentListener (this);

    if (options.fadeTimeSeconds > 0.0f)
        alphaDecrement = 1.0f / (options.fadeTimeSeconds * 30.0f);

    updateIcon = MB_LOAD_SVG (MoonbaseBinary::Icon_Update_Available_svg);

    addAndMakeVisible (updateButton);
    updateButton.setLookAndFeel (&updateButtonLAF);
    updateButton.onClick = std::bind (&UpdateBadge::openDownloadUrl, this);

    addAndMakeVisible (doNotShowAgainButton);
    doNotShowAgainButton.setLookAndFeel (&doNotShowAgainButtonLAF);
    doNotShowAgainButtonLAF.setColours (juce::Colour (0xFF807373), juce::Colour (0xFFCCC4C4));
    doNotShowAgainButton.onClick = std::bind (&UpdateBadge::writeDoNotShowAgainFile, this);
    doNotShowAgainFile = api.getLicenseFile ().getSiblingFile ("do_not_show_update_badge");       
    
    addAndMakeVisible (closeButton);
    closeButton.setLookAndFeel (&closeButtonLAF);
    closeButton.onClick = std::bind (&UpdateBadge::closeBadge, this);

    if (MB_IS_UNLOCKED_OBFUSCATED(api).first)
        showBadgeIfUpdateIsAvailable ();

    api.addActivationStateChangedCallback (
        [&, weak = juce::WeakReference(this)] (const bool unlocked, const bool trial, const bool offlineActivated) {
            juce::ignoreUnused (trial, offlineActivated);
            if (weak && unlocked)
                showBadgeIfUpdateIsAvailable ();
        }
    );
}

UpdateBadge::~UpdateBadge ()
{
    stopTimer ();

    if (parent != nullptr)
    {
        parent->removeChildComponent (this);
        parent->removeComponentListener (this);
    }

    updateButton.setLookAndFeel (nullptr);
    doNotShowAgainButton.setLookAndFeel (nullptr);
    closeButton.setLookAndFeel (nullptr);
}

void UpdateBadge::showBadgeIfUpdateIsAvailable (bool force)
{
    if (api.isUpdateAvailable() && !badgeWasShown && (!doNotShowAgain () || force))
    {
        badgeWasShown = true;
        setVisible (true);

        if (!isMouseOver (true))
            startTimer (4000);  //wait for 4s, then begin fadeout
    }
}

void UpdateBadge::writeDoNotShowAgainFile ()
{
    if (doNotShowAgainFile.existsAsFile ())
        doNotShowAgainFile.deleteFile();
    
    doNotShowAgainFile.create();

    doNotShowAgainFile.replaceWithText (api.getCurrentReleaseVersion ());

    setVisible (false);
}

bool UpdateBadge::doNotShowAgain () const
{
    if (! doNotShowAgainFile.existsAsFile ())
        return false;

    const auto version = doNotShowAgainFile.loadFileAsString().trim();
    return version == api.getCurrentReleaseVersion();
}

void UpdateBadge::timerCallback ()
{
    if (! badgeIsFading)
    {
        badgeIsFading = true;
        startTimerHz (30); 
    }
    
    const auto currentAlpha = getAlpha();
    const auto targetAlpha = currentAlpha - alphaDecrement;
    if (targetAlpha <= 0.f)
    {
        setAlpha (0.f);
        stopTimer ();
        setVisible (false);
    }
    else
    {
        setAlpha (targetAlpha);
    }
}

void UpdateBadge::closeBadge ()
{
    stopTimer ();
    setVisible (false);
    
    badgeIsFading = false;
}

void UpdateBadge::openDownloadUrl ()
{
    const auto downloadUrl = api.getBaseUrl ()
        .getChildURL ("/api/public/products/" + api.getProductId() + "/download-redirect");
    downloadUrl.launchInDefaultBrowser();
}

void UpdateBadge::mouseEnter (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    stopTimer ();
    badgeIsFading = false;
    setAlpha (1.0f);
    repaint ();
}

void UpdateBadge::mouseExit (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    repaint ();

    if (!isMouseOver (true))
        startTimer (2000);  //wait for 2s, then begin fadeout
}

void UpdateBadge::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == this)
        openDownloadUrl();
}

void UpdateBadge::componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized)
{
    if (&component == parent.get () && (wasMoved || wasResized))
    {
        // Reposition the badge relative to the parent component
        auto parentBounds = options.inset.subtractedFrom (parent->getLocalBounds());
        
        juce::Rectangle<int> badgeArea { 264, 127 };
        if (options.justification == juce::Justification::topLeft)
        {
            badgeArea.setX (parentBounds.getX());
            badgeArea.setY (parentBounds.getY());
        }
        else if (options.justification == juce::Justification::topRight)
        {
            badgeArea.setX (parentBounds.getRight() - badgeArea.getWidth());
            badgeArea.setY (parentBounds.getY());
        }
        else if (options.justification == juce::Justification::bottomLeft)
        {
            badgeArea.setX (parentBounds.getX());
            badgeArea.setY (parentBounds.getBottom() - badgeArea.getHeight());
        }
        else if (options.justification == juce::Justification::bottomRight)
        {
            badgeArea.setX (parentBounds.getRight() - badgeArea.getWidth());
            badgeArea.setY (parentBounds.getBottom() - badgeArea.getHeight());
        }

        setBounds (badgeArea);
    }
}

void UpdateBadge::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds ().toFloat ().reduced (1.f);
    
    const juce::ColourGradient gradient {
        juce::Colour (0xFF181818), 
        juce::Point<float> (area.getWidth () * 0.5f, 0.f),
        juce::Colour (0xFF101010),
        juce::Point<float> (area.getWidth () * 0.5f, area.getHeight ()),
        false
    };
    g.setGradientFill (gradient);

    const auto cornerSize = 15.f;
    g.fillRoundedRectangle (area, cornerSize);

    juce::Colour borderColour { 0xFF5465FF };
    if (isMouseOver () || updateButton.isMouseOver ())
        borderColour = borderColour.brighter ();

    g.setColour (borderColour);
    g.drawRoundedRectangle (area, cornerSize, 1.f);

    if (updateIcon && !updateIconArea.isEmpty ())
        updateIcon->drawWithin (g, updateIconArea, juce::RectanglePlacement::centred, 1.0f);


    g.setFont (roboto.withHeight (updateTitleArea.getHeight ()));
    g.setColour (juce::Colour (0xFFCCC4C4));
    g.drawText ("Update Available", updateTitleArea, juce::Justification::centred);

    g.setFont (roboto.withHeight (updateTextArea.getHeight ()));
    g.setColour (juce::Colour (0xFF998D8D));
    g.drawText ("Click here to download v" + api.getCurrentReleaseVersion(), updateTextArea, juce::Justification::centred);
}

void UpdateBadge::resized ()
{
    const auto area = getLocalBounds ().reduced (1);

    const auto buttonHeight = (26);
    const auto buttonWidth = updateButtonLAF.getWidthForHeight (buttonHeight);

    juce::Rectangle<int> buttonArea { buttonWidth, buttonHeight };
    buttonArea.setCentre (area.getCentreX(), area.getBottom() - (int)(static_cast<float> (area.getHeight ()) * (0.32f)));
    updateButton.setBounds (buttonArea);

    const auto updateIconSize = (19);
    updateIconArea = juce::Rectangle<float> (0, 0, updateIconSize, updateIconSize);
    updateIconArea.setCentre (static_cast<float> (area.getCentreX ()), static_cast<float> (area.getY ()) + static_cast<float> (area.getHeight ()) * (0.17f));

    const auto doNotShowAgainButtonHeight =  (9);
    const auto doNotShowAgainButtonWidth =  (75);
    juce::Rectangle<int> doNotShowAgainButtonArea { doNotShowAgainButtonWidth, doNotShowAgainButtonHeight };
    doNotShowAgainButtonArea.setCentre (area.getCentreX(), area.getBottom() - (int)(static_cast<float> (area.getHeight()) * (0.12f)));
    doNotShowAgainButton.setBounds (doNotShowAgainButtonArea);

    const auto titleHeight = (12.f);
    const auto textHeight = (10.f);

    updateTitleArea = juce::Rectangle<float> (0, 0, static_cast<float> (area.getWidth()) * 0.8f, titleHeight);
    updateTitleArea.setCentre (static_cast<float> (area.getCentreX ()), static_cast<float> (area.getY ()) + static_cast<float> (area.getHeight()) * (0.355f));

    updateTextArea = juce::Rectangle<float> (0, 0, static_cast<float> (area.getWidth()) * 0.8f, textHeight);
    updateTextArea.setCentre (static_cast<float> (area.getCentreX ()), static_cast<float> (area.getY ()) + static_cast<float> (area.getHeight()) * (0.47f));

    const auto closeButtonSize =  (15);
    const auto closeButtonInset =  (10);
    juce::Rectangle<int> closeButtonArea { closeButtonSize, closeButtonSize };
    closeButtonArea.setX (area.getRight() - closeButtonSize - closeButtonInset);
    closeButtonArea.setY (area.getY() + closeButtonInset);
    closeButton.setBounds (closeButtonArea);
}

#endif //INCLUDE_MOONBASE_UI
