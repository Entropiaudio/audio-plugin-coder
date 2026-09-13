#pragma once

#if INCLUDE_MOONBASE_UI

#include "JuceIncludes.h"
#include "Macros.h"
#include "WebUIHelpers.h"

#include "Implementations/UI/UpdateBadge.h"

namespace Moonbase
{

namespace JUCEClient
{

struct API;
struct ActivationUI
{
    ActivationUI (API&,  juce::Component& parent);
    ~ActivationUI ();
    
    API& getAPI () const;
    
    struct Visibility 
    {
        bool isVisible = false;
        bool mustBeVisible = false;
    };
    
    void update ();
    void show ();
    void hide ();

    void enableUpdateBadge (const UpdateBadge::Options& options = {});

    const Visibility getVisibility () const;

    void setBounds (const juce::Rectangle<int>& bounds);

    void setCompanyLogo (std::unique_ptr<juce::Component> logo);
    juce::Component* getCompanyLogo ();

    void setCompanyLogoScale (const float scale);

    void setWelcomePageText (const juce::String& line1, const juce::String& line2);
    std::pair<juce::String, juce::String> getWelcomePageText () const;

    // setWelcomeButtonTextScale is rarely needed to make very long app names fit on the button
    // default scale is 0.37 (37% height of button asset)
    void setWelcomeButtonTextScale (const float scaleNormalized);

    void setSpinnerLogo (std::unique_ptr<juce::Drawable> logo);
    void setSpinnerLogoScale (const float scaleNormalized);
    
    void setMaxWidth (const float maxWidth);

    struct Listener {
        virtual ~Listener () = default;
        virtual void onActivationUiVisibilityChanged (const Visibility& /*visibility*/) {}
    };

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    MOONBASE_DECLARE_IMPLEMENTATION
    JUCE_DECLARE_WEAK_REFERENCEABLE (ActivationUI)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationUI)
};

} // namespace JUCEClient
} // namespace Moonbase
#endif
