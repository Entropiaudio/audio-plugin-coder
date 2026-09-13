#pragma once

#include "Includes.h"

namespace Moonbase
{
namespace JUCEClient
{

    struct API;
    struct APIImpl;

    //==============================================================================
    //==============================================================================
    class AutoActivationImpl
    {
    public:
        AutoActivationImpl (APIImpl& api, const AutoActivationConfig& config);
        ~AutoActivationImpl ();

        juce::URL getPollUrl () const { return config.pollUrl; }
        juce::URL getBrowserUrl () const { return config.browserUrl; }

        void startPolling (const AutoActivationPollCallback& callback);
        void stopPolling ();

    private:
        APIImpl& api;
        const AutoActivationConfig config;
        AutoActivationPollCallback callback;


        int currentRequestInterval = 1000; //ms
        int requestCounter = 0;
        bool reachedRequestLimit = false;

        void poll ();
        void processAutoActivationPollResponse (bool wasOk, juce::String response, Moonbase::HttpHelper* helper);
        void incrementWaitInterval ();

        JUCE_DECLARE_WEAK_REFERENCEABLE (AutoActivationImpl)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoActivationImpl)
    };

} // namespace JUCEClient
} // namespace Moonbase
