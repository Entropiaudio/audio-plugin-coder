#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../../../JuceIncludes.h"
#include "../ContentBase.h"
namespace Moonbase
{
namespace JUCEClient
{
    //==============================================================================

    class LoginPage : public ContentBase
    {
    public:
        LoginPage (ContentHolder& parent, API& api_);
        ~LoginPage () override;

    private:
        BackButtonConfig getBackButtonConfig () override;
        float getErrorDisplayCentreYRelative () override;
        StringPair getTitleAndSubtitle () override;

        juce::TextEditor emailEditor { "E-Mail" };
        juce::TextEditor passwordEditor { "Password" };

        TextEditorLAF textEditorLAF;

         juce::ToggleButton forgotPasswordButton { "Forgot your password?" };
         juce::ToggleButton loginButton { "Login" };

        void loginButtonClicked ();
        void textEditorsChanged ();

        void paint  (juce::Graphics& g) override;
        void resized () override;

        void mouseUp (const juce::MouseEvent& e) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoginPage)
    };

} // namespace JUCEClient
} // namespace Moonbase

#endif
