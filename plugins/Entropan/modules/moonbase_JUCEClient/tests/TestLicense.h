#pragma once

#include "Implementations/API/Includes.h"

static const juce::String trialLicenseToken =
    "anonymous\n"
    "PG1ldGFkYXRhPgogICAgPGlkPjQ0MzA0OTIyNzA4ZGNmMzAxOThiOTAzYWM1MWJkOTllPC9pZD4KCTx2ZW5kb3JJZD5kZW1vPC92ZW5kb3JJZD4KCTx2ZW5kb3JOYW1lPkRlbW8gQ28uPC92ZW5kb3JOYW1lPgoJPHVzZXJJZD4wMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDA8L3VzZXJJZD4KCTx1c2VyTmFtZT5hbm9ueW1vdXM8L3VzZXJOYW1lPgoJPHVzZXJFbWFpbD5hbm9ueW1vdXM8L3VzZXJFbWFpbD4KCTxsaWNlbnNlSWQ+NDQzMDQ5MjI3MDhkY2YzMDE5OGI5MDNhYzUxYmQ5OWU8L2xpY2Vuc2VJZD4KCTxwcm9kdWN0SWQ+anVjZS1hcHAtZGVtbzwvcHJvZHVjdElkPgoJPHByb2R1Y3ROYW1lPkpVQ0UgQXBwIERlbW88L3Byb2R1Y3ROYW1lPgoJPHByb2R1Y3RDdXJyZW50UmVsZWFzZT4xLjAuMDwvcHJvZHVjdEN1cnJlbnRSZWxlYXNlPgoJPGFjdGl2YXRpb25NZXRob2Q+b25saW5lPC9hY3RpdmF0aW9uTWV0aG9kPgoJPGNyZWF0ZWRBdD4yMDI1LTA3LTMxVDEzOjUyOjE3LjI3NFo8L2NyZWF0ZWRBdD4KCTxsYXN0VmVyaWZpY2F0aW9uPjIwMjUtMDctMzFUMTM6NTI6MTcuMjc1WjwvbGFzdFZlcmlmaWNhdGlvbj4KCTx0cmlhbD50cnVlPC90cmlhbD4KCTxleHBpcnk+MjAyOC0wNC0yNVQxMzo1MjoxNy4yNzRaPC9leHBpcnk+Cgk8Y29tcHV0ZXJTaWduYXR1cmU+dGVzdC1zaWduYXR1cmU8L2NvbXB1dGVyU2lnbmF0dXJlPgo8L21ldGFkYXRhPg==\n"
    "NkMyNTZBMTkzMDcwMEE3ODI5MTFEMDREQzMwNjVFRDhDOTFFRUFDNEI1QUY4Q0VCOUEzQzNFOTI0NzU4QjY1MkNDRkY4RTdFNkUzRDBGNjMxRjAxODQ1MDQzRTQxNEYzNkFFQ0E3NTlFQUIwMENGNTZFMUZFNjUxODRCRDNBMzhGRjQ4MzY1NUVENDdBQzc2M0ExQTNCQkU1QTI4N0QzRjYzRDg2RUYwNjQwQ0I3MTdERjk5RTIyMzRBRDVDNEJDQUM2NzBBNERDRkI5ODczMDBBNUMxNEFFOUU1Njg2Qjg3Q0VCRDJDRjJBQzI1MjUxREVCRTZDOTcyRUQ3Q0Q2MjUwQTVBRjRERDhCMDhDRUJBRjIyNDlGQjhCMThCMzNGNDdGMjFBNUNDOThBRTgwOTI2RDI1MkUxRTQzQkRBMEREOUM3MjFDMDEzMjU0MzRFMEJFQjc2OTk3NDMwODAzRkRGNkM2M0Y2NzJEQkE1Q0JDREM4QzRCREMwQkZGNzZCMzFFM0ZEMkFCRTQzQjVDMjdCQTMwNzMwQkRCREFGQTlCM0ZCQzlDQzE4M0UzMTgyRTMyREE0QTcwOTc0NDQxM0E0QjRBNTQzMzU0Qzc3NjUxM0U4NzJGNDkyRDU4QjlBQkIxM0FCQjBFMzlFMkJDODAzMTkwRjREMzIwNDZGRTk=";

