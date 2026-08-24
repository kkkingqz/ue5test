#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/UiSchema.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
struct GV2_CONTENT_CORE_API FSchemaKey final
{
    std::string DefinitionType;
    std::int64_t SchemaVersion = 0;

    bool operator==(const FSchemaKey&) const = default;
    bool operator<(const FSchemaKey& Other) const;
};

/** Parsed schema resource plus package-relative provenance. */
class GV2_CONTENT_CORE_API FSchemaResource final
{
public:
    const FSchemaKey& GetKey() const { return Key; }
    const std::string& GetSchemaId() const { return SchemaId; }
    bool IsUiSchema() const { return SchemaDomain.has_value(); }
    std::optional<EUiSchemaDomain> GetSchemaDomain() const { return SchemaDomain; }
    const FValue& GetRootSpec() const { return RootSpec; }
    const FCompiledFieldSpecPtr& GetCompiledRootSpec() const { return CompiledRootSpec; }
    const FCompiledUiFieldSpecPtr& GetCompiledUiRootSpec() const { return CompiledUiRootSpec; }
    const std::vector<std::string>& GetSemanticValidators() const { return SemanticValidators; }
    const FValue& GetExtensions() const { return Extensions; }
    const std::string& GetPackageId() const { return PackageId; }
    std::uint32_t GetPackageLoadIndex() const { return PackageLoadIndex; }
    const std::string& GetRelativeSource() const { return RelativeSource; }
    const FSourceSpan& GetSourceSpan() const { return SourceSpan; }

private:
    FSchemaResource(
        FSchemaKey InKey,
        std::string InSchemaId,
        std::optional<EUiSchemaDomain> InSchemaDomain,
        FValue InRootSpec,
        FCompiledFieldSpecPtr InCompiledRootSpec,
        FCompiledUiFieldSpecPtr InCompiledUiRootSpec,
        std::vector<std::string> InSemanticValidators,
        FValue InExtensions,
        std::string InPackageId,
        std::uint32_t InPackageLoadIndex,
        std::string InRelativeSource,
        FSourceSpan InSourceSpan);

    friend std::optional<FSchemaResource> ParseSchemaResource(
        const FParsedDocument&,
        const FSchemaBinding&,
        std::string,
        std::uint32_t,
        std::string,
        std::vector<FDiagnostic>&,
        const IUiSchemaResolver*);

    FSchemaKey Key;
    std::string SchemaId;
    std::optional<EUiSchemaDomain> SchemaDomain;
    FValue RootSpec;
    FCompiledFieldSpecPtr CompiledRootSpec;
    FCompiledUiFieldSpecPtr CompiledUiRootSpec;
    std::vector<std::string> SemanticValidators;
    FValue Extensions;
    std::string PackageId;
    std::uint32_t PackageLoadIndex = 0;
    std::string RelativeSource;
    FSourceSpan SourceSpan;
};

/** Build-time registry; consumers use const exact lookup and never fall back by version. */
class GV2_CONTENT_CORE_API FSchemaRegistry final : public IUiSchemaResolver
{
public:
    const FSchemaResource* Find(std::string_view DefinitionType, std::int64_t SchemaVersion) const;
    const FSchemaResource* FindById(std::string_view SchemaId) const;
    std::size_t Num() const { return Resources.size(); }

    bool Register(FSchemaResource Resource, std::vector<FDiagnostic>& OutDiagnostics);

    std::optional<FResolvedUiSchema> FindUiSchema(std::string_view SchemaId) const override;

private:
    std::map<FSchemaKey, FSchemaResource> Resources;
};

/** Parses and checks the closed schema-resource envelope against its descriptor binding. */
GV2_CONTENT_CORE_API std::optional<FSchemaResource> ParseSchemaResource(
    const FParsedDocument& Document,
    const FSchemaBinding& Binding,
    std::string PackageId,
    std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics,
    const IUiSchemaResolver* UiResolver = nullptr);
}
