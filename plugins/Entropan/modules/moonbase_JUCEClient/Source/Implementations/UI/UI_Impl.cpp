#if INCLUDE_MOONBASE_UI
#include "UI_Impl.h"
#include "../../SupressMSVCWarnings.h"

UI_Impl::UI_Impl (API& api_,  juce::Component& parent_)
:
api (api_),
parent (parent_)
{
    setAlwaysOnTop (true);
    updateVisibility ();

    activationComponent = std::make_unique<ActivationComponent> (api, *this);
    addAndMakeVisible (*activationComponent);
}

void UI_Impl::show (const bool shouldShow)
{
    shouldBeVisible = shouldShow;
    updateVisibility ();
}

void UI_Impl::setCompanyLogo (std::unique_ptr< juce::Component> logo)
{
    companyLogo = std::move (logo);
}

 juce::Component* UI_Impl::getCompanyLogo ()
{
    return companyLogo.get ();
}

void UI_Impl::setCompanyLogoScale (const float scale)
{
    logoScale = scale;
    resized ();
}


float UI_Impl::getCompanyLogoScale () const
{
    return logoScale;
}

void UI_Impl::setWelcomeButtonTextScale (const float scaleNormalized)
{
    welcomeButtonTextScale = scaleNormalized;
    resized ();
}

float UI_Impl::getWelcomeButtonTextScale () const
{
    return welcomeButtonTextScale;
}

void UI_Impl::paint  (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.79f));

    g.setFont (Roboto::Regular ().withHeight (12.f));
    g.setColour (juce::Colours::grey.withAlpha (0.5f));
    g.drawText ("v" + api.getProductVersion (), getLocalBounds ().reduced (2),  juce::Justification::bottomLeft);
}

void UI_Impl::setBoundsWrapping (const  juce::Rectangle<int>& b)
{
    setBounds (b);
    resized ();
}

void UI_Impl::setMaxWidth (const float w)
{
    maxWidth = w;
    resized ();
}

void UI_Impl::resized ()
{
    if (activationComponent != nullptr)
    {
        const auto area = getLocalBounds ();
        auto activationWidth = (int)(juce::jmin (static_cast<float> (getWidth ()) * 0.9f, maxWidth));
        auto activationHeight = activationComponent->getHeightForWidth (activationWidth);

        const auto maxHeight = (int)(static_cast<float> (getHeight ()) * 0.9f);
        if (activationHeight > maxHeight)
        {
            activationHeight = maxHeight;
            activationWidth = activationComponent->getWidthForHeight (activationHeight);
        }

         juce::Rectangle<int> activationArea (activationWidth, activationHeight);
        activationArea.setCentre (area.getCentre ());
        activationComponent->setBounds (activationArea);
        activationComponent->resized ();
    }
}

