#include "GV2ContentCore/Testing/DiagnosticModelConformance.h"

#include "GV2ContentCore/Diagnostic.h"

#include <algorithm>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunDiagnosticModelConformance()
{
    // 1. Absent context verification (std::nullopt instead of empty placeholders)
    FDiagnostic MinimalDiag;
    MinimalDiag.Code = "core:diagnostic.json5.syntax";
    MinimalDiag.Severity = EDiagnosticSeverity::Error;
    MinimalDiag.Message = "Unexpected character";

    if (MinimalDiag.PackageId.has_value()
        || MinimalDiag.RelativeSource.has_value()
        || MinimalDiag.Span.has_value()
        || MinimalDiag.RelatedSpan.has_value()
        || MinimalDiag.DefinitionId.has_value()
        || MinimalDiag.SchemaId.has_value()
        || MinimalDiag.SchemaVersion.has_value()
        || MinimalDiag.JsonPointer.has_value())
    {
        return "diagnostic_model.minimal_absent_context_is_nullopt";
    }

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

    if (!FullDiag.PackageId.has_value() || *FullDiag.PackageId != "core"
        || !FullDiag.Span.has_value() || FullDiag.Span->StartLine != 10
        || !FullDiag.RelatedSpan.has_value() || FullDiag.RelatedSpan->StartLine != 8
        || !FullDiag.JsonPointer.has_value() || *FullDiag.JsonPointer != "/properties/name"
        || FullDiag.SchemaVersion != std::optional<std::int64_t>(1))
    {
        return "diagnostic_model.full_context_fields_retained";
    }

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
    if (Diagnostics[0].Span->StartLine != 5u
        || Diagnostics[1].Span->StartLine != 15u
        || !Diagnostics[2].PackageId.has_value() || *Diagnostics[2].PackageId != "mod_a")
    {
        return "diagnostic_model.deterministic_sorting_by_package_and_span";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
