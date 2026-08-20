#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentEditor/Testing/EditorAdapterConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentEditorTest,
    "GV2.Editor.ContentEditor.AdapterConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentEditorTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentEditor::Testing::RunEditorAdapterConformance();
    TestTrue(
        FString::Printf(TEXT("EditorAdapter conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
