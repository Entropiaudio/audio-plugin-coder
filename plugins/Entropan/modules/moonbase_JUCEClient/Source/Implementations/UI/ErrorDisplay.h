#if INCLUDE_MOONBASE_UI
#pragma once

#include "../../JuceIncludes.h"


namespace Moonbase
{
namespace JUCEClient
{

  //==============================================================================
    class ErrorDisplay : public  juce::Component
    {
    public:
        ErrorDisplay (API& api_);
        void update ();

    private:
        friend class ContentBase;
        API& api;
        std::unique_ptr <juce::Drawable> warningIcon;
        juce::TextEditor errorText;

         juce::Rectangle<float> warningIconArea;

        void paint  (juce::Graphics& g) override;
        void resized () override;

        // makes error text actually scrollable - mediates a juce bug
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ErrorDisplay)
    };


} // namespace JUCEClient
} // namespace Moonbase

#endif
