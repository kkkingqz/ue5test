#include "GV2ContentCore/Testing/ContainerValidationConformance.h"

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunContainerValidationConformance()
{
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "container.self_test.json5";
    Context.SchemaId = "core:schema.container.self_test";

    // 1. Discriminated union inside array inside object
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { entries: { kind: 'array', unique: true, items: { "
        "kind: 'union', discriminator: 'kind', variants: { add: { kind: 'object', fields: { "
        "kind: { kind: 'enum', values: ['add'] }, value: { kind: 'int64' } } } } } } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "container_validation.parse_union_spec";
    }

    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty())
    {
        return "container_validation.compile_union_spec";
    }

    const FValue Valid = FValue::MakeObject({
        { "entries", FValue::MakeArray({ FValue::MakeObject({
            { "kind", FValue("add") }, { "value", FValue(2) } }) }) }
    });
    FValue Materialized;
    if (!ValidateFieldValue(Valid, *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || !Diagnostics.empty()
        || Materialized.FindField("entries") == nullptr
        || Materialized.FindField("entries")->AsArray().empty()
        || Materialized.FindField("entries")->AsArray()[0].FindField("value")->AsInteger() != 2)
    {
        return "container_validation.valid_union_materialization";
    }

    // 2. Invalid union variant rejected
    Diagnostics.clear();
    const FValue Invalid = FValue::MakeObject({
        { "entries", FValue::MakeArray({ FValue::MakeObject({
            { "kind", FValue("unknown") } }) }) }
    });
    if (ValidateFieldValue(Invalid, *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.invalid_union_variant"
        || Diagnostics.front().JsonPointer != std::optional<std::string>("/data/entries/0/kind"))
    {
        return "container_validation.invalid_union_variant_rejected";
    }

    // 3. Unique object array item validation (with permutations)
    Diagnostics.clear();
    auto UniqueObjectDocument = ParseJson5Document(
        "{ kind: 'array', unique: true, items: { kind: 'object', fields: {"
        "a: { kind: 'int64' }, b: { kind: 'int64' } } } }",
        FParseLimits{}, Diagnostics);
    if (!UniqueObjectDocument.has_value() || !Diagnostics.empty())
    {
        return "container_validation.parse_unique_object_spec";
    }

    const FCompiledFieldSpecPtr UniqueObjectSpec = CompileFieldSpec(
        UniqueObjectDocument->GetRootValue(), &*UniqueObjectDocument, "", Context, Diagnostics);
    if (UniqueObjectSpec == nullptr || !Diagnostics.empty())
    {
        return "container_validation.compile_unique_object_spec";
    }

    const FValue PermutedDuplicates = FValue::MakeArray({
        FValue::MakeObject({ { "a", FValue(1) }, { "b", FValue(2) } }),
        FValue::MakeObject({ { "b", FValue(2) }, { "a", FValue(1) } }),
    });
    if (ValidateFieldValue(
            PermutedDuplicates, *UniqueObjectSpec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.duplicate_array_item")
    {
        return "container_validation.permuted_duplicate_array_item_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
