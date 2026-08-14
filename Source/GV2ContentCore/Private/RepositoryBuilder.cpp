#include "GV2ContentCore/RepositoryBuilder.h"

#include "CanonicalHash.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace GV2ContentCore
{
namespace
{
struct FParsedSource final
{
    const FPackageDescriptor* Package = nullptr;
    std::string RelativeSource;
    FParsedDocument Document;
};

struct FValidatedDefinition final
{
    FValue Value;
    const FSchemaResource* Schema = nullptr;
    const FPackageDescriptor* Package = nullptr;
    const FParsedDocument* Document = nullptr;
    std::string RelativeSource;
    std::size_t SourceIndex = 0;
    FSourceSpan SourceSpan;
};

const FParsedDocument* FindParsedDocument(
    const std::vector<FParsedSource>& Sources,
    const FPackageDescriptor* Package,
    const std::string_view RelativeSource)
{
    for (const FParsedSource& Source : Sources)
    {
        if (Source.Package == Package && Source.RelativeSource == RelativeSource)
        {
            return &Source.Document;
        }
    }
    return nullptr;
}

FDiagnostic MakeResolutionDiagnostic(
    std::string Code,
    std::string Message,
    const FValidatedDefinition& Definition,
    const std::string_view SchemaId,
    const std::int64_t SchemaVersion,
    const std::string& JsonPointer)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = Definition.Package->GetPackageId();
    Diagnostic.PackageLoadIndex = Definition.Package->GetLoadIndex();
    Diagnostic.RelativeSource = Definition.RelativeSource;
    Diagnostic.DefinitionId = Definition.Value.FindField("id")->AsString();
    Diagnostic.SchemaId = std::string(SchemaId);
    Diagnostic.SchemaVersion = SchemaVersion;
    Diagnostic.JsonPointer = JsonPointer;
    if (const FParsedLocation* Location = Definition.Document->FindLocation(JsonPointer))
    {
        Diagnostic.Span = Location->ValueSpan;
    }
    else
    {
        Diagnostic.Span = Definition.SourceSpan;
    }
    return Diagnostic;
}

std::string EscapeJsonPointerToken(const std::string_view Token)
{
    std::string Escaped;
    Escaped.reserve(Token.size());
    for (const char Character : Token)
    {
        if (Character == '~') Escaped += "~0";
        else if (Character == '/') Escaped += "~1";
        else Escaped.push_back(Character);
    }
    return Escaped;
}

void ResolveReferences(
    FValue& Value,
    const FCompiledFieldSpec& Spec,
    const std::string& JsonPointer,
    const FValidatedDefinition& Definition,
    const std::map<std::string, FValidatedDefinition*>& Winners,
    const std::map<std::string, std::string>& Redirects,
    const std::set<std::string>& Tombstones,
    const std::string_view SchemaId,
    const std::int64_t SchemaVersion,
    std::vector<FDiagnostic>& Diagnostics)
{
    if (Value.IsNull()) return;
    if (Spec.Kind == EFieldKind::Reference
        || Spec.Kind == EFieldKind::TextId
        || Spec.Kind == EFieldKind::ResourceReference)
    {
        const std::string OriginalTarget = Value.AsString();
        if (Tombstones.contains(OriginalTarget))
        {
            Diagnostics.push_back(MakeResolutionDiagnostic(
                "core:diagnostic.reference.target_tombstoned",
                "Referenced definition is tombstoned",
                Definition, SchemaId, SchemaVersion, JsonPointer));
            return;
        }
        const auto Redirect = Redirects.find(OriginalTarget);
        const std::string& CanonicalTarget = Redirect == Redirects.end()
            ? OriginalTarget : Redirect->second;
        const auto Target = Winners.find(CanonicalTarget);
        if (Target == Winners.end())
        {
            Diagnostics.push_back(MakeResolutionDiagnostic(
                "core:diagnostic.reference.target_missing",
                "Referenced definition does not exist after full-override selection",
                Definition, SchemaId, SchemaVersion, JsonPointer));
            return;
        }
        if (Redirect != Redirects.end()) Value = FValue(CanonicalTarget);
        const FValue* TargetType = Target->second->Value.FindField("type");
        if (TargetType == nullptr || TargetType->AsString() != Spec.ExpectedStableIdKind)
        {
            Diagnostics.push_back(MakeResolutionDiagnostic(
                "core:diagnostic.reference.target_kind_mismatch",
                "Referenced definition has a different kind than the FieldSpec requires",
                Definition, SchemaId, SchemaVersion, JsonPointer));
            return;
        }
        if (Spec.Kind == EFieldKind::ResourceReference)
        {
            const FValue* TargetData = Target->second->Value.FindField("data");
            const FValue* ResourceClass = TargetData != nullptr
                ? TargetData->FindField("resource_class") : nullptr;
            if (ResourceClass == nullptr || !ResourceClass->IsString()
                || ResourceClass->AsString() != Spec.ResourceClass)
            {
                Diagnostics.push_back(MakeResolutionDiagnostic(
                    "core:diagnostic.reference.resource_class_mismatch",
                    "Referenced resource has a different resource_class than the FieldSpec requires",
                    Definition, SchemaId, SchemaVersion, JsonPointer));
            }
        }
        return;
    }
    if (Spec.Kind == EFieldKind::Scalar) return;
    if (Spec.Kind == EFieldKind::Array)
    {
        for (std::size_t Index = 0; Index < Value.AsArray().size(); ++Index)
        {
            ResolveReferences(
                Value.AsArray()[Index], *Spec.Items,
                JsonPointer + "/" + std::to_string(Index), Definition, Winners,
                Redirects, Tombstones, SchemaId, SchemaVersion, Diagnostics);
        }
        return;
    }
    if (Spec.Kind == EFieldKind::Map)
    {
        for (auto& [Name, Entry] : Value.AsObject())
        {
            ResolveReferences(
                Entry, *Spec.MapValues, JsonPointer + "/" + EscapeJsonPointerToken(Name),
                Definition, Winners, Redirects, Tombstones,
                SchemaId, SchemaVersion, Diagnostics);
        }
        return;
    }
    if (Spec.Kind == EFieldKind::Object)
    {
        for (const FCompiledObjectField& Field : Spec.Fields)
        {
            if (FValue* FieldValue = Value.FindField(Field.Name))
            {
                ResolveReferences(
                    *FieldValue, *Field.Spec, JsonPointer + "/" + Field.Name,
                    Definition, Winners, Redirects, Tombstones,
                    SchemaId, SchemaVersion, Diagnostics);
            }
        }
        return;
    }

    const FValue* Tag = Value.FindField(Spec.Discriminator);
    if (Tag == nullptr || !Tag->IsString()) return;
    const auto Variant = std::find_if(
        Spec.Variants.begin(), Spec.Variants.end(),
        [&Tag](const FCompiledUnionVariant& Candidate)
        {
            return Candidate.DiscriminatorValue == Tag->AsString();
        });
    if (Variant != Spec.Variants.end())
    {
        ResolveReferences(
            Value, *Variant->Spec, JsonPointer, Definition, Winners,
            Redirects, Tombstones, SchemaId, SchemaVersion, Diagnostics);
    }
}

FProviderProvenance MakeProviderProvenance(const FValidatedDefinition& Definition)
{
    return FProviderProvenance{
        Definition.Package->GetPackageId(),
        Definition.Package->GetLoadIndex(),
        Definition.RelativeSource,
        Definition.SourceSpan,
        Definition.Schema->GetSchemaId(),
        Definition.Schema->GetKey().SchemaVersion,
    };
}

FValue MakeProviderHashValue(const FProviderProvenance& Provider)
{
    return FValue::MakeObject({
        { "package_id", FValue(Provider.PackageId) },
        { "load_index", FValue(static_cast<std::int64_t>(Provider.PackageLoadIndex)) },
        { "relative_source", FValue(Provider.RelativeSource) },
        { "schema_id", FValue(Provider.SchemaId) },
        { "schema_version", FValue(Provider.SchemaVersion) },
    });
}

FValue MakeProvenanceHashValue(const FDefinitionProvenance& Provenance)
{
    FValue::FArray Chain;
    for (const std::string& Id : Provenance.RedirectChain) Chain.emplace_back(Id);
    FValue::FArray Shadowed;
    for (const FProviderProvenance& Provider : Provenance.ShadowedProviders)
    {
        Shadowed.push_back(MakeProviderHashValue(Provider));
    }
    return FValue::MakeObject({
        { "original_id", FValue(Provenance.OriginalId) },
        { "canonical_id", FValue(Provenance.CanonicalId) },
        { "redirect_chain", FValue::MakeArray(std::move(Chain)) },
        { "winner", MakeProviderHashValue(Provenance.Winner) },
        { "shadowed", FValue::MakeArray(std::move(Shadowed)) },
    });
}
}

