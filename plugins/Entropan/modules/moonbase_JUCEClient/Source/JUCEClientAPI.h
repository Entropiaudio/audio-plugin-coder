#pragma once
#include "JuceIncludes.h"

#include "Macros.h"

#include "Config.h"

#include "Implementations/UI/UpdateBadge.h"

namespace Moonbase
{

class HttpHelper;

namespace JUCEClient
{
    #if INCLUDE_MOONBASE_UI
        struct ActivationUI;
    #endif
    struct API
    {
        API (const APIInitializer& initializer);
        ~API ();

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

        /**
            * isUnlocked ()
            * Returns a pair of <isUnlocked, errorMessage>.
            * 
            * For additional obfuscation, don't call the isUnlocked () method directly, but use the MB_IS_UNLOCKED_OBFUSCATED macro instead (same return type).
        */
        const std::pair<juce::var, juce::String/*error*/> isUnlocked () const;



        const juce::var isTrial    () const;
        const juce::var isOfflineActivated () const;

        void prepareToPlay (const double sampleRate, const int samplesPerBlock);
        void processBlock (juce::AudioBuffer<float>& buffer);

        void setError (const juce::String& error); //this is good for showing a non-default error in the designated area of the default UI
        const juce::String getLastError () const;

        // the grace period is the time in days after online verification has failed to complete (e.g. no internet connection) before the license is considered invalid
        // this is only relevant for online-activated licenses, default is 14 days
        void setGracePeriodDays (const int days);
        int getGracePeriodDays () const;

        
        void setTransmitAnalytics (const bool transmit, const bool extendedDefaultAnalytics);
        bool transmitsAnalytics () const;
        bool includesExtendedDefaultAnalytics () const;
        
        void registerGetAnalyticsCallback (const GetAnalyticsCallback& callback);

        void addActivationStateChangedCallback (const ActivationStateChangedCallback& callback);

        void requestAutoActivation (const AutoActivationRequestCallback& callback);
        void startPollingAutoActivationResult (const AutoActivationPollCallback& callback);
        void stopPollingAutoActivationResult ();

        void requestOnlineActivation (const juce::String& email, const juce::String& pw);
        void generateMachineFile     (const MachineFileCallback& callback);

        bool loadOfflineLicenseFile  (const juce::File& file); //returns true if the file was loaded successfully

        void deactivateLicense (const DeactivationCallback& callback);

        bool isUpdateAvailable () const;

        juce::Time getLicenseExpiration () const;
        juce::Time getLastOnlineVerification () const;

        void setOnlineVerificationGracePeriodDays (const int days);
        int getOnlineVerificationGracePeriodDays () const;

        #if INCLUDE_MOONBASE_UI
            /*You have to take ownership of the UI object, if you use this call to create the default UI*/
            ActivationUI*  createActivationUi  ( juce::Component& parent);
            ActivationUI* getActivationUi ();
            void showActivationUi ();
            void hideActivationUi ();
            
            // you can either use this to create and manage your own update badge component, 
            // or if you use the moonbase ActivationUI you can just call ActivationUI::enableUpdateBadge()
            std::unique_ptr<UpdateBadge> createUpdateBadgeComponent (juce::Component& parent, const UpdateBadge::Options& options = {});
        #endif



        MOONBASE_DECLARE_IMPLEMENTATION
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (API)
    };

} // namespace JUCEClient
} // namespace Moonbase
