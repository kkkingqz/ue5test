#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentAuthoring/Json5AstRewriter.h"

#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace GV2ContentAuthoring
{

namespace
{

constexpr std::array<std::uint32_t, 64> K{
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

std::uint32_t RotateRight(const std::uint32_t Value, const int Amount)
{
    return (Value >> Amount) | (Value << (32 - Amount));
}

std::string ComputeSha256Hex(std::string Input)
{
    const std::uint64_t BitLength = static_cast<std::uint64_t>(Input.size()) * 8;
    Input.push_back(static_cast<char>(0x80));
    while ((Input.size() % 64) != 56) Input.push_back('\0');
    for (int Shift = 56; Shift >= 0; Shift -= 8)
    {
        Input.push_back(static_cast<char>((BitLength >> Shift) & 0xff));
    }

    std::array<std::uint32_t, 8> Hash{
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    for (std::size_t Offset = 0; Offset < Input.size(); Offset += 64)
    {
        std::array<std::uint32_t, 64> Words{};
        for (std::size_t Index = 0; Index < 16; ++Index)
        {
            const auto* Bytes = reinterpret_cast<const unsigned char*>(Input.data() + Offset + Index * 4);
            Words[Index] = (static_cast<std::uint32_t>(Bytes[0]) << 24)
                | (static_cast<std::uint32_t>(Bytes[1]) << 16)
                | (static_cast<std::uint32_t>(Bytes[2]) << 8)
                | static_cast<std::uint32_t>(Bytes[3]);
        }
        for (std::size_t Index = 16; Index < 64; ++Index)
        {
            const std::uint32_t S0 = RotateRight(Words[Index - 15], 7)
                ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
            const std::uint32_t S1 = RotateRight(Words[Index - 2], 17)
                ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
            Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
        }
        auto [A, B, C, D, E, F, G, H] = Hash;
        for (std::size_t Index = 0; Index < 64; ++Index)
        {
            const std::uint32_t S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const std::uint32_t Choice = (E & F) ^ ((~E) & G);
            const std::uint32_t Temp1 = H + S1 + Choice + K[Index] + Words[Index];
            const std::uint32_t S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const std::uint32_t Majority = (A & B) ^ (A & C) ^ (B & C);
            const std::uint32_t Temp2 = S0 + Majority;
            H = G; G = F; F = E; E = D + Temp1;
            D = C; C = B; B = A; A = Temp1 + Temp2;
        }
        Hash[0] += A; Hash[1] += B; Hash[2] += C; Hash[3] += D;
        Hash[4] += E; Hash[5] += F; Hash[6] += G; Hash[7] += H;
    }

    std::ostringstream HexStream;
    HexStream << std::hex << std::setfill('0');
    for (const std::uint32_t Val : Hash)
    {
        HexStream << std::setw(8) << Val;
    }
    return HexStream.str();
}

bool ReadEntireFile(const std::filesystem::path& Path, std::string& OutContent)
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

bool AtomicWriteFile(const std::filesystem::path& TargetPath, const std::string& Content, std::string& OutError)
{
    std::error_code Ec;
    std::filesystem::path ParentDir = TargetPath.parent_path();
    if (!ParentDir.empty() && !std::filesystem::exists(ParentDir, Ec))
    {
        std::filesystem::create_directories(ParentDir, Ec);
        if (Ec)
        {
            OutError = "Failed to create directory: " + ParentDir.string() + " (" + Ec.message() + ")";
            return false;
        }
    }

    const auto Now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::filesystem::path TempPath = TargetPath.string() + ".tmp." + std::to_string(Now);

    {
        std::ofstream Stream(TempPath, std::ios::binary | std::ios::trunc);
        if (!Stream.is_open())
        {
            OutError = "Failed to open temporary file for writing: " + TempPath.string();
            return false;
        }
        Stream.write(Content.data(), static_cast<std::streamsize>(Content.size()));
        Stream.flush();
        if (!Stream.good())
        {
            OutError = "Failed to write content to temporary file: " + TempPath.string();
            Stream.close();
            std::filesystem::remove(TempPath, Ec);
            return false;
        }
    }

    std::filesystem::rename(TempPath, TargetPath, Ec);
    if (Ec)
    {
        const std::string RenameError = Ec.message();
        std::error_code RemoveError;
        std::filesystem::remove(TempPath, RemoveError);
        OutError = "Failed to atomically replace target file: " + TargetPath.string()
            + " (" + RenameError + ")";
        return false;
    }

    return true;
}

struct FDiscoveredPackageSet
{
    bool bSuccess = false;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::vector<GV2ContentCore::FPackageDescriptor> Descriptors;
    std::vector<std::filesystem::path> Roots;
    std::size_t TargetIndex = 0;
};

FDiscoveredPackageSet DiscoverSet(const std::filesystem::path& TargetRoot)
{
    FDiscoveredPackageSet Set;
    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(TargetRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : TargetRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        Set.ErrorCode = "package_root_not_found";
        Set.ErrorMessage = "Package root not found or not a directory: " + Root.string();
        return Set;
    }

    std::vector<std::filesystem::path> Roots = { Root };

    // Resolve dependencies for single root
    const std::filesystem::path Parent = Root.parent_path();
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    auto TargetDesc = GV2ContentHostSupport::DiscoverPackageFromDirectory(Root, Diags);
    if (TargetDesc && TargetDesc->GetPackageId() != "core" && std::filesystem::is_directory(Parent, Ec))
    {
        std::vector<std::string> DepQueue;
        std::set<std::string> VisitedDeps;
        for (const auto& Dep : TargetDesc->GetDependencies())
        {
            if (VisitedDeps.insert(Dep.GetPackageId()).second)
            {
                DepQueue.push_back(Dep.GetPackageId());
            }
        }

        std::size_t Head = 0;
        while (Head < DepQueue.size())
        {
            const std::string DepId = DepQueue[Head++];
            const std::filesystem::path SiblingPkgDir = Parent / DepId;
            if (std::filesystem::is_directory(SiblingPkgDir, Ec) && std::filesystem::exists(SiblingPkgDir / "package.json5", Ec))
            {
                std::vector<GV2ContentCore::FDiagnostic> SubDiags;
                auto SiblingDesc = GV2ContentHostSupport::DiscoverPackageFromDirectory(SiblingPkgDir, SubDiags);
                if (SiblingDesc)
                {
                    for (const auto& SubDep : SiblingDesc->GetDependencies())
                    {
                        if (VisitedDeps.insert(SubDep.GetPackageId()).second)
                        {
                            DepQueue.push_back(SubDep.GetPackageId());
                        }
                    }
                }
            }
        }

        VisitedDeps.insert("core");
        std::vector<std::filesystem::path> OrderedPackageRoots;
        if (std::filesystem::exists(Parent / "core" / "package.json5", Ec))
        {
            OrderedPackageRoots.push_back(Parent / "core");
        }
        for (const auto& DepId : DepQueue)
        {
            if (DepId != "core" && DepId != TargetDesc->GetPackageId())
            {
                const std::filesystem::path SiblingPkgDir = Parent / DepId;
                if (std::filesystem::exists(SiblingPkgDir / "package.json5", Ec))
                {
                    if (std::find(OrderedPackageRoots.begin(), OrderedPackageRoots.end(), SiblingPkgDir) == OrderedPackageRoots.end())
                    {
                        OrderedPackageRoots.push_back(SiblingPkgDir);
                    }
                }
            }
        }
        OrderedPackageRoots.push_back(Root);
        Roots = std::move(OrderedPackageRoots);
    }

    auto DiscoveredDescriptors = GV2ContentHostSupport::DiscoverPackagesFromDirectories(Roots, Set.Diagnostics);
    if (!DiscoveredDescriptors)
    {
        Set.ErrorCode = "package_discovery_failed";
        Set.ErrorMessage = "Failed to discover package set";
        return Set;
    }

    Set.Roots = std::move(Roots);
    Set.Descriptors = std::move(*DiscoveredDescriptors);
    Set.TargetIndex = Set.Descriptors.empty() ? 0 : Set.Descriptors.size() - 1;

    std::string ManifestContent;
    if (!Set.Descriptors.empty()
        && ReadEntireFile(Set.Roots[Set.TargetIndex] / "package.json5", ManifestContent))
    {
        GV2ContentCore::FParseLimits Limits;
        std::vector<GV2ContentCore::FDiagnostic> ManifestDiagnostics;
        auto Manifest = GV2ContentCore::ParseJson5Document(
            ManifestContent, Limits, ManifestDiagnostics,
            Set.Descriptors[Set.TargetIndex].GetPackageId(),
            Set.Descriptors[Set.TargetIndex].GetLoadIndex(), "package.json5");
        if (Manifest.has_value() && Manifest->GetRootValue().IsObject())
        {
            const auto* Frozen = Manifest->GetRootValue().FindField("frozen");
            const auto* Published = Manifest->GetRootValue().FindField("published");
            const bool bFrozen = Frozen && Frozen->IsBoolean() && Frozen->AsBoolean();
            const bool bPublished = Published && Published->IsBoolean() && Published->AsBoolean();
            if (bFrozen || bPublished)
            {
                Set.ErrorCode = "package_frozen";
                Set.ErrorMessage = "package is published/frozen and cannot be modified in place";
                return Set;
            }
        }
    }

    Set.bSuccess = true;
    return Set;
}

using FCandidateOverrides = std::map<std::string, std::string, std::less<>>;

class FCandidateSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FCandidateSourceProvider(
        const std::vector<GV2ContentCore::FPackageDescriptor>& InDescriptors,
        const std::vector<std::filesystem::path>& InRoots,
        std::string InTargetPackageId,
        const FCandidateOverrides& InOverrides)
        : Descriptors(InDescriptors)
        , Roots(InRoots)
        , TargetPackageId(std::move(InTargetPackageId))
        , Overrides(InOverrides)
    {
    }

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        if (PackageId == TargetPackageId)
        {
            const auto Override = Overrides.find(std::string(RelativeSource));
            if (Override != Overrides.end())
            {
                return Override->second;
            }
        }

        for (std::size_t Index = 0; Index < Descriptors.size() && Index < Roots.size(); ++Index)
        {
            if (Descriptors[Index].GetPackageId() == PackageId)
            {
                std::string Content;
                if (ReadEntireFile(Roots[Index] / std::string(RelativeSource), Content))
                {
                    return Content;
                }
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

private:
    const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors;
    const std::vector<std::filesystem::path>& Roots;
    std::string TargetPackageId;
    const FCandidateOverrides& Overrides;
};

std::vector<GV2ContentCore::FPackageDescriptor> MakeCandidateDescriptors(
    const FDiscoveredPackageSet& Set,
    const FCandidateOverrides& Overrides)
{
    std::vector<GV2ContentCore::FPackageDescriptor> CandidateDescriptors;
    CandidateDescriptors.reserve(Set.Descriptors.size());
    for (std::size_t Index = 0; Index < Set.Descriptors.size(); ++Index)
    {
        const auto& Descriptor = Set.Descriptors[Index];
        if (Index != Set.TargetIndex)
        {
            CandidateDescriptors.push_back(Descriptor);
            continue;
        }

        std::vector<std::string> Sources = Descriptor.GetRelativeSources();
        auto Redirects = Descriptor.GetRedirects();
        auto Tombstones = Descriptor.GetTombstones();

        for (const auto& [RelativeSource, Content] : Overrides)
        {
            if (RelativeSource == "package.json5")
            {
                GV2ContentCore::FParseLimits Limits;
                std::vector<GV2ContentCore::FDiagnostic> Diags;
                auto PkgDoc = GV2ContentCore::ParseJson5Document(Content, Limits, Diags, Descriptor.GetPackageId(), Descriptor.GetLoadIndex(), "package.json5");
                if (PkgDoc.has_value() && PkgDoc->GetRootValue().IsObject())
                {
                    if (const auto* RedVal = PkgDoc->GetRootValue().FindField("redirects"))
                    {
                        if (RedVal->IsObject())
                        {
                            Redirects.clear();
                            for (const auto& [SrcId, TgtVal] : RedVal->AsObject())
                            {
                                if (TgtVal.IsString())
                                {
                                    Redirects.emplace_back(SrcId, TgtVal.AsString());
                                }
                            }
                        }
                    }
                    if (const auto* TombVal = PkgDoc->GetRootValue().FindField("tombstones"))
                    {
                        if (TombVal->IsArray())
                        {
                            Tombstones.clear();
                            for (const auto& T : TombVal->AsArray())
                            {
                                if (T.IsString())
                                {
                                    Tombstones.push_back(T.AsString());
                                }
                            }
                        }
                    }
                }
                continue;
            }

            if (std::find(Sources.begin(), Sources.end(), RelativeSource) == Sources.end())
            {
                Sources.push_back(RelativeSource);
            }
        }
        std::sort(Sources.begin(), Sources.end());

        CandidateDescriptors.emplace_back(
            Descriptor.GetPackageId(),
            Descriptor.GetNamespace(),
            Descriptor.GetLoadIndex(),
            std::move(Sources),
            Descriptor.GetSchemaBindings(),
            Descriptor.GetExtensionSchemaBindings(),
            std::move(Redirects),
            std::move(Tombstones),
            Descriptor.GetVersion(),
            Descriptor.GetDependencies());
    }
    return CandidateDescriptors;
}

bool ValidateCandidateRepository(
    const FDiscoveredPackageSet& Set,
    const FCandidateOverrides& Overrides,
    FAuthoringResult& OutResult)
{
    auto CandidateDescriptors = MakeCandidateDescriptors(Set, Overrides);
    const std::string& TargetPackageId = CandidateDescriptors[Set.TargetIndex].GetPackageId();
    FCandidateSourceProvider Provider(CandidateDescriptors, Set.Roots, TargetPackageId, Overrides);
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;
    auto BuildResult = GV2ContentCore::BuildRepository(CandidateDescriptors, Options);
    if (BuildResult.IsSuccess())
    {
        return true;
    }

    OutResult.Status = EAuthoringStatus::ValidationFailed;
    OutResult.ErrorCode = "validation_failed";
    OutResult.ErrorMessage = "Candidate repository failed authoritative validation";
    OutResult.Diagnostics = BuildResult.GetDiagnostics();
    return false;
}

std::string FormatDuplicatedDefinitionEntry(
    GV2ContentCore::FValue SourceEntry,
    const std::string& TargetDefinitionId)
{
    if (auto* Id = SourceEntry.FindField("id"))
    {
        *Id = GV2ContentCore::FValue(TargetDefinitionId);
    }
    std::ostringstream Output;
    Output << "    ";
    FormatJson5Value(Output, SourceEntry, 2);
    Output << ",\n";
    return Output.str();
}

std::string FormatNewDefinitionEntry(
    const std::string& DefinitionId,
    const GV2ContentCore::FValue& Data,
    const std::vector<std::string>& Tags)
{
    GV2ContentCore::FValue::FArray TagValues;
    TagValues.reserve(Tags.size());
    for (const auto& Tag : Tags) TagValues.emplace_back(Tag);
    GV2ContentCore::FValue Entry = GV2ContentCore::FValue::MakeObject({
        {"id", GV2ContentCore::FValue(DefinitionId)},
        {"data", Data},
        {"tags", GV2ContentCore::FValue(std::move(TagValues))},
        {"deprecated", GV2ContentCore::FValue(false)},
        {"extensions", GV2ContentCore::FValue::MakeObject({})},
    });
    std::ostringstream Output;
    Output << "    ";
    FormatJson5Value(Output, Entry, 2);
    Output << ",\n";
    return Output.str();
}

} // namespace

bool FFileStateStamp::Matches(std::string_view ActualContent) const
{
    if (ContentHash.empty()) return true;
    return ComputeSha256Hex(std::string(ActualContent)) == ContentHash;
}

FFileStateStamp FFileStateStamp::FromContent(std::string_view Content)
{
    FFileStateStamp Stamp;
    Stamp.ContentHash = ComputeSha256Hex(std::string(Content));
    return Stamp;
}

FFileStateStamp FFileStateStamp::FromFile(const std::filesystem::path& FilePath)
{
    std::string Content;
    if (ReadEntireFile(FilePath, Content))
    {
        return FromContent(Content);
    }
    return {};
}

FFileStateStamp FAuthoringService::GetFileStateStamp(const std::filesystem::path& FilePath)
{
    return FFileStateStamp::FromFile(FilePath);
}

FFileStateStamp FAuthoringService::ComputeStamp(std::string_view Content)
{
    return FFileStateStamp::FromContent(Content);
}

FAuthoringResult FAuthoringService::CreateDefinition(const FCreateDefinitionParams& Params)
{
    FAuthoringResult Result;

    GV2ContentCore::FStableIdView IdView;
    GV2ContentCore::EStableIdError IdErr = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(Params.DefinitionId, IdView, &IdErr))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.DefinitionId + "' is not a valid definition id";
        return Result;
    }

    if (IdView.Kind != Params.DefinitionType)
    {
        Result.Status = EAuthoringStatus::IdKindMismatch;
        Result.ErrorCode = "id_kind_mismatch";
        Result.ErrorMessage = "Definition ID kind '" + std::string(IdView.Kind) + "' does not match definition type '" + Params.DefinitionType + "'";
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    // Find schema binding
    const GV2ContentCore::FSchemaBinding* FoundBinding = nullptr;
    std::size_t SchemaPackageIndex = 0;
    for (std::size_t p = 0; p < Set.Descriptors.size(); ++p)
    {
        for (const auto& Binding : Set.Descriptors[p].GetSchemaBindings())
        {
            if (Binding.GetDefinitionType() == Params.DefinitionType)
            {
                FoundBinding = &Binding;
                SchemaPackageIndex = p;
                break;
            }
        }
        if (FoundBinding != nullptr) break;
    }

    if (FoundBinding == nullptr)
    {
        Result.Status = EAuthoringStatus::SchemaNotFound;
        Result.ErrorCode = "unknown_definition_type";
        Result.ErrorMessage = "Unknown definition type '" + Params.DefinitionType
            + "' in package '" + TargetDescriptor.GetPackageId()
            + "' or its dependencies";
        return Result;
    }

    // Check if definition already exists in target package
    GV2ContentCore::FParseLimits Limits;
    std::string TargetRelativeSource;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FullPath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FullPath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDoc) continue;

        auto DefFileOpt = GV2ContentCore::ParseDefinitionFileEnvelope(
            *ParsedDoc, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource, Diags);
        if (!DefFileOpt) continue;

        if (DefFileOpt->GetDefinitionType() == Params.DefinitionType && TargetRelativeSource.empty())
        {
            TargetRelativeSource = RelSource;
        }

        for (const auto& Entry : DefFileOpt->GetDefinitions())
        {
            if (Entry.GetId() == Params.DefinitionId)
            {
                Result.Status = EAuthoringStatus::DuplicateDefinitionId;
                Result.ErrorCode = "duplicate_definition_id";
                Result.ErrorMessage = "Definition ID '" + Params.DefinitionId + "' already exists in package";
                return Result;
            }
        }
    }

    if (TargetRelativeSource.empty())
    {
        TargetRelativeSource = "definitions/" + Params.DefinitionType + "s.json5";
    }

    // Prepare data
    GV2ContentCore::FValue DefData;
    if (Params.InitialData.has_value())
    {
        DefData = *Params.InitialData;
    }
    else
    {
        // Load and parse schema resource
        std::filesystem::path SchemaPath = Set.Roots[SchemaPackageIndex] / FoundBinding->GetRelativePath();
        std::string SchemaContent;
        ReadEntireFile(SchemaPath, SchemaContent);

        std::vector<GV2ContentCore::FDiagnostic> SchemaDiags;
        auto SchemaDoc = GV2ContentCore::ParseJson5Document(
            SchemaContent, Limits, SchemaDiags, Set.Descriptors[SchemaPackageIndex].GetPackageId(),
            Set.Descriptors[SchemaPackageIndex].GetLoadIndex(), FoundBinding->GetRelativePath());

        if (SchemaDoc.has_value())
        {
            auto SchemaResOpt = GV2ContentCore::ParseSchemaResource(
                *SchemaDoc, *FoundBinding, Set.Descriptors[SchemaPackageIndex].GetPackageId(),
                Set.Descriptors[SchemaPackageIndex].GetLoadIndex(), FoundBinding->GetRelativePath(), SchemaDiags);

            if (SchemaResOpt.has_value() && SchemaResOpt->GetCompiledRootSpec() != nullptr)
            {
                DefData = GeneratePlaceholderValue(
                    *SchemaResOpt->GetCompiledRootSpec(),
                    std::string(IdView.Namespace),
                    Params.DefinitionType,
                    std::string(IdView.Path),
                    "");
            }
            else
            {
                DefData = GV2ContentCore::FValue(GV2ContentCore::FValue::FObject{});
            }
        }
        else
        {
            DefData = GV2ContentCore::FValue(GV2ContentCore::FValue::FObject{});
        }
    }

    std::string FormattedEntry = FormatNewDefinitionEntry(
        Params.DefinitionId, DefData, Params.Tags);
    std::filesystem::path TargetFile = TargetRoot / TargetRelativeSource;
    Result.TargetFilePath = TargetFile;

    std::string NewContent;
    std::error_code Ec;
    if (std::filesystem::exists(TargetFile, Ec))
    {
        if (Params.ExpectedStamp.has_value())
        {
            FFileStateStamp CurrentStamp = FFileStateStamp::FromFile(TargetFile);
            if (!CurrentStamp.Matches(*Params.ExpectedStamp))
            {
                Result.Status = EAuthoringStatus::StaleFileState;
                Result.ErrorCode = "stale_file_state";
                Result.ErrorMessage = "File on disk has been modified externally";
                return Result;
            }
        }

        std::string OriginalContent;
        if (!ReadEntireFile(TargetFile, OriginalContent))
        {
            Result.Status = EAuthoringStatus::FileWriteFailed;
            Result.ErrorCode = "file_read_failed";
            Result.ErrorMessage = "Failed to read target definition file";
            return Result;
        }

        std::string ErrorMsg;
        if (!InsertDefinitionEntryIntoJson5(OriginalContent, FormattedEntry, NewContent, ErrorMsg))
        {
            Result.Status = EAuthoringStatus::InvalidValue;
            Result.ErrorCode = "insert_entry_failed";
            Result.ErrorMessage = ErrorMsg;
            return Result;
        }
    }
    else
    {
        NewContent = CreateNewDefinitionFileContent(1, Params.DefinitionType, FormattedEntry);
    }

    if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, NewContent}}, Result))
    {
        return Result;
    }

    // Atomic write
    std::string WriteErr;
    if (!AtomicWriteFile(TargetFile, NewContent, WriteErr))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_write_failed";
        Result.ErrorMessage = WriteErr;
        return Result;
    }

    Result.Status = EAuthoringStatus::Success;
    Result.UpdatedFileContent = NewContent;
    Result.NewStamp = FFileStateStamp::FromContent(NewContent);
    Result.AffectedDefinitionsCount = 1;
    Result.AffectedFilesCount = 1;
    Result.AffectedFilePaths = {TargetFile};
    return Result;
}