FBuildResult BuildRepository(
    const std::vector<FPackageDescriptor>& PackageSet,
    const FBuildOptions& Options)
{
    FBuildResult::FDiagnosticList Diagnostics = ValidatePackageDescriptors(PackageSet);
    if (!Diagnostics.empty())
    {
        return FBuildResult::Failure(std::move(Diagnostics));
    }

    std::vector<const FPackageDescriptor*> OrderedPackages;
    OrderedPackages.reserve(PackageSet.size());
    for (const FPackageDescriptor& Package : PackageSet)
    {
        OrderedPackages.push_back(&Package);
    }
    std::sort(
        OrderedPackages.begin(),
        OrderedPackages.end(),
        [](const FPackageDescriptor* Left, const FPackageDescriptor* Right)
        {
            return Left->GetLoadIndex() < Right->GetLoadIndex();
        });

    std::vector<FParsedSource> ParsedSources;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        std::vector<std::string> OrderedSources = Package->GetRelativeSources();
        for (const FSchemaBinding& Binding : Package->GetSchemaBindings())
        {
            OrderedSources.push_back(Binding.GetRelativePath());
        }
        for (const FExtensionSchemaBinding& Binding : Package->GetExtensionSchemaBindings())
        {
            OrderedSources.push_back(Binding.GetRelativePath());
        }
        std::sort(OrderedSources.begin(), OrderedSources.end());
        OrderedSources.erase(std::unique(OrderedSources.begin(), OrderedSources.end()), OrderedSources.end());
        for (const std::string& RelativeSource : OrderedSources)
        {
            if (Options.SourceProvider == nullptr)
            {
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.repository.source_provider_missing";
                Diagnostic.Message = "Repository build requires an immutable source provider";
                Diagnostic.PackageId = Package->GetPackageId();
                Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
                Diagnostic.RelativeSource = RelativeSource;
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }

            std::optional<std::string> SourceBytes = Options.SourceProvider->ReadSource(
                Package->GetPackageId(), RelativeSource);
            if (!SourceBytes.has_value())
            {
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.repository.source_unavailable";
                Diagnostic.Message = "Resolved source provider did not return the declared source";
                Diagnostic.PackageId = Package->GetPackageId();
                Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
                Diagnostic.RelativeSource = RelativeSource;
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }

            auto Document = ParseJson5Document(
                *SourceBytes,
                Options.ParseLimits,
                Diagnostics,
                Package->GetPackageId(),
                Package->GetLoadIndex(),
                RelativeSource);
            if (Document.has_value())
            {
                ParsedSources.push_back(FParsedSource{ Package, RelativeSource, std::move(*Document) });
            }
        }
    }

    if (!Diagnostics.empty())
    {
        return FBuildResult::Failure(std::move(Diagnostics));
    }

    FSchemaRegistry SchemaRegistry;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        std::vector<const FSchemaBinding*> OrderedBindings;
        OrderedBindings.reserve(Package->GetSchemaBindings().size());
        for (const FSchemaBinding& Binding : Package->GetSchemaBindings())
        {
            OrderedBindings.push_back(&Binding);
        }
        std::sort(
            OrderedBindings.begin(),
            OrderedBindings.end(),
            [](const FSchemaBinding* Left, const FSchemaBinding* Right)
            {
                return std::tuple(
                    Left->GetDefinitionType(),
                    Left->GetSchemaVersion(),
                    Left->GetSchemaId(),
                    Left->GetRelativePath())
                    < std::tuple(
                        Right->GetDefinitionType(),
                        Right->GetSchemaVersion(),
                        Right->GetSchemaId(),
                        Right->GetRelativePath());
            });

        for (const FSchemaBinding* Binding : OrderedBindings)
        {
            const FParsedDocument* Document = FindParsedDocument(
                ParsedSources, Package, Binding->GetRelativePath());
            if (Document == nullptr)
            {
                continue; // A provider/parse diagnostic has already been emitted.
            }
            auto Resource = ParseSchemaResource(
                *Document,
                *Binding,
                Package->GetPackageId(),
                Package->GetLoadIndex(),
                Binding->GetRelativePath(),
                Diagnostics);
            if (Resource.has_value())
            {
                SchemaRegistry.Register(std::move(*Resource), Diagnostics);
            }
        }
    }

    if (!Diagnostics.empty())
    {
        return FBuildResult::Failure(std::move(Diagnostics));
    }

    FExtensionSchemaRegistry ExtensionSchemaRegistry;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        std::vector<const FExtensionSchemaBinding*> OrderedBindings;
        OrderedBindings.reserve(Package->GetExtensionSchemaBindings().size());
        for (const FExtensionSchemaBinding& Binding : Package->GetExtensionSchemaBindings())
        {
            OrderedBindings.push_back(&Binding);
        }
        std::sort(
            OrderedBindings.begin(),
            OrderedBindings.end(),
            [](const FExtensionSchemaBinding* Left, const FExtensionSchemaBinding* Right)
            {
                return std::tuple(
                    Left->GetDefinitionType(), Left->GetSchemaVersion(), Left->GetExtensionSite(),
                    Left->GetExtensionNamespace(), Left->GetSchemaId(), Left->GetRelativePath())
                    < std::tuple(
                        Right->GetDefinitionType(), Right->GetSchemaVersion(), Right->GetExtensionSite(),
                        Right->GetExtensionNamespace(), Right->GetSchemaId(), Right->GetRelativePath());
            });
        for (const FExtensionSchemaBinding* Binding : OrderedBindings)
        {
            const FParsedDocument* Document = FindParsedDocument(
                ParsedSources, Package, Binding->GetRelativePath());
            if (Document == nullptr) continue;
            if (SchemaRegistry.Find(
                    Binding->GetDefinitionType(), Binding->GetSchemaVersion()) == nullptr)
            {
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.extension.schema.target_schema_missing";
                Diagnostic.Message = "Extension schema binding requires an exact definition schema target";
                Diagnostic.PackageId = Package->GetPackageId();
                Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
                Diagnostic.RelativeSource = Binding->GetRelativePath();
                Diagnostic.SchemaId = Binding->GetSchemaId();
                Diagnostic.SchemaVersion = Binding->GetSchemaVersion();
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }
            auto Resource = ParseExtensionSchemaResource(
                *Document,
                *Binding,
                Package->GetPackageId(),
                Package->GetLoadIndex(),
                Binding->GetRelativePath(),
                Diagnostics);
            if (!Resource.has_value()) continue;
            if (const FSchemaResource* Existing = SchemaRegistry.FindById(Resource->GetSchemaId()))
            {
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.extension.schema.schema_id_conflict";
                Diagnostic.Message = "schema_id cannot identify both a definition schema and an extension schema";
                Diagnostic.PackageId = Package->GetPackageId();
                Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
                Diagnostic.RelativeSource = Binding->GetRelativePath();
                Diagnostic.SchemaId = Resource->GetSchemaId();
                Diagnostic.SchemaVersion = Binding->GetSchemaVersion();
                Diagnostic.Span = Resource->GetSourceSpan();
                Diagnostic.RelatedSpan = Existing->GetSourceSpan();
                Diagnostic.RelatedMessage = "Definition schema registered at "
                    + Existing->GetPackageId() + "/" + Existing->GetRelativeSource();
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }
            ExtensionSchemaRegistry.Register(std::move(*Resource), Diagnostics);
        }
    }

    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        for (const FSchemaBinding& Binding : Package->GetSchemaBindings())
        {
            const FParsedDocument* Document = FindParsedDocument(
                ParsedSources, Package, Binding.GetRelativePath());
            const FSchemaResource* Schema = SchemaRegistry.Find(
                Binding.GetDefinitionType(), Binding.GetSchemaVersion());
            if (Document == nullptr || Schema == nullptr) continue;
            FValidationDiagnosticContext Context;
            Context.PackageId = Package->GetPackageId();
            Context.PackageLoadIndex = Package->GetLoadIndex();
            Context.RelativeSource = Binding.GetRelativePath();
            Context.SchemaId = Schema->GetSchemaId();
            Context.SchemaVersion = Binding.GetSchemaVersion();
            FValue MaterializedSchemaExtensions;
            ValidateExtensionBlocks(
                Schema->GetExtensions(),
                MaterializedSchemaExtensions,
                ExtensionSchemaRegistry,
                Binding.GetDefinitionType(),
                Binding.GetSchemaVersion(),
                EExtensionSite::SchemaResource,
                Package->GetNamespace(),
                Document,
                "/extensions",
                Context,
                Diagnostics);
        }
    }

    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    std::vector<FDefinitionFile> DefinitionFiles;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        std::vector<std::string> OrderedDefinitionSources = Package->GetRelativeSources();
        std::sort(OrderedDefinitionSources.begin(), OrderedDefinitionSources.end());
        for (const std::string& RelativeSource : OrderedDefinitionSources)
        {
            const FParsedDocument* Document = FindParsedDocument(ParsedSources, Package, RelativeSource);
            if (Document == nullptr) continue;
            auto DefinitionFile = ParseDefinitionFileEnvelope(
                *Document,
                Package->GetPackageId(),
                Package->GetLoadIndex(),
                RelativeSource,
                Diagnostics);
            if (DefinitionFile.has_value()) DefinitionFiles.push_back(std::move(*DefinitionFile));
        }
    }

    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));
    ValidatePackageDefinitionIds(DefinitionFiles, Diagnostics);
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    std::set<std::string> PriorProviderIds;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        std::vector<std::string> AcceptedPackageIds;
        for (const FDefinitionFile& DefinitionFile : DefinitionFiles)
        {
            if (DefinitionFile.GetPackageId() != Package->GetPackageId()) continue;
            for (const FDefinitionEntry& Definition : DefinitionFile.GetDefinitions())
            {
                FStableIdView ParsedId;
                FStableId::Parse(Definition.GetId(), ParsedId);
                const bool bOwnedNewId = ParsedId.Namespace == Package->GetNamespace();
                const bool bExistingForeignOverride = PriorProviderIds.contains(Definition.GetId());
                if (!bOwnedNewId && !bExistingForeignOverride)
                {
                    FDiagnostic Diagnostic;
                    Diagnostic.Code = "core:diagnostic.repository.identity.foreign_new_id";
                    Diagnostic.Message = "Package may introduce new definitions only in its own namespace";
                    Diagnostic.PackageId = Package->GetPackageId();
                    Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
                    Diagnostic.RelativeSource = DefinitionFile.GetRelativeSource();
                    Diagnostic.DefinitionId = Definition.GetId();
                    Diagnostic.Span = Definition.GetSourceSpan();
                    Diagnostics.push_back(std::move(Diagnostic));
                    continue;
                }
                AcceptedPackageIds.push_back(Definition.GetId());
            }
        }
        PriorProviderIds.insert(AcceptedPackageIds.begin(), AcceptedPackageIds.end());
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    std::vector<FValidatedDefinition> ValidatedDefinitions;
    for (const FDefinitionFile& DefinitionFile : DefinitionFiles)
    {
        const FParsedDocument* Document = nullptr;
        const FPackageDescriptor* Package = nullptr;
        for (const FParsedSource& Source : ParsedSources)
        {
            if (Source.Package->GetPackageId() == DefinitionFile.GetPackageId()
                && Source.RelativeSource == DefinitionFile.GetRelativeSource())
            {
                Document = &Source.Document;
                Package = Source.Package;
                break;
            }
        }
        if (Document == nullptr || Package == nullptr) continue;

        const FSchemaResource* Schema = SchemaRegistry.Find(
            DefinitionFile.GetDefinitionType(), DefinitionFile.GetSchemaVersion());
        if (Schema == nullptr)
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.schema.binding.missing";
            Diagnostic.Message = "No exact schema binding for definition source";
            Diagnostic.PackageId = Package->GetPackageId();
            Diagnostic.PackageLoadIndex = Package->GetLoadIndex();
            Diagnostic.RelativeSource = DefinitionFile.GetRelativeSource();
            Diagnostic.JsonPointer = "/schema_version";
            if (const FParsedLocation* Location = Document->FindLocation("/schema_version"))
            {
                Diagnostic.Span = Location->ValueSpan;
            }
            Diagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        if (Schema->GetCompiledRootSpec() != nullptr)
        {
            FValidationDiagnosticContext FileContext;
            FileContext.PackageId = Package->GetPackageId();
            FileContext.PackageLoadIndex = Package->GetLoadIndex();
            FileContext.RelativeSource = DefinitionFile.GetRelativeSource();
            FileContext.SchemaId = Schema->GetSchemaId();
            FileContext.SchemaVersion = DefinitionFile.GetSchemaVersion();
            FValue MaterializedFileExtensions;
            ValidateExtensionBlocks(
                DefinitionFile.GetExtensions(),
                MaterializedFileExtensions,
                ExtensionSchemaRegistry,
                DefinitionFile.GetDefinitionType(),
                DefinitionFile.GetSchemaVersion(),
                EExtensionSite::DefinitionFile,
                Package->GetNamespace(),
                Document,
                "/extensions",
                FileContext,
                Diagnostics);

            for (const FDefinitionEntry& Definition : DefinitionFile.GetDefinitions())
            {
                FValidationDiagnosticContext Context;
                Context.PackageId = Package->GetPackageId();
                Context.PackageLoadIndex = Package->GetLoadIndex();
                Context.RelativeSource = DefinitionFile.GetRelativeSource();
                Context.DefinitionId = Definition.GetId();
                Context.SchemaId = Schema->GetSchemaId();
                Context.SchemaVersion = DefinitionFile.GetSchemaVersion();
                FValue MaterializedExtensions;
                const bool bExtensionsValid = ValidateExtensionBlocks(
                    Definition.GetExtensions(),
                    MaterializedExtensions,
                    ExtensionSchemaRegistry,
                    DefinitionFile.GetDefinitionType(),
                    DefinitionFile.GetSchemaVersion(),
                    EExtensionSite::DefinitionEntry,
                    Package->GetNamespace(),
                    Document,
                    "/definitions/" + std::to_string(Definition.GetSourceIndex()) + "/extensions",
                    Context,
                    Diagnostics);
                FValue MaterializedData;
                const bool bDataValid = ValidateFieldValue(
                    Definition.GetData(),
                    *Schema->GetCompiledRootSpec(),
                    MaterializedData,
                    Document,
                    "/definitions/" + std::to_string(Definition.GetSourceIndex()) + "/data",
                    Context,
                    Diagnostics);
                if (bDataValid && bExtensionsValid)
                {
                    FValue::FArray Tags;
                    Tags.reserve(Definition.GetTags().size());
                    for (const std::string& Tag : Definition.GetTags()) Tags.emplace_back(Tag);
                    FValue MaterializedDefinition = FValue::MakeObject({
                        { "id", FValue(Definition.GetId()) },
                        { "type", FValue(DefinitionFile.GetDefinitionType()) },
                        { "schema_version", FValue(DefinitionFile.GetSchemaVersion()) },
                        { "data", std::move(MaterializedData) },
                        { "tags", FValue::MakeArray(std::move(Tags)) },
                        { "deprecated", FValue(Definition.IsDeprecated()) },
                        { "extensions", std::move(MaterializedExtensions) },
                    });
                    ValidatedDefinitions.push_back(FValidatedDefinition{
                        std::move(MaterializedDefinition),
                        Schema,
                        Package,
                        Document,
                        DefinitionFile.GetRelativeSource(),
                        Definition.GetSourceIndex(),
                        Definition.GetSourceSpan(),
                    });
                }
            }
        }
    }

    if (!Diagnostics.empty())
    {
        return FBuildResult::Failure(std::move(Diagnostics));
    }

    std::map<std::string, FValidatedDefinition*> Winners;
    std::map<std::string, std::vector<FValidatedDefinition*>> ProvidersById;
    for (FValidatedDefinition& Definition : ValidatedDefinitions)
    {
        const std::string& Id = Definition.Value.FindField("id")->AsString();
        Winners[Id] = &Definition;
        ProvidersById[Id].push_back(&Definition);
    }

    std::map<std::string, std::string> DeclaredRedirects;
    std::map<std::string, const FPackageDescriptor*> RedirectOwners;
    std::set<std::string> Tombstones;
    std::map<std::string, const FPackageDescriptor*> TombstoneOwners;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        for (const FRedirectDescriptor& Redirect : Package->GetRedirects())
        {
            DeclaredRedirects.emplace(Redirect.GetSourceId(), Redirect.GetTargetId());
            RedirectOwners.emplace(Redirect.GetSourceId(), Package);
        }
        for (const std::string& Tombstone : Package->GetTombstones())
        {
            Tombstones.insert(Tombstone);
            TombstoneOwners.emplace(Tombstone, Package);
        }
    }

    for (const auto& [SourceId, TargetId] : DeclaredRedirects)
    {
        (void)TargetId;
        if (Winners.contains(SourceId))
        {
            const FPackageDescriptor* Owner = RedirectOwners.at(SourceId);
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.repository.redirect.active_source_conflict";
            Diagnostic.Message = "Redirect source cannot coexist with an active definition";
            Diagnostic.PackageId = Owner->GetPackageId();
            Diagnostic.PackageLoadIndex = Owner->GetLoadIndex();
            Diagnostic.DefinitionId = SourceId;
            Diagnostics.push_back(std::move(Diagnostic));
        }
    }
    for (const std::string& Tombstone : Tombstones)
    {
        if (Winners.contains(Tombstone))
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.repository.tombstone.active_definition_conflict";
            Diagnostic.Message = "Tombstone cannot coexist with an active definition";
            Diagnostic.PackageId = TombstoneOwners.at(Tombstone)->GetPackageId();
            Diagnostic.PackageLoadIndex = TombstoneOwners.at(Tombstone)->GetLoadIndex();
            Diagnostic.DefinitionId = Tombstone;
            Diagnostics.push_back(std::move(Diagnostic));
        }
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    std::map<std::string, std::string> ResolvedRedirects;
    std::map<std::string, std::vector<std::string>> RedirectChains;
    for (const auto& [SourceId, ImmediateTarget] : DeclaredRedirects)
    {
        (void)ImmediateTarget;
        std::set<std::string> Visited;
        std::vector<std::string> Chain;
        std::string Current = SourceId;
        while (true)
        {
            if (!Visited.insert(Current).second)
            {
                const FPackageDescriptor* Owner = RedirectOwners.at(SourceId);
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.repository.redirect.cycle";
                Diagnostic.Message = "Redirect chain contains a cycle";
                Diagnostic.PackageId = Owner->GetPackageId();
                Diagnostic.PackageLoadIndex = Owner->GetLoadIndex();
                Diagnostic.DefinitionId = SourceId;
                Diagnostics.push_back(std::move(Diagnostic));
                break;
            }
            Chain.push_back(Current);
            const auto Next = DeclaredRedirects.find(Current);
            if (Next == DeclaredRedirects.end())
            {
                if (Tombstones.contains(Current))
                {
                    const FPackageDescriptor* Owner = RedirectOwners.at(SourceId);
                    FDiagnostic Diagnostic;
                    Diagnostic.Code = "core:diagnostic.repository.redirect.target_tombstoned";
                    Diagnostic.Message = "Redirect final target is tombstoned";
                    Diagnostic.PackageId = Owner->GetPackageId();
                    Diagnostic.PackageLoadIndex = Owner->GetLoadIndex();
                    Diagnostic.DefinitionId = SourceId;
                    Diagnostics.push_back(std::move(Diagnostic));
                }
                else if (!Winners.contains(Current))
                {
                    const FPackageDescriptor* Owner = RedirectOwners.at(SourceId);
                    FDiagnostic Diagnostic;
                    Diagnostic.Code = "core:diagnostic.repository.redirect.target_missing";
                    Diagnostic.Message = "Redirect final target is not an active definition";
                    Diagnostic.PackageId = Owner->GetPackageId();
                    Diagnostic.PackageLoadIndex = Owner->GetLoadIndex();
                    Diagnostic.DefinitionId = SourceId;
                    Diagnostics.push_back(std::move(Diagnostic));
                }
                else
                {
                    ResolvedRedirects.emplace(SourceId, Current);
                    RedirectChains.emplace(SourceId, std::move(Chain));
                }
                break;
            }
            Current = Next->second;
        }
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    for (auto& [Id, Definition] : Winners)
    {
        (void)Id;
        ResolveReferences(
            *Definition->Value.FindField("data"),
            *Definition->Schema->GetCompiledRootSpec(),
            "/definitions/" + std::to_string(Definition->SourceIndex) + "/data",
            *Definition,
            Winners,
            ResolvedRedirects,
            Tombstones,
            Definition->Schema->GetSchemaId(),
            Definition->Schema->GetKey().SchemaVersion,
            Diagnostics);

        FValue* Extensions = Definition->Value.FindField("extensions");
        if (Extensions == nullptr) continue;
        for (auto& [ExtensionNamespace, ExtensionValue] : Extensions->AsObject())
        {
            const FExtensionSchemaResource* ExtensionSchema = ExtensionSchemaRegistry.Find(
                Definition->Schema->GetKey().DefinitionType,
                Definition->Schema->GetKey().SchemaVersion,
                EExtensionSite::DefinitionEntry,
                ExtensionNamespace);
            if (ExtensionSchema == nullptr) continue;
            ResolveReferences(
                ExtensionValue,
                *ExtensionSchema->GetCompiledRootSpec(),
                "/definitions/" + std::to_string(Definition->SourceIndex)
                    + "/extensions/" + EscapeJsonPointerToken(ExtensionNamespace),
                *Definition,
                Winners,
                ResolvedRedirects,
                Tombstones,
                ExtensionSchema->GetSchemaId(),
                ExtensionSchema->GetKey().SchemaVersion,
                Diagnostics);
        }
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    FValue::FArray ResolvedDefinitions;
    ResolvedDefinitions.reserve(Winners.size());
    std::vector<const FValidatedDefinition*> ResolvedMetadata;
    ResolvedMetadata.reserve(Winners.size());
    for (const auto& [Id, Definition] : Winners)
    {
        (void)Id;
        ResolvedDefinitions.push_back(Definition->Value);
        ResolvedMetadata.push_back(Definition);
    }

    const FSemanticCandidateView CandidateView(ResolvedDefinitions);
    const FSemanticValidatorRegistry& CoreValidators = GetCoreSemanticValidatorRegistry();
    if (Options.SemanticValidatorRegistry != nullptr)
    {
        for (const std::string& ValidatorId : Options.SemanticValidatorRegistry->ListIds())
        {
            if (CoreValidators.Find(ValidatorId) == nullptr) continue;
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.semantic.validator.duplicate_id";
            Diagnostic.Message = "Semantic validator ID is registered by both core and host registries: "
                + ValidatorId;
            Diagnostics.push_back(std::move(Diagnostic));
        }
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));
    for (std::size_t Index = 0; Index < ResolvedDefinitions.size(); ++Index)
    {
        const FValidatedDefinition& Definition = *ResolvedMetadata[Index];
        FSemanticValidationContext Context;
        Context.PackageId = Definition.Package->GetPackageId();
        Context.PackageLoadIndex = Definition.Package->GetLoadIndex();
        Context.RelativeSource = Definition.RelativeSource;
        Context.DefinitionSpan = Definition.SourceSpan;
        Context.DefinitionId = Definition.Value.FindField("id")->AsString();
        Context.SchemaId = Definition.Schema->GetSchemaId();
        Context.SchemaVersion = Definition.Schema->GetKey().SchemaVersion;
        for (const std::string& ValidatorId : Definition.Schema->GetSemanticValidators())
        {
            const ISemanticValidator* Validator = CoreValidators.Find(ValidatorId);
            if (Validator == nullptr && Options.SemanticValidatorRegistry != nullptr)
            {
                Validator = Options.SemanticValidatorRegistry->Find(ValidatorId);
            }
            if (Validator == nullptr)
            {
                FDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.semantic.validator.unavailable";
                Diagnostic.Message = "Schema declares a semantic validator that is not registered";
                Diagnostic.PackageId = Context.PackageId;
                Diagnostic.PackageLoadIndex = Context.PackageLoadIndex;
                Diagnostic.RelativeSource = Context.RelativeSource;
                Diagnostic.DefinitionId = Context.DefinitionId;
                Diagnostic.SchemaId = Context.SchemaId;
                Diagnostic.SchemaVersion = Context.SchemaVersion;
                Diagnostic.Span = Context.DefinitionSpan;
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }
            Validator->Validate(ResolvedDefinitions[Index], CandidateView, Context, Diagnostics);
        }
    }
    if (!Diagnostics.empty()) return FBuildResult::Failure(std::move(Diagnostics));

    std::map<std::string, FDefinitionProvenance> ProvenanceById;
    for (const auto& [Id, Providers] : ProvidersById)
    {
        FDefinitionProvenance Provenance;
        Provenance.OriginalId = Id;
        Provenance.CanonicalId = Id;
        Provenance.Winner = MakeProviderProvenance(*Providers.back());
        for (std::size_t Index = 0; Index + 1 < Providers.size(); ++Index)
        {
            Provenance.ShadowedProviders.push_back(MakeProviderProvenance(*Providers[Index]));
        }
        ProvenanceById.emplace(Id, std::move(Provenance));
    }
    for (const auto& [SourceId, CanonicalId] : ResolvedRedirects)
    {
        FDefinitionProvenance Provenance = ProvenanceById.at(CanonicalId);
        Provenance.OriginalId = SourceId;
        Provenance.CanonicalId = CanonicalId;
        Provenance.RedirectChain = RedirectChains.at(SourceId);
        ProvenanceById.emplace(SourceId, std::move(Provenance));
    }

    std::map<std::string, std::size_t> ById;
    std::map<std::string, std::vector<std::string>> ByKind;
    for (std::size_t Index = 0; Index < ResolvedDefinitions.size(); ++Index)
    {
        const std::string& Id = ResolvedDefinitions[Index].FindField("id")->AsString();
        const std::string& Kind = ResolvedDefinitions[Index].FindField("type")->AsString();
        ById.emplace(Id, Index);
        ByKind[Kind].push_back(Id);
    }

    FValue RootValue = FValue::MakeObject({
        { "definitions", FValue::MakeArray(std::move(ResolvedDefinitions)) },
    });
    FValue::FArray PackageHashValues;
    FValue::FArray SchemaHashValues;
    for (const FPackageDescriptor* Package : OrderedPackages)
    {
        PackageHashValues.push_back(FValue::MakeObject({
            { "package_id", FValue(Package->GetPackageId()) },
            { "namespace", FValue(Package->GetNamespace()) },
            { "load_index", FValue(static_cast<std::int64_t>(Package->GetLoadIndex())) },
        }));
        std::vector<std::pair<std::string, const FParsedDocument*>> OrderedSchemaDocuments;
        for (const FSchemaBinding& Binding : Package->GetSchemaBindings())
        {
            OrderedSchemaDocuments.emplace_back(
                Binding.GetSchemaId(),
                FindParsedDocument(ParsedSources, Package, Binding.GetRelativePath()));
        }
        for (const FExtensionSchemaBinding& Binding : Package->GetExtensionSchemaBindings())
        {
            OrderedSchemaDocuments.emplace_back(
                Binding.GetSchemaId(),
                FindParsedDocument(ParsedSources, Package, Binding.GetRelativePath()));
        }
        std::sort(OrderedSchemaDocuments.begin(), OrderedSchemaDocuments.end(),
            [](const auto& Left, const auto& Right) { return Left.first < Right.first; });
        for (const auto& [SchemaId, Document] : OrderedSchemaDocuments)
        {
            if (Document == nullptr) continue;
            SchemaHashValues.push_back(FValue::MakeObject({
                { "schema_id", FValue(SchemaId) },
                { "resource", Document->GetRootValue() },
            }));
        }
    }
    FValue::FArray ProvenanceHashValues;
    for (const auto& [Id, Provenance] : ProvenanceById)
    {
        (void)Id;
        ProvenanceHashValues.push_back(MakeProvenanceHashValue(Provenance));
    }
    FValue::FArray RedirectHashValues;
    for (const auto& [SourceId, CanonicalId] : ResolvedRedirects)
    {
        RedirectHashValues.push_back(FValue::MakeObject({
            { "source_id", FValue(SourceId) },
            { "canonical_id", FValue(CanonicalId) },
        }));
    }
    FValue::FArray TombstoneHashValues;
    for (const std::string& Id : Tombstones) TombstoneHashValues.emplace_back(Id);
    const FValue HashPayload = FValue::MakeObject({
        { "format", FValue("gv2_repository_snapshot_v1") },
        { "packages", FValue::MakeArray(std::move(PackageHashValues)) },
        { "schemas", FValue::MakeArray(std::move(SchemaHashValues)) },
        { "active", RootValue },
        { "provenance", FValue::MakeArray(std::move(ProvenanceHashValues)) },
        { "redirects", FValue::MakeArray(std::move(RedirectHashValues)) },
        { "tombstones", FValue::MakeArray(std::move(TombstoneHashValues)) },
    });
    const std::string ContentHash = ComputeCanonicalHash(HashPayload);
    std::shared_ptr<const FRepositorySnapshot> Snapshot(new FRepositorySnapshot(
        std::move(RootValue), std::move(ById), std::move(ByKind),
        std::move(ProvenanceById), std::move(ResolvedRedirects),
        std::move(Tombstones), ContentHash));
    return FBuildResult::Success(FCandidate(std::move(Snapshot)));
}
}
