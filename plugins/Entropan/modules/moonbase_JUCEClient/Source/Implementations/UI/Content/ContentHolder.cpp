#if INCLUDE_MOONBASE_UI
#include "ContentHolder.h"

//==============================================================================
//==============================================================================
ContentHolder::ContentHolder (API& api_)
:
api (api_)
{
    #define FS_INIT_CONTENT(page) content.add (new page (*this, api_))

    FS_INIT_CONTENT (WelcomePage);
    FS_INIT_CONTENT (ActivationMethodPage);

    FS_INIT_CONTENT (AutoActivationPage);

    FS_INIT_CONTENT (OfflineActivationPage1);
    FS_INIT_CONTENT (OfflineActivationPage2);
    FS_INIT_CONTENT (OfflineActivationPage3);

    FS_INIT_CONTENT (ActivatedPage);

    jassert (content.size () == NUMPAGES); //forgot to fill in the Pages enum for a new page?

    for (auto& c : content)
        addAndMakeVisible (c);

    juce::WeakReference<ContentHolder> weakThis (this);
    juce::MessageManager::callAsync ([weakThis]() { if (weakThis != nullptr) weakThis->setNextPage ((MB_IS_UNLOCKED_OBFUSCATED(weakThis->api).first && ! weakThis->api.isTrial ()) ? Activated : Welcome, true, false, true); });
}

void ContentHolder::update ()
{
    resized ();

    for (auto& c : content)
        c->update ();
}

void ContentHolder::setSpinnerLogo (std::unique_ptr <juce::Drawable> logo)
{
    for (auto& c : content)
    {
        if (logo != nullptr)
            c->setSpinnerLogo (logo->createCopy ());
        else c->setSpinnerLogo (nullptr);
    }
}

void ContentHolder::setSpinnerLogoScale (const float scaleNormalized)
{
    for (auto& c : content)
        c->setSpinnerLogoScale (scaleNormalized);
}

ContentBase& ContentHolder::getPage (const Page& page)
{
    jassert (page < NUMPAGES);
    jassert (page < content.size ());
    return *content[page];
}

void ContentHolder::setNextPage (const Page& page, const bool fromRight, const bool fromLeft, const bool force)
{
    if (page == currentPage && ! force)
        return;

    jassert (page < NUMPAGES);
    jassert (page < content.size ());

    if ( ! resizedOnce  && ! force)
        return;

    auto* curPage = &getPage (currentPage);
    auto* newPage = &getPage (page);

    jassert (curPage != nullptr && newPage != nullptr);

    const auto curPageNewBounds = curPage->getBounds ().withX (fromRight ? -getWidth () : fromLeft ? getWidth () : 0);
    const auto newPageStartBounds = curPage->getBounds ().withX (fromRight ? getWidth () : fromLeft ? -getWidth () : 0);
    const auto newPageEndBounds = curPage->getBounds ();
    newPage->setBounds (newPageStartBounds);

    newPage->pageBecomingVisible ();

    animator.animateComponent (curPage, curPageNewBounds, 0.0f, 150, false, 0.0f, 0.3f);
    animator.animateComponent (newPage, newPageEndBounds, 1.0f, 150, false, 0.0f, 0.3f);

    currentPage = page;
}

void ContentHolder::resized ()
{
    const auto area = getLocalBounds ();

    for (auto& page : content)
    {
        if (page != &getPage (currentPage))
        {
            page->setBounds (area.withX (-area.getWidth () * 2).withY(-area.getHeight () * 2));
            page->setAlpha (0.0f);
        }
        else
        {
            page->setBounds (area);
            page->setAlpha (1.0f);
        }
        page->resized ();
    }

    if (!resizedOnce)
        resizedOnce = true;
}
#endif