FAuthoringResult FAuthoringService::BatchSetFields(const FBatchSetFieldsParams& Params)
{
    FAuthoringResult Result;

    if (Params.Changes.empty())
    {
        Result.Status = EAuthoringStatus::Success;
        return Result;
    }

    GV2ContentCore::FStableIdView IdView;
    GV2ContentCore::EStableIdError IdErr = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(Params.DefinitionId, IdView, &IdErr))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.DefinitionId + "' is not a valid definition id";
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    // Find the definition file containing DefinitionId
    std::filesystem::path TargetFilePath;
    std::string TargetRelativeSource;
    std::size_t TargetDefIndex = std::string::npos;
    GV2ContentCore::FParseLimits Limits;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FilePath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FilePath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDoc || !ParsedDoc->GetRootValue().IsObject()) continue;

        const auto* DefsArr = ParsedDoc->GetRootValue().FindField("definitions");
        if (DefsArr == nullptr || !DefsArr->IsArray()) continue;

        for (std::size_t i = 0; i < DefsArr->AsArray().size(); ++i)
        {
            const auto& Def = DefsArr->AsArray()[i];
            if (!Def.IsObject()) continue;
            const auto* IdVal = Def.FindField("id");
            if (IdVal != nullptr && IdVal->IsString() && IdVal->AsString() == Params.DefinitionId)
            {
                TargetFilePath = FilePath;
                TargetRelativeSource = RelSource;
                TargetDefIndex = i;
                break;
            }
        }
        if (TargetDefIndex != std::string::npos) break;
    }

    if (TargetDefIndex == std::string::npos)
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = "definition_not_found";
        Result.ErrorMessage = "Definition '" + Params.DefinitionId + "' not found in package";
        return Result;
    }

    Result.TargetFilePath = TargetFilePath;

    // Check ExpectedStamp
    if (Params.ExpectedStamp.has_value())
    {
        FFileStateStamp CurrentStamp = FFileStateStamp::FromFile(TargetFilePath);
        if (!CurrentStamp.Matches(*Params.ExpectedStamp))
        {
            Result.Status = EAuthoringStatus::StaleFileState;
            Result.ErrorCode = "stale_file_state";
            Result.ErrorMessage = "File on disk has been modified externally";
            return Result;
        }
    }

    std::string CurrentContent;
    if (!ReadEntireFile(TargetFilePath, CurrentContent))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_read_failed";
        Result.ErrorMessage = "Failed to read target file";
        return Result;
    }

    // Apply all changes in-memory sequentially
    for (const auto& Change : Params.Changes)
    {
        std::string FullPointer = Change.JsonPointer;
        if (FullPointer.rfind("/definitions/", 0) != 0)
        {
            if (FullPointer.rfind("/data/", 0) == 0 || FullPointer == "/data")
            {
                FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + FullPointer;
            }
            else if (FullPointer.rfind("data/", 0) == 0)
            {
                FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/" + FullPointer;
            }
            else if (FullPointer == "/deprecated" || FullPointer == "/tags"
                     || FullPointer.rfind("/tags/", 0) == 0 || FullPointer.rfind("/extensions", 0) == 0
                     || FullPointer == "/id")
            {
                FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + FullPointer;
            }
            else if (FullPointer.rfind("/", 0) == 0)
            {
                FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/data" + FullPointer;
            }
            else
            {
                FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/data/" + FullPointer;
            }
        }

        auto SetRes = SetFieldValue(
            CurrentContent,
            FullPointer,
            Change.NewValue,
            TargetDescriptor.GetPackageId(),
            TargetRelativeSource);

        if (SetRes.Status != ESetFieldValueStatus::Success)
        {
            if (SetRes.Status == ESetFieldValueStatus::PointerNotFound)
            {
                Result.Status = EAuthoringStatus::PointerNotFound;
            }
            else if (SetRes.Status == ESetFieldValueStatus::TargetIsContainer)
            {
                Result.Status = EAuthoringStatus::TargetIsContainer;
            }
            else if (SetRes.Status == ESetFieldValueStatus::InvalidValue)
            {
                Result.Status = EAuthoringStatus::InvalidValue;
            }
            else
            {
                Result.Status = EAuthoringStatus::SpanMappingFailed;
            }
            Result.ErrorCode = SetRes.ErrorCode.empty() ? "set_field_failed" : SetRes.ErrorCode;
            Result.ErrorMessage = SetRes.ErrorMessage.empty() ? "Failed to set field value" : SetRes.ErrorMessage;
            return Result;
        }

        CurrentContent = std::move(SetRes.UpdatedContent);
    }

    if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, CurrentContent}}, Result))
    {
        return Result;
    }

    // Atomic write
    std::string WriteErr;
    if (!AtomicWriteFile(TargetFilePath, CurrentContent, WriteErr))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_write_failed";
        Result.ErrorMessage = WriteErr;
        return Result;
    }

    Result.Status = EAuthoringStatus::Success;
    Result.UpdatedFileContent = CurrentContent;
    Result.NewStamp = FFileStateStamp::FromContent(CurrentContent);
    Result.AffectedDefinitionsCount = 1;
    Result.AffectedFilesCount = 1;
    Result.AffectedFilePaths = {TargetFilePath};
    return Result;
}

