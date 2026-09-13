#include "OnlineValidator.h"
#include "APIImpl.h"

OnlineValidator::OnlineValidator (APIImpl& api_)
:
api (api_),
revalidationInterval (api_.initializer.parallelOnlineValidationTimeout)
{
    const auto ctorRoutine = [&]() -> const juce::var {
        const bool unlocked = MB_IS_UNLOCKED_OBFUSCATED(api).first;

    OBF_BEGIN
        const bool offline = api.isOfflineActivated ();
        
        if (unlocked && !offline)
            delayedStart ();
    
        const auto returnVal = juce::var(false);
        RETURN(returnVal);

    OBF_END
        return juce::var(false);
    };
    ctorRoutine();
}

OnlineValidator::~OnlineValidator ()
{

}

#ifdef MB_CATCH2_TESTING
OnlineValidator::TestValidationResponseProvider& OnlineValidator::getTestValidationResponseProvider ()
{
    static TestValidationResponseProvider provider;
    return provider;
}

void OnlineValidator::setTestValidationResponseProvider (TestValidationResponseProvider provider)
{
    getTestValidationResponseProvider () = provider;
}
#endif

void OnlineValidator::startNow ()
{
    const auto reqRoutine = [&, weakThis = juce::WeakReference<OnlineValidator> (this)]() -> juce::var
    {
        if (weakThis == nullptr) return juce::var(false);

        const int unlocked = MB_IS_UNLOCKED_OBFUSCATED(api).first;

    OBF_BEGIN
        const int offline = api.isOfflineActivated ();
       
        if (!unlocked || offline)
        {
            //we get here when we offline activated
            return juce::var(false);
        }

#ifdef MB_CATCH2_TESTING
        auto& provider = getTestValidationResponseProvider ();
        if (provider != nullptr)
        {
            juce::String response;
            int statusCode = 500;

            if (provider (api, response, statusCode))
            {
                const bool wasOk = juce::Range<int> (200, 299).contains (statusCode);
                handleValidationResponse (wasOk, response, statusCode);
                return juce::var(false);
            }
        }
#endif

        juce::MessageManager::callAsync ([&, weakThis]()
        {
            if (weakThis == nullptr) return;

            DBG("Starting Online Validation");
            auto licenseFile { api.getLicenseFile () };
            jassert (licenseFile.exists ()); /// this should not be called if there is not license file... if you get here, why?

            const auto content  { licenseFile.loadFileAsString () };

            const auto url { WithAnalyticsParameters (
                                api.getBaseUrl ()
                                    .getChildURL ("/api/client/licenses/" + api.getProductId () + "/validate")
                                    .withParameter ("format", "Legacy"),
                                api)
                            .withPOSTData (content) };

            httpHelper = std::make_unique<HttpHelper> (url, api.plainTextHeader, true, [&, weakThis](bool wasOk, juce::String response, HttpHelper* helper)
            {
                if (weakThis == nullptr) return;
                jassert (helper == weakThis->httpHelper.get());
                handleValidationResponse (wasOk, response, helper->getResponseCode ());
            });
        });

        const auto returnVal = juce::var(false);
        RETURN(returnVal);
    OBF_END
        return juce::var(false);
    };
    reqRoutine();
}

void OnlineValidator::delayedStart ()
{
    const auto reqRoutine = [&]() -> juce::var
    {
    OBF_BEGIN
        //starting this with a random delay between 0 - 2000ms, so that we don't have all clients start at the same time
        const int delay = juce::Random::getSystemRandom ().nextInt (3000);
        juce::WeakReference<OnlineValidator> weakThis (this);
        juce::Timer::callAfterDelay (delay, [&, weakThis]()
        {
            if (weakThis == nullptr) return;

            const auto lastVerification = api.getLastOnlineVerification ();
            const auto now = juce::Time::getCurrentTime ();
            const auto timeSinceLastVerification = now - lastVerification;

            if (timeSinceLastVerification.inMinutes () > static_cast<double> (revalidationInterval))
            {
                startNow ();
            }
            else
            {
                DBG("Skipping online validation, last validation was only " << timeSinceLastVerification.inSeconds () << "s ago.");
            }
        });
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    reqRoutine();
}

void OnlineValidator::handleValidationResponse (bool /*wasOk*/, const juce::String& response, const int statusCode)
{
    const auto reqRoutine = [&]() -> juce::var
    {
    OBF_BEGIN
        const auto deactivateMachine = [&](const juce::String& msg)
        {
            DBG(msg);
            api.setError (msg);
            api.deleteLocalLicenseFile ();
            api.triggerActivationStateChangedCallbacks ();
            api.updateUiIfPresent ();
        };

        const auto parseError = [&, response]() -> juce::String
        {
            juce::String error;
            auto jsonResponse = juce::JSON::parse (response);
            DBG(response);

            if (auto obj = jsonResponse.getDynamicObject ())
            {
                if (obj->hasProperty ("error"))
                    error = obj->getProperty ("error").toString ();
                else if (obj->hasProperty ("detail"))
                    error = obj->getProperty ("detail").toString ();
                else
                    error = "Unknown error";
            }
            else
            {
                error = "Error: " + response;

                if (jsonResponse.hasProperty ("detail"))
                    error = "Error: " + jsonResponse.getProperty ("detail", "Error: " + response).toString ();
            }
            DBG (error);
            return error;
        };

        const auto graceCheckedDeactivation = [&, deactivateMachine, statusCode]()
        {
            const auto gracePeriodDays = api.getOnlineVerificationGracePeriodDays ();
            const auto gracePeriodSeconds = gracePeriodDays * 24 * 60 * 60;
            const auto lastVerification = api.getLastOnlineVerification ();
            const auto graceLimit = lastVerification + juce::RelativeTime (gracePeriodSeconds);
            const auto now = juce::Time::getCurrentTime ();

            if (now < graceLimit)
            {
                DBG("Ignoring online validation timeout, grace period not expired yet. Grace Limit is: " + graceLimit.toString (true, true));
                api.setError ("Online validation failed (" + juce::String(statusCode) +"). " + api.getProductName () + " will deactivate at " + graceLimit.toString (true, true) + ". If your computer is online and this problem persists after a DAW restart, try disabling any Proxies or VPNs. If the problem still persists contact support with this message.");
                api.triggerActivationStateChangedCallbacks ();
                api.updateUiIfPresent ();
            }
            else
            {
                deactivateMachine ("Online validation failed (" + juce::String(statusCode) + ") and grace period expired. Connect the computer to the internet to re-activate.");
            }
        };



        if (statusCode == 200)
        {
            //validation ok, replace license
            const auto newLicense = response;
            api.replaceLocalLicenseFile (newLicense, false);
        }
        else if (statusCode == 400)
        {
            //server says deactivate
            deactivateMachine (parseError());
        }
        else if (juce::Range<int> (401, 600).contains (statusCode))
        {
            //common errors
            graceCheckedDeactivation ();
        }
        else
        {
            //weird errors
            graceCheckedDeactivation ();
            DBG(response);
            api.setError (parseError());
            api.triggerActivationStateChangedCallbacks ();
            api.updateUiIfPresent ();
            jassertfalse;
        }

        httpHelper.reset ();

        const auto returnVal = juce::var(false);
        RETURN(returnVal);

    OBF_END
        return juce::var(false);
    };
    reqRoutine();
}
