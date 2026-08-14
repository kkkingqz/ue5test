#include "GV2ContentCore/SchemaRegistry.h"

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
    const FParsedDocument& Document,
    const std::string& JsonPointer,
    const std::string& PackageId,
    const std::uint32_t PackageLoadIndex,
    const std::string& RelativeSource)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = PackageId;
    Diagnostic.PackageLoadIndex = PackageLoadIndex;
    Diagnostic.RelativeSource = RelativeSource;
    Diagnostic.JsonPointer = JsonPointer;
    if (const FParsedLocation* Location = Document.FindLocation(JsonPointer))
    {
        Diagnostic.Span = Location->ValueSpan;
    }
    else if (const FParsedLocation* RootLocation = Document.FindLocation(""))
    {
        Diagnostic.Span = RootLocation->ValueSpan;
    }
    return Diagnostic;
}

FDiagnostic MakeRegistryDiagnostic(
    std::string Code,
    std::string Message,
    const FSchemaResource& Resource)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = Resource.GetPackageId();
    Diagnostic.PackageLoadIndex = Resource.GetPackageLoadIndex();
    Diagnostic.RelativeSource = Resource.GetRelativeSource();
    Diagnostic.Span = Resource.GetSourceSpan();
    Diagnostic.SchemaId = Resource.GetSchemaId();
    Diagnostic.SchemaVersion = Resource.GetKey().SchemaVersion;
    return Diagnostic;
}
}

bool FSchemaKey::operator<(const FSchemaKey& Other) const
{
    return std::tie(DefinitionType, SchemaVersion)
        < std::tie(Other.DefinitionType, Other.SchemaVersion);
}

FSchemaResource::FSchemaResource(
    FSchemaKey InKey,
    std::string InSchemaId,
    FValue InRootSpec,
    FCompiledFieldSpecPtr InCompiledRootSpec,
    std::vector<std::string> InSemanticValidators,
    FValue InExtensions,
    std::string InPackageId,
    const std::uint32_t InPackageLoadIndex,
    std::string InRelativeSource,
    const FSourceSpan InSourceSpan)
    : Key(std::move(InKey))
    , SchemaId(std::move(InSchemaId))
    , RootSpec(std::move(InRootSpec))
    , CompiledRootSpec(std::move(InCompiledRootSpec))
    , SemanticValidators(std::move(InSemanticValidators))
    , Extensions(std::move(InExtensions))
    , PackageId(std::move(InPackageId))
    , PackageLoadIndex(InPackageLoadIndex)
    , RelativeSource(std::move(InRelativeSource))
    , SourceSpan(InSourceSpan)
{
}

const FSchemaResource* FSchemaRegistry::Find(
    const std::string_view DefinitionType,
    const std::int64_t SchemaVersion) const
{
    const auto Found = Resources.find(FSchemaKey{ std::string(DefinitionType), SchemaVersion });
    return Found == Resources.end() ? nullptr : &Found->second;
}

const FSchemaResource* FSchemaRegistry::FindById(const std::string_view SchemaId) const
{
    for (const auto& [Key, Resource] : Resources)
    {
        if (Resource.GetSchemaId() == SchemaId) return &Resource;
    }
    return nullptr;
}

bool FSchemaRegistry::Register(
    FSchemaResource Resource,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    if (const auto Existing = Resources.find(Resource.GetKey()); Existing != Resources.end())
    {
        const bool bSameIdentity = Existing->second.GetSchemaId() == Resource.GetSchemaId();
        FDiagnostic Diagnostic = MakeRegistryDiagnostic(
            bSameIdentity
                ? "core:diagnostic.schema.binding.duplicate"
                : "core:diagnostic.schema.binding.conflict",
            bSameIdentity
                ? "Duplicate exact schema binding"
                : "Conflicting schema IDs for the same (definition_type, schema_version)",
            Resource);
        Diagnostic.RelatedSpan = Existing->second.GetSourceSpan();
        Diagnostic.RelatedMessage = "Previously registered at "
            + Existing->second.GetPackageId() + "/" + Existing->second.GetRelativeSource();
        OutDiagnostics.push_back(std::move(Diagnostic));
        return false;
    }

    for (const auto& ExistingEntry : Resources)
    {
        const FSchemaResource& ExistingResource = ExistingEntry.second;
        if (ExistingResource.GetSchemaId() == Resource.GetSchemaId())
        {
            FDiagnostic Diagnostic = MakeRegistryDiagnostic(
                "core:diagnostic.schema.binding.schema_id_conflict",
                "One schema_id cannot identify different exact schema bindings",
                Resource);
            Diagnostic.RelatedSpan = ExistingResource.GetSourceSpan();
            Diagnostic.RelatedMessage = "Previously registered at "
                + ExistingResource.GetPackageId() + "/" + ExistingResource.GetRelativeSource();
            OutDiagnostics.push_back(std::move(Diagnostic));
            return false;
        }
    }

    Resources.emplace(Resource.GetKey(), std::move(Resource));
    return true;
}

