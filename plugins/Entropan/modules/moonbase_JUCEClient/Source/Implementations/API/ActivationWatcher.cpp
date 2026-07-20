#include "ActivationWatcher.h"

#include "../StaticMethods.h"

//==============================================================================
//==============================================================================
ActivationWatcher::ActivationWatcher (APIImpl& apiRef)
:
api (apiRef)
{
    juce::WeakReference<ActivationWatcher> weakThis (this);
    juce::MessageManager::callAsync ([weakThis]()
    {
        if (weakThis != nullptr)
            weakThis->update ();
    });
}

void ActivationWatcher::update ()
{
    const auto unlocked = MB_IS_UNLOCKED_OBFUSCATED(api);
    if ( ! unlocked.first && ! isTimerRunning () )
    {
        api.setError (unlocked.second);
        startTimerHz (1);
    }
    else if (unlocked.first)
    {
        stopTimer ();
        api.updateUiIfPresent ();
    }
}

void ActivationWatcher::timerCallback ()
{
    update ();
}
