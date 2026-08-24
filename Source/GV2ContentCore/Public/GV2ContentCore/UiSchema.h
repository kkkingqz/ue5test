#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
// UPP-01/ADR-0040: root-level classification of a UI schema resource.
// ui_field schemas are bound to a Screen Field (`field_id` in the document
// envelope); ui_value schemas are reusable nested shapes referenced only via
// `schema_ref` (UPP-03) and never bound to a field directly.
enum class EUiSchemaDomain : std::uint8_t
{
    UiField,
    UiValue,
};

GV2_CONTENT_CORE_API std::optional<EUiSchemaDomain> ParseUiSchemaDomain(std::string_view Value);
GV2_CONTENT_CORE_API std::string_view ToString(EUiSchemaDomain Domain);

// Standard Core kinds a `ui_field`/`ui_value` schema node may declare
// (proposal §7.3). `schema_ref` is a compile-time composition directive, not
// a runtime kind (UPP-03), and is not represented here.
enum class EUiFieldKind : std::uint8_t
{
    Scalar,       // bool / integer / number / string; detail in Scalar.
    Key,          // repeated/local identity; not display text, not coercible from string.
    Text,         // TextSpec; portable-validated shape here, resolved in UE preparation (UPP-04+).
    Ref,          // Stable ID of a declared target_kind (resource, screen, ...).
    Binding,      // opaque BindingSpec; only FGV2UiBindingHandle ever reaches the Widget.
    Object,       // always-closed property map.
    Array,        // ordered values; keyed_by required for interactive/reconciled collections.
    ScreenFields, // nested Screen Field envelope (TabContainer children).
};

struct FCompiledUiFieldSpec;
using FCompiledUiFieldSpecPtr = std::shared_ptr<const FCompiledUiFieldSpec>;

struct GV2_CONTENT_CORE_API FCompiledUiObjectField final
{
    std::string Name;
    bool bRequired = false;
    FCompiledUiFieldSpecPtr Spec;
};

/**
 * Immutable recursive representation of one compiled `ui_field`/`ui_value`
 * schema node. Produced by CompileUiFieldSpec; never mutated afterward.
 */
struct GV2_CONTENT_CORE_API FCompiledUiFieldSpec final
{
    EUiFieldKind Kind = EUiFieldKind::Scalar;
    bool bNullable = false;
    std::optional<FValue> DefaultValue;

    // Scalar (bool / integer / number / string only; enum is not part of the
    // standard UI kind set).
    std::optional<FScalarFieldSpec> Scalar;

    // Ref
    std::string RefTargetKind;

    // Binding
    std::optional<std::string> BindingInputSchemaId;

    // Object (always closed: an unknown property is a fatal candidate error).
    std::vector<FCompiledUiObjectField> Fields;

    // Array
    FCompiledUiFieldSpecPtr Items;
    std::optional<std::string> KeyedBy;
    std::optional<std::size_t> MinimumItems;
    std::optional<std::size_t> MaximumItems;
};

struct GV2_CONTENT_CORE_API FResolvedUiSchema final
{
    const FValue* RootSpec = nullptr;
    const FParsedDocument* Document = nullptr;
    std::string SchemaId;
    std::string PackageId;
    std::string RelativeSource;
};

class GV2_CONTENT_CORE_API IUiSchemaResolver
{
public:
    virtual ~IUiSchemaResolver() = default;
    virtual std::optional<FResolvedUiSchema> FindUiSchema(std::string_view SchemaId) const = 0;
};

/**
 * In-memory resolver for tests and standalone compilation.
 */
class GV2_CONTENT_CORE_API FInMemoryUiSchemaResolver final : public IUiSchemaResolver
{
public:
    void RegisterUiSchema(
        std::string SchemaId,
        FValue RootSpec,
        std::string PackageId = "",
        std::string RelativeSource = "");

    void RegisterUiSchemaDocument(
        std::string SchemaId,
        std::shared_ptr<const FParsedDocument> Document,
        std::string PackageId = "",
        std::string RelativeSource = "");

    std::optional<FResolvedUiSchema> FindUiSchema(std::string_view SchemaId) const override;

private:
    struct FEntry
    {
        std::optional<FValue> RootSpec;
        std::shared_ptr<const FParsedDocument> Document;
        std::string PackageId;
        std::string RelativeSource;
    };
    std::map<std::string, FEntry, std::less<>> Entries;
};

/**
 * Compiles one `ui_field`/`ui_value` FieldSpec node into an immutable tree.
 * Rejects any kind outside the standard UI vocabulary, rejects `default` on
 * every non-scalar kind, and never infers Key from a `string`-kind field:
 * `key: { kind: "string" }` compiles to Scalar/String, structurally distinct
 * from Key, and an array's `keyed_by` is rejected unless it names a sibling
 * field actually compiled as Key.
 *
 * Resolves `schema_ref` directives through the provided resolver, inlines the
 * referenced compiled spec, checks namespace boundaries, and detects direct and
 * indirect cycles.
 */
GV2_CONTENT_CORE_API FCompiledUiFieldSpecPtr CompileUiFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics,
    const IUiSchemaResolver* Resolver = nullptr,
    std::vector<std::string>* ActiveResolutionChain = nullptr);
}
