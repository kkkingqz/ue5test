#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace GV2ContentCore
{
enum class EScalarFieldKind : std::uint8_t
{
    Boolean,
    Integer,
    Number,
    String,
    Enum,
};

struct GV2_CONTENT_CORE_API FScalarFieldSpec final
{
    EScalarFieldKind Kind = EScalarFieldKind::Boolean;
    bool bNullable = false;

    std::optional<std::int64_t> MinimumInteger;
    std::optional<std::int64_t> MaximumInteger;
    std::optional<double> MinimumNumber;
    std::optional<double> MaximumNumber;
    bool bMinimumExclusive = false;
    bool bMaximumExclusive = false;

    std::optional<std::size_t> MinimumLength;
    std::optional<std::size_t> MaximumLength;
    std::optional<std::string> Pattern;
    std::optional<std::regex> CompiledPattern;
    std::optional<std::string> Format;

    std::vector<FValue> EnumValues;
};

struct GV2_CONTENT_CORE_API FValidationDiagnosticContext final
{
    std::optional<std::string> PackageId;
    std::optional<std::uint32_t> PackageLoadIndex;
    std::optional<std::string> RelativeSource;
    std::optional<std::string> DefinitionId;
    std::optional<std::string> SchemaId;
    std::optional<std::int64_t> SchemaVersion;
};

GV2_CONTENT_CORE_API bool IsScalarFieldKind(std::string_view Kind);

/** Compiles and validates a closed scalar FieldSpec. */
GV2_CONTENT_CORE_API std::optional<FScalarFieldSpec> CompileScalarFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics);

/** Validates one present scalar; recursive presence/default handling lives in FieldValidation. */
GV2_CONTENT_CORE_API bool ValidateScalarValue(
    const FValue& Value,
    const FScalarFieldSpec& FieldSpec,
    const FParsedDocument* ValueDocument,
    std::string ValueJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics);
}
