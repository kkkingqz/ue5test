#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/ScalarValidation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore
{
enum class EFieldKind : std::uint8_t
{
    Scalar,
    Array,
    Map,
    Object,
    Union,
    Reference,
    TextId,
    ResourceReference,
};

enum class EStoragePolicy : std::uint8_t
{
    Definition,
    RuntimeState,
};

enum class EWritePolicy : std::uint8_t
{
    ReadOnly,
    Plain,
    Managed,
};

enum class EReferenceKind : std::uint8_t
{
    Definition,
    Instance,
};

struct FCompiledFieldSpec;
using FCompiledFieldSpecPtr = std::shared_ptr<const FCompiledFieldSpec>;

struct GV2_CONTENT_CORE_API FCompiledObjectField final
{
    std::string Name;
    bool bRequired = false;
    FCompiledFieldSpecPtr Spec;
};

struct GV2_CONTENT_CORE_API FCompiledUnionVariant final
{
    std::string DiscriminatorValue;
    FCompiledFieldSpecPtr Spec;
};

/** Immutable recursive representation used by the single schema-validation path. */
struct GV2_CONTENT_CORE_API FCompiledFieldSpec final
{
    EFieldKind Kind = EFieldKind::Scalar;
    bool bNullable = false;
    std::optional<FValue> DefaultValue;
    std::optional<FScalarFieldSpec> Scalar;

    EStoragePolicy Storage = EStoragePolicy::Definition;
    EWritePolicy WritePolicy = EWritePolicy::ReadOnly;
    EReferenceKind ReferenceKind = EReferenceKind::Definition;
    std::vector<std::string> Operations;

    FCompiledFieldSpecPtr Items;
    FCompiledFieldSpecPtr MapKeys;
    FCompiledFieldSpecPtr MapValues;
    std::optional<std::size_t> MinimumSize;
    std::optional<std::size_t> MaximumSize;
    bool bUnique = false;

    std::vector<FCompiledObjectField> Fields;
    std::string Discriminator;
    std::vector<FCompiledUnionVariant> Variants;

    std::string ExpectedStableIdKind;
    std::string ResourceClass;
    bool bBootstrapRequired = false;
};

/** Compiles scalar and recursive container FieldSpec nodes into one immutable tree. */
GV2_CONTENT_CORE_API FCompiledFieldSpecPtr CompileFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics);

/**
 * Validates and materializes one value recursively. OutMaterializedValue is
 * assigned only on success; the input and compiled defaults are never mutated.
 */
GV2_CONTENT_CORE_API bool ValidateFieldValue(
    const FValue& Value,
    const FCompiledFieldSpec& FieldSpec,
    FValue& OutMaterializedValue,
    const FParsedDocument* ValueDocument,
    std::string ValueJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics);
}
