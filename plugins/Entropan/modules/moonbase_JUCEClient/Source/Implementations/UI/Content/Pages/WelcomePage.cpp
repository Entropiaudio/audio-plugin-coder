#if INCLUDE_MOONBASE_UI

#include "WelcomePage.h"

#include "../ContentHolder.h"
#include "../../ActivationComponent.h"
#include "../../UI_Impl.h"
//==============================================================================
//==============================================================================
WelcomePage::WelcomePage (ContentHolder& parent, API& api_)
:
ContentBase (parent, api_),
activatePluginButton (
    #if MOONBASE_APP_MODE
        "Activate " + api_.getProductName ()
    #else
        "Activate your plugin"
    #endif
)
{
    addAndMakeVisible (activatePluginButton);
    activatePluginButton.setButtonText (activatePluginButton.getName ());
    activatePluginButton.setLookAndFeel (&wideTextButtonLAF);
    activatePluginButton.onClick = [&]()
    {
        contentHolder.setNextPage (ContentHolder::ActivationMethod);
    };

    initUiImplPtr ();
}

WelcomePage::~WelcomePage ()
{
    activatePluginButton.setLookAndFeel (nullptr);
}

void WelcomePage::initUiImplPtr ()
{
    if (auto activationComp = findParentComponentOfClass<ActivationComponent> ())
    {
        uiImpl = &activationComp->uiImpl;
        resized ();
    }
    else
    {
        juce::Timer::callAfterDelay (10, [weak = juce::WeakReference<WelcomePage> (this)]()
        {
            if (weak != nullptr)
                weak->initUiImplPtr ();
        });
    }
}

float WelcomePage::getErrorDisplayCentreYRelative ()
{
    return (0.35f);
}

void WelcomePage::paint  (juce::Graphics& g)
{
    ContentBase::paint (g);

    if (auto activationUi = api.getActivationUi ())
    {
        const auto area = getLocalBounds ().toFloat ();

        const auto welcomePageText { activationUi->getWelcomePageText () };
        static const juce::String line1 { welcomePageText.first };
        static const juce::String line2 { welcomePageText.second };

        const auto line1Y = static_cast<float> (getHeight ()) * (0.47f);
        const auto lineSpacing = static_cast<float> (getHeight ()) * (0.06f);
        const auto line2Y = line1Y + lineSpacing;

        g.setFont (static_cast<float> (getHeight ()) * (0.05f));
        g.setColour ( juce::Colour(0xFFD0D0D0));

        g.drawText (line1, area.withCentre ({area.getCentreX(), line1Y}),  juce::Justification::centred);
        g.drawText (line2, area.withCentre ({area.getCentreX(), line2Y}),  juce::Justification::centred);
    }
}

void WelcomePage::resized ()
{
    ContentBase::resized ();

    const auto area = getLocalBounds ();

    activatePluginButton.setBounds (bottomCenterWideButtonArea);

    if (uiImpl != nullptr)
    {
        wideTextButtonLAF.textHeightNormal = uiImpl->getWelcomeButtonTextScale ();

        if (companyLogo != nullptr && companyLogo->getParentComponent () == this)
        {
            companyLogo->setVisible (true);
            float scale = uiImpl->getCompanyLogoScale ();

            const auto logoSize = (int)(static_cast<float> (getWidth ()) * (0.125f) * scale);
            const auto logoCentreY = (int)(static_cast<float> (getHeight ()) * (0.23f));
            juce::Rectangle<int> logoArea (logoSize, logoSize);
            logoArea.setCentre (area.getCentreX(), logoCentreY);
            companyLogo->setBounds (logoArea);
        }
    }
}
#endif
