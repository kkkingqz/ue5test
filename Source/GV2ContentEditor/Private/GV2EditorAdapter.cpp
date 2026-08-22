#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/ReferenceScanner.h"
#include "GV2ContentEditor/SchemaFormModel.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentAuthoring/AuthoringTypes.h"
#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

namespace GV2ContentEditor
{

namespace
{

bool ReadFileContent(const std::filesystem::path& Path, std::string& OutContent)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open())
    {
        return false;
    }
    OutContent.assign(
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
    return true;
}

std::string NormalizePointer(const std::string& Pointer)
{
    if (Pointer.empty() || Pointer == "/") return "/";
    if (Pointer.front() != '/') return "/" + Pointer;
    return Pointer;
}

const GV2ContentCore::FValue* ResolveValueByPointer(
    const GV2ContentCore::FValue& Root,
    const std::string& Pointer)
{
    if (Pointer.empty() || Pointer == "/") return &Root;

    const GV2ContentCore::FValue* Current = &Root;
    std::size_t Pos = 0;
    while (Pos < Pointer.size())
    {
        if (Pointer[Pos] == '/') ++Pos;
        if (Pos >= Pointer.size()) break;

        std::size_t NextSlash = Pointer.find('/', Pos);
        std::string Token = (NextSlash == std::string::npos)
            ? Pointer.substr(Pos)
            : Pointer.substr(Pos, NextSlash - Pos);
        Pos = (NextSlash == std::string::npos) ? Pointer.size() : NextSlash;

        if (Current->IsObject())
        {
            Current = Current->FindField(Token);
            if (Current == nullptr) return nullptr;
        }
        else if (Current->IsArray())
        {
            try
            {
                std::size_t Index = std::stoul(Token);
                const auto& Arr = Current->AsArray();
                if (Index >= Arr.size()) return nullptr;
                Current = &Arr[Index];
            }
            catch (...)
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    return Current;
}

const GV2ContentCore::FValue* ResolveDefinitionRelativeValue(
    const FGV2LoadedDefinition& Definition,
    const std::string& Pointer)
{
    if (Pointer == "/data")
    {
        return &Definition.CanonicalData;
    }
    if (Pointer.rfind("/data/", 0) == 0)
    {
        return ResolveValueByPointer(Definition.CanonicalData, Pointer.substr(5));
    }
    if (Pointer == "/extensions")
    {
        return &Definition.Extensions;
    }
    if (Pointer.rfind("/extensions/", 0) == 0)
    {
        return ResolveValueByPointer(Definition.Extensions, Pointer.substr(11));
    }
    return nullptr;
}

} // namespace

bool FGV2EditorAdapter::Initialize(
    const std::filesystem::path& InContentRoot,
    std::vector<FGV2EditorDiagnostic>& OutDiagnostics)
{
    ContentRoot = InContentRoot;
    DiscoveredPackages.clear();
    PackageRoots.clear();
    IndexedDefinitions.clear();
    CurrentDefinition.reset();
    CurrentValues.clear();
    DirtyFields.clear();

    std::error_code Ec;
    if (!std::filesystem::is_directory(ContentRoot, Ec) || Ec)
    {
        FGV2EditorDiagnostic Diag;
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Code = "core:diagnostic.package.discovery.root_not_found";
        Diag.Message = "Content root directory not found: " + ContentRoot.string();
        OutDiagnostics.push_back(std::move(Diag));
        return false;
    }

    std::vector<GV2ContentCore::FDiagnostic> DiscoveryDiags;
    std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Discovered;

    if (std::filesystem::exists(ContentRoot / "package.json5", Ec))
    {
        auto SinglePkg = GV2ContentHostSupport::DiscoverPackageFromDirectory(ContentRoot, DiscoveryDiags);
        if (SinglePkg.has_value())
        {
            PackageRoots = { ContentRoot };
            DiscoveredPackages.push_back(std::move(*SinglePkg));
        }
    }
    else
    {
        Discovered = GV2ContentHostSupport::DiscoverPackagesFromContainer(ContentRoot, DiscoveryDiags, &PackageRoots);
        if (Discovered.has_value())
        {
            DiscoveredPackages = std::move(*Discovered);
        }
    }

    for (const auto& D : DiscoveryDiags)
    {
        OutDiagnostics.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
    }

    if (DiscoveredPackages.empty())
    {
        return false;
    }

    return RefreshIndex(OutDiagnostics);
}

bool FGV2EditorAdapter::RefreshIndex(std::vector<FGV2EditorDiagnostic>& OutDiagnostics)
{
    AuthoringIndex.Clear();
    IndexedDefinitions.clear();
    GV2ContentCore::FParseLimits Limits;

    for (std::size_t PkgIdx = 0; PkgIdx < DiscoveredPackages.size(); ++PkgIdx)
    {
        const auto& PkgDesc = DiscoveredPackages[PkgIdx];
        const auto& PkgRoot = PackageRoots[PkgIdx];

        for (const std::string& RelSource : PkgDesc.GetRelativeSources())
        {
            std::filesystem::path FilePath = PkgRoot / RelSource;
            std::string Content;
            if (!ReadFileContent(FilePath, Content)) continue;

            std::vector<GV2ContentCore::FDiagnostic> ParseDiags;
            auto ParsedDoc = GV2ContentCore::ParseJson5Document(
                Content, Limits, ParseDiags, PkgDesc.GetPackageId(), PkgDesc.GetLoadIndex(), RelSource);

            for (const auto& D : ParseDiags)
            {
                OutDiagnostics.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
            }

            if (!ParsedDoc.has_value()) continue;

            std::vector<GV2ContentCore::FDiagnostic> EnvDiags;
            auto DefFileOpt = GV2ContentCore::ParseDefinitionFileEnvelope(
                *ParsedDoc, PkgDesc.GetPackageId(), PkgDesc.GetLoadIndex(), RelSource, EnvDiags);

            for (const auto& D : EnvDiags)
            {
                OutDiagnostics.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
            }

            if (!DefFileOpt.has_value()) continue;

            for (const auto& Entry : DefFileOpt->GetDefinitions())
            {
                GV2ContentAuthoring::FAuthoringLocator Loc;
                Loc.PackageId = PkgDesc.GetPackageId();
                Loc.RelativeSource = RelSource;
                Loc.DefinitionId = Entry.GetId();
                Loc.DefinitionType = DefFileOpt->GetDefinitionType();
                Loc.LoadIndex = PkgDesc.GetLoadIndex();
                Loc.bDeprecated = Entry.IsDeprecated();
                Loc.Tags = Entry.GetTags();
                AuthoringIndex.AddEntry(Loc);
            }
        }
    }

    AuthoringIndex.Finalize();

    for (const auto& Loc : AuthoringIndex.GetEffectiveDefinitions())
    {
        IndexedDefinitions.push_back(FGV2DefinitionSummary::FromLocator(Loc));
    }

    return true;
}

std::vector<FGV2DefinitionSummary> FGV2EditorAdapter::ListDefinitions(
    const std::optional<std::string>& FilterType) const
{
    std::vector<FGV2DefinitionSummary> Result;
    for (const auto& Loc : AuthoringIndex.GetEffectiveDefinitions(FilterType))
    {
        Result.push_back(FGV2DefinitionSummary::FromLocator(Loc));
    }
    return Result;
}

std::vector<std::string> FGV2EditorAdapter::GetAvailableDefinitionTypes() const
{
    std::set<std::string> Types;
    for (const auto& Pkg : DiscoveredPackages)
    {
        for (const auto& Binding : Pkg.GetSchemaBindings())
        {
            Types.insert(Binding.GetDefinitionType());
        }
    }
    for (const auto& Type : AuthoringIndex.GetAvailableDefinitionTypes())
    {
        Types.insert(Type);
    }
    return std::vector<std::string>(Types.begin(), Types.end());
}

std::optional<FGV2LoadedDefinition> FGV2EditorAdapter::LoadDefinition(
    const std::string& DefinitionId,
    std::vector<FGV2EditorDiagnostic>& OutDiagnostics)
{
    auto WinnerOpt = AuthoringIndex.GetEffectiveWinner(DefinitionId);
    if (!WinnerOpt.has_value())
    {
        FGV2EditorDiagnostic Diag;
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Code = "core:diagnostic.definition_not_found";
        Diag.Message = "Definition not found: " + DefinitionId;
        Diag.StableId = DefinitionId;
        OutDiagnostics.push_back(std::move(Diag));
        return std::nullopt;
    }

    return LoadDefinition(*WinnerOpt, OutDiagnostics);
}

std::optional<FGV2LoadedDefinition> FGV2EditorAdapter::LoadDefinition(
    const GV2ContentAuthoring::FAuthoringLocator& Locator,
    std::vector<FGV2EditorDiagnostic>& OutDiagnostics)
{
    CurrentDefinition.reset();
    CurrentValues.clear();
    DirtyFields.clear();

    if (Locator.DefinitionId.empty() || Locator.PackageId.empty())
    {
        FGV2EditorDiagnostic Diag;
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Code = "core:diagnostic.invalid_locator";
        Diag.Message = "Invalid authoring locator: " + Locator.ToDebugString();
        Diag.StableId = Locator.DefinitionId;
        OutDiagnostics.push_back(std::move(Diag));
        return std::nullopt;
    }

    std::filesystem::path PkgRoot = FindPackageRootById(Locator.PackageId);
    std::filesystem::path FilePath = PkgRoot / Locator.RelativeSource;

    std::string Content;
    if (!ReadFileContent(FilePath, Content))
    {
        FGV2EditorDiagnostic Diag;
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Code = "core:diagnostic.io_read_failed";
        Diag.Message = "Failed to read definition file: " + FilePath.string();
        Diag.RelativeSource = Locator.RelativeSource;
        Diag.StableId = Locator.DefinitionId;
        OutDiagnostics.push_back(std::move(Diag));
        return std::nullopt;
    }

    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FDiagnostic> ParseDiags;
    auto ParsedDoc = GV2ContentCore::ParseJson5Document(
        Content, Limits, ParseDiags, Locator.PackageId, 0, Locator.RelativeSource);

    for (const auto& D : ParseDiags)
    {
        OutDiagnostics.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
    }

    if (!ParsedDoc.has_value() || !ParsedDoc->GetRootValue().IsObject())
    {
        return std::nullopt;
    }

    const auto* DefsArr = ParsedDoc->GetRootValue().FindField("definitions");
    if (DefsArr == nullptr || !DefsArr->IsArray())
    {
        return std::nullopt;
    }

    const GV2ContentCore::FValue* TargetDefVal = nullptr;
    for (const auto& Def : DefsArr->AsArray())
    {
        if (!Def.IsObject()) continue;
        const auto* IdVal = Def.FindField("id");
        if (IdVal != nullptr && IdVal->IsString() && IdVal->AsString() == Locator.DefinitionId)
        {
            TargetDefVal = &Def;
            break;
        }
    }

    if (TargetDefVal == nullptr)
    {
        FGV2EditorDiagnostic Diag;
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Code = "core:diagnostic.definition_not_found";
        Diag.Message = "Definition not found in source file: " + Locator.DefinitionId;
        Diag.RelativeSource = Locator.RelativeSource;
        Diag.StableId = Locator.DefinitionId;
        OutDiagnostics.push_back(std::move(Diag));
        return std::nullopt;
    }

    FGV2LoadedDefinition Loaded;
    Loaded.Id = Locator.DefinitionId;
    Loaded.Type = Locator.DefinitionType;
    Loaded.PackageId = Locator.PackageId;
    Loaded.RelativeSource = Locator.RelativeSource;
    Loaded.AbsolutePath = FilePath;
    Loaded.Locator = Locator;
    Loaded.Stamp = GV2ContentAuthoring::FFileStateStamp::FromContent(Content);

    const auto* DataField = TargetDefVal->FindField("data");
    if (DataField != nullptr)
    {
        Loaded.CanonicalData = *DataField;
    }
    else
    {
        Loaded.CanonicalData = GV2ContentCore::FValue::MakeObject({});
    }

    const auto* TagsField = TargetDefVal->FindField("tags");
    if (TagsField != nullptr && TagsField->IsArray())
    {
        for (const auto& T : TagsField->AsArray())
        {
            if (T.IsString()) Loaded.Tags.push_back(T.AsString());
        }
    }

    const auto* DepField = TargetDefVal->FindField("deprecated");
    if (DepField != nullptr && DepField->IsBoolean())
    {
        Loaded.bDeprecated = DepField->AsBoolean();
    }

    const auto* ExtField = TargetDefVal->FindField("extensions");
    if (ExtField != nullptr)
    {
        Loaded.Extensions = *ExtField;
    }

    CurrentDefinition = Loaded;
    return CurrentDefinition;
}

std::optional<GV2ContentAuthoring::FAuthoringLocator> FGV2EditorAdapter::GetCurrentLocator() const
{
    if (CurrentDefinition.has_value())
    {
        return CurrentDefinition->Locator;
    }
    return std::nullopt;
}

std::vector<GV2ContentAuthoring::FAuthoringLocator> FGV2EditorAdapter::GetProvidersForDefinition(
    const std::string& DefinitionId) const
{
    return AuthoringIndex.GetEntriesForDefinition(DefinitionId);
}

const FGV2LoadedDefinition* FGV2EditorAdapter::GetCurrentDefinition() const
{
    return CurrentDefinition.has_value() ? &(*CurrentDefinition) : nullptr;
}

void FGV2EditorAdapter::SetCurrentFieldValue(
    const std::string& JsonPointer,
    const GV2ContentCore::FValue& NewValue)
{
    if (!CurrentDefinition.has_value()) return;

    std::string NormPtr = NormalizePointer(JsonPointer);
    if (NormPtr.rfind("/definitions/", 0) == 0)
    {
        return;
    }
    CurrentValues[NormPtr] = NewValue;

    const auto* BaselineVal = ResolveDefinitionRelativeValue(*CurrentDefinition, NormPtr);
    if (BaselineVal != nullptr && *BaselineVal == NewValue)
    {
        DirtyFields.erase(NormPtr);
    }
    else
    {
        DirtyFields[NormPtr] = NewValue;
    }
}

std::optional<GV2ContentCore::FValue> FGV2EditorAdapter::GetCurrentFieldValue(
    const std::string& JsonPointer) const
{
    if (!CurrentDefinition.has_value()) return std::nullopt;

    std::string NormPtr = NormalizePointer(JsonPointer);
    if (NormPtr.rfind("/definitions/", 0) == 0)
    {
        return std::nullopt;
    }
    auto It = CurrentValues.find(NormPtr);
    if (It != CurrentValues.end())
    {
        return It->second;
    }

    const auto* BaselineVal = ResolveDefinitionRelativeValue(*CurrentDefinition, NormPtr);
    if (BaselineVal != nullptr)
    {
        return *BaselineVal;
    }
    return std::nullopt;
}

bool FGV2EditorAdapter::IsDirty() const
{
    return !DirtyFields.empty();
}

void FGV2EditorAdapter::DiscardCurrentChanges()
{
    CurrentValues.clear();
    DirtyFields.clear();
}

bool FGV2EditorAdapter::CheckFileState() const
{
    if (!CurrentDefinition.has_value()) return true;

    auto DiskStamp = GV2ContentAuthoring::FFileStateStamp::FromFile(CurrentDefinition->AbsolutePath);
    return DiskStamp.Matches(CurrentDefinition->Stamp);
}

FGV2EditorAuthoringResult FGV2EditorAdapter::SaveCurrentDefinition()
{
    FGV2EditorAuthoringResult Result;
    if (!CurrentDefinition.has_value())
    {
        Result.Outcome = EEditorAuthoringOutcome::DefinitionNotFound;
        Result.ErrorCode = "no_definition_loaded";
        Result.ErrorMessage = "No definition currently loaded in editor adapter";
        return Result;
    }

    if (DirtyFields.empty())
    {
        Result.Outcome = EEditorAuthoringOutcome::Success;
        Result.AffectedFile = CurrentDefinition->AbsolutePath;
        Result.NewStamp = CurrentDefinition->Stamp;
        return Result;
    }

    // CED-07: check stale file state
    if (!CheckFileState())
    {
        Result.Outcome = EEditorAuthoringOutcome::StaleFileState;
        Result.ErrorCode = "stale_file_state";
        Result.ErrorMessage = "File on disk has been modified externally since load";
        Result.AffectedFile = CurrentDefinition->AbsolutePath;
        return Result;
    }

    GV2ContentAuthoring::FBatchSetFieldsParams BatchParams;
    BatchParams.PackageRoot = FindPackageRootById(CurrentDefinition->PackageId);
    BatchParams.DefinitionId = CurrentDefinition->Id;
    BatchParams.ExpectedStamp = CurrentDefinition->Stamp;

    for (const auto& [Ptr, Val] : DirtyFields)
    {
        GV2ContentAuthoring::FFieldChange Change;
        Change.JsonPointer = Ptr;
        Change.NewValue = Val;
        BatchParams.Changes.push_back(std::move(Change));
    }

    auto AuthResult = GV2ContentAuthoring::FAuthoringService::BatchSetFields(BatchParams);
    Result = ConvertAuthoringResult(AuthResult);

    if (Result.IsSuccess())
    {
        // Reload exact definition locator into memory to update baseline and stamp
        const auto SavedLocator = CurrentDefinition->Locator;
        std::vector<FGV2EditorDiagnostic> ReloadDiags;
        LoadDefinition(SavedLocator, ReloadDiags);
        for (auto& D : ReloadDiags)
        {
            Result.Diagnostics.push_back(std::move(D));
        }
    }

    return Result;
}

FGV2EditorAuthoringResult FGV2EditorAdapter::CreateDefinition(
    const std::string& PackageId,
    const std::string& DefinitionId,
    const std::string& DefinitionType,
    const std::optional<GV2ContentCore::FValue>& InitialData)
{
    GV2ContentAuthoring::FCreateDefinitionParams Params;
    Params.PackageRoot = FindPackageRootById(PackageId);
    Params.DefinitionId = DefinitionId;
    Params.DefinitionType = DefinitionType;
    Params.InitialData = InitialData;

    auto AuthResult = GV2ContentAuthoring::FAuthoringService::CreateDefinition(Params);
    auto Result = ConvertAuthoringResult(AuthResult);

    if (Result.IsSuccess())
    {
        std::vector<FGV2EditorDiagnostic> Diags;
        RefreshIndex(Diags);
    }

    return Result;
}

FGV2EditorAuthoringResult FGV2EditorAdapter::DuplicateDefinition(
    const std::string& SourceDefinitionId,
    const std::string& TargetDefinitionId)
{
    GV2ContentAuthoring::FDuplicateDefinitionParams Params;
    Params.PackageRoot = FindPackageRootForDefinition(SourceDefinitionId);
    Params.SourceDefinitionId = SourceDefinitionId;
    Params.TargetDefinitionId = TargetDefinitionId;
    if (CurrentDefinition.has_value() && CurrentDefinition->Id == SourceDefinitionId)
    {
        Params.ExpectedStamp = CurrentDefinition->Stamp;
    }

    auto AuthResult = GV2ContentAuthoring::FAuthoringService::DuplicateDefinition(Params);
    auto Result = ConvertAuthoringResult(AuthResult);

    if (Result.IsSuccess())
    {
        std::vector<FGV2EditorDiagnostic> Diags;
        RefreshIndex(Diags);
    }

    return Result;
}

FGV2EditorAuthoringResult FGV2EditorAdapter::DeleteDefinition(
    const std::string& DefinitionId)
{
    GV2ContentAuthoring::FDeleteDefinitionParams Params;
    Params.PackageRoot = FindPackageRootForDefinition(DefinitionId);
    Params.DefinitionId = DefinitionId;
    if (CurrentDefinition.has_value() && CurrentDefinition->Id == DefinitionId)
    {
        Params.ExpectedStamp = CurrentDefinition->Stamp;
    }

    auto AuthResult = GV2ContentAuthoring::FAuthoringService::DeleteDefinition(Params);
    auto Result = ConvertAuthoringResult(AuthResult);

    if (Result.IsSuccess())
    {
        if (CurrentDefinition.has_value() && CurrentDefinition->Id == DefinitionId)
        {
            CurrentDefinition.reset();
            CurrentValues.clear();
            DirtyFields.clear();
        }
        std::vector<FGV2EditorDiagnostic> Diags;
        RefreshIndex(Diags);
    }

    return Result;
}

FGV2EditorAuthoringResult FGV2EditorAdapter::RenameDefinition(
    const std::string& OldDefinitionId,
    const std::string& NewDefinitionId)
{
    GV2ContentAuthoring::FRenameDefinitionParams Params;
    Params.PackageRoot = FindPackageRootForDefinition(OldDefinitionId);
    Params.OldDefinitionId = OldDefinitionId;
    Params.NewDefinitionId = NewDefinitionId;
    if (CurrentDefinition.has_value() && CurrentDefinition->Id == OldDefinitionId)
    {
        Params.ExpectedStamp = CurrentDefinition->Stamp;
    }

    auto AuthResult = GV2ContentAuthoring::FAuthoringService::RenameDefinition(Params);
    auto Result = ConvertAuthoringResult(AuthResult);

    if (Result.IsSuccess())
    {
        std::vector<FGV2EditorDiagnostic> Diags;
        RefreshIndex(Diags);
        if (CurrentDefinition.has_value() && CurrentDefinition->Id == OldDefinitionId)
        {
            LoadDefinition(NewDefinitionId, Diags);
        }
    }

    return Result;
}

std::vector<FGV2EditorDiagnostic> FGV2EditorAdapter::ValidateRepository() const
{
    std::vector<FGV2EditorDiagnostic> Out;

    struct FMemoryContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
    {
        const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors;
        const std::vector<std::filesystem::path>& Roots;

        FMemoryContentSourceProvider(
            const std::vector<GV2ContentCore::FPackageDescriptor>& InDesc,
            const std::vector<std::filesystem::path>& InRoots)
            : Descriptors(InDesc), Roots(InRoots) {}

        std::optional<std::string> ReadSource(
            std::string_view RequestedPackageId,
            std::string_view RelativeSource) const override
        {
            for (std::size_t i = 0; i < Descriptors.size(); ++i)
            {
                if (Descriptors[i].GetPackageId() == RequestedPackageId)
                {
                    std::string Content;
                    if (ReadFileContent(Roots[i] / std::string(RelativeSource), Content))
                    {
                        return Content;
                    }
                }
            }
            return std::nullopt;
        }
    };

    FMemoryContentSourceProvider SourceProvider(DiscoveredPackages, PackageRoots);
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &SourceProvider;
    auto BuildResult = GV2ContentCore::BuildRepository(DiscoveredPackages, Options);

    if (BuildResult.IsFailure())
    {
        for (const auto& D : BuildResult.GetDiagnostics())
        {
            Out.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
        }
    }

    return Out;
}

std::optional<FGV2SchemaFormModel> FGV2EditorAdapter::GetFormModelForDefinitionType(
    const std::string& DefinitionType,
    const std::optional<std::string>& OwningPackageId) const
{
    const GV2ContentCore::FPackageDescriptor* SchemaPkgDesc = nullptr;
    const GV2ContentCore::FSchemaBinding* MatchingBinding = nullptr;
    std::filesystem::path SchemaPkgRoot;

    for (std::size_t i = 0; i < DiscoveredPackages.size(); ++i)
    {
        for (const auto& Binding : DiscoveredPackages[i].GetSchemaBindings())
        {
            if (Binding.GetDefinitionType() == DefinitionType)
            {
                SchemaPkgDesc = &DiscoveredPackages[i];
                MatchingBinding = &Binding;
                SchemaPkgRoot = PackageRoots[i];
                break;
            }
        }
        if (SchemaPkgDesc != nullptr) break;
    }

    if (SchemaPkgDesc == nullptr || MatchingBinding == nullptr)
    {
        return std::nullopt;
    }

    std::filesystem::path SchemaPath = SchemaPkgRoot / MatchingBinding->GetRelativePath();
    std::string SchemaContent;
    if (!ReadFileContent(SchemaPath, SchemaContent))
    {
        return std::nullopt;
    }

    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    auto SchemaDoc = GV2ContentCore::ParseJson5Document(
        SchemaContent, Limits, Diags, SchemaPkgDesc->GetPackageId(), SchemaPkgDesc->GetLoadIndex(), MatchingBinding->GetRelativePath());
    if (!SchemaDoc.has_value()) return std::nullopt;

    auto SchemaResourceOpt = GV2ContentCore::ParseSchemaResource(
        *SchemaDoc, *MatchingBinding, SchemaPkgDesc->GetPackageId(), SchemaPkgDesc->GetLoadIndex(), MatchingBinding->GetRelativePath(), Diags);
    if (!SchemaResourceOpt.has_value()) return std::nullopt;

    // Load UI metadata if present
    std::string UiMetaRelPath = MatchingBinding->GetRelativePath();
    if (UiMetaRelPath.size() >= 13 && UiMetaRelPath.compare(UiMetaRelPath.size() - 13, 13, ".schema.json5") == 0)
    {
        UiMetaRelPath = UiMetaRelPath.substr(0, UiMetaRelPath.size() - 13) + ".ui.json5";
    }
    else if (UiMetaRelPath.size() >= 6 && UiMetaRelPath.compare(UiMetaRelPath.size() - 6, 6, ".json5") == 0)
    {
        UiMetaRelPath = UiMetaRelPath.substr(0, UiMetaRelPath.size() - 6) + ".ui.json5";
    }
    else
    {
        UiMetaRelPath += ".ui.json5";
    }

    std::optional<GV2ContentCore::FSchemaUiMetadata> UiMeta;
    std::filesystem::path UiMetaPath = SchemaPkgRoot / UiMetaRelPath;
    std::string UiMetaContent;
    if (ReadFileContent(UiMetaPath, UiMetaContent))
    {
        std::vector<GV2ContentCore::FDiagnostic> UiDiags;
        UiMeta = GV2ContentCore::ParseSchemaUiMetadata(
            UiMetaContent, *SchemaResourceOpt, UiDiags, SchemaPkgDesc->GetPackageId(), UiMetaRelPath);
    }

    // Load any extension schemas
    std::vector<GV2ContentCore::FExtensionSchemaResource> ExtensionSchemas;
    for (std::size_t i = 0; i < DiscoveredPackages.size(); ++i)
    {
        for (const auto& ExtBinding : DiscoveredPackages[i].GetExtensionSchemaBindings())
        {
            if (ExtBinding.GetDefinitionType() == DefinitionType
                && (!OwningPackageId.has_value()
                    || ExtBinding.GetExtensionNamespace() == *OwningPackageId))
            {
                std::filesystem::path ExtPath = PackageRoots[i] / ExtBinding.GetRelativePath();
                std::string ExtContent;
                if (ReadFileContent(ExtPath, ExtContent))
                {
                    std::vector<GV2ContentCore::FDiagnostic> ExtDiags;
                    auto ExtDoc = GV2ContentCore::ParseJson5Document(
                        ExtContent, Limits, ExtDiags, DiscoveredPackages[i].GetPackageId(), DiscoveredPackages[i].GetLoadIndex(), ExtBinding.GetRelativePath());
                    if (ExtDoc.has_value())
                    {
                        auto ExtResOpt = GV2ContentCore::ParseExtensionSchemaResource(
                            *ExtDoc, ExtBinding, DiscoveredPackages[i].GetPackageId(), DiscoveredPackages[i].GetLoadIndex(), ExtBinding.GetRelativePath(), ExtDiags);
                        if (ExtResOpt.has_value())
                        {
                            ExtensionSchemas.push_back(std::move(*ExtResOpt));
                        }
                    }
                }
            }
        }
    }

    return FGV2SchemaFormModel::BuildFromSchema(
        *SchemaResourceOpt,
        UiMeta.has_value() ? &(*UiMeta) : nullptr,
        ExtensionSchemas);
}

std::vector<FGV2ReferenceItem> FGV2EditorAdapter::GetOutgoingReferences() const
{
    if (!CurrentDefinition.has_value())
    {
        return {};
    }
    return FGV2ReferenceScanner::FindOutgoingReferences(*CurrentDefinition);
}

std::vector<FGV2ReferenceItem> FGV2EditorAdapter::GetIncomingReferences(
    const std::string& DefinitionId) const
{
    return FGV2ReferenceScanner::FindIncomingReferences(
        DefinitionId,
        IndexedDefinitions,
        PackageRoots,
        DiscoveredPackages);
}

std::vector<std::string> FGV2EditorAdapter::GetCompatibleReferenceTargets(
    const std::string& ExpectedKind) const
{
    return FGV2ReferenceScanner::FindCompatibleTargets(
        ExpectedKind,
        IndexedDefinitions);
}

std::vector<std::string> FGV2EditorAdapter::GetCompatibleResourceTargets(
    const std::string& ResourceClass) const
{
    std::vector<std::string> Result;
    GV2ContentCore::FParseLimits Limits;
    for (const auto& Summary : IndexedDefinitions)
    {
        if (Summary.Type != "resource") continue;
        std::string Content;
        if (!ReadFileContent(FindPackageRootById(Summary.PackageId) / Summary.RelativeSource, Content)) continue;
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        auto Document = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diagnostics, Summary.PackageId, 0, Summary.RelativeSource);
        if (!Document.has_value()) continue;
        const auto* Definitions = Document->GetRootValue().FindField("definitions");
        if (Definitions == nullptr || !Definitions->IsArray()) continue;
        for (const auto& Definition : Definitions->AsArray())
        {
            const auto* Id = Definition.FindField("id");
            const auto* Data = Definition.FindField("data");
            const auto* Class = Data && Data->IsObject() ? Data->FindField("resource_class") : nullptr;
            if (Id && Id->IsString() && Id->AsString() == Summary.Id
                && Class && Class->IsString() && Class->AsString() == ResourceClass)
            {
                Result.push_back(Summary.Id);
                break;
            }
        }
    }
    std::sort(Result.begin(), Result.end());
    return Result;
}

std::filesystem::path FGV2EditorAdapter::FindPackageRootById(const std::string& PackageId) const
{
    for (std::size_t i = 0; i < DiscoveredPackages.size(); ++i)
    {
        if (DiscoveredPackages[i].GetPackageId() == PackageId)
        {
            return PackageRoots[i];
        }
    }
    return ContentRoot;
}

std::filesystem::path FGV2EditorAdapter::FindPackageRootForDefinition(const std::string& DefinitionId) const
{
    for (const auto& Summary : IndexedDefinitions)
    {
        if (Summary.Id == DefinitionId)
        {
            return FindPackageRootById(Summary.PackageId);
        }
    }

    GV2ContentCore::FStableIdView IdView;
    if (GV2ContentCore::FStableId::Parse(DefinitionId, IdView))
    {
        return FindPackageRootById(std::string(IdView.Namespace));
    }

    return ContentRoot;
}

FGV2EditorAuthoringResult FGV2EditorAdapter::ConvertAuthoringResult(
    const GV2ContentAuthoring::FAuthoringResult& InResult)
{
    FGV2EditorAuthoringResult Out;
    Out.ErrorCode = InResult.ErrorCode;
    Out.ErrorMessage = InResult.ErrorMessage;
    Out.AffectedFile = InResult.TargetFilePath;
    Out.NewStamp = InResult.NewStamp;
    Out.AffectedDefinitionsCount = InResult.AffectedDefinitionsCount;

    switch (InResult.Status)
    {
    case GV2ContentAuthoring::EAuthoringStatus::Success:
        Out.Outcome = EEditorAuthoringOutcome::Success;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::StaleFileState:
        Out.Outcome = EEditorAuthoringOutcome::StaleFileState;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::ValidationFailed:
        Out.Outcome = EEditorAuthoringOutcome::ValidationFailed;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::DefinitionNotFound:
        Out.Outcome = EEditorAuthoringOutcome::DefinitionNotFound;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::DuplicateDefinitionId:
        Out.Outcome = EEditorAuthoringOutcome::DuplicateDefinitionId;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::InvalidDefinitionId:
        Out.Outcome = EEditorAuthoringOutcome::InvalidDefinitionId;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::IdKindMismatch:
        Out.Outcome = EEditorAuthoringOutcome::IdKindMismatch;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::PackageNotFound:
        Out.Outcome = EEditorAuthoringOutcome::PackageNotFound;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::SchemaNotFound:
        Out.Outcome = EEditorAuthoringOutcome::SchemaNotFound;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::PointerNotFound:
        Out.Outcome = EEditorAuthoringOutcome::PointerNotFound;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::TargetIsContainer:
        Out.Outcome = EEditorAuthoringOutcome::TargetIsContainer;
        break;
    case GV2ContentAuthoring::EAuthoringStatus::FileWriteFailed:
    default:
        Out.Outcome = EEditorAuthoringOutcome::IOError;
        break;
    }

    for (const auto& D : InResult.Diagnostics)
    {
        Out.Diagnostics.push_back(FGV2EditorDiagnostic::FromDiagnostic(D));
    }

    return Out;
}

} // namespace GV2ContentEditor
