#include "ValidationCoordinator.h"

#if defined(MB_CATCH2_TESTING)

#include "APIImpl.h"

namespace Moonbase
{
namespace JUCEClient
{

namespace
{
    constexpr int validationCoordinatorLockTimeoutMs = 1000;
    constexpr juce::int64 validationCoordinatorMsPerSecond = 1000;
    constexpr juce::int64 validationCoordinatorMsPerMinute = 60 * validationCoordinatorMsPerSecond;
    constexpr int validationCoordinatorStateSchemaVersion = 1;

    struct ScopedInterProcessUnlock
    {
        explicit ScopedInterProcessUnlock (juce::InterProcessLock& lockToRelease)
            : lock (lockToRelease)
        {
        }

        ~ScopedInterProcessUnlock ()
        {
            lock.exit ();
        }

        juce::InterProcessLock& lock;
    };

    static juce::String createIdentityHash (const APIImpl& api)
    {
        juce::String fingerprint;
        fingerprint << api.getCompanyName () << "\n"
                    << api.getProductName () << "\n"
                    << api.getProductVersion () << "\n"
                    << api.getProductId () << "\n"
                    << api.getLicenseFile ().getFullPathName ();

        return juce::SHA256 (fingerprint.toUTF8 ()).toHexString ();
    }

    static juce::int64 coordinatorToInt64 (const juce::var& value)
    {
        return value.toString().getLargeIntValue();
    }

}

ValidationCoordinator::ValidationCoordinator (const APIImpl& api)
    : identityHash (createIdentityHash (api)),
      stateFile (createStateFile (api, identityHash)),
      interProcessLock (std::make_unique<juce::InterProcessLock> ("moonbase_validation_" + identityHash))
{
}

ValidationCoordinator::Decision ValidationCoordinator::tryReserveValidationSlot (juce::int64 successCooldownMinutes,
                                                                                  juce::int64 inFlightWindowMs,
                                                                                  juce::int64 minRetryCooldownSeconds)
{
    Decision decision;

    if (interProcessLock == nullptr || ! interProcessLock->enter (validationCoordinatorLockTimeoutMs))
    {
        decision.reason = "lock-unavailable";
        return decision;
    }

    ScopedInterProcessUnlock scopedUnlock (*interProcessLock);

    auto state = readState ();
    const auto nowMs = juce::Time::getCurrentTime ().toMilliseconds ();

    if (state.inFlightUntilMs > nowMs)
    {
        decision.reason = "in-flight";
        return decision;
    }

    const auto successCooldownMs = juce::jmax<juce::int64> (0, successCooldownMinutes) * validationCoordinatorMsPerMinute;
    if (successCooldownMs > 0 && state.lastSuccessMs > 0 && nowMs - state.lastSuccessMs < successCooldownMs)
    {
        decision.reason = "recent-success";
        return decision;
    }

    const auto retryCooldownMs = juce::jmax<juce::int64> (0, minRetryCooldownSeconds) * validationCoordinatorMsPerSecond;
    if (retryCooldownMs > 0 && state.lastAttemptMs > 0 && nowMs - state.lastAttemptMs < retryCooldownMs)
    {
        decision.reason = "recent-attempt";
        return decision;
    }

    state.lastAttemptMs = nowMs;
    state.inFlightUntilMs = nowMs + juce::jmax<juce::int64> (1000, inFlightWindowMs);
    writeState (state);

    decision.shouldStart = true;
    return decision;
}

void ValidationCoordinator::completeValidation (bool success)
{
    if (interProcessLock == nullptr || ! interProcessLock->enter (validationCoordinatorLockTimeoutMs))
        return;

    ScopedInterProcessUnlock scopedUnlock (*interProcessLock);

    auto state = readState ();
    const auto nowMs = juce::Time::getCurrentTime ().toMilliseconds ();

    state.inFlightUntilMs = 0;
    if (success)
        state.lastSuccessMs = nowMs;

    writeState (state);
}

juce::String ValidationCoordinator::getIdentityHash () const
{
    return identityHash;
}

juce::File ValidationCoordinator::getStateFile () const
{
    return stateFile;
}

juce::File ValidationCoordinator::createStateFile (const APIImpl& api, const juce::String& key)
{
    const auto keyPrefix = key.substring (0, 20);
    return api.getLicenseFile ().getSiblingFile (".mb_validation_" + keyPrefix + ".json");
}

ValidationCoordinator::State ValidationCoordinator::readState () const
{
    State state;

    if (! stateFile.existsAsFile ())
        return state;

    const auto content = stateFile.loadFileAsString ();
    if (content.isEmpty ())
        return state;

    const auto parsed = juce::JSON::parse (content);
    if (auto* object = parsed.getDynamicObject ())
    {
        const auto schema = object->getProperty ("schema").toString ().getIntValue ();
        if (schema != validationCoordinatorStateSchemaVersion)
            return state;

        state.lastAttemptMs = coordinatorToInt64 (object->getProperty ("lastAttemptMs"));
        state.lastSuccessMs = coordinatorToInt64 (object->getProperty ("lastSuccessMs"));
        state.inFlightUntilMs = coordinatorToInt64 (object->getProperty ("inFlightUntilMs"));
    }

    return state;
}

void ValidationCoordinator::writeState (const State& state) const
{
    auto parent = stateFile.getParentDirectory ();
    if (! parent.exists ())
        parent.createDirectory ();

    juce::DynamicObject::Ptr object (new juce::DynamicObject ());
    object->setProperty ("schema", validationCoordinatorStateSchemaVersion);
    object->setProperty ("lastAttemptMs", state.lastAttemptMs);
    object->setProperty ("lastSuccessMs", state.lastSuccessMs);
    object->setProperty ("inFlightUntilMs", state.inFlightUntilMs);

    stateFile.replaceWithText (juce::JSON::toString (juce::var (object.get ())));
}

} // namespace JUCEClient
} // namespace Moonbase

#endif
