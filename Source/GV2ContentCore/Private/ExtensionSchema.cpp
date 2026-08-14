#include "GV2ContentCore/ExtensionSchema.h"

#include "GV2ContentCore/StableId.h"

#include <set>
#include <tuple>
#include <utility>

namespace GV2ContentCore
{
namespace
{
std::string EscapeJsonPointerToken(const std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~') Escaped += "~0";
        else if (Character == '/') Escaped += "~1";
        else Escaped.push_back(Character);
    }
    return Escaped;
}

FDiagnostic MakeDiagnostic(
    std::string Code,
    std::string Message,
    const FParsedDocument* Document,
    const std::string& JsonPointer,
    const FValidationDiagnosticContext& Context,
    const bool bPreferKeySpan = false)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = Context.PackageId;
    Diagnostic.PackageLoadIndex = Context.PackageLoadIndex;
    Diagnostic.RelativeSource = Context.RelativeSource;
    Diagnostic.DefinitionId = Context.DefinitionId;
    Diagnostic.SchemaId = Context.SchemaId;
    Diagnostic.SchemaVersion = Context.SchemaVersion;
    Diagnostic.JsonPointer = JsonPointer;
    if (Document != nullptr)
    {
        if (const FParsedLocation* Location = Document->FindLocation(JsonPointer))
        {
            Diagnostic.Span = bPreferKeySpan && Location->KeySpan.has_value()
                ? Location->KeySpan
                : std::optional<FSourceSpan>(Location->ValueSpan);
        }
        else if (const FParsedLocation* RootLocation = Document->FindLocation(""))
        {
            Diagnostic.Span = RootLocation->ValueSpan;
        }
    }
    return Diagnostic;
}
}

std::optional<EExtensionSite> ParseExtensionSite(const std::string_view Value)
{
    if (Value == "definition_file") return EExtensionSite::DefinitionFile;
    if (Value == "definition_entry") return EExtensionSite::DefinitionEntry;
    if (Value == "schema_resource") return EExtensionSite::SchemaResource;
    return std::nullopt;
}

std::string_view ToString(const EExtensionSite Site)
{
    switch (Site)
    {
    case EExtensionSite::DefinitionFile: return "definition_file";
    case EExtensionSite::DefinitionEntry: return "definition_entry";
    case EExtensionSite::SchemaResource: return "schema_resource";
    }
    return {};
}

bool FExtensionSchemaKey::operator<(const FExtensionSchemaKey& Other) const
{
    return std::tie(DefinitionType, SchemaVersion, Site, ExtensionNamespace)
        < std::tie(Other.DefinitionType, Other.SchemaVersion, Other.Site, Other.ExtensionNamespace);
}

FExtensionSchemaResource::FExtensionSchemaResource(
    FExtensionSchemaKey InKey,
    std::string InSchemaId,
    FCompiledFieldSpecPtr InCompiledRootSpec,
    std::string InPackageId,
    const std::uint32_t InPackageLoadIndex,
    std::string InRelativeSource,
    const FSourceSpan InSourceSpan)
    : Key(std::move(InKey))
    , SchemaId(std::move(InSchemaId))
    , CompiledRootSpec(std::move(InCompiledRootSpec))
    , PackageId(std::move(InPackageId))
    , PackageLoadIndex(InPackageLoadIndex)
    , RelativeSource(std::move(InRelativeSource))
    , SourceSpan(InSourceSpan)
{
}

const FExtensionSchemaResource* FExtensionSchemaRegistry::Find(
    const std::string_view DefinitionType,
    const std::int64_t SchemaVersion,
    const EExtensionSite Site,
    const std::string_view ExtensionNamespace) const
{
    const auto Found = Resources.find(FExtensionSchemaKey{
        std::string(DefinitionType), SchemaVersion, Site, std::string(ExtensionNamespace) });
    return Found == Resources.end() ? nullptr : &Found->second;
}

