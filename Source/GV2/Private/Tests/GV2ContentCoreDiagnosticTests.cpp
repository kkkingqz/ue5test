#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Diagnostic.h"
#include "Misc/AutomationTest.h"
#include <algorithm>
#include <vector>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreDiagnosticModelTest,
    "GV2.Runtime.ContentCore.DiagnosticModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreDiagnosticModelTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // 1. Absent context verification (std::nullopt instead of empty placeholders)
    FDiagnostic MinimalDiag;
    MinimalDiag.Code = "core:diagnostic.json5.syntax";
    MinimalDiag.Severity = EDiagnosticSeverity::Error;
    MinimalDiag.Message = "Unexpected character";

    TestFalse(TEXT("PackageId absent returns nullopt"), MinimalDiag.PackageId.has_value());
    TestFalse(TEXT("RelativeSource absent returns nullopt"), MinimalDiag.RelativeSource.has_value());
    TestFalse(TEXT("Span absent returns nullopt"), MinimalDiag.Span.has_value());
    TestFalse(TEXT("RelatedSpan absent returns nullopt"), MinimalDiag.RelatedSpan.has_value());
    TestFalse(TEXT("DefinitionId absent returns nullopt"), MinimalDiag.DefinitionId.has_value());
    TestFalse(TEXT("SchemaId absent returns nullopt"), MinimalDiag.SchemaId.has_value());
    TestFalse(TEXT("SchemaVersion absent returns nullopt"), MinimalDiag.SchemaVersion.has_value());
    TestFalse(TEXT("JsonPointer absent returns nullopt"), MinimalDiag.JsonPointer.has_value());

    // 2. Full Diagnostic with all context fields
    FDiagnostic FullDiag;
    FullDiag.Code = "core:diagnostic.schema.missing_property";
    FullDiag.Severity = EDiagnosticSeverity::Error;
    FullDiag.Message = "Required field 'name' is missing";
    FullDiag.PackageId = "core";
    FullDiag.PackageLoadIndex = 0;
    FullDiag.RelativeSource = "definitions/items.json5";
    FullDiag.Span = FSourceSpan{ 10, 5, 10, 20 };
    FullDiag.RelatedSpan = FSourceSpan{ 8, 5, 8, 20 };
    FullDiag.RelatedMessage = "Related declaration";
    FullDiag.DefinitionId = "core:item.iron_sword";
    FullDiag.SchemaId = "core:schema.item.v1";
    FullDiag.SchemaVersion = 1;
    FullDiag.JsonPointer = "/properties/name";

    TestTrue(TEXT("PackageId has value"), FullDiag.PackageId.has_value() && *FullDiag.PackageId == "core");
    TestTrue(TEXT("Span has start line 10"), FullDiag.Span.has_value() && FullDiag.Span->StartLine == 10);
    TestTrue(TEXT("Related span has start line 8"), FullDiag.RelatedSpan.has_value() && FullDiag.RelatedSpan->StartLine == 8);
    TestTrue(TEXT("JsonPointer has value"), FullDiag.JsonPointer.has_value() && *FullDiag.JsonPointer == "/properties/name");
    TestTrue(TEXT("SchemaVersion has value"), FullDiag.SchemaVersion == std::optional<std::int64_t>(1));

    // 3. Deterministic comparator & sorting verification
    FDiagnostic Diag1 = FullDiag;
    Diag1.PackageId = "core";
    Diag1.Span = FSourceSpan{ 15, 1, 15, 10 };

    FDiagnostic Diag2 = FullDiag;
    Diag2.PackageId = "core";
    Diag2.Span = FSourceSpan{ 5, 1, 5, 10 };

    FDiagnostic Diag3 = FullDiag;
    Diag3.PackageId = "mod_a";
    Diag3.PackageLoadIndex = 1;
    Diag3.Span = FSourceSpan{ 1, 1, 1, 10 };

    std::vector<FDiagnostic> Diagnostics = { Diag1, Diag3, Diag2 };
    std::sort(Diagnostics.begin(), Diagnostics.end());

    // Expected order: Diag2 (core, line 5) < Diag1 (core, line 15) < Diag3 (mod_a, line 1)
    TestEqual(TEXT("Sorted 0 is Diag2 (line 5)"), Diagnostics[0].Span->StartLine, 5u);
    TestEqual(TEXT("Sorted 1 is Diag1 (line 15)"), Diagnostics[1].Span->StartLine, 15u);
    TestEqual(TEXT("Sorted 2 is Diag3 (mod_a)"), *Diagnostics[2].PackageId, std::string("mod_a"));

    return true;
}

#endif
