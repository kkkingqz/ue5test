#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
enum class EExtensionSite : std::uint8_t
{
    DefinitionFile,
    DefinitionEntry,
    SchemaResource,
};

GV2_CONTENT_CORE_API std::optional<EExtensionSite> ParseExtensionSite(std::string_view Value);
GV2_CONTENT_CORE_API std::string_view ToString(EExtensionSite Site);

struct GV2_CONTENT_CORE_API FExtensionSchemaKey final
{
    std::string DefinitionType;
    std::int64_t SchemaVersion = 0;
    EExtensionSite Site = EExtensionSite::DefinitionEntry;
    std::string ExtensionNamespace;

    bool operator==(const FExtensionSchemaKey&) const = default;
    bool operator<(const FExtensionSchemaKey& Other) const;
};

class GV2_CONTENT_CORE_API FExtensionSchemaResource final
{
public:
    const FExtensionSchemaKey& GetKey() const { return Key; }
    const std::string& GetSchemaId() const { return SchemaId; }
    const FCompiledFieldSpecPtr& GetCompiledRootSpec() const { return CompiledRootSpec; }
    const std::string& GetPackageId() const { return PackageId; }
    std::uint32_t GetPackageLoadIndex() const { return PackageLoadIndex; }
    const std::string& GetRelativeSource() const { return RelativeSource; }
    const FSourceSpan& GetSourceSpan() const { return SourceSpan; }

private:
    FExtensionSchemaResource(
        FExtensionSchemaKey InKey,
        std::string InSchemaId,
        FCompiledFieldSpecPtr InCompiledRootSpec,
        std::string InPackageId,
        std::uint32_t InPackageLoadIndex,
        std::string InRelativeSource,
        FSourceSpan InSourceSpan);

    friend std::optional<FExtensionSchemaResource> ParseExtensionSchemaResource(
        const FParsedDocument&, const FExtensionSchemaBinding&, std::string,
        std::uint32_t, std::string, std::vector<FDiagnostic>&);

    FExtensionSchemaKey Key;
    std::string SchemaId;
    FCompiledFieldSpecPtr CompiledRootSpec;
    std::string PackageId;
    std::uint32_t PackageLoadIndex = 0;
    std::string RelativeSource;
    FSourceSpan SourceSpan;
};

class GV2_CONTENT_CORE_API FExtensionSchemaRegistry final
{
public:
    const FExtensionSchemaResource* Find(
        std::string_view DefinitionType,
        std::int64_t SchemaVersion,
        EExtensionSite Site,
        std::string_view ExtensionNamespace) const;
    std::size_t Num() const { return Resources.size(); }

    bool Register(FExtensionSchemaResource Resource, std::vector<FDiagnostic>& OutDiagnostics);

private:
    std::map<FExtensionSchemaKey, FExtensionSchemaResource> Resources;
};

/** Parses a closed extension-schema resource and checks exact descriptor identity. */
GV2_CONTENT_CORE_API std::optional<FExtensionSchemaResource> ParseExtensionSchemaResource(
    const FParsedDocument& Document,
    const FExtensionSchemaBinding& Binding,
    std::string PackageId,
    std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics);

/** Validates and transactionally materializes all registered extension blocks. */
GV2_CONTENT_CORE_API bool ValidateExtensionBlocks(
    const FValue& Extensions,
    FValue& OutMaterializedExtensions,
    const FExtensionSchemaRegistry& Registry,
    std::string_view DefinitionType,
    std::int64_t SchemaVersion,
    EExtensionSite Site,
    std::string_view PackageNamespace,
    const FParsedDocument* Document,
    std::string JsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics);
}
