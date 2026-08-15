#include "GV2ContentCore/Testing/SpecialFieldValidationConformance.h"

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunSpecialFieldValidationConformance()
{
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "special.self_test.json5";
    Context.SchemaId = "core:schema.special.self_test";

    // 1. Special field specs compilation
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { target: { kind: 'ref', target_kind: 'screen' }, "
        "title: { kind: 'text_id', default: 'core:text.default.title' }, "
        "icon: { kind: 'resource_ref', resource_class: 'texture_2d', bootstrap_required: true } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "special_field.parse_spec";
    }

    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty())
    {
        return "special_field.compile_spec";
    }

    // 2. Valid special fields materialization
    FValue Materialized;
    if (!ValidateFieldValue(
            FValue::MakeObject({
                { "target", FValue("core:screen.main") },
                { "icon", FValue("core:resource.ui.icon") }
            }),
            *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Materialized.FindField("title") == nullptr
        || Materialized.FindField("title")->AsString() != "core:text.default.title")
    {
        return "special_field.valid_materialization";
    }

    // 3. Stable ID target kind mismatch rejected
    Diagnostics.clear();
    if (ValidateFieldValue(
            FValue::MakeObject({ { "target", FValue("core:item.wrong_kind") } }),
            *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.stable_id_wrong_kind")
    {
        return "special_field.stable_id_wrong_kind_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
