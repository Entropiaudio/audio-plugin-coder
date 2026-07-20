#pragma once

#include "../../JuceIncludes.h"

#if defined(MB_CATCH2_TESTING)

namespace Moonbase
{
namespace JUCEClient
{

struct APIImpl;

class ValidationCoordinator
{
public:
    struct Decision
    {
        bool shouldStart = false;
        juce::String reason;
    };

    explicit ValidationCoordinator (const APIImpl& api);

    Decision tryReserveValidationSlot (juce::int64 successCooldownMinutes,
                                       juce::int64 inFlightWindowMs,
                                       juce::int64 minRetryCooldownSeconds = 15);

    void completeValidation (bool success);

    juce::String getIdentityHash () const;
    juce::File getStateFile () const;

private:
    struct State
    {
        juce::int64 lastAttemptMs = 0;
        juce::int64 lastSuccessMs = 0;
        juce::int64 inFlightUntilMs = 0;
    };

    static juce::File createStateFile (const APIImpl& api, const juce::String& identityHash);

    State readState () const;
    void writeState (const State& state) const;

    juce::String identityHash;
    juce::File stateFile;
    std::unique_ptr<juce::InterProcessLock> interProcessLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValidationCoordinator)
};

} // namespace JUCEClient
} // namespace Moonbase

#endif
