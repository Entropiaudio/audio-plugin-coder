#if INCLUDE_MOONBASE_UI
#pragma once
#include "../../JuceIncludes.h"
#include "../../../Assets/MoonbaseBinary.h"


#define MB_LOAD_SVG(BINARYNAME) \
    juce::Drawable::createFromImageData (BINARYNAME, BINARYNAME##Size)

namespace Moonbase
{
namespace JUCEClient
{
    class MBDrawableButtonLAF : public juce::LookAndFeel_V4
    {
    public:
        MBDrawableButtonLAF (std::unique_ptr <juce::Drawable> offDisabled_ = nullptr,
                            std::unique_ptr <juce::Drawable> offNormal_= nullptr,
                            std::unique_ptr <juce::Drawable> offHover_= nullptr,
                            std::unique_ptr <juce::Drawable> offClick_= nullptr,
                            std::unique_ptr <juce::Drawable> onDisabled_= nullptr,
                            std::unique_ptr <juce::Drawable> onNormal_= nullptr,
                            std::unique_ptr <juce::Drawable> onHover_= nullptr,
                            std::unique_ptr <juce::Drawable> onClick_= nullptr);

        void setDisabledAlpha (const float alpha);
        void setRectanglePlacement (const juce::RectanglePlacement& r);
        float getRatioWoH ();
        float getRatioHoW ();

        void setImage (int index, std::unique_ptr <juce::Drawable> img);

        void drawToggleButton  (juce::Graphics&, juce::ToggleButton&,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void setDrawFallbackRect (const bool b);

    protected:
        virtual void drawAdditionalStuff  (juce::Graphics&, juce::ToggleButton&, bool /*over*/, bool /*down*/,  juce::Rectangle<float> /*area*/) {}


        std::unique_ptr <juce::Drawable> offStates [4];
        std::unique_ptr <juce::Drawable> onStates [4];

        float disabledAlpha { 0.5f };
        juce::RectanglePlacement rectPlacement { juce::RectanglePlacement::stretchToFit };

        bool drawFallbackRect = true;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MBDrawableButtonLAF)
    };

    //==============================================================================
    struct CloseButtonLAF : public MBDrawableButtonLAF {
        CloseButtonLAF ();
    };

    //==============================================================================
    struct BackButtonLAF : public MBDrawableButtonLAF {
        BackButtonLAF ();
    };

    //==============================================================================
    struct WideTextButtonLAF : public MBDrawableButtonLAF {
        WideTextButtonLAF ();
        void drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area) override;
        int getWidthForHeight (const int height);
        void setRed ();

        float textHeightNormal = 0.37f;
    };

    //==============================================================================
    struct NarrowTextButtonLAF : public MBDrawableButtonLAF {
        NarrowTextButtonLAF();
        void drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area) override;
        int getWidthForHeight (const int height);

        float textHeightNormal = 0.37f;
    };

    //==============================================================================
    struct SolidTextButtonLAF : public MBDrawableButtonLAF {
        SolidTextButtonLAF();
        void drawAdditionalStuff  (juce::Graphics&g, juce::ToggleButton& b, bool over, bool down,  juce::Rectangle<float> area) override;
        int getWidthForHeight (const int height);

        float textHeightNormal = 0.49f;
    };

    //==============================================================================
    // struct ActivationMethodOptionLAF : public LookAndFeel_V4{
    //     ActivationMethodOptionLAF ();
    //     void drawToggleButton  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override;
    // private:
    //     std::unique_ptr <juce::Drawable> buttonIcon [2][3];
    // };

    struct RadioButtonLAF : public juce::LookAndFeel_V4 {
        RadioButtonLAF ();
        void drawToggleButton  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override;
    private:
        std::unique_ptr <juce::Drawable> buttonIcon [2][3];
    };
    //==============================================================================
    struct TextEditorLAF : public juce::LookAndFeel_V4 {
        TextEditorLAF ();
        void drawTextEditorOutline  (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override;
        int getWidthForHeight (int height);
    private:
        std::unique_ptr <juce::Drawable> textEditorOutline;
    };

    //==============================================================================
    struct TextLinkLAF : public juce::LookAndFeel_V4 {
        TextLinkLAF ();
        void setUnderlined (const bool state);
        void setColours (const  juce::Colour& normal, const  juce::Colour& over);
        void drawToggleButton  (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override;

    private:
        bool underlined = false;
         juce::Colour colours [2];

    };


} // namespace JUCEClient
} // namespace Moonbase
#endif
