#include "Implementations/API/Includes.h"
#include "Implementations/API/Includes.cpp"
//==============================================================================
//==============================================================================
API::API (const APIInitializer& initializer)
{
    const auto ctorRoutine = [&]() -> juce::var {
    OBF_BEGIN
        impl = new APIImpl (initializer, *this);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    ctorRoutine ();
}

API::~API ()
{
    const auto dtorRoutine = [&]() -> juce::var {
    OBF_BEGIN
        delete &GetImpl (*this);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    dtorRoutine();
}

const juce::String API::getProductId () const
{
OBF_BEGIN
    return GetImpl (*this).getProductId ();
OBF_END
    return {};
}

const juce::File API::getLicenseFile () const
{
OBF_BEGIN
    return GetImpl (*this).getLicenseFile ();
OBF_END
    return {};
}

const juce::URL API::getBaseUrl () const
{
OBF_BEGIN
    return GetImpl (*this).getBaseUrl ();
OBF_END
    return {};
}

const juce::URL API::getOfflineUrl () const
{
OBF_BEGIN
    return GetImpl (*this).getOfflineUrl ();
OBF_END
    return {};
}

const juce::String API::getPublicKey () const
{
OBF_BEGIN
    return GetImpl (*this).getPublicKey ();
OBF_END
    return {};
}

const juce::String API::getCompanyName () const
{
OBF_BEGIN
    return GetImpl (*this).getCompanyName ();
OBF_END
    return {};
}

const juce::String API::getProductName () const
{
OBF_BEGIN
    return GetImpl (*this).getProductName ();
OBF_END
    return {};
}

const juce::String API::getProductVersion () const
{
OBF_BEGIN
    return GetImpl (*this).getProductVersion ();
OBF_END
    return {};
}

const juce::String API::getUserEmail () const
{
OBF_BEGIN
    return GetImpl (*this).getUserEmail ();
OBF_END
    return {};
}

const juce::String API::getUserName () const
{
OBF_BEGIN
    return GetImpl (*this).getUserName ();
OBF_END
   return {};
}

const juce::String API::getUserId () const
{
OBF_BEGIN
    return GetImpl (*this).getUserId ();
OBF_END
    return {};
}

const juce::String API::getLicenseId () const
{
OBF_BEGIN
    return GetImpl (*this).getLicenseId ();
OBF_END
    return {};
}

const juce::String API::getCurrentReleaseVersion () const
{
OBF_BEGIN
    return GetImpl (*this).getCurrentReleaseVersion ();
OBF_END
    return {};
}

const juce::StringArray API::getOwnedSubProducts () const
{
OBF_BEGIN
    return GetImpl (*this).getOwnedSubProducts ();
OBF_END
    return {};
}

const juce::StringPairArray API::getProductProperties () const
{
OBF_BEGIN
    return GetImpl (*this).getProductProperties ();
OBF_END
    return {};
}

const juce::StringPairArray API::getCustomerProperties () const
{
OBF_BEGIN
    return GetImpl (*this).getCustomerProperties ();
OBF_END
    return {};
}

const juce::StringPairArray API::getLicenseProperties () const
{
OBF_BEGIN
    return GetImpl (*this).getLicenseProperties ();
OBF_END
    return {};
}

const juce::StringPairArray API::getTrialProperties () const
{
OBF_BEGIN
    return GetImpl (*this).getTrialProperties ();
OBF_END
    return {};
}

const std::pair<juce::var, juce::String/*error*/> API::isUnlocked () const
{
OBF_BEGIN
    const bool trueVal = true;
    const bool falseVal = false;
    const auto activationState = GetImpl (*this).isUnlocked ();
    const bool first = activationState.first;
    const auto second = activationState.second;
    IF (V(first) == V(falseVal) || V(first) == V(trueVal)) // only for obfuscations sake, we'll always return here
        const std::pair<juce::var, juce::String> returnPair {first, second};
        RETURN(returnPair);
    ENDIF

    const auto returnPair = std::pair<juce::var, juce::String> {juce::var(false), ""};
    RETURN(returnPair);
OBF_END 
    return { juce::var (false), "" };
}

const juce::var API::isTrial () const
{
OBF_BEGIN
    return GetImpl (*this).isTrial ();
OBF_END
    return juce::var(false);
}

const juce::var API::isOfflineActivated () const
{
OBF_BEGIN
    return GetImpl (*this).isOfflineActivated ();
OBF_END
    return juce::var(false);
}

void API::setError (const juce::String& error)
{
    const auto setErrorRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).setError (error);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setErrorRoutine();
}

const juce::String API::getLastError () const
{
OBF_BEGIN
    return GetImpl (*this).getLastError ();
OBF_END
    return {};
}

void API::setGracePeriodDays (const int days)
{
    const auto setGracePeriodDaysRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).setGracePeriodDays (days);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setGracePeriodDaysRoutine();
}

int API::getGracePeriodDays () const
{
OBF_BEGIN
    return GetImpl (*this).getGracePeriodDays ();
OBF_END
    return 0;
}

