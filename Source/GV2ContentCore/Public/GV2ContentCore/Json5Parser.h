#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/Json5Lexer.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/Value.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
    struct GV2_CONTENT_CORE_API FParsedLocation final
    {
        std::string JsonPointer;
        FSourceSpan ValueSpan;
        std::optional<FSourceSpan> KeySpan;

        bool operator==(const FParsedLocation&) const = default;
    };

    /** Transient parsed document used only during candidate build and validation. */
    class GV2_CONTENT_CORE_API FParsedDocument final
    {
    public:
        FParsedDocument(FValue InRootValue, std::vector<FParsedLocation> InLocations);

        const FValue& GetRootValue() const { return RootValue; }
        const std::vector<FParsedLocation>& GetLocations() const { return Locations; }
        const FParsedLocation* FindLocation(std::string_view JsonPointer) const;

    private:
        FValue RootValue;
        std::vector<FParsedLocation> Locations;
    };

    GV2_CONTENT_CORE_API std::optional<FParsedDocument> ParseJson5Document(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);

    /**
     * Convenience projection through the same parser path when source locations are not needed.
     * Returns std::nullopt on syntax error or limit violation, populating OutDiagnostics.
     */
    GV2_CONTENT_CORE_API std::optional<FValue> ParseJson5(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);
}
