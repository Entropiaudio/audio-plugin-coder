#if INCLUDE_MOONBASE_UI
#include "WebUIHelpers.h"

#include "JUCEClientAPI.h"


#if ENABLE_WEBUI_HELPERS

#include "JUCEClientUI.h"

using namespace Moonbase;
using namespace Moonbase::JUCEClient;


WebBrowserOptions::WebBrowserOptions (API& a, juce::WebBrowserComponent& browser)
: api (a), webBrowser (browser)
{

}

WebBrowserOptions::WebBrowserOptions (const WebBrowserOptions& other)
: api (other.api), webBrowser (other.webBrowser)
{

}

#define MOONBASE_WEBUI_FUNCTION_FORWARD(func_name) \
    {#func_name, [&](auto& varArr, auto complete) { \
        func_name(varArr, complete); \
    }}

WebBrowserOptions::operator juce::WebBrowserComponent::Options()
{
    auto options = juce::WebBrowserComponent::Options ()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2 {}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory))
        )
        .withNativeIntegrationEnabled ();

    std::map<juce::Identifier, juce::WebBrowserComponent::NativeFunction> nativeFunctions {
        MOONBASE_WEBUI_FUNCTION_FORWARD (getLicensingInfo),
        MOONBASE_WEBUI_FUNCTION_FORWARD (requestAutoActivation),
        MOONBASE_WEBUI_FUNCTION_FORWARD (startPollingActivationStatus),
        MOONBASE_WEBUI_FUNCTION_FORWARD (stopPollingActivationStatus),
        MOONBASE_WEBUI_FUNCTION_FORWARD (generateMachineFile),
        MOONBASE_WEBUI_FUNCTION_FORWARD (loadOfflineLicenseFile),
        MOONBASE_WEBUI_FUNCTION_FORWARD (deactivateLicense),
        MOONBASE_WEBUI_FUNCTION_FORWARD (openInExternalBrowserWindow)
    };

    for (auto& nf : nativeFunctions)
        options = options.withNativeFunction (nf.first, nf.second);

    return options;
}

void WebBrowserOptions::getLicensingInfo (auto& varArr, auto complete)
{
    auto obj = new juce::DynamicObject ();

    obj->setProperty ("serviceId", "moonbase");

    obj->setProperty ("productId", api.getProductId ());
    obj->setProperty ("offlineUrl", api.getOfflineUrl ().toString (true));
    obj->setProperty ("companyName", api.getCompanyName ());
    obj->setProperty ("productName", api.getProductName ());
    obj->setProperty ("productVersion", api.getProductVersion ());

    obj->setProperty ("userEmail", api.getUserEmail ());
    obj->setProperty ("licenseId", api.getLicenseId ());

    const auto unlockResult = MB_IS_UNLOCKED_OBFUSCATED(api);
    obj->setProperty ("isUnlocked", unlockResult.first);
    obj->setProperty ("unlockError", unlockResult.second);
    obj->setProperty ("isTrial", api.isTrial ());
    obj->setProperty ("isOfflineActivated", api.isOfflineActivated ());

    obj->setProperty ("expiry", api.getLicenseExpiration ().toMilliseconds ());

    if (complete != nullptr) complete (obj);
};

void WebBrowserOptions::requestAutoActivation (auto& varArr, auto complete)
{
    api.requestAutoActivation ([&, complete, weak = juce::WeakReference<WebBrowserOptions> (this)](const Moonbase::JUCEClient::AutoActivationConfig& config)
    {
        if (weak == nullptr || complete == nullptr) return;

        auto obj = new juce::DynamicObject ();
        obj->setProperty ("result", true);
        obj->setProperty ("pollUrl", config.pollUrl.toString (true));
        obj->setProperty ("browserUrl", config.browserUrl.toString (true));
        if (complete != nullptr) complete (obj);
    });
}

