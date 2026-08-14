#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore
{
class FDefinitionFile;

/** One validated definition-entry shell. Data remains untyped until schema validation. */
class GV2_CONTENT_CORE_API FDefinitionEntry final
{
public:
    const std::string& GetId() const { return Id; }
    const FValue& GetData() const { return Data; }
    const std::vector<std::string>& GetTags() const { return Tags; }
    bool IsDeprecated() const { return bDeprecated; }
    const FValue& GetExtensions() const { return Extensions; }
    std::size_t GetSourceIndex() const { return SourceIndex; }
    const FSourceSpan& GetSourceSpan() const { return SourceSpan; }

private:
    FDefinitionEntry(
        std::string InId,
        FValue InData,
        std::vector<std::string> InTags,
        bool bInDeprecated,
        FValue InExtensions,
        std::size_t InSourceIndex,
        FSourceSpan InSourceSpan);

    friend std::optional<FDefinitionFile> ParseDefinitionFileEnvelope(
        const FParsedDocument&, std::string, std::uint32_t, std::string,
        std::vector<FDiagnostic>&);

    std::string Id;
    FValue Data;
    std::vector<std::string> Tags;
    bool bDeprecated = false;
    FValue Extensions;
    std::size_t SourceIndex = 0;
    FSourceSpan SourceSpan;
};

/** Closed definition-file envelope plus package-relative provenance. */
class GV2_CONTENT_CORE_API FDefinitionFile final
{
public:
    std::int64_t GetSchemaVersion() const { return SchemaVersion; }
    const std::string& GetDefinitionType() const { return DefinitionType; }
    const std::vector<FDefinitionEntry>& GetDefinitions() const { return Definitions; }
    const FValue& GetExtensions() const { return Extensions; }
    const std::string& GetPackageId() const { return PackageId; }
    std::uint32_t GetPackageLoadIndex() const { return PackageLoadIndex; }
    const std::string& GetRelativeSource() const { return RelativeSource; }

private:
    FDefinitionFile(
        std::int64_t InSchemaVersion,
        std::string InDefinitionType,
        std::vector<FDefinitionEntry> InDefinitions,
        FValue InExtensions,
        std::string InPackageId,
        std::uint32_t InPackageLoadIndex,
        std::string InRelativeSource);

    friend std::optional<FDefinitionFile> ParseDefinitionFileEnvelope(
        const FParsedDocument&, std::string, std::uint32_t, std::string,
        std::vector<FDiagnostic>&);

    std::int64_t SchemaVersion = 0;
    std::string DefinitionType;
    std::vector<FDefinitionEntry> Definitions;
    FValue Extensions;
    std::string PackageId;
    std::uint32_t PackageLoadIndex = 0;
    std::string RelativeSource;
};

/** Validates the closed root/entry shell without resolving extension contents. */
GV2_CONTENT_CORE_API std::optional<FDefinitionFile> ParseDefinitionFileEnvelope(
    const FParsedDocument& Document,
    std::string PackageId,
    std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics);

/** Rejects repeated definition IDs within each package, including across files. */
GV2_CONTENT_CORE_API bool ValidatePackageDefinitionIds(
    const std::vector<FDefinitionFile>& DefinitionFiles,
    std::vector<FDiagnostic>& OutDiagnostics);
}