FAuthoringResult FAuthoringService::ApplyOperations(const FApplyOperationsParams& Params)
{
    FAuthoringResult Result;
    if (Params.Operations.empty())
    {
        Result.Status = EAuthoringStatus::Success;
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    std::filesystem::path TargetFilePath;
    std::string TargetRelativeSource;
    std::size_t TargetDefIndex = std::string::npos;
    GV2ContentCore::FParseLimits Limits;
    std::optional<GV2ContentCore::FParsedDocument> TargetParsedDoc;
    std::string CurrentContent;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FilePath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FilePath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDoc || !ParsedDoc->GetRootValue().IsObject()) continue;

        const auto* DefsArr = ParsedDoc->GetRootValue().FindField("definitions");
        if (DefsArr == nullptr || !DefsArr->IsArray()) continue;

        for (std::size_t i = 0; i < DefsArr->AsArray().size(); ++i)
        {
            const auto& Def = DefsArr->AsArray()[i];
            if (!Def.IsObject()) continue;
            const auto* IdVal = Def.FindField("id");
            if (IdVal != nullptr && IdVal->IsString() && IdVal->AsString() == Params.DefinitionId)
            {
                TargetFilePath = FilePath;
                TargetRelativeSource = RelSource;
                TargetDefIndex = i;
                TargetParsedDoc = std::move(ParsedDoc);
                CurrentContent = std::move(Content);
                break;
            }
        }
        if (TargetDefIndex != std::string::npos) break;
    }

    if (TargetDefIndex == std::string::npos || !TargetParsedDoc.has_value())
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = "definition_not_found";
        Result.ErrorMessage = "Definition '" + Params.DefinitionId + "' not found in package";
        return Result;
    }

    Result.TargetFilePath = TargetFilePath;

    // Check ExpectedStamp
    if (Params.ExpectedStamp.has_value())
    {
        FFileStateStamp CurrentStamp = FFileStateStamp::FromFile(TargetFilePath);
        if (!CurrentStamp.Matches(*Params.ExpectedStamp))
        {
            Result.Status = EAuthoringStatus::StaleFileState;
            Result.ErrorCode = "stale_file_state";
            Result.ErrorMessage = "File on disk has been modified externally";
            return Result;
        }
    }

    bool bCanUseSpanUpdates = true;
    for (const auto& Op : Params.Operations)
    {
        if (Op.OpType != EFieldOpType::Set)
        {
            bCanUseSpanUpdates = false;
            break;
        }
    }

    if (bCanUseSpanUpdates)
    {
        std::string ProposedContent = CurrentContent;
        bool bAllSpansSucceeded = true;
        for (const auto& Op : Params.Operations)
        {
            std::string FullPointer = Op.JsonPointer;
            if (FullPointer.rfind("/definitions/", 0) != 0)
            {
                if (FullPointer.rfind("/data/", 0) == 0 || FullPointer == "/data")
                {
                    FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + FullPointer;
                }
                else if (FullPointer.rfind("data/", 0) == 0)
                {
                    FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/" + FullPointer;
                }
                else if (FullPointer == "/deprecated" || FullPointer == "/tags"
                         || FullPointer.rfind("/tags/", 0) == 0 || FullPointer.rfind("/extensions", 0) == 0
                         || FullPointer == "/id")
                {
                    FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + FullPointer;
                }
                else if (FullPointer.rfind("/", 0) == 0)
                {
                    FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/data" + FullPointer;
                }
                else
                {
                    FullPointer = "/definitions/" + std::to_string(TargetDefIndex) + "/data/" + FullPointer;
                }
            }

            auto SetRes = SetFieldValue(
                ProposedContent,
                FullPointer,
                Op.Value,
                TargetDescriptor.GetPackageId(),
                TargetRelativeSource);

            if (SetRes.Status == ESetFieldValueStatus::Success)
            {
                ProposedContent = std::move(SetRes.UpdatedContent);
            }
            else
            {
                bAllSpansSucceeded = false;
                break;
            }
        }

        if (bAllSpansSucceeded)
        {
            if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, ProposedContent}}, Result))
            {
                return Result;
            }

            std::string WriteErr;
            if (!AtomicWriteFile(TargetFilePath, ProposedContent, WriteErr))
            {
                Result.Status = EAuthoringStatus::FileWriteFailed;
                Result.ErrorCode = "file_write_failed";
                Result.ErrorMessage = WriteErr;
                return Result;
            }

            Result.Status = EAuthoringStatus::Success;
            Result.UpdatedFileContent = ProposedContent;
            Result.NewStamp = FFileStateStamp::FromContent(ProposedContent);
            Result.AffectedDefinitionsCount = 1;
            Result.AffectedFilesCount = 1;
            Result.AffectedFilePaths = {TargetFilePath};
            return Result;
        }
    }

    // Clone definition value
    auto CandidateDef = TargetParsedDoc->GetRootValue().FindField("definitions")->AsArray()[TargetDefIndex];

    // Apply all operations to CandidateDef
    for (const auto& Op : Params.Operations)
    {
        std::string OpErr;
        if (!ApplyFieldOpToDefinitionValue(CandidateDef, Op.JsonPointer, Op, OpErr))
        {
            Result.Status = EAuthoringStatus::InvalidValue;
            Result.ErrorCode = "operation_failed";
            Result.ErrorMessage = OpErr.empty() ? "Failed to apply operation to definition" : OpErr;
            return Result;
        }
    }

    // Format updated candidate definition and replace it in CurrentContent
    const std::string EntryPointer = "/definitions/" + std::to_string(TargetDefIndex);
    const auto* Location = TargetParsedDoc->FindLocation(EntryPointer);
    if (Location == nullptr)
    {
        Result.Status = EAuthoringStatus::SpanMappingFailed;
        Result.ErrorCode = "span_mapping_failed";
        Result.ErrorMessage = "Failed to find location span for " + EntryPointer;
        return Result;
    }

    std::size_t StartByte = 0;
    std::size_t EndByte = 0;
    if (!SourceSpanToByteRange(CurrentContent, Location->ValueSpan, StartByte, EndByte))
    {
        Result.Status = EAuthoringStatus::SpanMappingFailed;
        Result.ErrorCode = "span_mapping_failed";
        Result.ErrorMessage = "Failed to map location span to byte offsets";
        return Result;
    }

    std::ostringstream DefStream;
    FormatJson5Value(DefStream, CandidateDef, 2);
    std::string FormattedDef = DefStream.str();

    std::string UpdatedContent = CurrentContent.substr(0, StartByte)
        + FormattedDef
        + CurrentContent.substr(EndByte);

    std::vector<GV2ContentCore::FDiagnostic> VerifyDiagnostics;
    auto VerifyDoc = GV2ContentCore::ParseJson5Document(
        UpdatedContent, Limits, VerifyDiagnostics, TargetDescriptor.GetPackageId(), 0, TargetRelativeSource);
    if (!VerifyDoc.has_value())
    {
        Result.Status = EAuthoringStatus::ValidationFailed;
        Result.ErrorCode = "rewritten_document_invalid";
        Result.ErrorMessage = "Rewritten document failed to parse";
        return Result;
    }

    if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, UpdatedContent}}, Result))
    {
        return Result;
    }

    // Atomic write
    std::string WriteErr;
    if (!AtomicWriteFile(TargetFilePath, UpdatedContent, WriteErr))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_write_failed";
        Result.ErrorMessage = WriteErr;
        return Result;
    }

    Result.Status = EAuthoringStatus::Success;
    Result.UpdatedFileContent = UpdatedContent;
    Result.NewStamp = FFileStateStamp::FromContent(UpdatedContent);
    Result.AffectedDefinitionsCount = 1;
    Result.AffectedFilesCount = 1;
    Result.AffectedFilePaths = {TargetFilePath};
    return Result;
}

