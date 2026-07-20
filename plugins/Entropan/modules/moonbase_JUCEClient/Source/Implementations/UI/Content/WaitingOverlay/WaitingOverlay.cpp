/*
  ==============================================================================

    WaitingOverlay.cpp
    Created: 22 Jun 2021 9:04:27pm
    Author:  Benedikt Sailer

  ==============================================================================
*/

#if INCLUDE_MOONBASE_UI

#include "WaitingOverlay.h"

using namespace Moonbase;
//==============================================================================
WaitingOverlay::WaitingOverlay()
{
    logo = juce::Drawable::createFromImageData (MoonbaseBinary::Logo_svg,
                                          MoonbaseBinary::Logo_svgSize);

    rings[0] = juce::Drawable::createFromImageData (MoonbaseBinary::Ring1_svg,
                                              MoonbaseBinary::Ring1_svgSize);
    rings[1] = juce::Drawable::createFromImageData (MoonbaseBinary::Ring2_svg,
                                              MoonbaseBinary::Ring2_svgSize);
    rings[2] = juce::Drawable::createFromImageData (MoonbaseBinary::Ring3_svg,
                                              MoonbaseBinary::Ring3_svgSize);
}

WaitingOverlay::~WaitingOverlay()
{
}

void WaitingOverlay::setLogoScale (const float scale)
{
    logoScale = scale;
}

void WaitingOverlay::show (const juce::String& msg, bool darkenBackground_)
{
    auto func = [&, msg, darkenBackground_]()
    {
        darkenBackground = darkenBackground_;
        message = msg;
        startTimerHz(fps);
        setVisible(true);
    };

    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::MessageManager::callAsync(func);
    }
    else
    {
        func();
    }

}

void WaitingOverlay::hide ()
{
    juce::WeakReference <WaitingOverlay> ref (this);
    juce::MessageManager::callAsync ([&, ref]()
    {
        if (ref.get ())
        {
            message = "";
            setVisible (false);
            stopTimer ();
        }
    });
}

void WaitingOverlay::timerCallback ()
{
    for (int i = 0; i < 3; i++)
    {
        currentInc[i] = currentInc[i] + incInc;
        if (currentInc[i] > incRanges[i].getEnd ())
            currentInc[i] = incRanges[i].getStart ();

        if (i == 0)
        {
            rotations[i] = rotations[i] - currentInc[i];

            if (rotations[i] < 0)
                rotations[i] = juce::MathConstants<float>::twoPi;
        }
        else
        {
            rotations[i] = rotations[i] + currentInc[i];

            if (rotations[i] > juce::MathConstants<float>::twoPi)
                rotations[i] = 0;
        }

    }

    repaint();
}

void WaitingOverlay::setBgColours (const  juce::Colour& layer1, const  juce::Colour& layer2)
{
    bgColours[0] = layer1;
    bgColours[1] = layer2;
}

void WaitingOverlay::setLogo (std::unique_ptr <juce::Drawable> l)
{
    logo = std::move (l);
}

void WaitingOverlay::paint (juce::Graphics& g)
{
    const auto area { getLocalBounds().toFloat() };

    if (darkenBackground)
    {
        g.fillAll(bgColours[0]);
        g.fillAll(bgColours[1]);
    }

    g.setColour (juce::Colours::white);

    const auto spinnerSize = (float)juce::jmin (110, juce::jmin (getWidth(), getHeight()) - 20);
    juce::Rectangle <float> r (spinnerSize, spinnerSize);
    r.setCentre (area.getCentre());

    if (logo != nullptr)
    {
        const auto logoSize = spinnerSize * logoScale;
        const auto logoArea = r.withSizeKeepingCentre (logoSize, logoSize);
        logo->drawWithin (g, logoArea, juce::RectanglePlacement::centred, 1.0f);
    }

    for (int i = 0; i < 3; i++)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform().rotated (rotations[i],
                                                   area.getCentreX(),
                                                   area.getCentreY()));
        rings[i]->drawWithin (g, r, juce::RectanglePlacement::centred, 1.0f);
    }
    
    const auto fontHeight = static_cast<float> (getHeight ()) * (0.08f);
    #if JUCE_VERSION >= 0x080002
        g.setFont ( juce::Font (juce::FontOptions(fontHeight)));
    #else
        g.setFont ( juce::Font (juce::Font(fontHeight)));
    #endif

    g.drawText (message,
                getLocalBounds().toFloat ().withY(juce::jmin (r.getBottom () + fontHeight,
                                                               static_cast<float> (getHeight ()) - fontHeight)),
                 juce::Justification::centredTop);
}

void WaitingOverlay::resized()
{

}

#endif
