#if INCLUDE_MOONBASE_UI
#include "ContentBase.h"
#include "../ActivationComponent.h"
#include "Pages/WelcomePage.h"
#include "Pages/ActivatedPage.h"
#include "../Fonts.h"
#include "../UI_Impl.h"
//==============================================================================
//==============================================================================
ContentBase::ContentBase (ContentHolder& parent, API& api_)
:
api (api_),
contentHolder (parent),
errorDisplay (api)
{
    addChildComponent (closeButton);
    closeButton.setLookAndFeel (&closeButtonLAF);
    closeButton.onClick = [&] ()
    {
        api.hideActivationUi ();
    };

    addChildComponent (backButton);
    backButton.setLookAndFeel (&backButtonLAF);
    //click implemented by getBackButtonConfig ()

    addAndMakeVisible (errorDisplay);

    addChildComponent (waitingOverlay);
    waitingOverlay.setBgColours ( juce::Colour (0xFF141414),  juce::Colour (0xFF141414));
    waitingOverlay.setAlwaysOnTop (true);

    juce::WeakReference<ContentBase> weakThis (this);
    juce::MessageManager::callAsync ([weakThis]() { if (weakThis != nullptr) weakThis->update (); });
}

ContentBase::~ContentBase ()
{
    closeButton.setLookAndFeel (nullptr);
    backButton.setLookAndFeel (nullptr);
}

void ContentBase::setSpinnerLogo (std::unique_ptr <juce::Drawable> logo)
{
    waitingOverlay.setLogo (std::move (logo));
}

void ContentBase::setSpinnerLogoScale (const float scaleNormalized)
{
    waitingOverlay.setLogoScale (scaleNormalized);
}

void ContentBase::pageBecomingVisible ()
{
    updateCompanyLogo ();
}

void ContentBase::update ()
{
    updateCloseButtonVisibility ();
    updateBackButton ();

    errorDisplay.update ();
    resized ();
}

void ContentBase::updateCompanyLogo ()
{
    if (auto activationComp = findParentComponentOfClass<ActivationComponent> ())
    {
        companyLogo = activationComp->uiImpl.getCompanyLogo ();

        if (companyLogo != nullptr)
            addChildComponent (companyLogo.get ());
    }
    else jassertfalse;

    resized ();
}

void ContentBase::updateCloseButtonVisibility ()
{
    closeButton.setVisible (MB_IS_UNLOCKED_OBFUSCATED(api).first);
}

void ContentBase::updateBackButton ()
{
    const auto backButtonConfig = getBackButtonConfig ();
    backButton.setVisible (backButtonConfig.hasBackButton);
    backButton.onClick = backButtonConfig.callback;
}

void ContentBase::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    errorDisplay.errorText.setHighlightedRegion ({0,0});
}

void ContentBase::paint  (juce::Graphics& g)
{
    const auto titleAndSub = getTitleAndSubtitle ();
    const auto title = titleAndSub.first;
    const auto subtitle = titleAndSub.second;

    juce::Font f = Roboto::Regular ();
    g.setFont (f.withHeight (titleArea.getHeight ()));
    g.setColour ( juce::Colour (0xFFD0D0D0));
    g.drawText (title, titleArea,  juce::Justification::centred);

    g.setFont (f.withHeight (subtitleArea.getHeight ()));
    g.setColour ( juce::Colour (0xFF747474));
    g.drawText (subtitle, subtitleArea,  juce::Justification::centred);
}

void ContentBase::resized ()
{
    if (companyLogo != nullptr && companyLogo->getParentComponent () == this)
        companyLogo->setBounds (-1000,-1000,0,0); //hidden by default, overriding class can position it if needed

    const auto area = getLocalBounds ();
    const auto closeButtonSize = (int) (static_cast<float> (area.getHeight ()) * (0.075f));
    const auto closeButtonX = (int) (static_cast<float> (area.getWidth ()) * (0.9f));
    const auto closeButtonY = (int) (static_cast<float> (area.getHeight ()) * (0.055f));
    const juce::Rectangle<int> closeButtonArea (closeButtonX, closeButtonY, closeButtonSize, closeButtonSize);
    closeButton.setBounds (closeButtonArea);

    const auto backButtonSize = closeButtonSize;
    const auto backButtonX = (int) (static_cast<float> (area.getWidth ()) * (0.044f));
    const auto backButtonY = (int) (static_cast<float> (area.getHeight ()) * (0.055f));
    const  juce::Rectangle<int> backButtonArea (backButtonX, backButtonY, backButtonSize, backButtonSize);
    backButton.setBounds (backButtonArea);

    const auto errorDisplayWidth = (int) (static_cast<float> (area.getWidth ()) * (0.5f));
    const auto errorDisplayHeight = (int) (static_cast<float> (area.getHeight ()) * (0.1f));
    juce::Rectangle<int> errorDisplayArea (errorDisplayWidth, errorDisplayHeight);
    errorDisplayArea.setCentre (area.getCentreX (),
                                (int)(static_cast<float> (area.getHeight ()) * getErrorDisplayCentreYRelative ()));
    errorDisplay.setBounds (errorDisplayArea);


    {
        titleArea = juce::Rectangle<float> ((float)getWidth (), (float)static_cast<float> (getHeight ()) * (0.065f));
        titleArea.setCentre ((float)area.getCentreX (), static_cast<float> (area.getHeight ()) * (0.1f));
        subtitleArea = juce::Rectangle<float> ((float)getWidth (), (float)static_cast<float> (getHeight ()) * (0.035f));
        subtitleArea.setCentre ((float)area.getCentreX (), static_cast<float> (area.getHeight ()) * (0.16f));
    }

    {
        const auto nextButtonHeight = (int) (static_cast<float> (getHeight ()) * (0.136f));
        const auto nextButtonWidth = narrowTextButtonLAF.getWidthForHeight (nextButtonHeight);
        const auto nextButtonCentreX = (int) (static_cast<float> (getWidth ()) * (0.83f));
        const auto nextButtonCentreY = (int) (static_cast<float> (getHeight ()) * (0.89f));
        bottomRightSmallButtonArea = juce::Rectangle<int> (nextButtonWidth, nextButtonHeight);
        bottomRightSmallButtonArea.setCentre (nextButtonCentreX, nextButtonCentreY);
    }

    {

        const auto bottomCenterWideButtonHeight = (int) (static_cast<float> (getHeight ()) * (0.136f));
        const auto bottomCenterWideButtonWidth = wideTextButtonLAF.getWidthForHeight (bottomCenterWideButtonHeight);
        const auto bottomCenterWideButtonCentreY = (int) (static_cast<float> (getHeight ()) * (0.79f));
        bottomCenterWideButtonArea = juce::Rectangle<int>(bottomCenterWideButtonWidth, bottomCenterWideButtonHeight);
        bottomCenterWideButtonArea.setCentre (area.getCentreX(), bottomCenterWideButtonCentreY);
    }
}

#endif
