#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "JUCEClientAPI.h"
#include "TestLicense.h"

// Regression tests for the API construction/destruction lifecycle.
//
// These tests guard against a heap-use-after-free race discovered by
// AddressSanitizer:
//
//   APIImpl owns a background thread via
//     std::unique_ptr<PeriodicSignalDestroyer> signalDestroyer;
//   which periodically calls api.isUnlocked() on its owner, transitively
//   reading juce::String members like machineHashSalt. Prior to the fix,
//   signalDestroyer was declared before machineHashSalt in APIImpl.h, so
//   reverse-order member destruction freed machineHashSalt while the
//   background thread was still looping and dereferencing it.
//
// These tests exercise repeated API construction and destruction and are
// intended to be run under -fsanitize=address. Without ASAN, the race is
// rarely visible; with ASAN, the original bug aborts reliably on the
// very first destructor.
//
// Required fix: at the top of APIImpl::~APIImpl(), reset any unique_ptr
// members that own background threads (signalDestroyer, onlineValidator,
// activationWatcher) so their threads are joined before any member
// juce::String they may read is destroyed.

TEST_CASE("API lifecycle: construct and destroy with license file", "[APILifecycle]")
{
    // This test reliably triggers the UAF only when a license file is present,
    // because isUnlocked() short-circuits before reading machineHashSalt when
    // there is nothing to validate. MB_API_TEST_SETUP installs a trial license
    // file on disk so the PeriodicSignalDestroyer thread, on its first loop
    // iteration, walks the full validation path (getMachineHashSalt → string
    // members → the dangling pointer).
    //
    // Under -fsanitize=address, destructing the `api` local at end-of-section
    // reliably aborts on the unfixed module.

    const auto apiOptions = GetApiOptions();

    SECTION("construct-sleep-destruct widens the race window")
    {
        // Sleep briefly between construction and destruction so the
        // PeriodicSignalDestroyer background thread definitely enters its
        // first isUnlocked() call — which is where the UAF manifests when
        // APIImpl's destructor later frees juce::String members out from
        // under the still-running thread.
        MB_API_TEST_SETUP(trialLicenseToken);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        api.reset();
    }

    SECTION("repeated construct-destruct cycles")
    {
        // Over many cycles, the probability of the race manifesting
        // approaches 1 in the absence of a fix.
        for (int i = 0; i < 50; ++i)
        {
            MB_API_TEST_SETUP(trialLicenseToken);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            api.reset();
        }
    }

    SECTION("perpetual license variant")
    {
        MB_API_TEST_SETUP(perpetualLicenseToken);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        api.reset();
    }
}
