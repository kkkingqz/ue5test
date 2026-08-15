#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/PackageDescriptorConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCorePackageDescriptorTest,
    "GV2.Runtime.ContentCore.PackageDescriptorValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCorePackageDescriptorTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunPackageDescriptorConformance();
    TestTrue(
        FString::Printf(TEXT("PackageDescriptor conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
