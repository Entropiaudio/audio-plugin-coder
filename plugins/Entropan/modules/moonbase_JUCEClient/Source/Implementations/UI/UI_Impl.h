#if INCLUDE_MOONBASE_UI

#pragma once

#include "../../JuceIncludes.h"
#include "ActivationComponent.h"

#include "UpdateBadge.h"


namespace Moonbase
{
namespace JUCEClient
{
    struct UI_Impl : public  juce::Component
    {
        UI_Impl (API& api_,  juce::Component& parent_);

        void show (const bool shouldShow);

        API& api;
        juce::Component& parent;
        std::unique_ptr<ActivationComponent> activationComponent;

        void setCompanyLogo (std::unique_ptr<juce::Component> logo);
        juce::Component* getCompanyLogo ();
        void setCompanyLogoScale (const float scale);
        float getCompanyLogoScale () const;

        void setWelcomePageText (const juce::String& line1, const juce::String& line2);
        std::pair<juce::String, juce::String> getWelcomePageText () const;
        void setWelcomeButtonTextScale (const float scaleNormalized);
        float getWelcomeButtonTextScale () const;

        void setSpinnerLogo (std::unique_ptr<juce::Drawable> logo);
        void setSpinnerLogoScale (const float scaleNormalized);

        void setMaxWidth (const float maxWidth);
        
        bool shouldBeVisible = false;
        const ActivationUI::Visibility getVisibility () const;

        void paint  (juce::Graphics& g) override;

        void resized () override;

        void updateVisibility ();

        void mouseUp (const juce::MouseEvent& e) override;

        void setBoundsWrapping (const  juce::Rectangle<int>& b);


        void addListener (ActivationUI::Listener* listener);
        void removeListener (ActivationUI::Listener* listener);

        void enableUpdateBadge (const UpdateBadge::Options& options);

    private:
        friend class ContentBase;
        std::unique_ptr< juce::Component> companyLogo;
        float logoScale = 1.f;
        float welcomeButtonTextScale = 0.37f;
        float maxWidth = 550.f;


        std::pair<juce::String, juce::String> welcomePageText {
            "Weightless",
            "Licensing"
        };

        juce::ListenerList<ActivationUI::Listener> listeners;
        void callVisibilityChangedListenersIfNecessary ();
        ActivationUI::Visibility lastVisibility;
        
        bool initialVisibilityListenerCalled = false;

        std::unique_ptr<UpdateBadge> updateBadge = nullptr;

        JUCE_DECLARE_WEAK_REFERENCEABLE (UI_Impl)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UI_Impl)
    };

} // namespace JUCEClient
} // namespace Moonbase

static inline UI_Impl& GetUiImpl (const ActivationUI& ui)
{
    return * static_cast <UI_Impl*> (ui.impl);
}

#endif
