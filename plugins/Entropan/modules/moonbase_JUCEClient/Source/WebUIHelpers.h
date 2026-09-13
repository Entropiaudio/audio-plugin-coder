#if INCLUDE_MOONBASE_UI

#pragma once

#include "JuceIncludes.h"

#if ENABLE_WEBUI_HELPERS

namespace Moonbase
{
namespace JUCEClient
{
    class API;
    class WebBrowserOptions
    {
    public:
        WebBrowserOptions (API& api, juce::WebBrowserComponent& browser);
        WebBrowserOptions (const WebBrowserOptions& other);

        operator juce::WebBrowserComponent::Options();

    private:
        API& api;
        juce::WebBrowserComponent& webBrowser;

        void getLicensingInfo             (auto& varArr, auto complete);
        void requestAutoActivation        (auto& varArr, auto complete);
        void startPollingActivationStatus (auto& varArr, auto complete);
        void stopPollingActivationStatus  (auto& varArr, auto complete);
        void generateMachineFile          (auto& varArr, auto complete);
        void loadOfflineLicenseFile       (auto& varArr, auto complete);
        void deactivateLicense            (auto& varArr, auto complete);

        void openInExternalBrowserWindow  (auto& varArr, auto complete);

        void sendMessageToFrontend (const juce::String& identifier, const juce::Array<juce::var>& args);

        JUCE_DECLARE_WEAK_REFERENCEABLE (WebBrowserOptions)
    };


};
};

#endif
#endif