static const juce::String perpetualLicenseToken =
    "test@moonwater.no\n"
    "PG1ldGFkYXRhPgogICAgPGlkPjFmNDBiN2YxMmZiYTM2YmQ3MzQ1YWE2OGY2ZDExMjQ5PC9pZD4KCTx2ZW5kb3JJZD5kZW1vPC92ZW5kb3JJZD4KCTx2ZW5kb3JOYW1lPkRlbW8gQ28uPC92ZW5kb3JOYW1lPgoJPHVzZXJJZD5lYzI0ZThkMy1hYmJkLTQ3MTItODdjZC1kZDQyM2QyMjI5ZjU8L3VzZXJJZD4KCTx1c2VyTmFtZT5Ub2JpYXMgTMO4bm5lcsO4ZCBNYWRzZW48L3VzZXJOYW1lPgoJPHVzZXJFbWFpbD50ZXN0QG1vb253YXRlci5ubzwvdXNlckVtYWlsPgoJPGxpY2Vuc2VJZD42ZmY1Njg5ZC1mYjNlLTQ0YzUtOTgzYS0wZTJjOTA1ZjA1MDM8L2xpY2Vuc2VJZD4KCTxwcm9kdWN0SWQ+anVjZS1hcHAtZGVtbzwvcHJvZHVjdElkPgoJPHByb2R1Y3ROYW1lPkpVQ0UgQXBwIERlbW88L3Byb2R1Y3ROYW1lPgoJPHByb2R1Y3RDdXJyZW50UmVsZWFzZT4xLjAuMDwvcHJvZHVjdEN1cnJlbnRSZWxlYXNlPgoJPHN1YlByb2R1Y3RzT3duZWQ+cHJlc2V0LXBhY2stMTwvc3ViUHJvZHVjdHNPd25lZD4KCTxhY3RpdmF0aW9uTWV0aG9kPm9ubGluZTwvYWN0aXZhdGlvbk1ldGhvZD4KCTxjcmVhdGVkQXQ+MjAyNC0xMS0yMFQxNTo0MToxMy40ODNaPC9jcmVhdGVkQXQ+Cgk8bGFzdFZlcmlmaWNhdGlvbj4yMDI2LTA0LTA1VDE5OjM5OjIzLjY0Nlo8L2xhc3RWZXJpZmljYXRpb24+Cgk8dHJpYWw+ZmFsc2U8L3RyaWFsPgoJPGNvbXB1dGVyU2lnbmF0dXJlPnRlc3Qtc2lnbmF0dXJlPC9jb21wdXRlclNpZ25hdHVyZT4KCTxwcm9wZXJ0aWVzPgoJCTxwcm9kdWN0PgoJCQk8dGVzdF9wcm9wZXJ0eT50ZXN0PC90ZXN0X3Byb3BlcnR5PgoJCQk8b2JqZWN0X2V4YW1wbGU+CgkJCQk8aW5uZXI+MTwvaW5uZXI+CgkJCTwvb2JqZWN0X2V4YW1wbGU+CgkJPC9wcm9kdWN0PgoJCTxjdXN0b21lcj4KCQkJPGNvb2xuZXNzPndoYWNrPC9jb29sbmVzcz4KCQk8L2N1c3RvbWVyPgoJCTxsaWNlbnNlPgoJCQk8bGljZW5zZV90aWVyPnBybzwvbGljZW5zZV90aWVyPgoJCTwvbGljZW5zZT4KCTwvcHJvcGVydGllcz4KPC9tZXRhZGF0YT4=\n"
    "OTRERTE2MzZBNDVFMTI3ODlDNTIzRTkwRDM2MEZBRjM5RjRFOUEyMEVFNzI5RkM3ODdDMTNBRTY2MEUzMEI0QjBBNEYwQzcwNzE5OTZDRjVBMjlCOUVCRjk1RkY2Rjc3QjY4NTQ0QjdEQzgwRTI2RTJDNDhFOTc3OTA2RERERDVCMzVEMzkwRkNENUI0ODYxNDU1QUQ0NDBFNEM1QkVFRTM0MTY1QzZBMzJBOEZBNUNCMDZGQUFBM0QyQkQ1MjREREQzQzc5Mzg1NjZDRkEwRUFGNTY0MjNBNjIxNTIwMjA0MTA4MUFCNEM0OUM0MTg2NkQ2MkRENTBFQzhBN0ZDNEIzNkIwMDIxMkI3MEE5Qzg2NDY0QzkxM0FGODRENjM4NEQzODI1MTc2NTdGQjZDNzkyNDMzMTMyM0ZGN0Y0MUEwNDQzQTU2OTYyNUFGNEYzRjdGRjk3QTRCQjdBNTBDN0ZEQkMyRTQxN0UxMjI1N0RCRkUyQUJCNEU2MTg5QjFCRjU3MzBGNUYwREZFQTMzNzE1QjUyNEEwREY3NDAwMDM3RTRDQURDNkU4MUMwMDJBNDNDRTk1Q0U5NjRFMTU1QzlGNTg0MzJERDdFMjRBQzk4NUQ1MDVGN0UxMjRBQUY1NjhCNkVENjUxRTA1QzlDOTUwQTY4OTIzRTk4MkZGM0U=\n";

