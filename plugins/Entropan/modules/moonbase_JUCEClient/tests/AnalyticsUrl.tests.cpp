#include <catch2/catch_test_macros.hpp>

#include "JUCEClientAPI.h"
#include "Implementations/StaticMethods.h"

#include "TestLicense.h"

TEST_CASE("Analytics URL parameters are encoded exactly once", "[AnalyticsUrl]")
{
    const auto apiOptions = GetApiOptions();
    MB_API_TEST_SETUP(trialLicenseToken);

    const auto base = juce::URL ("https://example.test/activate")
                          .withParameter ("format", "Legacy");

    SECTION ("values containing spaces are single-encoded")
    {
        api->registerGetAnalyticsCallback ([](bool&) {
            juce::StringPairArray p;
            p.set ("JUCEVersion", "JUCE v8.0.11");
            return p;
        });

        const auto url = WithAnalyticsParameters (base, impl);
        const auto str = url.toString (true);

        CAPTURE (str);
        REQUIRE (str.contains ("format=Legacy"));
        REQUIRE (str.contains ("JUCE%20v8.0.11"));
        REQUIRE_FALSE (str.contains ("%2520"));
    }

    SECTION ("special characters round-trip without double escaping")
    {
        const juce::String original = "a&b=c d";

        api->registerGetAnalyticsCallback ([&](bool&) {
            juce::StringPairArray p;
            p.set ("Custom", original);
            return p;
        });

        const auto url = WithAnalyticsParameters (base, impl);
        const auto str = url.toString (true);

        CAPTURE (str);
        REQUIRE_FALSE (str.contains ("%2520"));
        REQUIRE_FALSE (str.contains ("%2526"));
        REQUIRE_FALSE (str.contains ("%253D"));

        const juce::URL parsed (str);
        REQUIRE (parsed.getParameterNames ().contains ("meta[Custom]"));
        const auto idx = parsed.getParameterNames ().indexOf ("meta[Custom]");
        REQUIRE (parsed.getParameterValues ()[idx] == original);
    }

    SECTION ("analytics opted out leaves URL unchanged")
    {
        api->setTransmitAnalytics (false, false);

        const auto url = WithAnalyticsParameters (base, impl);
        REQUIRE (url.toString (true) == base.toString (true));
    }

    SECTION ("format=Legacy is preserved alongside default analytics")
    {
        const auto url = WithAnalyticsParameters (base, impl);
        const auto str = url.toString (true);

        CAPTURE (str);
        REQUIRE (str.contains ("format=Legacy"));
        REQUIRE (str.contains ("appVersion="));
    }
}