bool FExtensionSchemaRegistry::Register(
    FExtensionSchemaResource Resource,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    if (const auto Existing = Resources.find(Resource.GetKey()); Existing != Resources.end())
    {
        FValidationDiagnosticContext Context;
        Context.PackageId = Resource.GetPackageId();
        Context.PackageLoadIndex = Resource.GetPackageLoadIndex();
        Context.RelativeSource = Resource.GetRelativeSource();
        Context.SchemaId = Resource.GetSchemaId();
        Context.SchemaVersion = Resource.GetKey().SchemaVersion;
        FDiagnostic Diagnostic = MakeDiagnostic(
            "core:diagnostic.extension.schema.duplicate_binding",
            "Duplicate exact extension schema binding",
            nullptr, "", Context);
        Diagnostic.Span = Resource.GetSourceSpan();
        Diagnostic.RelatedSpan = Existing->second.GetSourceSpan();
        Diagnostic.RelatedMessage = "Previously registered at "
            + Existing->second.GetPackageId() + "/" + Existing->second.GetRelativeSource();
        OutDiagnostics.push_back(std::move(Diagnostic));
        return false;
    }
    for (const auto& [ExistingKey, Existing] : Resources)
    {
        if (Existing.GetSchemaId() != Resource.GetSchemaId()) continue;
        FValidationDiagnosticContext Context;
        Context.PackageId = Resource.GetPackageId();
        Context.PackageLoadIndex = Resource.GetPackageLoadIndex();
        Context.RelativeSource = Resource.GetRelativeSource();
        Context.SchemaId = Resource.GetSchemaId();
        Context.SchemaVersion = Resource.GetKey().SchemaVersion;
        FDiagnostic Diagnostic = MakeDiagnostic(
            "core:diagnostic.extension.schema.schema_id_conflict",
            "One extension schema_id cannot identify different exact bindings",
            nullptr, "", Context);
        Diagnostic.Span = Resource.GetSourceSpan();
        Diagnostic.RelatedSpan = Existing.GetSourceSpan();
        OutDiagnostics.push_back(std::move(Diagnostic));
        return false;
    }
    Resources.emplace(Resource.GetKey(), std::move(Resource));
    return true;
}

std::optional<FExtensionSchemaResource> ParseExtensionSchemaResource(
    const FParsedDocument& Document,
    const FExtensionSchemaBinding& Binding,
    std::string PackageId,
    const std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    FValidationDiagnosticContext Context;
    Context.PackageId = PackageId;
    Context.PackageLoadIndex = PackageLoadIndex;
    Context.RelativeSource = RelativeSource;
    Context.SchemaId = Binding.GetSchemaId();
    Context.SchemaVersion = Binding.GetSchemaVersion();
    const std::size_t InitialCount = OutDiagnostics.size();
    const FValue& Root = Document.GetRootValue();
    if (!Root.IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_shape",
            "Extension schema resource root must be an object", &Document, "", Context));
        return std::nullopt;
    }

    static const std::set<std::string_view> AllowedFields{
        "id", "definition_type", "schema_version", "extension_site", "extension_namespace", "root"
    };
    for (const auto& [FieldName, FieldValue] : Root.AsObject())
    {
        if (AllowedFields.contains(FieldName)) continue;
        const std::string Pointer = "/" + EscapeJsonPointerToken(FieldName);
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.unknown_field",
            "Unknown extension schema resource field: " + FieldName,
            &Document, Pointer, Context, true));
    }

    const FValue* Id = Root.FindField("id");
    const FValue* DefinitionType = Root.FindField("definition_type");
    const FValue* SchemaVersion = Root.FindField("schema_version");
    const FValue* ExtensionSite = Root.FindField("extension_site");
    const FValue* ExtensionNamespace = Root.FindField("extension_namespace");
    const FValue* RootSpec = Root.FindField("root");
    const auto ParsedSite = ExtensionSite != nullptr && ExtensionSite->IsString()
        ? ParseExtensionSite(ExtensionSite->AsString())
        : std::nullopt;

    FStableIdView ParsedSchemaId;
    if (Id == nullptr || !Id->IsString() || !FStableId::Parse(Id->AsString(), ParsedSchemaId)
        || ParsedSchemaId.Kind != "schema" || ParsedSchemaId.Namespace != PackageId)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_id",
            "Extension schema requires a package-owned canonical schema Stable ID",
            &Document, "/id", Context));
    }
    if (DefinitionType == nullptr || !DefinitionType->IsString()
        || !FStableId::IsValidSegment(DefinitionType->IsString() ? DefinitionType->AsString() : ""))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_definition_type",
            "Extension schema requires a canonical definition_type",
            &Document, "/definition_type", Context));
    }
    if (SchemaVersion == nullptr || !SchemaVersion->IsInteger() || SchemaVersion->AsInteger() <= 0)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_version",
            "Extension schema requires a positive int64 schema_version",
            &Document, "/schema_version", Context));
    }
    if (!ParsedSite.has_value())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_site",
            "Extension schema requires a supported extension_site",
            &Document, "/extension_site", Context));
    }
    if (ExtensionNamespace == nullptr || !ExtensionNamespace->IsString()
        || ExtensionNamespace->AsString() != PackageId)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_namespace",
            "Extension schema namespace must equal its package namespace",
            &Document, "/extension_namespace", Context));
    }
    if (RootSpec == nullptr || !RootSpec->IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.invalid_root_spec",
            "Extension schema requires an object root FieldSpec",
            &Document, "/root", Context));
    }
    if (OutDiagnostics.size() != InitialCount) return std::nullopt;

    FCompiledFieldSpecPtr CompiledRoot = CompileFieldSpec(
        *RootSpec, &Document, "/root", Context, OutDiagnostics);
    if (CompiledRoot == nullptr) return std::nullopt;
    if (CompiledRoot->Kind != EFieldKind::Object)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.root_not_object",
            "Extension block schema root must be an object FieldSpec",
            &Document, "/root", Context));
        return std::nullopt;
    }

    const bool bMatches = Binding.GetDefinitionType() == DefinitionType->AsString()
        && Binding.GetSchemaVersion() == SchemaVersion->AsInteger()
        && Binding.GetExtensionSite() == ExtensionSite->AsString()
        && Binding.GetExtensionNamespace() == ExtensionNamespace->AsString()
        && Binding.GetSchemaId() == Id->AsString()
        && Binding.GetRelativePath() == RelativeSource;
    if (!bMatches)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.extension.schema.resource_mismatch",
            "Descriptor extension schema binding does not match resource identity",
            &Document, "", Context));
        return std::nullopt;
    }

    const FParsedLocation* RootLocation = Document.FindLocation("");
    return FExtensionSchemaResource(
        FExtensionSchemaKey{
            DefinitionType->AsString(), SchemaVersion->AsInteger(), *ParsedSite,
            ExtensionNamespace->AsString() },
        Id->AsString(),
        std::move(CompiledRoot),
        std::move(PackageId),
        PackageLoadIndex,
        std::move(RelativeSource),
        RootLocation == nullptr ? FSourceSpan{} : RootLocation->ValueSpan);
}