void UI_Impl::updateVisibility ()
{
    juce::WeakReference<UI_Impl> weakThis (this);
    juce::MessageManager::callAsync ([&, weakThis]()
    {
        if (weakThis == nullptr) return;

        const auto unlocked = MB_IS_UNLOCKED_OBFUSCATED(api);
        const auto hasToBeVisible = ! (bool) unlocked.first;
        if (shouldBeVisible || hasToBeVisible)
        {
            parent.addAndMakeVisible (this);
            toFront (true);
            setAlwaysOnTop (true);
            parent.resized ();
        }
        else if (!hasToBeVisible && !shouldBeVisible)
        {
            parent.removeChildComponent (this);
        }
        else jassertfalse;

        const bool isActivated = (bool) unlocked.first;
        const bool isTrial = (bool) api.isTrial ();

        if (activationComponent != nullptr)
        {
            juce::MessageManager::callAsync ([weakThis, isActivated, isTrial, unlocked]()
            {
                if (weakThis == nullptr || weakThis->activationComponent == nullptr) return;

                if (isActivated && !isTrial)
                {
                    weakThis->activationComponent->contentHolder.setNextPage (ContentHolder::Page::Activated, false, true);
                }
                else if (isTrial && weakThis->activationComponent->contentHolder.getCurrentPage () != ContentHolder::AutoActivation)
                {
                    weakThis->activationComponent->contentHolder.setNextPage (ContentHolder::Page::Welcome, false, true);
                    const auto expiration = weakThis->api.getLicenseExpiration ();

                    juce::String validUntilStr = "Trial valid until " + expiration.toString (true, true);

                    if (expiration == juce::Time ())
                        validUntilStr = "Trial expired!";

                    if (weakThis->api.getLastError () != validUntilStr)
                        weakThis->api.setError (validUntilStr);
                }
                else if (!isActivated && weakThis->activationComponent->contentHolder.getCurrentPage () == ContentHolder::Activated)
                {
                    weakThis->activationComponent->contentHolder.setNextPage (ContentHolder::Page::Welcome, false, true);
                    weakThis->api.setError (unlocked.second);
                }


                if (auto editor = weakThis->findParentComponentOfClass<juce::AudioProcessorEditor> ())
                {
                    if (auto resizeCorner = editor->resizableCorner.get ())
                        resizeCorner->toFront (false);
                }

                weakThis->activationComponent->update ();

                weakThis->parent.resized ();
                weakThis->repaint ();

                weakThis->callVisibilityChangedListenersIfNecessary ();
            });
        }
        callVisibilityChangedListenersIfNecessary ();
        
    });

}

namespace Moonbase::JUCEClient 
{
    struct ComparableVisibility : public ActivationUI::Visibility
    {
        ComparableVisibility (const ActivationUI::Visibility& visibility) {
            isVisible = visibility.isVisible;
            mustBeVisible = visibility.mustBeVisible;
        }

        bool operator== (const Visibility& other) const {
            return isVisible == other.isVisible && mustBeVisible == other.mustBeVisible;
        }
        bool operator!= (const Visibility& other) const {
            return !(*this == other);
        }
    };
}

void UI_Impl::callVisibilityChangedListenersIfNecessary ()
{
    const ComparableVisibility visibility = getVisibility ();
    
    bool shouldCall = !initialVisibilityListenerCalled;
        
    if (!shouldCall)    
        shouldCall = visibility != ComparableVisibility (lastVisibility);

    if (shouldCall)
    {
        listeners.call ([&](ActivationUI::Listener& l) { l.onActivationUiVisibilityChanged (visibility); });
        lastVisibility = visibility;

        if (!initialVisibilityListenerCalled)
            initialVisibilityListenerCalled = true;
    }
}

void UI_Impl::mouseUp (const juce::MouseEvent& e)
{
    if (activationComponent != nullptr && activationComponent->getBounds ().contains (e.getPosition ())) return;
    show (false);
}

void UI_Impl::setWelcomePageText (const juce::String& line1, const juce::String& line2)
{
    welcomePageText = std::make_pair (line1, line2);
}

std::pair<juce::String, juce::String> UI_Impl::getWelcomePageText () const
{
    return welcomePageText;
}

void UI_Impl::setSpinnerLogo (std::unique_ptr<juce::Drawable> logo)
{
    jassert (activationComponent != nullptr);
    if (activationComponent != nullptr)
        activationComponent->setSpinnerLogo (std::move (logo));
}

void UI_Impl::setSpinnerLogoScale (const float scaleNormalized)
{
    jassert (activationComponent != nullptr);
    if (activationComponent != nullptr)
        activationComponent->setSpinnerLogoScale (scaleNormalized);
}

const ActivationUI::Visibility UI_Impl::getVisibility () const
{
    return { Component::isVisible (), ! (bool) MB_IS_UNLOCKED_OBFUSCATED(api).first };
}

void UI_Impl::addListener (ActivationUI::Listener* listener)
{
    listeners.add (listener);
}

void UI_Impl::removeListener (ActivationUI::Listener* listener)
{
    listeners.remove (listener);
}

void UI_Impl::enableUpdateBadge (const UpdateBadge::Options& options)
{
    updateBadge = api.createUpdateBadgeComponent (parent, options);
}

#endif
