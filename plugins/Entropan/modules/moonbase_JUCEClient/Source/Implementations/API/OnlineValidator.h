#pragma once

#include "Includes.h"
#include "HttpHelper.h"

namespace Moonbase
{
namespace JUCEClient
{
    struct APIImpl;
    class OnlineValidator
    {
    public:
        OnlineValidator (APIImpl& api);
        ~OnlineValidator ();

        void startNow ();

#ifdef MB_CATCH2_TESTING
        using TestValidationResponseProvider = std::function<bool (const APIImpl&, juce::String& response, int& statusCode)>;
        static void setTestValidationResponseProvider (TestValidationResponseProvider provider);
#endif

    private:
        APIImpl& api;

        std::unique_ptr<HttpHelper> httpHelper;

#ifdef MB_CATCH2_TESTING
        static TestValidationResponseProvider& getTestValidationResponseProvider ();
#endif

        void delayedStart ();

        const juce::int64 revalidationInterval = 1;//min
        void handleValidationResponse (bool wasOk, const juce::String& response, const int statusCode);

        JUCE_DECLARE_WEAK_REFERENCEABLE (OnlineValidator)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnlineValidator)
    };

} // namespace JUCEClient
} // namespace Moonbase
