#include "GV2ContentEditor/ReferenceScanner.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <unordered_set>

namespace GV2ContentEditor
{

namespace
{

bool ReadFileContent(const std::filesystem::path& Path, std::string& OutContent)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open()) return false;
    OutContent.assign(
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
    return true;
}

std::string EscapeJsonPointerSegment(std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~')
        {
            Escaped += "~0";
        }
        else if (Character == '/')
        {
            Escaped += "~1";
        }
        else
        {
            Escaped.push_back(Character);
        }
    }
    return Escaped;
}

void ScanValueSchemaTyped(
    const GV2ContentCore::FValue& Value,
    const GV2ContentCore::FCompiledFieldSpec* Spec,
    const std::string& Pointer,
    const std::string& SourceDefId,
    const std::string& PackageId,
    const std::string& RelativeSource,
    const GV2ContentCore::FParsedDocument* Doc,
    std::vector<FGV2ReferenceItem>& OutItems)
{
    if (Spec == nullptr || Value.IsNull()) return;

    if (Spec->Kind == GV2ContentCore::EFieldKind::Reference && Value.IsString())
    {
        const std::string& Str = Value.AsString();
        if (!Str.empty() && Str != SourceDefId)
        {
            FGV2ReferenceItem Item;
            Item.SourceLocator.PackageId = PackageId;
            Item.SourceLocator.RelativeSource = RelativeSource;
            Item.SourceLocator.DefinitionId = SourceDefId;
            Item.SourceDefinitionId = SourceDefId;
            Item.TargetDefinitionId = Str;
            Item.TargetKind = Spec->ExpectedStableIdKind.empty() ? "*" : Spec->ExpectedStableIdKind;
            Item.ReferenceKind = GV2ContentCore::EFieldKind::Reference;
            Item.RelativeSource = RelativeSource;
            Item.JsonPointer = Pointer;
            if (Doc != nullptr)
            {
                if (const auto* Loc = Doc->FindLocation(Pointer))
                {
                    Item.Line = Loc->ValueSpan.StartLine;
                    Item.Column = Loc->ValueSpan.StartColumn;
                }
            }
            OutItems.push_back(std::move(Item));
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::TextId && Value.IsString())
    {
        const std::string& Str = Value.AsString();
        if (!Str.empty() && Str != SourceDefId)
        {
            FGV2ReferenceItem Item;
            Item.SourceLocator.PackageId = PackageId;
            Item.SourceLocator.RelativeSource = RelativeSource;
            Item.SourceLocator.DefinitionId = SourceDefId;
            Item.SourceDefinitionId = SourceDefId;
            Item.TargetDefinitionId = Str;
            Item.TargetKind = "text";
            Item.ReferenceKind = GV2ContentCore::EFieldKind::TextId;
            Item.RelativeSource = RelativeSource;
            Item.JsonPointer = Pointer;
            if (Doc != nullptr)
            {
                if (const auto* Loc = Doc->FindLocation(Pointer))
                {
                    Item.Line = Loc->ValueSpan.StartLine;
                    Item.Column = Loc->ValueSpan.StartColumn;
                }
            }
            OutItems.push_back(std::move(Item));
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::ResourceReference && Value.IsString())
    {
        const std::string& Str = Value.AsString();
        if (!Str.empty() && Str != SourceDefId)
        {
            FGV2ReferenceItem Item;
            Item.SourceLocator.PackageId = PackageId;
            Item.SourceLocator.RelativeSource = RelativeSource;
            Item.SourceLocator.DefinitionId = SourceDefId;
            Item.SourceDefinitionId = SourceDefId;
            Item.TargetDefinitionId = Str;
            Item.TargetKind = Spec->ResourceClass.empty() ? "resource" : Spec->ResourceClass;
            Item.ReferenceKind = GV2ContentCore::EFieldKind::ResourceReference;
            Item.RelativeSource = RelativeSource;
            Item.JsonPointer = Pointer;
            if (Doc != nullptr)
            {
                if (const auto* Loc = Doc->FindLocation(Pointer))
                {
                    Item.Line = Loc->ValueSpan.StartLine;
                    Item.Column = Loc->ValueSpan.StartColumn;
                }
            }
            OutItems.push_back(std::move(Item));
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Object && Value.IsObject())
    {
        for (const auto& Field : Spec->Fields)
        {
            const auto* ChildVal = Value.FindField(Field.Name);
            if (ChildVal != nullptr)
            {
                ScanValueSchemaTyped(
                    *ChildVal,
                    Field.Spec.get(),
                    Pointer + "/" + EscapeJsonPointerSegment(Field.Name),
                    SourceDefId, PackageId, RelativeSource, Doc, OutItems);
            }
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Array && Value.IsArray())
    {
        const auto& Arr = Value.AsArray();
        for (std::size_t i = 0; i < Arr.size(); ++i)
        {
            ScanValueSchemaTyped(
                Arr[i],
                Spec->Items.get(),
                Pointer + "/" + std::to_string(i),
                SourceDefId, PackageId, RelativeSource, Doc, OutItems);
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Map && Value.IsObject())
    {
        for (const auto& [Key, ChildVal] : Value.AsObject())
        {
            ScanValueSchemaTyped(
                ChildVal,
                Spec->MapValues.get(),
                Pointer + "/" + EscapeJsonPointerSegment(Key),
                SourceDefId, PackageId, RelativeSource, Doc, OutItems);
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Union && Value.IsObject())
    {
        const auto* DiscVal = Value.FindField(Spec->Discriminator);
        if (DiscVal != nullptr && DiscVal->IsString())
        {
            const std::string& DiscStr = DiscVal->AsString();
            for (const auto& Variant : Spec->Variants)
            {
                if (Variant.DiscriminatorValue == DiscStr)
                {
                    ScanValueSchemaTyped(
                        Value,
                        Variant.Spec.get(),
                        Pointer,
                        SourceDefId, PackageId, RelativeSource, Doc, OutItems);
                    break;
                }
            }
        }
        return;
    }
}

} // namespace

void FGV2AuthoringReferenceIndex::Clear()
{
    AllReferences.clear();
    OutgoingMap.clear();
    IncomingMap.clear();
    CompatibleTargetsMap.clear();
    ResourceTargetsMap.clear();
    PendingOutgoingOverlay.clear();
}

void FGV2AuthoringReferenceIndex::BuildIndex(
    const std::vector<std::filesystem::path>& PackageRoots,
    const std::vector<GV2ContentCore::FPackageDescriptor>& Packages,
    const std::vector<FGV2DefinitionSummary>& AllDefinitions)
{
    Clear();

    // Populate definitions by kind / type in CompatibleTargetsMap
    for (const auto& Def : AllDefinitions)
    {
        GV2ContentCore::FStableIdView IdView;
        if (GV2ContentCore::FStableId::Parse(Def.Id, IdView))
        {
            CompatibleTargetsMap[std::string(IdView.Kind)].push_back(Def.Id);
            if (Def.Type != IdView.Kind)
            {
                CompatibleTargetsMap[Def.Type].push_back(Def.Id);
            }
            CompatibleTargetsMap["*"].push_back(Def.Id);
        }
    }

    // Load all schemas and extension schemas across packages
    struct FLoadedPackageSchemas
    {
        std::unordered_map<std::string, GV2ContentCore::FSchemaResource> SchemasByType;
        std::vector<GV2ContentCore::FExtensionSchemaResource> ExtensionSchemas;
    };
    std::vector<FLoadedPackageSchemas> PackageSchemas(Packages.size());

    GV2ContentCore::FParseLimits Limits;
    for (std::size_t PkgIdx = 0; PkgIdx < Packages.size(); ++PkgIdx)
    {
        const auto& Pkg = Packages[PkgIdx];
        const auto& Root = PackageRoots[PkgIdx];

        // Schemas
        for (const auto& Binding : Pkg.GetSchemaBindings())
        {
            std::filesystem::path SchemaPath = Root / Binding.GetRelativePath();
            std::string SchemaContent;
            if (!ReadFileContent(SchemaPath, SchemaContent)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto SchemaDoc = GV2ContentCore::ParseJson5Document(
                SchemaContent, Limits, Diags, Pkg.GetPackageId(), Pkg.GetLoadIndex(), Binding.GetRelativePath());
            if (SchemaDoc.has_value())
            {
                auto SchemaRes = GV2ContentCore::ParseSchemaResource(
                    *SchemaDoc, Binding, Pkg.GetPackageId(), Pkg.GetLoadIndex(), Binding.GetRelativePath(), Diags);
                if (SchemaRes.has_value())
                {
                    PackageSchemas[PkgIdx].SchemasByType.emplace(Binding.GetDefinitionType(), std::move(*SchemaRes));
                }
            }
        }

        // Extension schemas
        for (const auto& ExtBinding : Pkg.GetExtensionSchemaBindings())
        {
            std::filesystem::path ExtPath = Root / ExtBinding.GetRelativePath();
            std::string ExtContent;
            if (!ReadFileContent(ExtPath, ExtContent)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto ExtDoc = GV2ContentCore::ParseJson5Document(
                ExtContent, Limits, Diags, Pkg.GetPackageId(), Pkg.GetLoadIndex(), ExtBinding.GetRelativePath());
            if (ExtDoc.has_value())
            {
                auto ExtRes = GV2ContentCore::ParseExtensionSchemaResource(
                    *ExtDoc, ExtBinding, Pkg.GetPackageId(), Pkg.GetLoadIndex(), ExtBinding.GetRelativePath(), Diags);
                if (ExtRes.has_value())
                {
                    PackageSchemas[PkgIdx].ExtensionSchemas.push_back(std::move(*ExtRes));
                }
            }
        }
    }

    // Now scan all definition files
    for (std::size_t PkgIdx = 0; PkgIdx < Packages.size(); ++PkgIdx)
    {
        const auto& Pkg = Packages[PkgIdx];
        const auto& Root = PackageRoots[PkgIdx];

        for (const std::string& RelSource : Pkg.GetRelativeSources())
        {
            std::filesystem::path FilePath = Root / RelSource;
            std::string Content;
            if (!ReadFileContent(FilePath, Content)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto Doc = GV2ContentCore::ParseJson5Document(
                Content, Limits, Diags, Pkg.GetPackageId(), Pkg.GetLoadIndex(), RelSource);
            if (!Doc.has_value() || !Doc->GetRootValue().IsObject()) continue;

            const auto* TypeVal = Doc->GetRootValue().FindField("type");
            if (TypeVal == nullptr || !TypeVal->IsString()) continue;
            const std::string DefType = TypeVal->AsString();

            // Find schema for DefType
            const GV2ContentCore::FSchemaResource* SchemaRes = nullptr;
            for (std::size_t SearchIdx = 0; SearchIdx < Packages.size(); ++SearchIdx)
            {
                auto It = PackageSchemas[SearchIdx].SchemasByType.find(DefType);
                if (It != PackageSchemas[SearchIdx].SchemasByType.end())
                {
                    SchemaRes = &It->second;
                    break;
                }
            }

            const auto* DefsArr = Doc->GetRootValue().FindField("definitions");
            if (DefsArr == nullptr || !DefsArr->IsArray()) continue;

            for (std::size_t DefIdx = 0; DefIdx < DefsArr->AsArray().size(); ++DefIdx)
            {
                const auto& Def = DefsArr->AsArray()[DefIdx];
                if (!Def.IsObject()) continue;

                const auto* IdVal = Def.FindField("id");
                if (IdVal == nullptr || !IdVal->IsString()) continue;
                const std::string DefId = IdVal->AsString();

                const auto* DataVal = Def.FindField("data");
                const auto* ExtVal = Def.FindField("extensions");

                // If this is a resource definition, index resource class
                if (DefType == "resource" && DataVal != nullptr && DataVal->IsObject())
                {
                    const auto* ClassVal = DataVal->FindField("resource_class");
                    if (ClassVal != nullptr && ClassVal->IsString())
                    {
                        ResourceTargetsMap[ClassVal->AsString()].push_back(DefId);
                    }
                }

                if (SchemaRes != nullptr && DataVal != nullptr)
                {
                    const std::string DefPointer = "/definitions/" + std::to_string(DefIdx);
                    std::vector<FGV2ReferenceItem> DefRefs;

                    ScanValueSchemaTyped(
                        *DataVal,
                        SchemaRes->GetCompiledRootSpec().get(),
                        DefPointer + "/data",
                        DefId,
                        Pkg.GetPackageId(),
                        RelSource,
                        &*Doc,
                        DefRefs);

                    if (ExtVal != nullptr && ExtVal->IsObject())
                    {
                        for (const auto& [ExtNs, ExtBlock] : ExtVal->AsObject())
                        {
                            for (const auto& ExtSchemaPkg : PackageSchemas)
                            {
                                for (const auto& ExtSchema : ExtSchemaPkg.ExtensionSchemas)
                                {
                                    if (ExtSchema.GetKey().DefinitionType == DefType && ExtSchema.GetKey().ExtensionNamespace == ExtNs)
                                    {
                                        ScanValueSchemaTyped(
                                            ExtBlock,
                                            ExtSchema.GetCompiledRootSpec().get(),
                                            DefPointer + "/extensions/" + EscapeJsonPointerSegment(ExtNs),
                                            DefId,
                                            Pkg.GetPackageId(),
                                            RelSource,
                                            &*Doc,
                                            DefRefs);
                                    }
                                }
                            }
                        }
                    }

                    for (auto& Item : DefRefs)
                    {
                        AllReferences.push_back(Item);
                        OutgoingMap[Item.SourceDefinitionId].push_back(Item);
                        IncomingMap[Item.TargetDefinitionId].push_back(Item);
                    }
                }
            }
        }

        // Redirects
        for (const auto& Redirect : Pkg.GetRedirects())
        {
            FGV2ReferenceItem Item;
            Item.SourceLocator.PackageId = Pkg.GetPackageId();
            Item.SourceLocator.RelativeSource = "package.json5";
            Item.SourceLocator.DefinitionId = Redirect.GetSourceId();
            Item.SourceDefinitionId = Redirect.GetSourceId();
            Item.TargetDefinitionId = Redirect.GetTargetId();
            Item.TargetKind = "*";
            Item.ReferenceKind = GV2ContentCore::EFieldKind::Reference;
            Item.RelativeSource = "package.json5";
            Item.JsonPointer = "/redirects/" + EscapeJsonPointerSegment(Redirect.GetSourceId());
            AllReferences.push_back(Item);
            OutgoingMap[Item.SourceDefinitionId].push_back(Item);
            IncomingMap[Item.TargetDefinitionId].push_back(Item);
        }
    }

    // Sort and deduplicate all target maps
    for (auto& [Kind, Targets] : CompatibleTargetsMap)
    {
        std::sort(Targets.begin(), Targets.end());
        Targets.erase(std::unique(Targets.begin(), Targets.end()), Targets.end());
    }
    for (auto& [Class, Targets] : ResourceTargetsMap)
    {
        std::sort(Targets.begin(), Targets.end());
        Targets.erase(std::unique(Targets.begin(), Targets.end()), Targets.end());
    }
}

std::vector<FGV2ReferenceItem> FGV2AuthoringReferenceIndex::GetOutgoingReferences(
    const std::string& SourceDefId) const
{
    auto OverlayIt = PendingOutgoingOverlay.find(SourceDefId);
    if (OverlayIt != PendingOutgoingOverlay.end())
    {
        return OverlayIt->second;
    }

    auto It = OutgoingMap.find(SourceDefId);
    if (It != OutgoingMap.end())
    {
        return It->second;
    }
    return {};
}

std::vector<FGV2ReferenceItem> FGV2AuthoringReferenceIndex::GetIncomingReferences(
    const std::string& TargetDefId) const
{
    std::vector<FGV2ReferenceItem> Result;

    auto BaseIt = IncomingMap.find(TargetDefId);
    if (BaseIt != IncomingMap.end())
    {
        for (const auto& Ref : BaseIt->second)
        {
            auto OverlayIt = PendingOutgoingOverlay.find(Ref.SourceDefinitionId);
            if (OverlayIt != PendingOutgoingOverlay.end())
            {
                bool bFoundInOverlay = false;
                for (const auto& OverlayRef : OverlayIt->second)
                {
                    if (OverlayRef.TargetDefinitionId == TargetDefId && OverlayRef.JsonPointer == Ref.JsonPointer)
                    {
                        bFoundInOverlay = true;
                        break;
                    }
                }
                if (bFoundInOverlay)
                {
                    Result.push_back(Ref);
                }
            }
            else
            {
                Result.push_back(Ref);
            }
        }
    }

    for (const auto& [SourceId, OverlayRefs] : PendingOutgoingOverlay)
    {
        for (const auto& OverlayRef : OverlayRefs)
        {
            if (OverlayRef.TargetDefinitionId == TargetDefId)
            {
                bool bAlreadyPresent = false;
                for (const auto& Existing : Result)
                {
                    if (Existing.SourceDefinitionId == SourceId && Existing.JsonPointer == OverlayRef.JsonPointer)
                    {
                        bAlreadyPresent = true;
                        break;
                    }
                }
                if (!bAlreadyPresent)
                {
                    Result.push_back(OverlayRef);
                }
            }
        }
    }

    return Result;
}

std::vector<std::string> FGV2AuthoringReferenceIndex::GetCompatibleReferenceTargets(
    const std::string& ExpectedKind) const
{
    std::string LookupKind = ExpectedKind.empty() ? "*" : ExpectedKind;
    auto It = CompatibleTargetsMap.find(LookupKind);
    if (It != CompatibleTargetsMap.end())
    {
        return It->second;
    }
    if (LookupKind != "*")
    {
        auto StarIt = CompatibleTargetsMap.find("*");
        if (StarIt != CompatibleTargetsMap.end())
        {
            std::vector<std::string> Filtered;
            for (const auto& Id : StarIt->second)
            {
                GV2ContentCore::FStableIdView IdView;
                if (GV2ContentCore::FStableId::Parse(Id, IdView) && IdView.Kind == ExpectedKind)
                {
                    Filtered.push_back(Id);
                }
            }
            return Filtered;
        }
    }
    return {};
}

std::vector<std::string> FGV2AuthoringReferenceIndex::GetCompatibleResourceTargets(
    const std::string& ResourceClass) const
{
    if (ResourceClass.empty() || ResourceClass == "*")
    {
        std::vector<std::string> All;
        for (const auto& [Class, Targets] : ResourceTargetsMap)
        {
            All.insert(All.end(), Targets.begin(), Targets.end());
        }
        std::sort(All.begin(), All.end());
        All.erase(std::unique(All.begin(), All.end()), All.end());
        return All;
    }

    auto It = ResourceTargetsMap.find(ResourceClass);
    if (It != ResourceTargetsMap.end())
    {
        return It->second;
    }
    return {};
}

void FGV2AuthoringReferenceIndex::SetPendingDefinitionReferences(
    const std::string& DefinitionId,
    const std::vector<FGV2ReferenceItem>& PendingOutgoing)
{
    PendingOutgoingOverlay[DefinitionId] = PendingOutgoing;
}

void FGV2AuthoringReferenceIndex::ClearPendingDefinitionReferences(
    const std::string& DefinitionId)
{
    PendingOutgoingOverlay.erase(DefinitionId);
}

std::vector<FGV2ReferenceItem> FGV2ReferenceScanner::ExtractTypedReferences(
    const GV2ContentCore::FValue& DataValue,
    const GV2ContentCore::FCompiledFieldSpec* RootSpec,
    const GV2ContentCore::FValue& ExtensionsValue,
    const std::vector<GV2ContentCore::FExtensionSchemaResource>& ExtensionSchemas,
    const std::string& SourceDefId,
    const std::string& PackageId,
    const std::string& RelativeSource,
    const GV2ContentCore::FParsedDocument* Doc)
{
    std::vector<FGV2ReferenceItem> Out;
    ScanValueSchemaTyped(DataValue, RootSpec, "/data", SourceDefId, PackageId, RelativeSource, Doc, Out);
    if (!ExtensionsValue.IsNull() && ExtensionsValue.IsObject())
    {
        for (const auto& [ExtNs, ExtBlock] : ExtensionsValue.AsObject())
        {
            for (const auto& ExtSchema : ExtensionSchemas)
            {
                if (ExtSchema.GetKey().ExtensionNamespace == ExtNs)
                {
                    ScanValueSchemaTyped(
                        ExtBlock,
                        ExtSchema.GetCompiledRootSpec().get(),
                        "/extensions/" + EscapeJsonPointerSegment(ExtNs),
                        SourceDefId, PackageId, RelativeSource, Doc, Out);
                }
            }
        }
    }
    return Out;
}

std::vector<FGV2ReferenceItem> FGV2ReferenceScanner::FindOutgoingReferences(
    const FGV2LoadedDefinition& LoadedDef,
    const FGV2SchemaFormModel* FormModel)
{
    std::vector<FGV2ReferenceItem> Out;
    const GV2ContentCore::FCompiledFieldSpec* RootSpec = FormModel ? FormModel->CompiledRootSpec.get() : nullptr;
    ScanValueSchemaTyped(
        LoadedDef.CanonicalData,
        RootSpec,
        "/data",
        LoadedDef.Id,
        LoadedDef.PackageId,
        LoadedDef.RelativeSource,
        nullptr,
        Out);

    std::sort(Out.begin(), Out.end(), [](const FGV2ReferenceItem& A, const FGV2ReferenceItem& B) {
        if (A.TargetDefinitionId != B.TargetDefinitionId) return A.TargetDefinitionId < B.TargetDefinitionId;
        return A.JsonPointer < B.JsonPointer;
    });

    return Out;
}

std::vector<FGV2ReferenceItem> FGV2ReferenceScanner::FindIncomingReferences(
    const std::string& TargetDefinitionId,
    const std::vector<FGV2DefinitionSummary>& AllDefinitions,
    const std::vector<std::filesystem::path>& PackageRoots,
    const std::vector<GV2ContentCore::FPackageDescriptor>& Packages)
{
    FGV2AuthoringReferenceIndex Index;
    Index.BuildIndex(PackageRoots, Packages, AllDefinitions);
    return Index.GetIncomingReferences(TargetDefinitionId);
}

std::vector<std::string> FGV2ReferenceScanner::FindCompatibleTargets(
    const std::string& ExpectedKind,
    const std::vector<FGV2DefinitionSummary>& AllDefinitions)
{
    std::set<std::string> Targets;
    for (const auto& Def : AllDefinitions)
    {
        GV2ContentCore::FStableIdView IdView;
        if (GV2ContentCore::FStableId::Parse(Def.Id, IdView))
        {
            if (ExpectedKind.empty() || ExpectedKind == "*" || IdView.Kind == ExpectedKind || Def.Type == ExpectedKind)
            {
                Targets.insert(Def.Id);
            }
        }
    }
    return std::vector<std::string>(Targets.begin(), Targets.end());
}

} // namespace GV2ContentEditor