FAuthoringResult FAuthoringService::SetField(const FSetFieldParams& Params)
{
    FBatchSetFieldsParams BatchParams;
    BatchParams.PackageRoot = Params.PackageRoot;
    BatchParams.DefinitionId = Params.DefinitionId;
    BatchParams.Changes = { { Params.JsonPointer, Params.NewValue } };
    BatchParams.ExpectedStamp = Params.ExpectedStamp;
    return BatchSetFields(BatchParams);
}

FAuthoringResult FAuthoringService::DuplicateDefinition(const FDuplicateDefinitionParams& Params)
{
    FAuthoringResult Result;

    GV2ContentCore::FStableIdView SrcIdView, DstIdView;
    if (!GV2ContentCore::FStableId::Parse(Params.SourceDefinitionId, SrcIdView))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.SourceDefinitionId + "' is not a valid source definition id";
        return Result;
    }
    if (!GV2ContentCore::FStableId::Parse(Params.TargetDefinitionId, DstIdView))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.TargetDefinitionId + "' is not a valid target definition id";
        return Result;
    }

    if (SrcIdView.Kind != DstIdView.Kind)
    {
        Result.Status = EAuthoringStatus::IdKindMismatch;
        Result.ErrorCode = "id_kind_mismatch";
        Result.ErrorMessage = "Source kind '" + std::string(SrcIdView.Kind) + "' does not match target kind '" + std::string(DstIdView.Kind) + "'";
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    std::filesystem::path TargetFilePath;
    std::string TargetRelativeSource;
    GV2ContentCore::FValue SourceEntryValue;
    bool bFoundSource = false;
    GV2ContentCore::FParseLimits Limits;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FilePath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FilePath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDoc || !ParsedDoc->GetRootValue().IsObject()) continue;

        const auto* DefsArr = ParsedDoc->GetRootValue().FindField("definitions");
        if (DefsArr == nullptr || !DefsArr->IsArray()) continue;

        for (const auto& Def : DefsArr->AsArray())
        {
            if (!Def.IsObject()) continue;
            const auto* IdVal = Def.FindField("id");
            if (IdVal != nullptr && IdVal->IsString())
            {
                if (IdVal->AsString() == Params.TargetDefinitionId)
                {
                    Result.Status = EAuthoringStatus::DuplicateDefinitionId;
                    Result.ErrorCode = "duplicate_definition_id";
                    Result.ErrorMessage = "Target definition id '" + Params.TargetDefinitionId + "' already exists";
                    return Result;
                }
                if (IdVal->AsString() == Params.SourceDefinitionId)
                {
                    TargetFilePath = FilePath;
                    TargetRelativeSource = RelSource;
                    SourceEntryValue = Def;
                    bFoundSource = true;
                }
            }
        }
    }

    if (!bFoundSource)
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = "definition_not_found";
        Result.ErrorMessage = "Source definition '" + Params.SourceDefinitionId + "' not found in package";
        return Result;
    }

    Result.TargetFilePath = TargetFilePath;

    // Check ExpectedStamp
    if (Params.ExpectedStamp.has_value())
    {
        FFileStateStamp CurrentStamp = FFileStateStamp::FromFile(TargetFilePath);
        if (!CurrentStamp.Matches(*Params.ExpectedStamp))
        {
            Result.Status = EAuthoringStatus::StaleFileState;
            Result.ErrorCode = "stale_file_state";
            Result.ErrorMessage = "File on disk has been modified externally";
            return Result;
        }
    }

    std::string OriginalContent;
    if (!ReadEntireFile(TargetFilePath, OriginalContent))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_read_failed";
        Result.ErrorMessage = "Failed to read target file";
        return Result;
    }

    std::string FormattedEntry = FormatDuplicatedDefinitionEntry(
        SourceEntryValue, Params.TargetDefinitionId);
    std::string NewContent;
    std::string ErrorMsg;
    if (!InsertDefinitionEntryIntoJson5(OriginalContent, FormattedEntry, NewContent, ErrorMsg))
    {
        Result.Status = EAuthoringStatus::InvalidValue;
        Result.ErrorCode = "insert_entry_failed";
        Result.ErrorMessage = ErrorMsg;
        return Result;
    }

    if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, NewContent}}, Result))
    {
        return Result;
    }

    // Atomic write
    std::string WriteErr;
    if (!AtomicWriteFile(TargetFilePath, NewContent, WriteErr))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_write_failed";
        Result.ErrorMessage = WriteErr;
        return Result;
    }

    Result.Status = EAuthoringStatus::Success;
    Result.UpdatedFileContent = NewContent;
    Result.NewStamp = FFileStateStamp::FromContent(NewContent);
    Result.AffectedDefinitionsCount = 1;
    Result.AffectedFilesCount = 1;
    Result.AffectedFilePaths = {TargetFilePath};
    return Result;
}