bool ValidateExtensionBlocks(
    const FValue& Extensions,
    FValue& OutMaterializedExtensions,
    const FExtensionSchemaRegistry& Registry,
    const std::string_view DefinitionType,
    const std::int64_t SchemaVersion,
    const EExtensionSite Site,
    const std::string_view PackageNamespace,
    const FParsedDocument* Document,
    std::string JsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const std::size_t InitialCount = OutDiagnostics.size();
    FValue::FObject MaterializedExtensions;
    MaterializedExtensions.reserve(Extensions.AsObject().size());
    for (const auto& [ExtensionNamespace, Block] : Extensions.AsObject())
    {
        const std::string Pointer = JsonPointer + "/" + EscapeJsonPointerToken(ExtensionNamespace);
        if (!FStableId::IsValidSegment(ExtensionNamespace))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.extension.block.invalid_namespace",
                "Extension block key must be a canonical package namespace",
                Document, Pointer, Context, true));
            continue;
        }
        if (ExtensionNamespace != PackageNamespace)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.extension.block.foreign_namespace",
                "Package may write only its own extension namespace",
                Document, Pointer, Context, true));
            continue;
        }
        const FExtensionSchemaResource* ExtensionSchema = Registry.Find(
            DefinitionType, SchemaVersion, Site, ExtensionNamespace);
        if (ExtensionSchema == nullptr)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.extension.block.unregistered_site",
                "Extension block has no exact registered schema for this site",
                Document, Pointer, Context, true));
            continue;
        }

        FValidationDiagnosticContext ExtensionContext = Context;
        ExtensionContext.SchemaId = ExtensionSchema->GetSchemaId();
        ExtensionContext.SchemaVersion = SchemaVersion;
        FValue MaterializedBlock;
        if (ValidateFieldValue(
            Block,
            *ExtensionSchema->GetCompiledRootSpec(),
            MaterializedBlock,
            Document,
            Pointer,
            ExtensionContext,
            OutDiagnostics))
        {
            MaterializedExtensions.emplace_back(ExtensionNamespace, std::move(MaterializedBlock));
        }
    }
    if (OutDiagnostics.size() != InitialCount) return false;
    OutMaterializedExtensions = FValue::MakeObject(std::move(MaterializedExtensions));
    return true;
}
}
