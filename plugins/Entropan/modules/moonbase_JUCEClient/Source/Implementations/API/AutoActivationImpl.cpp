#include "AutoActivationImpl.h"

#include "../StaticMethods.h"

//==============================================================================
//==============================================================================
AutoActivationImpl::AutoActivationImpl (APIImpl& apiRef, const AutoActivationConfig& autoActivationConfig)
:
api (apiRef),
config (autoActivationConfig)
{

}

AutoActivationImpl::~AutoActivationImpl ()
{

}

void AutoActivationImpl::startPolling (const AutoActivationPollCallback& cb)
{
    callback = cb;
    poll ();

}

void AutoActivationImpl::stopPolling ()
{
    if (callback != nullptr)
    {
        callback (true, false, "");
        callback = nullptr;
    }
}

void AutoActivationImpl::poll ()
{
    const auto reqRoutine = [&]() -> juce::var
    {
    OBF_BEGIN
        juce::WeakReference<AutoActivationImpl> weakThis (this);
        api.httpHelpers.add (new HttpHelper (config.pollUrl, api.jsonHeader, false, [&, weakThis](bool wasOk, juce::String response, HttpHelper* helper)
        {
            if (weakThis != nullptr)
                processAutoActivationPollResponse (wasOk, response, helper);
        }));
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    reqRoutine();
}

void AutoActivationImpl::processAutoActivationPollResponse (bool wasOk, juce::String response, HttpHelper* helper)
{
    const auto reqRoutine = [&]() -> juce::var
    {
    OBF_BEGIN
        static const int waitCode { 204 }; //http code for "no content"
        static const int successCode { 200 }; //http code for "ok"
        const auto responseCode = helper->getResponseCode ();

        if (wasOk && (responseCode == waitCode || responseCode == successCode) && !reachedRequestLimit )
        {
            if (responseCode == waitCode)
            {
                if (callback != nullptr)
                {
                    juce::WeakReference<AutoActivationImpl> weakThis (this);
                    juce::Timer::callAfterDelay (currentRequestInterval, [weakThis]()
                    {
                        if (weakThis != nullptr)
                        {
                            weakThis->incrementWaitInterval ();
                            weakThis->poll ();
                        }
                    });

                    callback (false, true, response);
                }
            }
            else if (responseCode == successCode)
            {
                api.processOnlineActivationResponse (wasOk, response, helper);
                if (callback != nullptr) callback (true, true, response);
                return juce::var(false);
            }
        }
        else
        {
            if (reachedRequestLimit)
            {
                auto timeoutError = new juce::DynamicObject ();
                timeoutError->setProperty ("error", "Timed out, please try again!");
                response = juce::JSON::toString (timeoutError);
            }

            api.parseErrorResponse (response);

            if (callback != nullptr) callback (true, false, response);
        }

        api.removeHelper (helper);
        api.updateUiIfPresent ();
        return juce::var(false);
    OBF_END
        return juce::var(false);
    };
    reqRoutine();
}

void AutoActivationImpl::incrementWaitInterval ()
{
    requestCounter++;
    if (requestCounter == 5)
    {
        currentRequestInterval = 2000;
    }
    else if (requestCounter == 10)
    {
        currentRequestInterval = 3000;
    }
    else if (requestCounter == 20)
    {
        currentRequestInterval = 4000;
    }
    else if (requestCounter == 80)
    {
        reachedRequestLimit = true;
    }
}