FAuthoringResult FAuthoringService::DeleteDefinition(const FDeleteDefinitionParams& Params)
{
    FAuthoringResult Result;

    GV2ContentCore::FStableIdView IdView;
    if (!GV2ContentCore::FStableId::Parse(Params.DefinitionId, IdView))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.DefinitionId + "' is not a valid definition id";
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    std::filesystem::path TargetFilePath;
    std::string TargetRelativeSource;
    bool bFoundDefinition = false;
    GV2ContentCore::FParseLimits Limits;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FilePath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FilePath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDoc || !ParsedDoc->GetRootValue().IsObject()) continue;

        const auto* DefsArr = ParsedDoc->GetRootValue().FindField("definitions");
        if (DefsArr == nullptr || !DefsArr->IsArray()) continue;

        for (const auto& Def : DefsArr->AsArray())
        {
            if (!Def.IsObject()) continue;
            const auto* IdVal = Def.FindField("id");
            if (IdVal != nullptr && IdVal->IsString() && IdVal->AsString() == Params.DefinitionId)
            {
                TargetFilePath = FilePath;
                TargetRelativeSource = RelSource;
                bFoundDefinition = true;
                break;
            }
        }
        if (bFoundDefinition) break;
    }

    if (!bFoundDefinition)
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = "definition_not_found";
        Result.ErrorMessage = "Definition '" + Params.DefinitionId + "' not found in package";
        return Result;
    }

    Result.TargetFilePath = TargetFilePath;

    // Check ExpectedStamp
    if (Params.ExpectedStamp.has_value())
    {
        FFileStateStamp CurrentStamp = FFileStateStamp::FromFile(TargetFilePath);
        if (!CurrentStamp.Matches(*Params.ExpectedStamp))
        {
            Result.Status = EAuthoringStatus::StaleFileState;
            Result.ErrorCode = "stale_file_state";
            Result.ErrorMessage = "File on disk has been modified externally";
            return Result;
        }
    }

    std::string OriginalContent;
    if (!ReadEntireFile(TargetFilePath, OriginalContent))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_read_failed";
        Result.ErrorMessage = "Failed to read target file";
        return Result;
    }

    auto RemoveRes = RemoveDefinitionEntry(
        OriginalContent,
        Params.DefinitionId,
        TargetDescriptor.GetPackageId(),
        TargetRelativeSource);

    if (RemoveRes.Status != ERemoveDefinitionStatus::Success)
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = RemoveRes.ErrorCode.empty() ? "remove_entry_failed" : RemoveRes.ErrorCode;
        Result.ErrorMessage = RemoveRes.ErrorMessage.empty() ? "Failed to remove definition entry" : RemoveRes.ErrorMessage;
        return Result;
    }

    std::string NewContent = std::move(RemoveRes.UpdatedContent);

    if (!ValidateCandidateRepository(Set, {{TargetRelativeSource, NewContent}}, Result))
    {
        return Result;
    }

    // Atomic write
    std::string WriteErr;
    if (!AtomicWriteFile(TargetFilePath, NewContent, WriteErr))
    {
        Result.Status = EAuthoringStatus::FileWriteFailed;
        Result.ErrorCode = "file_write_failed";
        Result.ErrorMessage = WriteErr;
        return Result;
    }

    Result.Status = EAuthoringStatus::Success;
    Result.UpdatedFileContent = NewContent;
    Result.NewStamp = FFileStateStamp::FromContent(NewContent);
    Result.AffectedDefinitionsCount = 1;
    Result.AffectedFilesCount = 1;
    Result.AffectedFilePaths = {TargetFilePath};
    return Result;
}

