#pragma once
#include "JuceIncludes.h"

namespace Moonbase
{
namespace JUCEClient
{
    using OptionalMachineIdCallback = std::function<const juce::String()>;

    struct APIInitializer {
        const juce::String companyName;
        const juce::String productName;
        const juce::String productVersion;

        /**
         * Prevents duplicate online validation requests by setting a timeout after a successful one.
         * Helps avoid simultaneous requests during session startup across multiple instances.
         * A 1-minute timeout keeps (de)activation behavior responsive
         * Longer timeouts (e.g., 60min) further limit requests but may allow parallel use on different machines — use with caution.
         */
        const juce::int64 parallelOnlineValidationTimeout = 1; //minute(s)
       
        /**
         * If provided, this function will be called to get a unique machine ID for offline activation.
         * If not provided, a default implementation will be used that uses SHA256 on various hardware identifiers.
         * Only use this, if you have a very specific reason to do so, as the default implementation should be sufficient for most use cases.
         */
        OptionalMachineIdCallback machineIdCallback = nullptr;

        /**
         * This customizes the hashes, that your products use to uniquely identify computers. 
         * Only set this, if you know why you need it. 
         * When left empty the standard implementation should be enough in most use-cases.
         */
        const juce::String customMachineHashSalt = ""; 
    };

    struct AutoActivationConfig {
        const juce::URL pollUrl;
        const juce::URL browserUrl;
    };

    using AutoActivationRequestCallback = std::function<void (const AutoActivationConfig& config)>;
    using AutoActivationPollCallback = std::function<void (const bool finished, bool success, const juce::String& responseContent)>;
    using MachineFileCallback = std::function<void (bool success, const juce::File& machineFile)>;
    using DeactivationCallback = std::function<void (bool success)>;

    using ActivationStateChangedCallback = std::function<void (const bool unlocked, const bool trial, const bool offlineActivated)>;

    using GetAnalyticsCallback = std::function <const juce::StringPairArray (bool& includeExtendedDefaultAnalytics)>;

} // namespace JUCEClient
} // namespace Moonbase