static const juce::String offlineLicenseToken = 
    "test@moonwater.no\n"
    "PG1ldGFkYXRhPgogICAgPGlkPjFmNDBiN2YxMmZiYTM2YmQ3MzQ1YWE2OGY2ZDExMjQ5PC9pZD4KCTx2ZW5kb3JJZD5kZW1vPC92ZW5kb3JJZD4KCTx2ZW5kb3JOYW1lPkRlbW8gQ28uPC92ZW5kb3JOYW1lPgoJPHVzZXJJZD5lYzI0ZThkMy1hYmJkLTQ3MTItODdjZC1kZDQyM2QyMjI5ZjU8L3VzZXJJZD4KCTx1c2VyTmFtZT5Ub2JpYXM8L3VzZXJOYW1lPgoJPHVzZXJFbWFpbD50ZXN0QG1vb253YXRlci5ubzwvdXNlckVtYWlsPgoJPGxpY2Vuc2VJZD42ZmY1Njg5ZC1mYjNlLTQ0YzUtOTgzYS0wZTJjOTA1ZjA1MDM8L2xpY2Vuc2VJZD4KCTxwcm9kdWN0SWQ+anVjZS1hcHAtZGVtbzwvcHJvZHVjdElkPgoJPHByb2R1Y3ROYW1lPkpVQ0UgQXBwIERlbW88L3Byb2R1Y3ROYW1lPgoJPHByb2R1Y3RDdXJyZW50UmVsZWFzZT4xLjAuMDwvcHJvZHVjdEN1cnJlbnRSZWxlYXNlPgoJPHN1YlByb2R1Y3RzT3duZWQ+cHJlc2V0LXBhY2stMTwvc3ViUHJvZHVjdHNPd25lZD4KCTxhY3RpdmF0aW9uTWV0aG9kPm9mZmxpbmU8L2FjdGl2YXRpb25NZXRob2Q+Cgk8Y3JlYXRlZEF0PjIwMjQtMTEtMjBUMTU6NDE6MTMuNDgzWjwvY3JlYXRlZEF0PgoJPGxhc3RWZXJpZmljYXRpb24+MjAyNS0wNy0zMVQxNDowNjo0My41NzhaPC9sYXN0VmVyaWZpY2F0aW9uPgoJPHRyaWFsPmZhbHNlPC90cmlhbD4KCTxjb21wdXRlclNpZ25hdHVyZT50ZXN0LXNpZ25hdHVyZTwvY29tcHV0ZXJTaWduYXR1cmU+CjwvbWV0YWRhdGE+\n"
    "MkUxOERCODcxOTFFOTE1NTU2Qjk5NTgzNjE4N0ZDODJDM0Q1RjMwOTdEQjAxM0ZFREEyNUUyNDI5RjQwN0FDMEFEQTY5MUQyOTQ2N0U1QTUzN0QwQzQ2NUNGQjFCMUE0Njc2RURDQzlFRTIxOEQwMDdENDJBNEM3MThBNDFBRjRGMjg4MERDOUQ1REY1MjM0NDIzODA0MDk2MEFCNDBGNDUwNzdENTBFRTFBOUY5NTY2NUZCNTkyNDdCRjE3MjEyOEExQkY1NUJDODc2OTM1RkE2NjJDMzQ5NENEMEIwQUMzMjA0QjlBNUM5Q0JFRDZEMUVBOTc5OEVFNjg0ODcxQjQ1NDEyQTI3RUFDQTgyNTExMzQ0MTlGMjc4N0EzQkM3NzY3NERCQzk4NjZBQ0IxNjdDNTFBOEJGMzJDRDM1N0ZCQjkwRTlCRjFERDIyNkM1QkVFMTNBNUQxRDZBMEZFREE5RUVENURFRThEMTlCOTdBNDc3NDIwQ0RDMEJFNUE1Mzk1RThDQjE0NEE2RURDQ0FDNDYzRUFBRTY5OEVFMTlCMEU3OEVBNzZCQUI2RDExNEJDMUI5QjFBQTEzRTQxQjJFQjZDQzUyNTI4QzgzQTgyNTg0N0FDOEFGM0MxNzZBMzYxOUJCMzE4RUUzODZGODVCMTg0RkU0REUwRUY4QTc=\n";

