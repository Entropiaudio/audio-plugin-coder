#include <catch2/catch_test_macros.hpp>

#include "Implementations/StaticMethods.h"

#include "JUCEClientAPI.h"
#include "TestLicense.h"

TEST_CASE("GetFieldFromLicense parses license metadata", "[LicenseSerde]")
{
    MB_INIT_JUCE_DISPATCH_LOOP;

    const auto apiOptions = GetApiOptions();

    REQUIRE(apiOptions.companyName == "Demo Co.");

    SECTION("given a license token, it parses fields")
    {
        CAPTURE ("given a license token, it parses fields");
        MB_API_TEST_SETUP(trialLicenseToken);

        auto result = GetFieldFromLicense(impl, "computerSignature");
        CAPTURE(result);
        REQUIRE(result == "test-signature");
    };

    SECTION("given a license token, it returns an empty string on invalid field")
    {
        CAPTURE ("given a license token, it returns an empty string on invalid field");
        MB_API_TEST_SETUP(trialLicenseToken);

        auto result = GetFieldFromLicense(impl, "invalidField");
        CAPTURE(result);
        REQUIRE(result == "");
    };

    SECTION("given a trial license token, gets common license fields")
    {
        CAPTURE ("given a trial license token, gets common license fields");
        MB_API_TEST_SETUP(trialLicenseToken);

        REQUIRE(api->getLicenseId() == "44304922708dcf30198b903ac51bd99e");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray());

        REQUIRE(api->getUserId() == "00000000-0000-0000-0000-000000000000");
        REQUIRE(api->getUserName() == "Trial");
        REQUIRE(api->getUserEmail() == "anonymous");

        REQUIRE(api->isTrial());
        REQUIRE(!api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2025, 6, 31, 13, 52, 17, 275, false).toISO8601(true));
        REQUIRE(api->getLicenseExpiration().toISO8601(true) == juce::Time(2028, 3, 25, 13, 52, 17, 274, false).toISO8601(true));
    };

    SECTION("given a perpetual license token, gets common license fields")
    {
        CAPTURE ("given a perpetual license token, gets common license fields");
        MB_API_TEST_SETUP(perpetualLicenseToken);

        REQUIRE(api->getLicenseId() == "6ff5689d-fb3e-44c5-983a-0e2c905f0503");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray({"preset-pack-1"}));

        REQUIRE(api->getUserId() == "ec24e8d3-abbd-4712-87cd-dd423d2229f5");
        REQUIRE(api->getUserName() == juce::String::fromUTF8("Tobias L\xc3\xb8nner\xc3\xb8" "d Madsen"));
        REQUIRE(api->getUserEmail() == "test@moonwater.no");

        REQUIRE(!api->isTrial());
        REQUIRE(!api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2026, 3, 5, 19, 39, 23, 646, false).toISO8601(true));
    };

    SECTION("given a perpetual license token with properties, gets custom properties")
    {
        CAPTURE ("given a perpetual license token with properties, gets custom properties");
        MB_API_TEST_SETUP(perpetualLicenseToken);

        auto productProps = api->getProductProperties();
        REQUIRE(productProps["test_property"] == "test");
        REQUIRE(productProps["object_example.inner"] == "1");
        REQUIRE(productProps.size() == 2);

        auto customerProps = api->getCustomerProperties();
        REQUIRE(customerProps["coolness"] == "whack");
        REQUIRE(customerProps.size() == 1);

        auto licenseProps = api->getLicenseProperties();
        REQUIRE(licenseProps["license_tier"] == "pro");
        REQUIRE(licenseProps.size() == 1);

        auto trialProps = api->getTrialProperties();
        REQUIRE(trialProps.size() == 0);
    };

    SECTION("given a token without properties, returns empty property arrays")
    {
        CAPTURE ("given a token without properties, returns empty property arrays");
        MB_API_TEST_SETUP(trialLicenseToken);

        REQUIRE(api->getProductProperties().size() == 0);
        REQUIRE(api->getCustomerProperties().size() == 0);
        REQUIRE(api->getLicenseProperties().size() == 0);
        REQUIRE(api->getTrialProperties().size() == 0);
    };

    SECTION("given a offline license token, gets common license fields")
    {
        CAPTURE ("given a offline license token, gets common license fields");
        MB_API_TEST_SETUP(offlineLicenseToken);

        REQUIRE(api->getLicenseId() == "6ff5689d-fb3e-44c5-983a-0e2c905f0503");

        REQUIRE(api->getCurrentReleaseVersion() == "1.0.0");
        REQUIRE(api->getOwnedSubProducts() == juce::StringArray({"preset-pack-1"}));

        REQUIRE(api->getUserId() == "ec24e8d3-abbd-4712-87cd-dd423d2229f5");
        REQUIRE(api->getUserName() == "Tobias");
        REQUIRE(api->getUserEmail() == "test@moonwater.no");

        REQUIRE(!api->isTrial());
        REQUIRE(api->isOfflineActivated());

        REQUIRE(api->getLastOnlineVerification().toISO8601(true) == juce::Time(2025, 6, 31, 14, 06, 43, 578, false).toISO8601(true));
    };

}
