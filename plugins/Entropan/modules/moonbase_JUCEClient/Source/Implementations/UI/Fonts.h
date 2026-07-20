#if INCLUDE_MOONBASE_UI
#pragma once

#include "../../JuceIncludes.h"
#include "../../../Assets/MoonbaseBinary.h"


namespace Roboto
{
    static inline  juce::Font& Regular ()
    {
        #if JUCE_VERSION >= 0x080002

        static  juce::Font f ( juce::FontOptions (juce::Typeface::createSystemTypefaceFor (
           MoonbaseBinary::RobotoRegular_ttf, MoonbaseBinary::RobotoRegular_ttfSize))
           .withHorizontalScale (1.0f)
           .withKerningFactor (0.05f));

        #else

        static  juce::Font f ( juce::Font (juce::Typeface::createSystemTypefaceFor (
           MoonbaseBinary::RobotoRegular_ttf, MoonbaseBinary::RobotoRegular_ttfSize))
           .withHorizontalScale (1.0f)
           .withExtraKerningFactor (0.05f));
           
        #endif

        return f;
    }
} // namespace Roboto
#endif