namespace
{

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

struct FLoadedSchemas
{
    std::unordered_map<std::string, GV2ContentCore::FSchemaResource> SchemasByType;
    std::vector<GV2ContentCore::FExtensionSchemaResource> ExtensionSchemas;
};

void CollectTypedReferencePointers(
    const GV2ContentCore::FValue& Value,
    const GV2ContentCore::FCompiledFieldSpec* Spec,
    const std::string& Pointer,
    const std::string& TargetDefinitionId,
    std::vector<std::string>& OutPointers)
{
    if (Spec == nullptr || Value.IsNull()) return;

    if (Spec->Kind == GV2ContentCore::EFieldKind::Reference ||
        Spec->Kind == GV2ContentCore::EFieldKind::TextId ||
        Spec->Kind == GV2ContentCore::EFieldKind::ResourceReference)
    {
        if (Value.IsString() && Value.AsString() == TargetDefinitionId)
        {
            OutPointers.push_back(Pointer);
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
                CollectTypedReferencePointers(
                    *ChildVal,
                    Field.Spec.get(),
                    Pointer + "/" + EscapeJsonPointerSegment(Field.Name),
                    TargetDefinitionId,
                    OutPointers);
            }
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Array && Value.IsArray())
    {
        const auto& Arr = Value.AsArray();
        for (std::size_t i = 0; i < Arr.size(); ++i)
        {
            CollectTypedReferencePointers(
                Arr[i],
                Spec->Items.get(),
                Pointer + "/" + std::to_string(i),
                TargetDefinitionId,
                OutPointers);
        }
        return;
    }

    if (Spec->Kind == GV2ContentCore::EFieldKind::Map && Value.IsObject())
    {
        for (const auto& [Key, ChildVal] : Value.AsObject())
        {
            CollectTypedReferencePointers(
                ChildVal,
                Spec->MapValues.get(),
                Pointer + "/" + EscapeJsonPointerSegment(Key),
                TargetDefinitionId,
                OutPointers);
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
                    CollectTypedReferencePointers(
                        Value,
                        Variant.Spec.get(),
                        Pointer,
                        TargetDefinitionId,
                        OutPointers);
                    break;
                }
            }
        }
        return;
    }
}

} // namespace