void API::setTransmitAnalytics (const bool transmit, const bool extended)
{
    const auto setTransmitAnalyticsRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).setTransmitAnalytics (transmit, extended);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setTransmitAnalyticsRoutine();
}

bool API::transmitsAnalytics () const
{
OBF_BEGIN
    return GetImpl (*this).transmitsAnalytics ();
OBF_END
    return false;
}

bool API::includesExtendedDefaultAnalytics () const
{
OBF_BEGIN
    return GetImpl (*this).includesExtendedDefaultAnalytics ();
OBF_END
    return false;
}

void API::registerGetAnalyticsCallback (const GetAnalyticsCallback& callback)
{
    const auto registerGetAnalyticsCallbackRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).registerGetAnalyticsCallback (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    registerGetAnalyticsCallbackRoutine();
}

#if INCLUDE_MOONBASE_UI
ActivationUI* API::createActivationUi ( juce::Component& parent)
{
OBF_BEGIN
    return GetImpl (*this).createActivationUi (parent);
OBF_END
    return nullptr;
}

ActivationUI* API::getActivationUi ()
{
OBF_BEGIN
    return GetImpl (*this).getActivationUi ();
OBF_END
    return nullptr;
}
#endif

void API::requestAutoActivation (const AutoActivationRequestCallback& callback)
{
    const auto requestAutoActivationRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).requestAutoActivation (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    requestAutoActivationRoutine();
}

void API::startPollingAutoActivationResult (const AutoActivationPollCallback& callback)
{
    const auto startPollingAutoActivationResultRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).startPollingAutoActivationResult (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    startPollingAutoActivationResultRoutine();
}

void API::stopPollingAutoActivationResult ()
{
    const auto stopPollingAutoActivationResultRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).stopPollingAutoActivationResult ();
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    stopPollingAutoActivationResultRoutine();
}

void API::requestOnlineActivation (const juce::String& email, const juce::String& password)
{
    const auto requestOnlineActivationRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).requestOnlineActivation (email, password);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    requestOnlineActivationRoutine();
}

void API::generateMachineFile (const MachineFileCallback& callback)
{
    const auto generateMachineFileRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).generateMachineFile (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    generateMachineFileRoutine();
}

#if INCLUDE_MOONBASE_UI
void API::showActivationUi ()
{
    const auto showActivationUiRoutine = [&]() -> juce::var {
    OBF_BEGIN
        if (auto ui = GetImpl (*this).currentUi.get ())
            ui->show ();
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    showActivationUiRoutine();
}

void API::hideActivationUi ()
{
    const auto hideActivationUiRoutine = [&]() -> juce::var {
    OBF_BEGIN
        if (auto ui = GetImpl (*this).currentUi.get ())
            ui->hide ();
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    hideActivationUiRoutine();
}

std::unique_ptr<UpdateBadge> API::createUpdateBadgeComponent (juce::Component& parent, const UpdateBadge::Options& options)
{
    return GetImpl (*this).createUpdateBadgeComponent (parent, options);
}

#endif


bool API::loadOfflineLicenseFile  (const juce::File& file)
{
OBF_BEGIN
    return GetImpl (*this).loadOfflineLicenseFile (file);
OBF_END
    return false;
}

void API::deactivateLicense (const DeactivationCallback& callback)
{
    const auto deactivateLicenseRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).deactivateLicense (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    deactivateLicenseRoutine();
}

juce::Time API::getLicenseExpiration () const
{
OBF_BEGIN
    return GetImpl (*this).getLicenseExpiration ();
OBF_END
    return {};
}

juce::Time API::getLastOnlineVerification () const
{
OBF_BEGIN
    return GetImpl (*this).getLastOnlineVerification ();
OBF_END
    return {};
}

void API::setOnlineVerificationGracePeriodDays (const int days)
{
    const auto setOnlineVerificationGracePeriodDaysRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).setOnlineVerificationGracePeriodDays (days);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    setOnlineVerificationGracePeriodDaysRoutine();
}

int API::getOnlineVerificationGracePeriodDays () const
{
OBF_BEGIN
    return GetImpl (*this).getOnlineVerificationGracePeriodDays ();
OBF_END
    return 0;
}

void API::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    GetImpl (*this).prepareToPlay (sampleRate, samplesPerBlock);
}

void API::processBlock (juce::AudioBuffer<float>& buffer)
{
    GetImpl (*this).processBlock (buffer);
}

void API::addActivationStateChangedCallback (const ActivationStateChangedCallback& callback)
{
    const auto addActivationStateChangedCallbackRoutine = [&]() -> juce::var {
    OBF_BEGIN
        GetImpl (*this).addActivationStateChangedCallback (callback);
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    addActivationStateChangedCallbackRoutine();
}

bool API::isUpdateAvailable () const
{
OBF_BEGIN
    return GetImpl (*this).isUpdateAvailable ();
OBF_END
    return false;
}
