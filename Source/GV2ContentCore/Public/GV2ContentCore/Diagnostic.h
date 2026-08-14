#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <cstdint>
#include <optional>
#include <string>

namespace GV2ContentCore
{
    /**
     * Source location span in a source file (1-indexed line and column).
     */
    struct GV2_CONTENT_CORE_API FSourceSpan
    {
        std::uint32_t StartLine = 1;
        std::uint32_t StartColumn = 1;
        std::uint32_t EndLine = 1;
        std::uint32_t EndColumn = 1;

        bool operator==(const FSourceSpan& Other) const;
        bool operator!=(const FSourceSpan& Other) const;
        bool operator<(const FSourceSpan& Other) const;
    };

    enum class EDiagnosticSeverity : std::uint8_t
    {
        Error,
        Warning,
        Info
    };

    /**
     * Portable diagnostic record for PortableContentCore validation and parsing errors.
     * Optional fields use std::optional to explicitly distinguish absent context from empty placeholders.
     */
    struct GV2_CONTENT_CORE_API FDiagnostic final
    {
        std::string Code;
        EDiagnosticSeverity Severity = EDiagnosticSeverity::Error;
        std::string Message;

        std::optional<std::string> PackageId;
        std::optional<std::uint32_t> PackageLoadIndex;
        std::optional<std::string> RelativeSource;
        std::optional<FSourceSpan> Span;
        std::optional<FSourceSpan> RelatedSpan;
        std::optional<std::string> RelatedMessage;
        std::optional<std::string> DefinitionId;
        std::optional<std::string> SchemaId;
        std::optional<std::int64_t> SchemaVersion;
        std::optional<std::string> JsonPointer;

        bool operator==(const FDiagnostic& Other) const;
        bool operator!=(const FDiagnostic& Other) const;

        /**
         * Deterministic order comparator.
         * PackageLoadIndex -> RelativeSource -> Span -> Code -> PackageId -> DefinitionId -> SchemaId -> SchemaVersion -> JsonPointer -> RelatedSpan -> RelatedMessage -> Severity -> Message.
         */
        bool operator<(const FDiagnostic& Other) const;
    };
}
