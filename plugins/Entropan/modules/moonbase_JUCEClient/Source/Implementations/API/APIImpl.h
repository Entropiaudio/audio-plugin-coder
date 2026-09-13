#pragma once

#include "Includes.h"
#include "ActivationWatcher.h"
#include "AutoActivationImpl.h"
#include "OnlineValidator.h"
#include "PeriodicSignalDestroyer.h"

#if INCLUDE_MOONBASE_UI
    #include "../UI/Includes.h"
#endif

namespace Moonbase
{
namespace JUCEClient
{

struct API;
struct APIImpl;

//==============================================================================
//==============================================================================
struct APIImpl : private juce::AsyncUpdater
{
    APIImpl (const APIInitializer& initializer, API& api);
    ~APIImpl () override;
    operator API& () { return api; }

    const juce::String  getProductId   () const;
    const juce::File    getLicenseFile () const;
    const juce::URL     getBaseUrl     () const;
    const juce::URL     getOfflineUrl  () const;
    const juce::String  getPublicKey   () const;

    const juce::String  getCompanyName () const;
    const juce::String  getProductName () const;
    const juce::String  getProductVersion () const;

    const juce::String getUserEmail () const;
    const juce::String getUserName () const;
    const juce::String getUserId () const;
    
    const juce::String getLicenseId () const;

    const juce::String getCurrentReleaseVersion () const;
    const juce::StringArray getOwnedSubProducts () const;

    const juce::StringPairArray getProductProperties () const;
    const juce::StringPairArray getCustomerProperties () const;
    const juce::StringPairArray getLicenseProperties () const;
    const juce::StringPairArray getTrialProperties () const;

    const std::pair<juce::var, juce::String/*error*/> isUnlocked () const;
    const juce::var isTrial () const;
    const juce::var isOfflineActivated () const;

    void setError (const juce::String& error);
    const juce::String getLastError () const;

    void setGracePeriodDays (const int days);
    int getGracePeriodDays () const;

    void setTransmitAnalytics (const bool transmit, const bool includeExtendedDefaultAnalytics);
    bool transmitsAnalytics () const;
    bool includesExtendedDefaultAnalytics () const;

    void registerGetAnalyticsCallback (const GetAnalyticsCallback& callback);
    GetAnalyticsCallback getGetAnalyticsCallback () const;
    void requestAutoActivation (const AutoActivationRequestCallback& callback);
    void startPollingAutoActivationResult (const AutoActivationPollCallback& callback);
    void stopPollingAutoActivationResult ();

    void processAutoActivationRequestResponse (bool wasOk, juce::String response, HttpHelper* helper);


    void requestOnlineActivation (const juce::String& email, const juce::String& pw);
    void processOnlineActivationResponse (bool wasOk, juce::String response, HttpHelper* helper);


    void generateMachineFile (const MachineFileCallback& callback, juce::File saveLocation = {});
    bool loadOfflineLicenseFile  (const juce::File& file); //returns true if the file was loaded successfully

    void deactivateLicense (const DeactivationCallback& callback);
    void processDeactivationResponse (bool wasOk, juce::String response, HttpHelper* helper);

#if INCLUDE_MOONBASE_UI
    ActivationUI* createActivationUi ( juce::Component& parent);
    ActivationUI* getActivationUi ();
    juce::WeakReference<ActivationUI> currentUi;
    
    std::unique_ptr<UpdateBadge> createUpdateBadgeComponent (juce::Component& parent, const UpdateBadge::Options& options);
#endif
    

    void deleteLocalLicenseFile ();
    void replaceLocalLicenseFile (const juce::String& newLicense, bool doOnlineValidation = true);

    juce::Time getLicenseExpiration () const;
    juce::Time getLastOnlineVerification () const;

    void setOnlineVerificationGracePeriodDays (const int days);
    int getOnlineVerificationGracePeriodDays () const;

    void updateUiIfPresent ();

    void prepareToPlay (const double sampleRate, const int samplesPerBlock);
    void processBlock (juce::AudioBuffer<float>& buffer);

    void addActivationStateChangedCallback (const ActivationStateChangedCallback& callback);

    bool isUpdateAvailable () const;

    std::unique_ptr<OnlineValidator> onlineValidator;
    
    APIInitializer initializer;
    
    juce::String getMachineHashSalt () const;
    
private:
    friend class AutoActivationImpl;
    friend class OnlineValidator;
    
    API& api;

    std::unique_ptr<ActivationWatcher> activationWatcher;
    juce::String lastError;

    juce::OwnedArray<HttpHelper> httpHelpers;
    void removeHelper (HttpHelper* helper);
    void parseErrorResponse (const juce::String& response);

    static inline const juce::String jsonHeader { "Content-Type: application/json\nUser-Agent: MoonbaseJUCEClient" };
    static inline const juce::String plainTextHeader { "Content-Type: text/plain\nUser-Agent: MoonbaseJUCEClient" };

    std::unique_ptr<AutoActivationImpl> autoActivationProcessor;
    AutoActivationRequestCallback autoActivationRequestCallback;

#if INCLUDE_MOONBASE_UI
    std::unique_ptr<juce::FileChooser> machineFileChooser;
#endif

    int onlineGracePeriodDays = 14;
    
    bool transmitAnalytics = true;
    bool includeExtendedDefaultAnalytics = false;

    

    std::unique_ptr<PeriodicSignalDestroyer> signalDestroyer;

    GetAnalyticsCallback getAnalyticsCallback = nullptr;

    static inline const juce::String machineNameId {
            "deviceName"
    };
    static inline const juce::String machineSignatureId {
            "deviceSignature"
    };

    juce::Array<ActivationStateChangedCallback> activationStateChangedCallbacks;
    void triggerActivationStateChangedCallbacks ();
    void handleAsyncUpdate () override;

    void setCustomMachineHashSalt (const juce::String& salt);
    juce::String machineHashSalt = "moonsalt_1";

    JUCE_DECLARE_WEAK_REFERENCEABLE (APIImpl)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (APIImpl)
};

} // namespace JUCEClient
} // namespace Moonbase

static inline APIImpl& GetImpl (const API& api)
{
    return * static_cast <APIImpl*> (api.impl);
}