FAuthoringResult FAuthoringService::RenameDefinition(const FRenameDefinitionParams& Params)
{
    FAuthoringResult Result;

    GV2ContentCore::FStableIdView OldIdView, NewIdView;
    if (!GV2ContentCore::FStableId::Parse(Params.OldDefinitionId, OldIdView))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.OldDefinitionId + "' is not a valid old definition id";
        return Result;
    }
    if (!GV2ContentCore::FStableId::Parse(Params.NewDefinitionId, NewIdView))
    {
        Result.Status = EAuthoringStatus::InvalidDefinitionId;
        Result.ErrorCode = "invalid_definition_id";
        Result.ErrorMessage = "'" + Params.NewDefinitionId + "' is not a valid new definition id";
        return Result;
    }

    if (OldIdView.Kind != NewIdView.Kind)
    {
        Result.Status = EAuthoringStatus::IdKindMismatch;
        Result.ErrorCode = "id_kind_mismatch";
        Result.ErrorMessage = "Old kind '" + std::string(OldIdView.Kind) + "' does not match new kind '" + std::string(NewIdView.Kind) + "'";
        return Result;
    }

    if (Params.OldDefinitionId == Params.NewDefinitionId)
    {
        Result.Status = EAuthoringStatus::Success;
        return Result;
    }

    auto Set = DiscoverSet(Params.PackageRoot);
    if (!Set.bSuccess)
    {
        Result.Status = EAuthoringStatus::PackageNotFound;
        Result.ErrorCode = Set.ErrorCode;
        Result.ErrorMessage = Set.ErrorMessage;
        return Result;
    }

    const auto& TargetDescriptor = Set.Descriptors[Set.TargetIndex];
    const auto& TargetRoot = Set.Roots[Set.TargetIndex];

    for (const auto& Redirect : TargetDescriptor.GetRedirects())
    {
        if (Redirect.GetSourceId() == Params.NewDefinitionId
            || Redirect.GetTargetId() == Params.NewDefinitionId)
        {
            Result.Status = EAuthoringStatus::DuplicateDefinitionId;
            Result.ErrorCode = "duplicate_definition_id";
            Result.ErrorMessage = "Definition ID '" + Params.NewDefinitionId
                + "' is already reserved by a redirect";
            return Result;
        }
    }
    if (std::find(
            TargetDescriptor.GetTombstones().begin(),
            TargetDescriptor.GetTombstones().end(),
            Params.NewDefinitionId) != TargetDescriptor.GetTombstones().end())
    {
        Result.Status = EAuthoringStatus::DuplicateDefinitionId;
        Result.ErrorCode = "duplicate_definition_id";
        Result.ErrorMessage = "Definition ID '" + Params.NewDefinitionId
            + "' is reserved by a tombstone";
        return Result;
    }

    // Load all schemas and extension schemas across packages in the set
    std::vector<FLoadedSchemas> LoadedSchemas(Set.Descriptors.size());
    GV2ContentCore::FParseLimits Limits;

    for (std::size_t PkgIdx = 0; PkgIdx < Set.Descriptors.size(); ++PkgIdx)
    {
        const auto& Pkg = Set.Descriptors[PkgIdx];
        const auto& Root = Set.Roots[PkgIdx];

        for (const auto& Binding : Pkg.GetSchemaBindings())
        {
            std::filesystem::path SchemaPath = Root / Binding.GetRelativePath();
            std::string SchemaContent;
            if (!ReadEntireFile(SchemaPath, SchemaContent)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto SchemaDoc = GV2ContentCore::ParseJson5Document(
                SchemaContent, Limits, Diags, Pkg.GetPackageId(), Pkg.GetLoadIndex(), Binding.GetRelativePath());
            if (SchemaDoc.has_value())
            {
                auto SchemaRes = GV2ContentCore::ParseSchemaResource(
                    *SchemaDoc, Binding, Pkg.GetPackageId(), Pkg.GetLoadIndex(), Binding.GetRelativePath(), Diags);
                if (SchemaRes.has_value())
                {
                    LoadedSchemas[PkgIdx].SchemasByType.emplace(Binding.GetDefinitionType(), std::move(*SchemaRes));
                }
            }
        }

        for (const auto& ExtBinding : Pkg.GetExtensionSchemaBindings())
        {
            std::filesystem::path ExtPath = Root / ExtBinding.GetRelativePath();
            std::string ExtContent;
            if (!ReadEntireFile(ExtPath, ExtContent)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto ExtDoc = GV2ContentCore::ParseJson5Document(
                ExtContent, Limits, Diags, Pkg.GetPackageId(), Pkg.GetLoadIndex(), ExtBinding.GetRelativePath());
            if (ExtDoc.has_value())
            {
                auto ExtRes = GV2ContentCore::ParseExtensionSchemaResource(
                    *ExtDoc, ExtBinding, Pkg.GetPackageId(), Pkg.GetLoadIndex(), ExtBinding.GetRelativePath(), Diags);
                if (ExtRes.has_value())
                {
                    LoadedSchemas[PkgIdx].ExtensionSchemas.push_back(std::move(*ExtRes));
                }
            }
        }
    }

    struct FPendingFileUpdate
    {
        std::filesystem::path FilePath;
        std::string RelativeSource;
        std::string OriginalContent;
        std::string Content;
    };
    std::vector<FPendingFileUpdate> FilesToUpdate;
    std::size_t TotalReplacements = 0;
    bool bFoundDefinition = false;
    std::filesystem::path DefinitionFilePath;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FilePath = TargetRoot / RelSource;
        std::string Content;
        if (!ReadEntireFile(FilePath, Content)) continue;

        std::vector<GV2ContentCore::FDiagnostic> ParseDiagnostics;
        auto ParsedDocument = GV2ContentCore::ParseJson5Document(
            Content, Limits, ParseDiagnostics,
            TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), RelSource);
        if (!ParsedDocument.has_value() || !ParsedDocument->GetRootValue().IsObject()) continue;

        const auto* TypeVal = ParsedDocument->GetRootValue().FindField("type");
        if (TypeVal == nullptr || !TypeVal->IsString()) continue;
        const std::string DefType = TypeVal->AsString();

        // Find schema for DefType
        const GV2ContentCore::FSchemaResource* SchemaRes = nullptr;
        for (std::size_t SearchIdx = 0; SearchIdx < Set.Descriptors.size(); ++SearchIdx)
        {
            auto It = LoadedSchemas[SearchIdx].SchemasByType.find(DefType);
            if (It != LoadedSchemas[SearchIdx].SchemasByType.end())
            {
                SchemaRes = &It->second;
                break;
            }
        }

        const auto* Definitions = ParsedDocument->GetRootValue().FindField("definitions");
        if (Definitions == nullptr || !Definitions->IsArray()) continue;

        std::vector<std::string> PointersToReplace;

        for (std::size_t DefIdx = 0; DefIdx < Definitions->AsArray().size(); ++DefIdx)
        {
            const auto& Definition = Definitions->AsArray()[DefIdx];
            if (!Definition.IsObject()) continue;

            const auto* Id = Definition.FindField("id");
            if (Id && Id->IsString())
            {
                if (Id->AsString() == Params.NewDefinitionId)
                {
                    Result.Status = EAuthoringStatus::DuplicateDefinitionId;
                    Result.ErrorCode = "duplicate_definition_id";
                    Result.ErrorMessage = "Definition ID '" + Params.NewDefinitionId
                        + "' already exists in package";
                    return Result;
                }
                if (Id->AsString() == Params.OldDefinitionId)
                {
                    bFoundDefinition = true;
                    DefinitionFilePath = FilePath;
                    PointersToReplace.push_back("/definitions/" + std::to_string(DefIdx) + "/id");
                }
            }

            // Scan typed references in data
            const auto* DataVal = Definition.FindField("data");
            if (SchemaRes != nullptr && DataVal != nullptr)
            {
                CollectTypedReferencePointers(
                    *DataVal,
                    SchemaRes->GetCompiledRootSpec().get(),
                    "/definitions/" + std::to_string(DefIdx) + "/data",
                    Params.OldDefinitionId,
                    PointersToReplace);
            }

            // Scan typed references in extensions
            const auto* ExtVal = Definition.FindField("extensions");
            if (ExtVal != nullptr && ExtVal->IsObject())
            {
                for (const auto& [ExtNs, ExtBlock] : ExtVal->AsObject())
                {
                    for (const auto& ExtPkg : LoadedSchemas)
                    {
                        for (const auto& ExtSchema : ExtPkg.ExtensionSchemas)
                        {
                            if (ExtSchema.GetKey().DefinitionType == DefType && ExtSchema.GetKey().ExtensionNamespace == ExtNs)
                            {
                                CollectTypedReferencePointers(
                                    ExtBlock,
                                    ExtSchema.GetCompiledRootSpec().get(),
                                    "/definitions/" + std::to_string(DefIdx) + "/extensions/" + EscapeJsonPointerSegment(ExtNs),
                                    Params.OldDefinitionId,
                                    PointersToReplace);
                            }
                        }
                    }
                }
            }
        }

        if (!PointersToReplace.empty())
        {
            std::string UpdatedContent = Content;
            for (const auto& Ptr : PointersToReplace)
            {
                auto SetRes = SetFieldValue(
                    UpdatedContent,
                    Ptr,
                    GV2ContentCore::FValue(Params.NewDefinitionId),
                    TargetDescriptor.GetPackageId(),
                    RelSource);
                if (SetRes.Status == ESetFieldValueStatus::Success)
                {
                    UpdatedContent = std::move(SetRes.UpdatedContent);
                }
            }

            FilesToUpdate.push_back({
                FilePath, RelSource, Content, std::move(UpdatedContent)});
            TotalReplacements += PointersToReplace.size();
        }
    }

    if (!bFoundDefinition || TotalReplacements == 0)
    {
        Result.Status = EAuthoringStatus::DefinitionNotFound;
        Result.ErrorCode = "source_definition_not_found";
        Result.ErrorMessage = "Definition '" + Params.OldDefinitionId + "' not found in package";
        return Result;
    }

    if (Params.ExpectedStamp.has_value()
        && !FFileStateStamp::FromFile(DefinitionFilePath).Matches(*Params.ExpectedStamp))
    {
        Result.Status = EAuthoringStatus::StaleFileState;
        Result.ErrorCode = "stale_file_state";
        Result.ErrorMessage = "Definition file on disk has been modified externally";
        return Result;
    }

    // Handle package.json5 redirects
    std::filesystem::path PkgJsonPath = TargetRoot / "package.json5";
    std::string PkgJsonContent;
    if (ReadEntireFile(PkgJsonPath, PkgJsonContent))
    {
        std::vector<GV2ContentCore::FDiagnostic> PkgDiags;
        auto PkgDoc = GV2ContentCore::ParseJson5Document(
            PkgJsonContent, Limits, PkgDiags, TargetDescriptor.GetPackageId(), TargetDescriptor.GetLoadIndex(), "package.json5");
        if (PkgDoc.has_value() && PkgDoc->GetRootValue().IsObject())
        {
            bool bPkgJsonModified = false;
            std::string UpdatedPkgJson = PkgJsonContent;

            // Check existing redirects
            const auto* RedVal = PkgDoc->GetRootValue().FindField("redirects");
            if (RedVal != nullptr && RedVal->IsObject())
            {
                for (const auto& [SrcId, TgtVal] : RedVal->AsObject())
                {
                    if (TgtVal.IsString() && TgtVal.AsString() == Params.OldDefinitionId)
                    {
                        auto SetRes = SetFieldValue(
                            UpdatedPkgJson,
                            "/redirects/" + EscapeJsonPointerSegment(SrcId),
                            GV2ContentCore::FValue(Params.NewDefinitionId),
                            TargetDescriptor.GetPackageId(),
                            "package.json5");
                        if (SetRes.Status == ESetFieldValueStatus::Success)
                        {
                            UpdatedPkgJson = std::move(SetRes.UpdatedContent);
                            bPkgJsonModified = true;
                            TotalReplacements++;
                        }
                    }
                }
            }

            if (Params.bCreateRedirect)
            {
                if (RedVal != nullptr && RedVal->IsObject())
                {
                    auto SetRes = SetFieldValue(
                        UpdatedPkgJson,
                        "/redirects/" + EscapeJsonPointerSegment(Params.OldDefinitionId),
                        GV2ContentCore::FValue(Params.NewDefinitionId),
                        TargetDescriptor.GetPackageId(),
                        "package.json5");
                    if (SetRes.Status == ESetFieldValueStatus::Success)
                    {
                        UpdatedPkgJson = std::move(SetRes.UpdatedContent);
                        bPkgJsonModified = true;
                    }
                }
                else
                {
                    auto NewRoot = PkgDoc->GetRootValue();
                    NewRoot.AsObject().push_back({
                        "redirects",
                        GV2ContentCore::FValue::MakeObject({
                            {Params.OldDefinitionId, GV2ContentCore::FValue(Params.NewDefinitionId)}
                        })
                    });
                    std::ostringstream Oss;
                    FormatJson5Value(Oss, NewRoot, 0);
                    UpdatedPkgJson = Oss.str() + "\n";
                    bPkgJsonModified = true;
                }
            }

            if (bPkgJsonModified)
            {
                FilesToUpdate.push_back({
                    PkgJsonPath, "package.json5", PkgJsonContent, std::move(UpdatedPkgJson)});
            }
        }
    }

    FCandidateOverrides Overrides;
    for (const auto& Update : FilesToUpdate)
    {
        Overrides.emplace(Update.RelativeSource, Update.Content);
    }
    if (!ValidateCandidateRepository(Set, Overrides, Result))
    {
        return Result;
    }

    // Write transaction journal for crash safety (CEH-17)
    std::filesystem::path JournalPath = TargetRoot / ".gv2_authoring_journal.json5";
    {
        GV2ContentCore::FValue::FArray JournalEntries;
        for (const auto& Update : FilesToUpdate)
        {
            JournalEntries.push_back(GV2ContentCore::FValue::MakeObject({
                {"relative_path", GV2ContentCore::FValue(Update.RelativeSource)},
                {"original_content", GV2ContentCore::FValue(Update.OriginalContent)}
            }));
        }
        auto JournalObj = GV2ContentCore::FValue::MakeObject({
            {"transaction_id", GV2ContentCore::FValue("rename_" + Params.OldDefinitionId + "_to_" + Params.NewDefinitionId)},
            {"package_id", GV2ContentCore::FValue(TargetDescriptor.GetPackageId())},
            {"old_id", GV2ContentCore::FValue(Params.OldDefinitionId)},
            {"new_id", GV2ContentCore::FValue(Params.NewDefinitionId)},
            {"entries", GV2ContentCore::FValue(std::move(JournalEntries))}
        });

        std::ostringstream Oss;
        FormatJson5Value(Oss, JournalObj, 0);
        std::string JournalContent = Oss.str() + "\n";
        std::string JournalWriteErr;
        if (!AtomicWriteFile(JournalPath, JournalContent, JournalWriteErr))
        {
            Result.Status = EAuthoringStatus::FileWriteFailed;
            Result.ErrorCode = "journal_write_failed";
            Result.ErrorMessage = "Failed to write transaction journal: " + JournalWriteErr;
            return Result;
        }
    }

    // Commit each file atomically
    std::size_t CommittedCount = 0;
    for (const auto& Update : FilesToUpdate)
    {
        std::string WriteErr;
        if (!AtomicWriteFile(Update.FilePath, Update.Content, WriteErr))
        {
            bool bRollbackSucceeded = true;
            while (CommittedCount > 0)
            {
                --CommittedCount;
                std::string RollbackError;
                const auto& Committed = FilesToUpdate[CommittedCount];
                if (!AtomicWriteFile(Committed.FilePath, Committed.OriginalContent, RollbackError))
                {
                    bRollbackSucceeded = false;
                    WriteErr += "; rollback failed for " + Committed.FilePath.string()
                        + ": " + RollbackError;
                }
            }
            std::filesystem::remove(JournalPath);
            Result.Status = EAuthoringStatus::FileWriteFailed;
            Result.ErrorCode = "file_write_failed";
            Result.ErrorMessage = bRollbackSucceeded
                ? WriteErr + "; previously replaced files were rolled back"
                : WriteErr;
            return Result;
        }
        ++CommittedCount;
    }

    // Transaction committed successfully: clean up journal
    std::filesystem::remove(JournalPath);

    Result.Status = EAuthoringStatus::Success;
    Result.AffectedDefinitionsCount = TotalReplacements;
    Result.AffectedFilesCount = FilesToUpdate.size();
    Result.ReplacementsCount = TotalReplacements;
    for (const auto& Update : FilesToUpdate)
    {
        Result.AffectedFilePaths.push_back(Update.FilePath);
    }
    return Result;
}

