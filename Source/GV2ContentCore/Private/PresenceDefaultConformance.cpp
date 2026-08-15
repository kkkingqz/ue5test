#include "GV2ContentCore/Testing/PresenceDefaultConformance.h"

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunPresenceDefaultConformance()
{
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "presence.self_test.json5";
    Context.SchemaId = "core:schema.presence.self_test";

    // 1. Presence and defaults compilation
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { name: { kind: 'string', required: true }, "
        "optional: { kind: 'string' }, count: { kind: 'int64', default: 2 }, "
        "items: { kind: 'array', items: { kind: 'string' }, default: [] } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "presence_default.parse_spec";
    }

    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty())
    {
        return "presence_default.compile_spec";
    }

    // 2. Default value injection & optional field absence
    const FValue Input = FValue::MakeObject({ { "name", FValue("valid") } });
    FValue First;
    if (!ValidateFieldValue(Input, *Spec, First, nullptr, "/data", Context, Diagnostics)
        || First.FindField("optional") != nullptr
        || First.FindField("count") == nullptr
        || First.FindField("count")->AsInteger() != 2
        || First.FindField("items") == nullptr
        || !First.FindField("items")->AsArray().empty()
        || Input.FindField("count") != nullptr)
    {
        return "presence_default.default_injection_and_optional_absence";
    }

    // 3. Deep copy isolation of injected defaults
    FValue Second;
    if (!ValidateFieldValue(Input, *Spec, Second, nullptr, "/data", Context, Diagnostics))
    {
        return "presence_default.validate_second_instance";
    }
    First.FindField("items")->AsArray().push_back(FValue("changed"));
    if (!Second.FindField("items")->AsArray().empty())
    {
        return "presence_default.defaults_deep_copy_isolation";
    }

    // 4. Missing required field error
    Diagnostics.clear();
    FValue Unchanged("sentinel");
    if (ValidateFieldValue(
            FValue::MakeObject(), *Spec, Unchanged, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.missing_required_field"
        || !Unchanged.IsString()
        || Unchanged.AsString() != "sentinel")
    {
        return "presence_default.missing_required_field_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