std::optional<FSchemaResource> ParseSchemaResource(
    const FParsedDocument& Document,
    const FSchemaBinding& Binding,
    std::string PackageId,
    const std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const FValue& Root = Document.GetRootValue();
    const std::size_t InitialDiagnosticCount = OutDiagnostics.size();
    if (!Root.IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_shape",
            "Schema resource root must be an object",
            Document, "", PackageId, PackageLoadIndex, RelativeSource));
        OutDiagnostics.back().SchemaId = Binding.GetSchemaId();
        OutDiagnostics.back().SchemaVersion = Binding.GetSchemaVersion();
        return std::nullopt;
    }

    static const std::set<std::string_view> AllowedFields{
        "id", "definition_type", "schema_version", "root", "semantic_validators", "extensions"
    };
    for (const auto& [FieldName, FieldValue] : Root.AsObject())
    {
        if (!AllowedFields.contains(FieldName))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.resource.unknown_field",
                "Unknown schema resource field: " + FieldName,
                Document, "/" + EscapeJsonPointerToken(FieldName), PackageId, PackageLoadIndex, RelativeSource));
        }
    }

    const FValue* Id = Root.FindField("id");
    const FValue* DefinitionType = Root.FindField("definition_type");
    const FValue* SchemaVersion = Root.FindField("schema_version");
    const FValue* RootSpec = Root.FindField("root");
    const FValue* SemanticValidators = Root.FindField("semantic_validators");
    const FValue* Extensions = Root.FindField("extensions");

    if (Id == nullptr || !Id->IsString() || !FStableId::IsOfKind(Id->IsString() ? Id->AsString() : "", "schema"))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_id",
            "Schema resource requires a canonical schema Stable ID",
            Document, "/id", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (DefinitionType == nullptr
        || !DefinitionType->IsString()
        || !FStableId::IsValidSegment(DefinitionType->IsString() ? DefinitionType->AsString() : ""))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_definition_type",
            "Schema resource requires a canonical definition_type",
            Document, "/definition_type", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (SchemaVersion == nullptr || !SchemaVersion->IsInteger() || SchemaVersion->AsInteger() <= 0)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_version",
            "Schema resource requires a positive int64 schema_version",
            Document, "/schema_version", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (RootSpec == nullptr || !RootSpec->IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_root_spec",
            "Schema resource requires an object root FieldSpec",
            Document, "/root", PackageId, PackageLoadIndex, RelativeSource));
    }

    std::vector<std::string> Validators;
    std::set<std::string> SeenValidators;
    if (SemanticValidators != nullptr)
    {
        if (!SemanticValidators->IsArray())
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.resource.invalid_semantic_validators",
                "semantic_validators must be an array",
                Document, "/semantic_validators", PackageId, PackageLoadIndex, RelativeSource));
        }
        else
        {
            for (std::size_t Index = 0; Index < SemanticValidators->AsArray().size(); ++Index)
            {
                const FValue& Validator = SemanticValidators->AsArray()[Index];
                if (!Validator.IsString() || !FStableId::IsOfKind(Validator.IsString() ? Validator.AsString() : "", "validator"))
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.resource.invalid_semantic_validator",
                        "semantic validator must be a canonical validator Stable ID",
                        Document,
                        "/semantic_validators/" + std::to_string(Index),
                        PackageId, PackageLoadIndex, RelativeSource));
                }
                else
                {
                    if (!SeenValidators.insert(Validator.AsString()).second)
                    {
                        OutDiagnostics.push_back(MakeDiagnostic(
                            "core:diagnostic.schema.resource.duplicate_semantic_validator",
                            "semantic_validators must not contain duplicate IDs",
                            Document,
                            "/semantic_validators/" + std::to_string(Index),
                            PackageId, PackageLoadIndex, RelativeSource));
                    }
                    else
                    {
                        Validators.push_back(Validator.AsString());
                    }
                }
            }
        }
    }
    if (Extensions != nullptr && !Extensions->IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.resource.invalid_extensions",
            "Schema resource extensions must be an object",
            Document, "/extensions", PackageId, PackageLoadIndex, RelativeSource));
    }

    if (OutDiagnostics.size() != InitialDiagnosticCount)
    {
        for (std::size_t Index = InitialDiagnosticCount; Index < OutDiagnostics.size(); ++Index)
        {
            OutDiagnostics[Index].SchemaId = Binding.GetSchemaId();
            OutDiagnostics[Index].SchemaVersion = Binding.GetSchemaVersion();
        }
        return std::nullopt;
    }

    FValidationDiagnosticContext Context;
    Context.PackageId = PackageId;
    Context.PackageLoadIndex = PackageLoadIndex;
    Context.RelativeSource = RelativeSource;
    Context.SchemaId = Binding.GetSchemaId();
    Context.SchemaVersion = Binding.GetSchemaVersion();
    FCompiledFieldSpecPtr CompiledRootSpec = CompileFieldSpec(
        *RootSpec, &Document, "/root", Context, OutDiagnostics);
    if (CompiledRootSpec == nullptr) return std::nullopt;

    const bool bBindingMatches = Binding.GetDefinitionType() == DefinitionType->AsString()
        && Binding.GetSchemaVersion() == SchemaVersion->AsInteger()
        && Binding.GetSchemaId() == Id->AsString()
        && Binding.GetRelativePath() == RelativeSource;
    if (!bBindingMatches)
    {
        FDiagnostic Diagnostic = MakeDiagnostic(
            "core:diagnostic.schema.binding.resource_mismatch",
            "Descriptor schema binding does not match resource identity",
            Document, "", PackageId, PackageLoadIndex, RelativeSource);
        Diagnostic.SchemaId = Id->AsString();
        Diagnostic.SchemaVersion = Binding.GetSchemaVersion();
        OutDiagnostics.push_back(std::move(Diagnostic));
        return std::nullopt;
    }

    const FParsedLocation* RootLocation = Document.FindLocation("");
    return FSchemaResource(
        FSchemaKey{ DefinitionType->AsString(), SchemaVersion->AsInteger() },
        Id->AsString(),
        *RootSpec,
        std::move(CompiledRootSpec),
        std::move(Validators),
        Extensions == nullptr ? FValue::MakeObject() : *Extensions,
        std::move(PackageId),
        PackageLoadIndex,
        std::move(RelativeSource),
        RootLocation == nullptr ? FSourceSpan{} : RootLocation->ValueSpan);
}
}
