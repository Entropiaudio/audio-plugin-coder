#include <catch2/catch_test_macros.hpp>

#ifdef RUN_VALIDATION_BENCHMARK
    #include <catch2/benchmark/catch_benchmark.hpp>
#endif

#include "JUCEClientAPI.h"
#include "Implementations/StaticMethods.h"
#include "Implementations/API/OnlineValidator.h"
#include "Implementations/API/ValidationCoordinator.h"

#include "TestLicense.h"

#include <cstdio>

namespace
{
    static Moonbase::JUCEClient::APIInitializer MakeUniqueApiOptions (const juce::String& tag)
    {
        const auto version = "1.0.0-" + tag + "-" + juce::Uuid ().toString ().substring (0, 12);
        return { "Demo Co.", "JUCE App Demo", version };
    }

#ifdef MB_CATCH2_TESTING
    struct ScopedOnlineValidationProvider
    {
        explicit ScopedOnlineValidationProvider (Moonbase::JUCEClient::OnlineValidator::TestValidationResponseProvider provider)
        {
            Moonbase::JUCEClient::OnlineValidator::setTestValidationResponseProvider (provider);
        }

        ~ScopedOnlineValidationProvider ()
        {
            Moonbase::JUCEClient::OnlineValidator::setTestValidationResponseProvider (nullptr);
        }
    };

    static void traceOnlineValidationTest (const char* stage)
    {
        std::fprintf (stderr, "[moonbase][online-test] %s\n", stage);
        std::fflush (stderr);
    }
#endif
}


TEST_CASE("Tests local license validation", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    const auto apiOptions = GetApiOptions();

    REQUIRE(apiOptions.companyName == "Demo Co.");

    SECTION("validates a trial license")
    {
        MB_API_TEST_SETUP(trialLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(trialLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }

    SECTION("validates a perpetual license token")
    {
        MB_API_TEST_SETUP(perpetualLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(perpetualLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }

    SECTION("validates a offline license token")
    {
        MB_API_TEST_SETUP(offlineLicenseToken);
        const auto validationResult = ValidateMoonbaseLicenseContent(offlineLicenseToken, impl);

        REQUIRE(validationResult.second == juce::String ());
        REQUIRE((bool)validationResult.first == true);
    }

#ifdef RUN_VALIDATION_BENCHMARK
    BENCHMARK_ADVANCED("Validation performance benchmark")(Catch::Benchmark::Chronometer meter) {
        MB_API_TEST_SETUP(trialLicenseToken);
        meter.measure([&] { return ValidateMoonbaseLicenseContent(trialLicenseToken, impl); });
    };
#endif
}


#ifdef MB_CATCH2_TESTING
TEST_CASE("Tests online license validation (trial)", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    traceOnlineValidationTest ("trial: setup provider");
    ScopedOnlineValidationProvider provider ([](const Moonbase::JUCEClient::APIImpl&, juce::String& response, int& statusCode)
    {
        response = trialLicenseToken;
        statusCode = 200;
        return true;
    });

    const auto apiOptions = MakeUniqueApiOptions ("online-trial");
    _MB_API_TEST_SETUP (trialLicenseToken, true);

    traceOnlineValidationTest ("trial: startNow begin");
    impl.onlineValidator->startNow ();
    traceOnlineValidationTest ("trial: startNow end");

    REQUIRE((bool)api->isUnlocked ().first == true);
    REQUIRE((bool)api->isTrial () == true);
    REQUIRE((bool)api->isOfflineActivated () == false);
}

TEST_CASE("Tests online license validation (perpetual)", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    traceOnlineValidationTest ("perpetual: setup provider");
    ScopedOnlineValidationProvider provider ([](const Moonbase::JUCEClient::APIImpl&, juce::String& response, int& statusCode)
    {
        response = perpetualLicenseToken;
        statusCode = 200;
        return true;
    });

    const auto apiOptions = MakeUniqueApiOptions ("online-perpetual");
    _MB_API_TEST_SETUP (perpetualLicenseToken, true);

    traceOnlineValidationTest ("perpetual: startNow begin");
    impl.onlineValidator->startNow ();
    traceOnlineValidationTest ("perpetual: startNow end");

    REQUIRE((bool)api->isUnlocked ().first == true);
    REQUIRE((bool)api->isTrial () == false);
    REQUIRE((bool)api->isOfflineActivated () == false);
}

TEST_CASE("Tests online license validation (machine mismatch)", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    traceOnlineValidationTest ("mismatch: setup provider");
    ScopedOnlineValidationProvider provider ([](const Moonbase::JUCEClient::APIImpl&, juce::String& response, int& statusCode)
    {
        response = "{\"error\":\"Machine signature mismatch\"}";
        statusCode = 400;
        return true;
    });

    const auto apiOptions = MakeUniqueApiOptions ("online-machine-mismatch");
    _MB_API_TEST_SETUP (perpetualLicenseToken, true);

    const auto licenseFileBefore = api->getLicenseFile ();
    REQUIRE(licenseFileBefore.existsAsFile ());

    traceOnlineValidationTest ("mismatch: startNow begin");
    impl.onlineValidator->startNow ();
    traceOnlineValidationTest ("mismatch: startNow end");

    REQUIRE(!api->getLicenseFile ().existsAsFile ());
    REQUIRE((bool)api->isUnlocked ().first == false);
}
#endif

TEST_CASE("Validation coordinator isolates per plugin identity", "[LicenseValidation]")
{
    MB_INIT_JUCE_DISPATCH_LOOP

    auto apiA = std::make_unique<Moonbase::JUCEClient::API> (MakeUniqueApiOptions ("coordinator-a"));
    auto apiB = std::make_unique<Moonbase::JUCEClient::API> (MakeUniqueApiOptions ("coordinator-b"));

    auto& implA = *static_cast<Moonbase::JUCEClient::APIImpl*> (apiA->impl);
    auto& implB = *static_cast<Moonbase::JUCEClient::APIImpl*> (apiB->impl);

    implA.onlineValidator.reset ();
    implB.onlineValidator.reset ();

    Moonbase::JUCEClient::ValidationCoordinator coordinatorA (implA);
    Moonbase::JUCEClient::ValidationCoordinator coordinatorASameIdentity (implA);
    Moonbase::JUCEClient::ValidationCoordinator coordinatorB (implB);

    REQUIRE(coordinatorA.getIdentityHash () == coordinatorASameIdentity.getIdentityHash ());
    REQUIRE(coordinatorA.getIdentityHash () != coordinatorB.getIdentityHash ());

    const auto reservationA = coordinatorA.tryReserveValidationSlot (60, 5000, 0);
    REQUIRE(reservationA.shouldStart == true);

    const auto reservationASameIdentity = coordinatorASameIdentity.tryReserveValidationSlot (60, 5000, 0);
    REQUIRE(reservationASameIdentity.shouldStart == false);
    REQUIRE(reservationASameIdentity.reason == "in-flight");

    const auto reservationB = coordinatorB.tryReserveValidationSlot (60, 5000, 0);
    REQUIRE(reservationB.shouldStart == true);

    coordinatorA.completeValidation (false);
    coordinatorB.completeValidation (false);

    coordinatorA.getStateFile ().deleteFile ();
    coordinatorB.getStateFile ().deleteFile ();
}