FAuthoringResult FAuthoringService::RecoverPendingJournal(const std::filesystem::path& PackageRoot)
{
    FAuthoringResult Result;
    std::filesystem::path JournalPath = PackageRoot / ".gv2_authoring_journal.json5";
    if (!std::filesystem::exists(JournalPath))
    {
        Result.Status = EAuthoringStatus::Success;
        return Result;
    }

    std::string JournalContent;
    if (!ReadEntireFile(JournalPath, JournalContent))
    {
        Result.Status = EAuthoringStatus::ToolFailure;
        Result.ErrorCode = "journal_read_failed";
        Result.ErrorMessage = "Failed to read pending transaction journal";
        return Result;
    }

    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    auto Doc = GV2ContentCore::ParseJson5Document(JournalContent, Limits, Diags, "", 0, ".gv2_authoring_journal.json5");
    if (!Doc.has_value() || !Doc->GetRootValue().IsObject())
    {
        std::filesystem::remove(JournalPath);
        Result.Status = EAuthoringStatus::InvalidValue;
        Result.ErrorCode = "invalid_journal";
        Result.ErrorMessage = "Corrupt transaction journal removed";
        return Result;
    }

    const auto* Entries = Doc->GetRootValue().FindField("entries");
    if (Entries != nullptr && Entries->IsArray())
    {
        for (const auto& Entry : Entries->AsArray())
        {
            if (Entry.IsObject())
            {
                const auto* RelPath = Entry.FindField("relative_path");
                const auto* OrigContent = Entry.FindField("original_content");
                if (RelPath && RelPath->IsString() && OrigContent && OrigContent->IsString())
                {
                    std::filesystem::path FilePath = PackageRoot / RelPath->AsString();
                    std::string WriteErr;
                    AtomicWriteFile(FilePath, OrigContent->AsString(), WriteErr);
                    Result.AffectedFilePaths.push_back(FilePath);
                }
            }
        }
    }

    std::filesystem::remove(JournalPath);
    Result.Status = EAuthoringStatus::Success;
    Result.AffectedFilesCount = Result.AffectedFilePaths.size();
    return Result;
}

} // namespace GV2ContentAuthoring