void WebBrowserOptions::startPollingActivationStatus (auto& varArr, auto complete)
{
    juce::Uuid listenerId;
    api.startPollingAutoActivationResult ([&, listenerId, weak = juce::WeakReference<WebBrowserOptions>(this)](const bool finished, bool success, const juce::String& responseContent)
    {
        if (weak == nullptr) return;

        auto obj = new juce::DynamicObject ();
        obj->setProperty ("finished", finished);
        obj->setProperty ("success", success);
        obj->setProperty ("responseContent", responseContent);
        sendMessageToFrontend (listenerId.toString (), {obj});

    });

    auto obj = new juce::DynamicObject ();
    obj->setProperty ("result", true);
    obj->setProperty ("info", "Polling started");
    obj->setProperty ("listenerId", listenerId.toString ());
    if (complete != nullptr) complete (obj);
}

void WebBrowserOptions::stopPollingActivationStatus (auto& varArr, auto complete)
{
    api.stopPollingAutoActivationResult ();
    auto obj = new juce::DynamicObject ();
    obj->setProperty ("result", true);
    obj->setProperty ("info", "Polling stopped");
    if (complete != nullptr) complete (obj);
}


void WebBrowserOptions::generateMachineFile (auto& varArr, auto complete)
{
    api.generateMachineFile ([&, complete, weak = juce::WeakReference<WebBrowserOptions> (this)](bool success, const juce::File& machineFile)
    {
        if (weak == nullptr || complete == nullptr) return;
        auto obj = new juce::DynamicObject ();
        obj->setProperty ("result", success);
        obj->setProperty ("machineFile", machineFile.getFullPathName ());
        if (complete != nullptr) complete (obj);
    });
}

void WebBrowserOptions::loadOfflineLicenseFile (auto& varArr, auto complete)
{
    auto file = juce::File (varArr[0].toString ());
    if (file.existsAsFile ())
    {
        const auto valid = api.loadOfflineLicenseFile (file);
        auto obj = new juce::DynamicObject ();
        obj->setProperty ("result", valid);
        obj->setProperty ("licenseFile", file.getFullPathName ());
    }
    else
    {
        auto obj = new juce::DynamicObject ();
        obj->setProperty ("result", false);
        obj->setProperty ("error", "File not found: " + file.getFullPathName ());
        complete (obj);
    }
}

void WebBrowserOptions::deactivateLicense (auto& varArr, auto complete)
{
    api.deactivateLicense ([&, complete, weak = juce::WeakReference<WebBrowserOptions> (this)](bool success)
    {
        if (weak == nullptr || complete == nullptr) return;
        auto obj = new juce::DynamicObject ();
        obj->setProperty ("result", success);
        complete (obj);
    });
}

void WebBrowserOptions::openInExternalBrowserWindow (auto& varArr, auto complete)
{
    jassert (varArr.size () == 1);
    const juce::URL url (varArr[0].toString ());
    jassert (url.isWellFormed ());

    if (url.isWellFormed ())
        url.launchInDefaultBrowser ();
}

void WebBrowserOptions::sendMessageToFrontend (const juce::String& identifier, const juce::Array<juce::var>& args)
{
    if (!webBrowser.isVisible ())
    {
        juce::Timer::callAfterDelay ((200), [&, identifier, args, weak = juce::WeakReference<WebBrowserOptions> (this)]() {
            if (weak != nullptr)
                sendMessageToFrontend (identifier, args);
        });

        return;
    }
    const auto objectAsString = juce::JSON::toString (args, true);
    const auto escaped = objectAsString.replace ("\\", "\\\\").replace ("'", "\\'");
    const auto evaluationHandler =  [&, args, identifier, escaped, objectAsString] (juce::WebBrowserComponent::EvaluationResult result)
    {
        if (auto r = result.getResult ())
        {
            // DBG("Sent message: " + identifier + ": " + objectAsString);
            // DBG("Result: " + r->toString ());
        }
        else if (auto e = result.getError ())
        {
            DBG(e->message);
            jassertfalse;
        }

    };

    webBrowser.evaluateJavascript ("window.__JUCE__.backend.emitByBackend(" + identifier.quoted() + ", "
                            + escaped.quoted ('\'')
                            + ");", evaluationHandler);
}

#endif
#endif
