#if INCLUDE_MOONBASE_UI
#pragma once 
#include "../../JuceIncludes.h"
#include "LAFs.h"


// The UpdateBadge is meant to be instantiated and positioned in your UI by using juce::Component::addChildComponent instead of the typical addAndMakeVisible
// Visibility is handled based on actual update availability
namespace Moonbase::JUCEClient
{

struct API;
class UpdateBadge : public juce::Component,
                    private juce::Timer,
                    private juce::ComponentListener
{
public:
    struct Options {
        const float fadeTimeSeconds { 3.0f }; // total time for the badge to fade out
        juce::Justification justification { juce::Justification::topRight }; //supports topLeft, topRight, bottomLeft, bottomRight
        juce::BorderSize<int> inset { 5 };
        static Options defaultOptions() { return {}; }
    };

    UpdateBadge  (API& api_, juce::Component& parent, const Options& options = Options::defaultOptions());
    ~UpdateBadge () override;
    
    void showBadgeIfUpdateIsAvailable (bool force = false); // force will ignore the do-not-show-again file

private:
    API& api;
    juce::WeakReference<juce::Component> parent;
    const Options options;
    float alphaDecrement = 1.0f; // used to control the alpha decrement per timer callback

    bool badgeWasShown = false; 
    bool badgeIsFading = false;
    
    juce::ToggleButton updateButton { "Update Now" };
    SolidTextButtonLAF updateButtonLAF;
    
    juce::ToggleButton doNotShowAgainButton { "Do not show again" };
    TextLinkLAF doNotShowAgainButtonLAF;

    std::unique_ptr<juce::Drawable> updateIcon;
    juce::Rectangle<float> updateIconArea;
    
    juce::Rectangle<float> updateTitleArea;
    juce::Rectangle<float> updateTextArea;

    juce::ToggleButton closeButton { "Close" };
    CloseButtonLAF closeButtonLAF;

    void openDownloadUrl ();
    
    void writeDoNotShowAgainFile ();
    bool doNotShowAgain () const; //returns false if the badge should be shown
    juce::File doNotShowAgainFile;

    void closeBadge ();

    void paint   (juce::Graphics& g) override;
    void resized () override;
    void componentMovedOrResized (Component& component,
                                          bool wasMoved,
                                          bool wasResized) override;
    
    void timerCallback () override;

    void mouseEnter (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& e) override;
    
    juce::Font roboto;

    JUCE_DECLARE_WEAK_REFERENCEABLE (UpdateBadge)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateBadge)
};

}

#endif //INCLUDE_MOONBASE_UI
