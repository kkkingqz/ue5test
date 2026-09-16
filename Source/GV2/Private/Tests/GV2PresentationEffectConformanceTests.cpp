#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GV2RuntimeCore/Testing/GV2PresentationEffectConformance.h"

// PEP-03 (ADR-0047): a dedicated file, not appended to GV2RuntimeCoreTests.cpp -- that
// file already sits at the TSR-02 test-file-size ceiling (Tools/Testing/
// test_content_coupling_baseline.json), and new coverage growth belongs in a new file
// under that ratchet, matching e.g. GV2Dca11InventoryTabsTests.cpp's own precedent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PresentationEffectConformanceCrossHostTest,
    "GV2.Runtime.Presentation.PresentationEffectConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PresentationEffectConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunPresentationEffectConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Presentation effect cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
