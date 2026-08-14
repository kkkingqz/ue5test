#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
    /**
     * Configurable limits for JSON5 input validation and parsing.
     */
    struct GV2_CONTENT_CORE_API FParseLimits final
    {
        static constexpr std::size_t MaxSupportedNestingDepth = 1024;
        std::size_t MaxFileSizeBytes = 16 * 1024 * 1024;     // Default 16 MB limit
        std::size_t MaxNestingDepth = 256;                   // Default 256 levels
        std::size_t MaxStringLengthBytes = 1 * 1024 * 1024;  // Default 1 MB per string
        std::size_t MaxContainerEntries = 100000;            // Default 100,000 entries per container

        bool operator==(const FParseLimits&) const = default;
    };

    /**
     * Cleaned UTF-8 source view with optional BOM metadata.
     */
    struct GV2_CONTENT_CORE_API FUtf8Source final
    {
        std::string_view CleanedView;
        bool bHasBOM = false;
        std::size_t ByteOffsetShift = 0;
    };

    /**
     * Validates raw input bytes against UTF-8 encoding rules and file size limits.
     * Strips UTF-8 BOM if present.
     * On error, appends a diagnostic to OutDiagnostics and returns std::nullopt.
     */
    GV2_CONTENT_CORE_API std::optional<FUtf8Source> ValidateUtf8AndLimits(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);

    /**
     * Helper to validate nesting depth limit.
     */
    GV2_CONTENT_CORE_API bool CheckNestingDepth(
        std::size_t CurrentDepth,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);

    /**
     * Helper to validate string length limit.
     */
    GV2_CONTENT_CORE_API bool CheckStringLength(
        std::size_t StringLengthBytes,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);

    /**
     * Helper to validate container entry count limit.
     */
    GV2_CONTENT_CORE_API bool CheckContainerEntries(
        std::size_t EntryCount,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);
}