static const Moonbase::JUCEClient::APIInitializer GetApiOptions (const juce::Uuid& testUuid = juce::Uuid ())
{
    juce::ignoreUnused (testUuid);

    return {
        "Demo Co.",
        "JUCE App Demo",
        "1.0.0"
    };
}

static inline juce::InterProcessLock& GetLicenseTestLock ()
{
    static juce::InterProcessLock lock ("moonbase_JUCEClient_license_test_lock");
    return lock;
}

struct ScopedLicenseTestLock
{
    ScopedLicenseTestLock (juce::InterProcessLock& lockToUse, int timeoutMs)
    : lock (lockToUse), isHeld (lock.enter (timeoutMs))
    {
    }

    ~ScopedLicenseTestLock ()
    {
        if (isHeld)
            lock.exit ();
    }

    juce::InterProcessLock& lock;
    bool isHeld = false;
};

#define MB_INIT_JUCE_DISPATCH_LOOP \
    juce::ScopedJuceInitialiser_GUI juceInit {};\
    juce::MessageManager::getInstance ()->setCurrentThreadAsMessageThread (); // this makes sure there is a message manager running, which is required for the API


#define MB_RUN_DISPATCH_LOOP_WHILE(future) \
{ \
    auto __mb_start = std::chrono::steady_clock::now(); \
    bool __mb_timed_out = false; \
    while (future.wait_for (std::chrono::milliseconds (1)) != std::future_status::ready) { \
        if (auto mm = juce::MessageManager::getInstance()) \
            mm->runDispatchLoopUntil(10); \
        if (std::chrono::steady_clock::now() - __mb_start > std::chrono::seconds (30)) { \
            UNSCOPED_INFO("Timeout waiting for async task (30s) in MB_RUN_DISPATCH_LOOP_WHILE"); \
            __mb_timed_out = true; \
            break; \
        } \
    } \
    if (__mb_timed_out) { \
        REQUIRE(false); \
    } \
}

#define _MB_API_TEST_SETUP(token, usesOnlineValidator) \
    auto& __mbLicenseTestLock = GetLicenseTestLock (); \
    ScopedLicenseTestLock __mbLicenseScopedLock (__mbLicenseTestLock, 30000); \
    REQUIRE (__mbLicenseScopedLock.isHeld); \
    auto api = std::make_unique<Moonbase::JUCEClient::API>(apiOptions); \
    \
    const auto licenseFile = api->getLicenseFile(); \
    licenseFile.create(); \
    licenseFile.replaceWithText(token); \
    api = std::make_unique<Moonbase::JUCEClient::API>(apiOptions); \
    api->setGracePeriodDays (99999); \
\
    auto& impl = *static_cast<Moonbase::JUCEClient::APIImpl*>(api->impl); \
    if (usesOnlineValidator && impl.onlineValidator == nullptr) { \
        impl.onlineValidator = std::make_unique<Moonbase::JUCEClient::OnlineValidator> (impl); \
    } \
    if (!usesOnlineValidator) { \
        impl.onlineValidator.reset (); \
    } \
\
    CAPTURE (api->getLicenseId ()); \
    CAPTURE (api->getCurrentReleaseVersion ()); \
    CAPTURE (api->getOwnedSubProducts ()); \
    CAPTURE (api->getUserId ()); \
    CAPTURE (api->getUserName ()); \
    CAPTURE (api->getUserEmail ()); \
    CAPTURE (api->isTrial ()); \
    CAPTURE (api->isOfflineActivated ()); \
    CAPTURE (api->getLastOnlineVerification().toISO8601(true)); \
    CAPTURE (api->getLicenseExpiration().toISO8601(true)); 

#define MB_API_TEST_SETUP(token) _MB_API_TEST_SETUP(token, false)
