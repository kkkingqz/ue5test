#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Lexer.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "GV2ContentCore/Testing/BuildResultConformance.h"
#include "GV2ContentCore/Testing/ContainerValidationConformance.h"
#include "GV2ContentCore/Testing/DefinitionEnvelopeConformance.h"
#include "GV2ContentCore/Testing/DiagnosticModelConformance.h"
#include "GV2ContentCore/Testing/ExtensionSchemaConformance.h"
#include "GV2ContentCore/Testing/Json5Conformance.h"
#include "GV2ContentCore/Testing/Json5LexerConformance.h"
#include "GV2ContentCore/Testing/Json5ParserConformance.h"
#include "GV2ContentCore/Testing/PackageDescriptorConformance.h"
#include "GV2ContentCore/Testing/ParseLimitsConformance.h"
#include "GV2ContentCore/Testing/PoParserConformance.h"
#include "GV2ContentCore/Testing/PresenceDefaultConformance.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"
#include "GV2ContentCore/Testing/ScalarValidationConformance.h"
#include "GV2ContentCore/Testing/SchemaRegistryConformance.h"
#include "GV2ContentCore/Testing/SpecialFieldValidationConformance.h"
#include "GV2ContentCore/Testing/ValueModelConformance.h"
#include "GV2ContentCore/Value.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2ContentHostSupport/Testing/PackageDiscoveryAndOrderConformance.h"
#include "GV2ContentHostSupport/Testing/PackageManifestConformance.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RunDigest.h"
#include "GV2RuntimeCore/GV2RunManifest.h"
#include "GV2RuntimeCore/GV2RunReplay.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/Testing/GV2LuaMarshallerConformance.h"
#include "GV2RuntimeCore/Testing/GV2LuaRepositoryConformance.h"
#include "GV2RuntimeCore/Testing/GV2RunDigestConformance.h"
#include "GV2RuntimeCore/Testing/GV2RunManifestConformance.h"
#include "GV2RuntimeCore/Testing/GV2ColdStartLoadConformance.h"
#include "GV2RuntimeCore/Testing/GV2RunReplayConformance.h"
#include "GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h"
#include "GV2RuntimeCore/Testing/GV2StableIdConformance.h"
#include "GV2RuntimeCore/Testing/GV2ValidatorRegistryConformance.h"
#include "GV2RuntimeCore/Testing/GV2LuaSpecRunnerConformance.h"
#include "GV2TestSupport/LuaSpecRunner.h"
#include "GV2TestSupport/CommandValidatorFixture.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
class FMetadataOnlyResourceCatalog final : public GV2RuntimeCore::IResourceCatalog
{
public:
    FMetadataOnlyResourceCatalog()
    {
        GV2RuntimeCore::FResourceMetadata Metadata;
        Metadata.ResourceId = "core:resource.image.headless_fixture";
        Metadata.Kind = GV2RuntimeCore::EResourceKind::Image;
        Metadata.bAvailable = true;
        Resources.emplace(Metadata.ResourceId, std::move(Metadata));
    }

    std::optional<GV2RuntimeCore::FResourceMetadata> FindMetadata(
        const std::string& ResourceId) const override
    {
        const auto Found = Resources.find(ResourceId);
        return Found != Resources.end()
            ? std::optional<GV2RuntimeCore::FResourceMetadata>(Found->second)
            : std::nullopt;
    }

private:
    std::map<std::string, GV2RuntimeCore::FResourceMetadata, std::less<>> Resources;
};

class FUnresolvedLocalizationAdapter final : public GV2RuntimeCore::ILocalizationAdapter
{
public:
    std::optional<std::string> Resolve(
        const GV2RuntimeCore::FTextSpec&,
        const std::string&) const override
    {
        return std::nullopt;
    }
};

bool TryParsePositive(const std::string& Text, std::int64_t& OutValue)
{
    const char* Begin = Text.data();
    const char* End = Begin + Text.size();
    const auto Result = std::from_chars(Begin, End, OutValue);
    return Result.ec == std::errc{} && Result.ptr == End && OutValue > 0;
}

bool LoadRuntimeSources(
    const char* ExecutableArgument,
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources)
{
    std::vector<std::filesystem::path> ScriptDirectories{
        std::filesystem::current_path() / "Scripts",
        std::filesystem::current_path() / ".." / "Scripts",
    };
    std::error_code PathError;
    const std::filesystem::path ExecutablePath = std::filesystem::absolute(
        ExecutableArgument,
        PathError);
    if (!PathError)
    {
        ScriptDirectories.push_back(
            ExecutablePath.parent_path().parent_path().parent_path() / "Scripts");
    }

    for (const std::filesystem::path& Directory : ScriptDirectories)
    {
        std::error_code IterationError;
        if (!std::filesystem::is_directory(Directory, IterationError) || IterationError)
        {
            continue;
        }

        std::vector<GV2ContentHostSupport::FDiscoveredScriptSource> Discovered =
            GV2ContentHostSupport::DiscoverPackageScripts(Directory.parent_path(), "core");
        if (Discovered.empty())
        {
            Discovered = GV2ContentHostSupport::DiscoverPackageScripts(Directory, "core");
        }
        if (!Discovered.empty())
        {
            OutSources.clear();
            OutSources.reserve(Discovered.size());
            for (auto& Src : Discovered)
            {
                OutSources.push_back({std::move(Src.Name), std::move(Src.Text)});
            }
            return true;
        }
    }
    return false;
}

class FHeadlessFilesystemContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FHeadlessFilesystemContentSourceProvider(std::filesystem::path InPackageRoot, std::string InPackageId)
        : PackageRoot(std::move(InPackageRoot))
        , PackageId(std::move(InPackageId))
    {
    }

    std::optional<std::string> ReadSource(
        const std::string_view RequestedPackageId,
        const std::string_view RelativeSource) const override
    {
        if (RequestedPackageId != PackageId)
        {
            return std::nullopt;
        }
        std::ifstream Stream(PackageRoot / std::string(RelativeSource), std::ios::binary);
        if (!Stream)
        {
            return std::nullopt;
        }
        return std::string{std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>()};
    }

private:
    std::filesystem::path PackageRoot;
    std::string PackageId;
};

// PCC-35: builds the single-package pinned repository read handle used
// before Lua bootstrap. Mirrors gv2-content's BuildFromPackageRoot
// (Tools/Content/Source/main.cpp) so CLI/headless/UE stay on one discovery
// convention (self-describing schema resources) and the same
// GV2ContentCore::BuildRepository() reference path (PCC-38 parity).
GV2ContentCore::FBuildResult BuildRepositoryFromDirectories(const std::vector<std::filesystem::path>& PackageRoots)
{
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Descriptors =
        GV2ContentHostSupport::DiscoverPackagesFromDirectories(PackageRoots, Diagnostics);
    if (!Descriptors)
    {
        return GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics));
    }

    GV2ContentHostSupport::FMultiPackageSourceProvider Provider;
    for (std::size_t Index = 0; Index < Descriptors->size(); ++Index)
    {
        Provider.RegisterPackage((*Descriptors)[Index].GetPackageId(), PackageRoots[Index]);
    }

    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;

    return GV2ContentCore::BuildRepository(*Descriptors, Options);
}

GV2ContentCore::FBuildResult BuildRepositoryFromDirectory(const std::filesystem::path& PackageRoot)
{
    return BuildRepositoryFromDirectories({PackageRoot});
}

std::optional<std::filesystem::path> LoadContentRoot(
    const char* ExecutableArgument,
    const std::optional<std::string>& ExplicitRoot)
{
    if (ExplicitRoot)
    {
        return std::filesystem::path(*ExplicitRoot);
    }

    std::vector<std::filesystem::path> CandidateRoots{
        std::filesystem::current_path() / "GameData" / "core",
        std::filesystem::current_path() / ".." / "GameData" / "core",
    };
    std::error_code PathError;
    const std::filesystem::path ExecutablePath = std::filesystem::absolute(ExecutableArgument, PathError);
    if (!PathError)
    {
        CandidateRoots.push_back(
            ExecutablePath.parent_path().parent_path().parent_path()
            / "GameData" / "core");
    }

    for (const std::filesystem::path& Candidate : CandidateRoots)
    {
        std::error_code CandidateError;
        if (std::filesystem::is_directory(Candidate, CandidateError) && !CandidateError)
        {
            return Candidate;
        }
    }
    return std::nullopt;
}

void PrintRepositoryDiagnostics(const std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    for (const GV2ContentCore::FDiagnostic& Diagnostic : Diagnostics)
    {
        std::cerr << (Diagnostic.Severity == GV2ContentCore::EDiagnosticSeverity::Error ? "error" : "warning")
                   << " " << Diagnostic.Code;
        if (Diagnostic.RelativeSource)
        {
            std::cerr << " " << *Diagnostic.RelativeSource;
            if (Diagnostic.Span)
            {
                std::cerr << ":" << Diagnostic.Span->StartLine << ":" << Diagnostic.Span->StartColumn;
            }
        }
        std::cerr << " " << Diagnostic.Message << "\n";
    }
}

bool RunSharedJson5FixtureConformance()
{
    namespace Fs = std::filesystem;
    using GV2ContentCore::Testing::FJson5Fixture;

    const std::vector<Fs::path> CandidateRoots{
        Fs::current_path() / "Tests" / "Fixtures" / "PortableContentCore",
        Fs::current_path() / ".." / "Tests" / "Fixtures" / "PortableContentCore",
    };

    Fs::path FixtureRoot;
    for (const Fs::path& Candidate : CandidateRoots)
    {
        if (Fs::is_regular_file(Candidate / "fixtures.index"))
        {
            FixtureRoot = Candidate;
            break;
        }
    }
    if (FixtureRoot.empty()) return false;

    std::ifstream IndexStream(FixtureRoot / "fixtures.index");
    if (!IndexStream) return false;

    std::vector<FJson5Fixture> Fixtures;
    std::string RelativePath;
    while (std::getline(IndexStream, RelativePath))
    {
        if (!RelativePath.empty() && RelativePath.back() == '\r') RelativePath.pop_back();
        if (RelativePath.empty() || RelativePath.front() == '#') continue;

        std::ifstream SourceStream(FixtureRoot / RelativePath, std::ios::binary);
        if (!SourceStream)
        {
            std::cerr << "Failed to open fixture: " << (FixtureRoot / RelativePath) << '\n';
            return false;
        }
        FJson5Fixture Fixture;
        Fixture.RelativePath = RelativePath;
        Fixture.Source.assign(
            std::istreambuf_iterator<char>(SourceStream),
            std::istreambuf_iterator<char>());
        // PKG-01 (plan PackageSupport): package.json5 manifests are
        // well-formed identity documents, not the fixture's own failure
        // mode — excluded from both blanket per-directory expectations
        // below even though they now live inside these fixture
        // directories (manifest is mandatory as of PKG-01).
        const bool bIsManifest = RelativePath.ends_with("/package.json5");
        if (!bIsManifest && RelativePath.starts_with("invalid/duplicate_key/"))
        {
            Fixture.ExpectedDiagnosticCode = "core:diagnostic.json5.duplicate_key";
        }
        else if (!bIsManifest && RelativePath.starts_with("invalid/nesting_depth_exceeded/"))
        {
            Fixture.ExpectedDiagnosticCode = "core:diagnostic.json5.limit.nesting_depth";
        }
        Fixtures.push_back(std::move(Fixture));
    }

    std::string Failure;
    if (!GV2ContentCore::Testing::RunJson5FixtureConformance(Fixtures, Failure))
    {
        std::cerr << Failure << '\n';
        return false;
    }

    class FDuplicateCoreValidator final : public GV2ContentCore::ISemanticValidator
    {
    public:
        std::string_view GetId() const override { return "core:validator.item.positive_price"; }
        void Validate(
            const GV2ContentCore::FValue&,
            const GV2ContentCore::FSemanticCandidateView&,
            const GV2ContentCore::FSemanticValidationContext&,
            std::vector<GV2ContentCore::FDiagnostic>&) const override {}
    };

    class FFixtureProvider final : public GV2ContentCore::IContentSourceProvider
    {
    public:
        std::map<std::string, Fs::path> PackageRoots;
        std::map<std::string, std::string> InlineSources;

        std::optional<std::string> ReadSource(
            const std::string_view PackageId,
            const std::string_view RelativeSource) const override
        {
            const std::string Key = std::string(PackageId) + "/" + std::string(RelativeSource);
            if (const auto Inline = InlineSources.find(Key); Inline != InlineSources.end())
            {
                return Inline->second;
            }
            const auto Root = PackageRoots.find(std::string(PackageId));
            if (Root == PackageRoots.end()) return std::nullopt;
            std::ifstream Stream(
                Root->second / std::string(RelativeSource),
                std::ios::binary);
            if (!Stream) return std::nullopt;
            return std::string{
                std::istreambuf_iterator<char>(Stream),
                std::istreambuf_iterator<char>()};
        }

    };

    FFixtureProvider Provider;
    Provider.PackageRoots.emplace("core", FixtureRoot / "valid" / "core");
    Provider.PackageRoots.emplace("test_mod", FixtureRoot / "valid" / "test_mod");
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const GV2ContentCore::FBuildResult CoreBuild = GV2ContentCore::BuildRepository(
        { GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor() }, Options);
    if (!CoreBuild.IsSuccess())
    {
        std::cerr << "CoreBuild failed: " << (CoreBuild.GetDiagnostics().empty() ? "none" : CoreBuild.GetDiagnostics().front().Code) << '\n';
        return false;
    }
    auto ContainsDefinitionId = [](const GV2ContentCore::FValue& DefinitionsArray, const char* Id) {
        for (const GV2ContentCore::FValue& Definition : DefinitionsArray.AsArray())
        {
            if (Definition.FindField("id")->AsString() == Id) return true;
        }
        return false;
    };

    const GV2ContentCore::FValue* Definitions =
        CoreBuild.GetCandidate().GetRootValue().FindField("definitions");
    // TAS-10: representative membership, not a pinned total count — the
    // frozen corpus (TAS-06) may still gain a definition when the subject
    // of the change is content-resolution rules themselves, and this check
    // must not need updating for that.
    if (Definitions == nullptr || !Definitions->IsArray() || Definitions->AsArray().empty()
        || !ContainsDefinitionId(*Definitions, "core:item.weapon.iron_sword")
        || !ContainsDefinitionId(*Definitions, "core:location.city.market")
        || !ContainsDefinitionId(*Definitions, "core:screen.main")
        || !ContainsDefinitionId(*Definitions, "core:text.item.iron_sword.name"))
    {
        std::cerr << "CoreBuild missing expected representative definitions\n";
        return false;
    }

    const auto Core = GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor();
    const auto TestMod = GV2ContentCore::Testing::MakeRepresentativeTestModPackageDescriptor();
    const GV2ContentCore::FBuildResult ModBuild = GV2ContentCore::BuildRepository(
        { Core, TestMod }, Options);
    if (!ModBuild.IsSuccess())
    {
        std::cerr << "representative M4 build failed: "
            << ModBuild.GetDiagnostics().front().Code << '\n';
        return false;
    }
    if (ModBuild.GetCandidate().GetStage()
        != GV2ContentCore::ECandidateStage::RepositoryResolved)
    {
        std::cerr << "ModBuild stage mismatch\n";
        return false;
    }
    Definitions = ModBuild.GetCandidate().GetRootValue().FindField("definitions");
    if (Definitions == nullptr || !Definitions->IsArray() || Definitions->AsArray().empty())
    {
        std::cerr << "ModBuild definitions empty or missing\n";
        return false;
    }
    {
        // TAS-10: uniqueness after override resolution is the real
        // invariant (every winner id appears exactly once — an override
        // must replace, never duplicate), not a pinned total count.
        std::set<std::string> SeenIds;
        for (const GV2ContentCore::FValue& Definition : Definitions->AsArray())
        {
            const std::string Id = Definition.FindField("id")->AsString();
            if (!SeenIds.insert(Id).second)
            {
                std::cerr << "ModBuild duplicate winner id: " << Id << '\n';
                return false;
            }
        }
        if (!SeenIds.count("test_mod:screen.codex_lab") || !SeenIds.count("core:location.city.market"))
        {
            std::cerr << "ModBuild missing expected winners\n";
            return false;
        }
    }
    const GV2ContentCore::FValue* Inventory = nullptr;
    for (const GV2ContentCore::FValue& Definition : Definitions->AsArray())
    {
        if (Definition.FindField("id")->AsString() == "core:screen.inventory") Inventory = &Definition;
    }
    if (Inventory == nullptr
        || Inventory->FindField("tags")->AsArray().size() != 1
        || Inventory->FindField("tags")->AsArray()[0].AsString() != "test_mod_override"
        || Inventory->FindField("deprecated")->AsBoolean()) return false;
    const GV2ContentCore::FRepositoryReadHandle Handle = ModBuild.GetCandidate().GetReadHandle();
    if (!Handle.IsValid() || Handle.GetContentHash().size() != 64) return false;
    const auto RedirectId = GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.codex_archive");
    const auto CanonicalId = GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.codex_lab");
    if (Handle.Find(RedirectId) != Handle.Find(CanonicalId)) return false;
    const auto* RedirectProvenance = Handle.GetProvenance(RedirectId);
    if (RedirectProvenance == nullptr
        || RedirectProvenance->CanonicalId != "test_mod:screen.codex_lab"
        || RedirectProvenance->RedirectChain.size() != 3) return false;
    const auto* InventoryProvenance = Handle.GetProvenance(
        GV2ContentCore::FDefinitionId::Require("core:screen.inventory"));
    if (InventoryProvenance == nullptr
        || InventoryProvenance->Winner.PackageId != "test_mod"
        || InventoryProvenance->ShadowedProviders.size() != 1
        || InventoryProvenance->ShadowedProviders[0].PackageId != "core"
        || InventoryProvenance->Winner.SchemaId != "core:schema.definition.screen.v1"
        || InventoryProvenance->Winner.SchemaVersion != 1
        || InventoryProvenance->Winner.SourceSpan.StartLine <= 1) return false;
    const auto Screens = Handle.List("screen");
    // TAS-10: membership and canonical sort order are the real invariants,
    // not a pinned count — the frozen corpus may still gain a screen.
    {
        const bool bSorted = std::is_sorted(Screens.begin(), Screens.end(),
            [](const auto& A, const auto& B) { return A.ToString() < B.ToString(); });
        bool bHasInventory = false;
        bool bHasCodexLab = false;
        for (const auto& Screen : Screens)
        {
            if (Screen.ToString() == "core:screen.inventory") bHasInventory = true;
            if (Screen.ToString() == "test_mod:screen.codex_lab") bHasCodexLab = true;
        }
        if (!bSorted || !bHasInventory || !bHasCodexLab) return false;
    }
    const auto Tombstoned = Handle.Require(GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.retired"));
    if (Tombstoned || !Tombstoned.Error.has_value()
        || Tombstoned.Error->Code != "core:diagnostic.repository.read.tombstoned") return false;
    const GV2ContentCore::FRepositoryReadHandle InvalidHandle;
    const auto InvalidHandleResult = InvalidHandle.Require(
        GV2ContentCore::FDefinitionId::Require("core:screen.inventory"));
    if (InvalidHandleResult || !InvalidHandleResult.Error.has_value()
        || InvalidHandleResult.Error->Code
            != "core:diagnostic.repository.read.invalid_handle") return false;
    bool bInvalidHashThrows = false;
    try
    {
        (void)InvalidHandle.GetContentHash();
    }
    catch (const std::logic_error&)
    {
        bInvalidHashThrows = true;
    }
    if (!bInvalidHashThrows) return false;
    const GV2ContentCore::FValue* Location = Handle.Find(
        GV2ContentCore::FDefinitionId::Require("core:location.city.market"));
    if (Location == nullptr
        || Location->FindField("data")->FindField("screen_ids")->AsArray()[0].AsString()
            != "test_mod:screen.codex_lab") return false;

    const GV2ContentCore::FPackageDescriptor FilePermutedMod(
        "test_mod", "test_mod", 1,
        { "definitions/texts.json5", "definitions/screens.json5", "definitions/locations.json5" }, {}, {},
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const GV2ContentCore::FBuildResult FilePermuted = GV2ContentCore::BuildRepository(
        { FilePermutedMod, Core }, Options);
    if (!FilePermuted.IsSuccess()
        || FilePermuted.GetCandidate() != ModBuild.GetCandidate()
        || FilePermuted.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash()) return false;

    FFixtureProvider EntryPermutedProvider = Provider;
    EntryPermutedProvider.InlineSources["test_mod/definitions/screens.json5"] = R"json5(
    {
      schema_version: 1,
      type: "screen",
      definitions: [
        {
          id: "test_mod:screen.codex_lab",
          data: { title_text_id: "test_mod:text.screen.codex_lab.title" },
        },
        {
          id: "core:screen.inventory",
          data: { title_text_id: "core:text.screen.inventory.title" },
          tags: ["test_mod_override"],
        },
      ],
    })json5";
    GV2ContentCore::FBuildOptions EntryPermutedOptions;
    EntryPermutedOptions.SourceProvider = &EntryPermutedProvider;
    const GV2ContentCore::FBuildResult EntryPermuted = GV2ContentCore::BuildRepository(
        { Core, TestMod }, EntryPermutedOptions);
    if (!EntryPermuted.IsSuccess()
        || EntryPermuted.GetCandidate() != ModBuild.GetCandidate()) return false;

    FFixtureProvider NumericSpellingProvider = Provider;
    NumericSpellingProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { type: "item", definitions: [{
      extensions: {}, deprecated: false, tags: ["weapon", "melee"],
      data: { icon_resource_id: "core:resource.item.iron_sword.icon",
        price: 0xa, label_text_id: "core:text.item.iron_sword.name" },
      id: "core:item.weapon.iron_sword",
    }], schema_version: 1, extensions: {} })json5";
    GV2ContentCore::FBuildOptions NumericSpellingOptions;
    NumericSpellingOptions.SourceProvider = &NumericSpellingProvider;
    const auto NumericSpelling = GV2ContentCore::BuildRepository(
        { Core, TestMod }, NumericSpellingOptions);
    if (!NumericSpelling.IsSuccess()
        || NumericSpelling.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash()) return false;
    // TAS-09: single source of truth, shared with
    // GV2ContentCoreRepositoryResolutionTests.cpp (UE) — this representative
    // core+test_mod merged content hash is pinned exactly once, in this
    // file, next to expected_core_content_hash.txt.
    std::ifstream MergedHashStream(FixtureRoot.parent_path() / "expected_merged_content_hash.txt");
    if (!MergedHashStream) return false;
    std::string ExpectedMergedHash(
        (std::istreambuf_iterator<char>(MergedHashStream)),
        std::istreambuf_iterator<char>());
    while (!ExpectedMergedHash.empty()
        && (ExpectedMergedHash.back() == '\n' || ExpectedMergedHash.back() == '\r' || ExpectedMergedHash.back() == ' '))
    {
        ExpectedMergedHash.pop_back();
    }
    if (Handle.GetContentHash() != ExpectedMergedHash) return false;

    FFixtureProvider ChangedActiveProvider = Provider;
    ChangedActiveProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { schema_version: 1, type: "item", definitions: [{
      id: "core:item.weapon.iron_sword",
      data: { label_text_id: "core:text.item.iron_sword.name", price: 11,
        icon_resource_id: "core:resource.item.iron_sword.icon" },
      tags: ["weapon", "melee"], deprecated: false, extensions: {},
    }], extensions: {} })json5";
    GV2ContentCore::FBuildOptions ChangedActiveOptions;
    ChangedActiveOptions.SourceProvider = &ChangedActiveProvider;
    const auto ChangedActive = GV2ContentCore::BuildRepository(
        { Core, TestMod }, ChangedActiveOptions);
    if (!ChangedActive.IsSuccess()
        || ChangedActive.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;

    const GV2ContentCore::FPackageDescriptor ReindexedMod(
        "test_mod", "test_mod", 2, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const auto Reindexed = GV2ContentCore::BuildRepository({ Core, ReindexedMod }, Options);
    if (!Reindexed.IsSuccess()
        || Reindexed.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;

    std::vector<std::string> ExtendedTombstones = TestMod.GetTombstones();
    ExtendedTombstones.push_back("test_mod:item.retired");
    const GV2ContentCore::FPackageDescriptor AdditionalRetirementMod(
        "test_mod", "test_mod", 1, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), std::move(ExtendedTombstones));
    const auto AdditionalRetirement = GV2ContentCore::BuildRepository(
        { Core, AdditionalRetirementMod }, Options);
    if (!AdditionalRetirement.IsSuccess()
        || AdditionalRetirement.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;
    const auto Repeated = GV2ContentCore::BuildRepository({ Core, TestMod }, Options);
    if (!Repeated.IsSuccess() || Repeated.GetCandidate() != ModBuild.GetCandidate()) return false;

    FDuplicateCoreValidator DuplicateCoreValidator;
    GV2ContentCore::FSemanticValidatorRegistry HostValidators;
    std::vector<GV2ContentCore::FDiagnostic> RegistryDiagnostics;
    if (!HostValidators.Register(DuplicateCoreValidator, RegistryDiagnostics)) return false;
    GV2ContentCore::FBuildOptions DuplicateValidatorOptions = Options;
    DuplicateValidatorOptions.SemanticValidatorRegistry = &HostValidators;
    const auto DuplicateValidatorBuild = GV2ContentCore::BuildRepository(
        { Core, TestMod }, DuplicateValidatorOptions);
    if (DuplicateValidatorBuild.IsSuccess()
        || DuplicateValidatorBuild.GetDiagnostics().front().Code
            != "core:diagnostic.semantic.validator.duplicate_id") return false;

    const auto RunInvalid = [&](const std::string& Group, const GV2ContentCore::FPackageDescriptor& Mod)
    {
        FFixtureProvider InvalidProvider;
        InvalidProvider.PackageRoots.emplace("core", FixtureRoot / "valid" / "core");
        InvalidProvider.PackageRoots.emplace("test_mod", FixtureRoot / "invalid" / Group / "test_mod");
        GV2ContentCore::FBuildOptions InvalidOptions;
        InvalidOptions.SourceProvider = &InvalidProvider;
        return GV2ContentCore::BuildRepository({ Core, Mod }, InvalidOptions);
    };
    const auto SingleSourceMod = [](const std::string& Source)
    {
        return GV2ContentCore::FPackageDescriptor("test_mod", "test_mod", 1, { Source });
    };
    const auto CheckFailure = [](const GV2ContentCore::FBuildResult& Result, const std::string& Code)
    {
        return Result.IsFailure() && !Result.GetDiagnostics().empty()
            && Result.GetDiagnostics().front().Code == Code;
    };
    if (!CheckFailure(
            RunInvalid("foreign_new_id", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.repository.identity.foreign_new_id")) return false;
    if (!CheckFailure(
            RunInvalid("broken_override", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.schema.value.missing_required_field")) return false;
    if (!CheckFailure(
            RunInvalid("missing_reference", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.reference.target_missing")) return false;
    if (!CheckFailure(
            RunInvalid("semantic_failure", SingleSourceMod("definitions/items.json5")),
            "core:diagnostic.semantic.item.price_not_positive")) return false;
    const GV2ContentCore::FPackageDescriptor ClassMod(
        "test_mod", "test_mod", 1,
        { "definitions/items.json5", "definitions/resources.json5" },
        { GV2ContentCore::FSchemaBinding(
            "resource", 2, "test_mod:schema.definition.resource.v2",
            "schemas/resource_v2.schema.json5") });
    if (!CheckFailure(
            RunInvalid("resource_class_mismatch", ClassMod),
            "core:diagnostic.reference.resource_class_mismatch")) return false;
    const GV2ContentCore::FPackageDescriptor CycleMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        {
            GV2ContentCore::FRedirectDescriptor("test_mod:screen.a", "test_mod:screen.b"),
            GV2ContentCore::FRedirectDescriptor("test_mod:screen.b", "test_mod:screen.a"),
        });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", CycleMod),
            "core:diagnostic.repository.redirect.cycle")) return false;
    const GV2ContentCore::FPackageDescriptor MissingRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old", "test_mod:screen.missing") });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", MissingRedirectTargetMod),
            "core:diagnostic.repository.redirect.target_missing")) return false;
    const GV2ContentCore::FPackageDescriptor TombstonedRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old", "test_mod:screen.removed") },
        { "test_mod:screen.removed" });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", TombstonedRedirectTargetMod),
            "core:diagnostic.repository.redirect.target_tombstoned")) return false;
    const GV2ContentCore::FPackageDescriptor ActiveRedirectMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old_name", "core:screen.main") });
    if (!CheckFailure(
            RunInvalid("active_redirect_source", ActiveRedirectMod),
            "core:diagnostic.repository.redirect.active_source_conflict")) return false;
    const GV2ContentCore::FPackageDescriptor ActiveTombstoneMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {}, {},
        { "test_mod:screen.old_name" });
    if (!CheckFailure(
            RunInvalid("active_redirect_source", ActiveTombstoneMod),
            "core:diagnostic.repository.tombstone.active_definition_conflict")) return false;
    return true;
}

int Run(
    const std::int64_t CommandCount,
    const std::int64_t Seed,
    const bool bSelfTest,
    const std::vector<GV2RuntimeCore::FRuntimeSource>& RuntimeSources,
    const std::optional<std::filesystem::path>& ContentRoot,
    const std::optional<std::string>& ManifestPath = std::nullopt,
    const std::optional<std::string>& OutputManifestPath = std::nullopt,
    const std::optional<std::string>& OutputDigestPath = std::nullopt)
{
    if (const std::string Error = GV2ContentCore::Testing::RunValueModelConformance(); !Error.empty())
    {
        std::cerr << "pcc_value_model_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunDiagnosticModelConformance(); !Error.empty())
    {
        std::cerr << "pcc_diagnostic_model_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunBuildResultConformance(); !Error.empty())
    {
        std::cerr << "pcc_build_result_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunPackageDescriptorConformance(); !Error.empty())
    {
        std::cerr << "pcc_package_descriptor_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunParseLimitsConformance(); !Error.empty())
    {
        std::cerr << "pcc_parse_limits_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunJson5LexerConformance(); !Error.empty())
    {
        std::cerr << "pcc_json5_lexer_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunJson5ParserConformance(); !Error.empty())
    {
        std::cerr << "pcc_json5_parser_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunSchemaRegistryConformance(); !Error.empty())
    {
        std::cerr << "pcc_schema_registry_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunScalarValidationConformance(); !Error.empty())
    {
        std::cerr << "pcc_scalar_validation_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunContainerValidationConformance(); !Error.empty())
    {
        std::cerr << "pcc_container_validation_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunPresenceDefaultConformance(); !Error.empty())
    {
        std::cerr << "pcc_presence_default_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunSpecialFieldValidationConformance(); !Error.empty())
    {
        std::cerr << "pcc_special_field_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunDefinitionEnvelopeConformance(); !Error.empty())
    {
        std::cerr << "pcc_definition_envelope_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunExtensionSchemaConformance(); !Error.empty())
    {
        std::cerr << "pcc_extension_schema_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (const std::string Error = GV2ContentCore::Testing::RunPoParserConformance(); !Error.empty())
    {
        std::cerr << "pcc_po_parser_self_test_failed: " << Error << "\n";
        return 1;
    }
    if (!RunSharedJson5FixtureConformance())
    {
        std::cerr << "pcc_shared_json5_fixture_conformance_failed\n";
        return 1;
    }
    if (const std::string RepoConformanceError = GV2RuntimeCore::Testing::RunLuaRepositoryAccessConformance(); !RepoConformanceError.empty())
    {
        std::cerr << "pcc_lua_repository_conformance_failed: " << RepoConformanceError << "\n";
        return 1;
    }
    if (const std::string ManifestConformanceError = GV2RuntimeCore::Testing::RunRunManifestConformance(); !ManifestConformanceError.empty())
    {
        std::cerr << "pcc_run_manifest_conformance_failed: " << ManifestConformanceError << "\n";
        return 1;
    }
    if (const std::string DigestConformanceError = GV2RuntimeCore::Testing::RunRunDigestConformance(); !DigestConformanceError.empty())
    {
        std::cerr << "pcc_run_digest_conformance_failed: " << DigestConformanceError << "\n";
        return 1;
    }
    if (const std::string ReplayConformanceError = GV2RuntimeCore::Testing::RunRunReplayConformance(); !ReplayConformanceError.empty())
    {
        std::cerr << "pcc_run_replay_conformance_failed: " << ReplayConformanceError << "\n";
        return 1;
    }

    // PCC-35: pin a repository read handle before Lua bootstrap. Missing or
    // invalid repository content blocks session startup entirely.
    if (!ContentRoot)
    {
        std::cerr << "content_root_not_found\n";
        return 9;
    }
    const GV2ContentCore::FBuildResult RepositoryBuild = BuildRepositoryFromDirectory(*ContentRoot);
    if (RepositoryBuild.IsFailure())
    {
        PrintRepositoryDiagnostics(RepositoryBuild.GetDiagnostics());
        std::cerr << "repository_build_failed\n";
        return 9;
    }
    const GV2ContentCore::FRepositoryReadHandle RepositoryHandle =
        RepositoryBuild.GetCandidate().GetReadHandle();
    // Exercise the pinned handle with a portable repository query.
    const std::size_t RepositoryItemCount = RepositoryHandle.List("item").size();

    FMetadataOnlyResourceCatalog Resources;
    const auto Resource = Resources.FindMetadata("core:resource.image.headless_fixture");
    if (!Resource || !Resource->bAvailable)
    {
        std::cerr << "metadata_resource_catalog_failed\n";
        return 3;
    }

    FUnresolvedLocalizationAdapter Localization;
    GV2RuntimeCore::FTextSpec Text;
    Text.TextId = "core:text.headless.fixture";
    if (Localization.Resolve(Text, "").has_value())
    {
        std::cerr << "headless_localization_should_remain_unresolved\n";
        return 4;
    }

    if (ManifestPath.has_value())
    {
        std::ifstream ManifestFile(*ManifestPath, std::ios::binary);
        if (!ManifestFile)
        {
            std::cerr << "failed to open manifest file: " << *ManifestPath << '\n';
            return 64;
        }
        std::string ManifestJson((std::istreambuf_iterator<char>(ManifestFile)), std::istreambuf_iterator<char>());
        GV2RuntimeCore::FRunManifest Manifest;
        std::string ParseError;
        if (!GV2RuntimeCore::DeserializeRunManifest(ManifestJson, Manifest, ParseError))
        {
            std::cerr << "manifest_parse_failed: " << ParseError << '\n';
            return 64;
        }

        if (Manifest.RepositoryContentHash != RepositoryHandle.GetContentHash())
        {
            std::cerr << "repository_content_hash_mismatch manifest=" << Manifest.RepositoryContentHash
                      << " pinned=" << RepositoryHandle.GetContentHash() << '\n';
            return 2;
        }

        const auto Started = std::chrono::steady_clock::now();
        GV2RuntimeCore::FRunResult RunResult;
        GV2RuntimeCore::FRuntimeFault Fault;
        if (!GV2RuntimeCore::ReplayRunManifest(Manifest, RepositoryHandle, RuntimeSources, RunResult, Fault))
        {
            std::cerr << "replay_failed code=" << Fault.Code << " message=" << Fault.Message << '\n';
            return 5;
        }
        const auto Finished = std::chrono::steady_clock::now();
        const double Seconds = std::chrono::duration<double>(Finished - Started).count();
        const double CommandsPerSecond = Seconds > 0.0
            ? static_cast<double>(Manifest.AcceptedCommands.size()) / Seconds
            : 0.0;

        const GV2RuntimeCore::FRunDigest Digest = GV2RuntimeCore::ComputeRunDigest(Manifest, RunResult);

        if (OutputManifestPath.has_value())
        {
            std::ofstream OutFile(*OutputManifestPath, std::ios::binary);
            if (!OutFile)
            {
                std::cerr << "failed to open output manifest file: " << *OutputManifestPath << '\n';
                return 70;
            }
            OutFile << GV2RuntimeCore::SerializeRunManifest(Manifest);
        }

        if (OutputDigestPath.has_value())
        {
            std::ofstream OutFile(*OutputDigestPath, std::ios::binary);
            if (!OutFile)
            {
                std::cerr << "failed to open output digest file: " << *OutputDigestPath << '\n';
                return 70;
            }
            OutFile << GV2RuntimeCore::SerializeRunDigest(Digest);
        }

        std::cout << "{\"ok\":true"
                  << ",\"lua_release_num\":" << GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber
                  << ",\"commands\":" << Manifest.AcceptedCommands.size()
                  << ",\"seed\":" << Manifest.Seed
                  << ",\"commands_per_second\":" << CommandsPerSecond
                  << ",\"repository_content_hash\":\"" << RepositoryHandle.GetContentHash() << "\""
                  << ",\"repository_item_count\":" << RepositoryItemCount
                  << ",\"media_payload_loaded\":false"
                  << ",\"localization_resolved\":false"
                  << ",\"digest_hash\":\"" << Digest.DigestHash << "\""
                  << ",\"digest\":{\"digest_hash\":\"" << Digest.DigestHash << "\""
                  << ",\"lua_release_num\":" << Digest.LuaReleaseNumber
                  << ",\"repository_content_hash\":\"" << Digest.RepositoryContentHash << "\""
                  << ",\"seed\":" << Digest.Seed
                  << ",\"executed_commands_count\":" << Digest.ExecutedCommandsCount
                  << ",\"success\":" << (Digest.bSuccess ? "true" : "false")
                  << ",\"final_screen_id\":\"" << Digest.FinalScreenId << "\""
                  << ",\"state_hash\":\"" << Digest.StateHash << "\""
                  << ",\"fault_code\":\"" << Digest.FaultCode << "\"}"
                  << "}\n";
        return 0;
    }

    GV2RuntimeCore::FRuntimeSession Runtime;
    GV2RuntimeCore::FRuntimeFault Fault;
    if (!Runtime.Start(1, RepositoryHandle, RuntimeSources, Fault))
    {
        std::cerr << "runtime_start_failed code=" << Fault.Code << " message=" << Fault.Message << '\n';
        return 2;
    }

    GV2RuntimeCore::FRunManifest Manifest;
    Manifest.LuaReleaseNumber = GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber;
    Manifest.RepositoryContentHash = RepositoryHandle.GetContentHash();
    Manifest.Seed = static_cast<std::uint64_t>(Seed);

    const auto Started = std::chrono::steady_clock::now();
    for (std::int64_t Index = 1; Index <= CommandCount; ++Index)
    {
        GV2RuntimeCore::FCommandRequest Request;
        Request.CommandId = "core:command.test.headless_step";
        Request.Sequence = Index;
        Request.Args.emplace("seed", GV2RuntimeCore::FValue(Seed));
        Request.Args.emplace("step", GV2RuntimeCore::FValue(Index));
        if (!Runtime.DispatchCommand(Request, Fault))
        {
            std::cerr << "command_failed sequence=" << Index
                      << " code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 5;
        }

        GV2RuntimeCore::FRunAcceptedCommand Accepted;
        Accepted.CommandId = Request.CommandId;
        Accepted.Sequence = Request.Sequence;
        Accepted.Args = Request.Args;
        Manifest.AcceptedCommands.push_back(std::move(Accepted));
    }
    const auto Finished = std::chrono::steady_clock::now();
    const double Seconds = std::chrono::duration<double>(Finished - Started).count();
    const double CommandsPerSecond = Seconds > 0.0
        ? static_cast<double>(CommandCount) / Seconds
        : 0.0;

    std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;

    if (bSelfTest)
    {
        const std::string StableIdFailure =
            GV2RuntimeCore::Testing::RunStableIdConformance();
        if (!StableIdFailure.empty())
        {
            std::cerr << "stable_id_conformance_failed case=" << StableIdFailure << '\n';
            return 8;
        }

        const std::string MarshallerFailure =
            GV2RuntimeCore::Testing::RunLuaMarshallerConformance();
        if (!MarshallerFailure.empty())
        {
            std::cerr << "lua_marshaller_conformance_failed case=" << MarshallerFailure << '\n';
            return 9;
        }

        const std::string ValidatorRegistryFailure =
            GV2RuntimeCore::Testing::RunValidatorRegistryConformance();
        if (!ValidatorRegistryFailure.empty())
        {
            std::cerr << "validator_registry_conformance_failed case=" << ValidatorRegistryFailure << '\n';
            return 10;
        }

        const std::string LuaSpecRunnerFailure =
            GV2RuntimeCore::Testing::RunLuaSpecRunnerConformance();
        if (!LuaSpecRunnerFailure.empty())
        {
            std::cerr << "lua_spec_runner_conformance_failed case=" << LuaSpecRunnerFailure << '\n';
            return 15;
        }

        // SAV-07: FFilesystemSaveSlotStorage conformance (plan SaveAndLoad).
        const std::string SaveSlotStorageFailure =
            GV2RuntimeCore::Testing::RunSaveSlotStorageConformance();
        if (!SaveSlotStorageFailure.empty())
        {
            std::cerr << "save_slot_storage_conformance_failed case=" << SaveSlotStorageFailure << '\n';
            return 17;
        }

        // SAV-12/13/17: cold-start load conformance (plan SaveAndLoad, M4).
        const std::string ColdStartLoadFailure =
            GV2RuntimeCore::Testing::RunColdStartLoadConformance();
        if (!ColdStartLoadFailure.empty())
        {
            std::cerr << "cold_start_load_conformance_failed case=" << ColdStartLoadFailure << '\n';
            return 18;
        }

        // PKG-01/02/03: package manifest conformance (plan PackageSupport, M1).
        const std::string PackageManifestFailure =
            GV2ContentHostSupport::Testing::RunPackageManifestConformance();
        if (!PackageManifestFailure.empty())
        {
            std::cerr << "package_manifest_conformance_failed case=" << PackageManifestFailure << '\n';
            return 19;
        }

        // PKG-05…09: package discovery and order conformance (plan PackageSupport, M2).
        const std::string DiscoveryOrderFailure =
            GV2ContentHostSupport::Testing::RunPackageDiscoveryAndOrderConformance();
        if (!DiscoveryOrderFailure.empty())
        {
            std::cerr << "package_discovery_and_order_conformance_failed case=" << DiscoveryOrderFailure << '\n';
            return 20;
        }

        // TAS-04: both hosts call this one runner over Tests/Lua/**/*.lua.
        // TAS-12: specs run on a dedicated session (same real GameData/core
        // repository and real Scripts/bootstrap module tree as Runtime
        // above), not the shared Runtime — a spec that opens the mutation
        // window to test a write must not leak that mutation into the
        // digest-relevant RunResult.StateHash captured later from Runtime.
        // A missing Tests/Lua directory is not an error. SAV-02/03: "save"
        // needs nothing beyond the production module tree either (the
        // codec is stateless), so it rides the same loop.
        for (const std::string& Subtree : {"world", "events", "resources", "lifecycle", "save"})
        {
            const std::vector<std::filesystem::path> SpecRootCandidates{
                std::filesystem::current_path() / "Tests" / "Lua" / Subtree,
                std::filesystem::current_path() / ".." / "Tests" / "Lua" / Subtree,
            };
            std::filesystem::path SpecRoot = SpecRootCandidates.front();
            for (const std::filesystem::path& Candidate : SpecRootCandidates)
            {
                std::error_code Ec;
                if (std::filesystem::is_directory(Candidate, Ec) && !Ec)
                {
                    SpecRoot = Candidate;
                    break;
                }
            }

            GV2RuntimeCore::FRuntimeSession SpecSession;
            GV2RuntimeCore::FRuntimeFault SpecSessionFault;
            if (!SpecSession.Start(1, RepositoryHandle, RuntimeSources, SpecSessionFault))
            {
                std::cerr << "lua_spec_session_start_failed code=" << SpecSessionFault.Code
                          << " message=" << SpecSessionFault.Message << '\n';
                return 16;
            }

            // SAV-10/11: Tests/Lua/save/ specs exercise game.save_slots.write
            // through a real (throwaway, temp-dir-rooted) storage backing,
            // not a mock — the same primitive gv2-headless would use for a
            // real save. Harmless to wire for every subtree since only
            // save/ specs call it.
            const std::filesystem::path SaveSlotSpecRoot = std::filesystem::temp_directory_path()
                / ("gv2_headless_save_spec_slots_"
                    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            GV2RuntimeCore::FFilesystemSaveSlotStorage SaveSlotSpecStorage(SaveSlotSpecRoot);
            SpecSession.SetSaveSlotStorage(&SaveSlotSpecStorage);

            GV2TestSupport::FLuaSpecRunResult SpecResult;
            const bool bSpecsPassed = GV2TestSupport::RunLuaSpecs(SpecRoot, SpecSession, SpecResult);
            SpecSession.Stop();
            {
                std::error_code RemoveEc;
                std::filesystem::remove_all(SaveSlotSpecRoot, RemoveEc);
            }
            if (!bSpecsPassed)
            {
                for (const GV2ContentHostSupport::FLuaSpecFailure& Failure : SpecResult.Failures)
                {
                    std::cerr << "lua_spec_failed id=" << Failure.Identifier
                              << " code=" << Failure.Code
                              << " message=" << Failure.Message << '\n';
                }
                return 16;
            }
        }

        // TAS-13: Tests/Lua/commands/*.lua specs need the isolated
        // CommandValidatorSpecs fixture session (test-scoped validators
        // registered before the registry freezes), not the real
        // production session's already-frozen, empty validator registry.
        {
            const std::vector<std::filesystem::path> CommandsSpecRootCandidates{
                std::filesystem::current_path() / "Tests" / "Lua" / "commands",
                std::filesystem::current_path() / ".." / "Tests" / "Lua" / "commands",
            };
            std::filesystem::path CommandsSpecRoot = CommandsSpecRootCandidates.front();
            for (const std::filesystem::path& Candidate : CommandsSpecRootCandidates)
            {
                std::error_code Ec;
                if (std::filesystem::is_directory(Candidate, Ec) && !Ec)
                {
                    CommandsSpecRoot = Candidate;
                    break;
                }
            }

            const std::vector<std::filesystem::path> ScriptsRootCandidates{
                std::filesystem::current_path() / "Scripts",
                std::filesystem::current_path() / ".." / "Scripts",
            };
            std::filesystem::path ScriptsRoot = ScriptsRootCandidates.front();
            for (const std::filesystem::path& Candidate : ScriptsRootCandidates)
            {
                std::error_code Ec;
                if (std::filesystem::is_directory(Candidate, Ec) && !Ec)
                {
                    ScriptsRoot = Candidate;
                    break;
                }
            }

            const std::vector<std::filesystem::path> FixtureRootCandidates{
                std::filesystem::current_path() / "Tests" / "Fixtures" / "CommandValidatorSpecs",
                std::filesystem::current_path() / ".." / "Tests" / "Fixtures" / "CommandValidatorSpecs",
            };
            std::filesystem::path FixtureRoot = FixtureRootCandidates.front();
            for (const std::filesystem::path& Candidate : FixtureRootCandidates)
            {
                std::error_code Ec;
                if (std::filesystem::is_directory(Candidate, Ec) && !Ec)
                {
                    FixtureRoot = Candidate;
                    break;
                }
            }

            GV2RuntimeCore::FRuntimeSession CommandValidatorSession;
            GV2RuntimeCore::FRuntimeFault FixtureFault;
            if (!GV2TestSupport::StartCommandValidatorFixtureSession(
                    ScriptsRoot, FixtureRoot, CommandValidatorSession, FixtureFault))
            {
                std::cerr << "command_validator_fixture_session_start_failed code=" << FixtureFault.Code
                          << " message=" << FixtureFault.Message << '\n';
                return 16;
            }

            GV2TestSupport::FLuaSpecRunResult CommandsSpecResult;
            const bool bCommandsSpecsPassed =
                GV2TestSupport::RunLuaSpecs(CommandsSpecRoot, CommandValidatorSession, CommandsSpecResult);
            CommandValidatorSession.Stop();
            if (!bCommandsSpecsPassed)
            {
                for (const GV2ContentHostSupport::FLuaSpecFailure& Failure : CommandsSpecResult.Failures)
                {
                    std::cerr << "lua_spec_failed id=" << Failure.Identifier
                              << " code=" << Failure.Code
                              << " message=" << Failure.Message << '\n';
                }
                return 16;
            }
        }

        GV2RuntimeCore::FSemanticInput Input;
        Input.SessionGeneration = 1;
        Input.UiInstanceId = "ui@1:1";
        Input.Revision = 1;
        Input.Sequence = CommandCount + 1;
        Input.NodeKeyPath = {"route", "button"};
        Input.CommandId = "core:command.test.semantic_step";
        if (!Runtime.DispatchSemanticInput(Input, Fault))
        {
            std::cerr << "semantic_input_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 6;
        }

        GV2RuntimeCore::FRunAcceptedCommand SemanticAccepted;
        SemanticAccepted.CommandId = Input.CommandId;
        SemanticAccepted.Sequence = Input.Sequence;
        SemanticAccepted.Args = Input.Args;
        Manifest.AcceptedCommands.push_back(std::move(SemanticAccepted));

        GV2RuntimeCore::FCommandRequest StartRequest;
        StartRequest.CommandId = "core:command.debug.start";
        StartRequest.Sequence = CommandCount + 2;
        if (!Runtime.DispatchCommand(StartRequest, Fault)
            || !Runtime.TakePendingScreen(PendingScreen, Fault)
            || !PendingScreen
            || PendingScreen->ScreenId != "core:screen.test"
            || PendingScreen->Fields.size() != 5
            || PendingScreen->Fields[0].FieldId != "buttons"
            || PendingScreen->Fields[1].FieldId != "checkbox"
            || PendingScreen->Fields[2].FieldId != "class_select"
            || PendingScreen->Fields[3].FieldId != "description"
            || PendingScreen->Fields[4].FieldId != "player_name")
        {
            std::cerr << "debug_start_flow_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 7;
        }

        GV2RuntimeCore::FRunAcceptedCommand StartAccepted;
        StartAccepted.CommandId = StartRequest.CommandId;
        StartAccepted.Sequence = StartRequest.Sequence;
        StartAccepted.Args = StartRequest.Args;
        Manifest.AcceptedCommands.push_back(std::move(StartAccepted));
    }

    GV2RuntimeCore::FRunResult RunResult;
    RunResult.bSuccess = true;
    RunResult.ExecutedCommandsCount = Manifest.AcceptedCommands.size();
    RunResult.FinalScreenId = PendingScreen ? PendingScreen->ScreenId : "";
    if (PendingScreen)
    {
        for (const auto& Field : PendingScreen->Fields)
        {
            RunResult.FinalScreenFields.emplace(Field.FieldId, Field.Value);
        }
    }
    RunResult.FaultCode = "";
    RunResult.StateHash = Runtime.GetCanonicalStateHash();

    Runtime.Stop();

    const GV2RuntimeCore::FRunDigest Digest = GV2RuntimeCore::ComputeRunDigest(Manifest, RunResult);

    if (OutputManifestPath.has_value())
    {
        std::ofstream OutFile(*OutputManifestPath, std::ios::binary);
        if (!OutFile)
        {
            std::cerr << "failed to open output manifest file: " << *OutputManifestPath << '\n';
            return 70;
        }
        OutFile << GV2RuntimeCore::SerializeRunManifest(Manifest);
    }

    if (OutputDigestPath.has_value())
    {
        std::ofstream OutFile(*OutputDigestPath, std::ios::binary);
        if (!OutFile)
        {
            std::cerr << "failed to open output digest file: " << *OutputDigestPath << '\n';
            return 70;
        }
        OutFile << GV2RuntimeCore::SerializeRunDigest(Digest);
    }

    std::cout << "{\"ok\":true"
              << ",\"lua_release_num\":" << GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber
              << ",\"commands\":" << CommandCount
              << ",\"seed\":" << Seed
              << ",\"commands_per_second\":" << CommandsPerSecond
              << ",\"repository_content_hash\":\"" << RepositoryHandle.GetContentHash() << "\""
              << ",\"repository_item_count\":" << RepositoryItemCount
              << ",\"media_payload_loaded\":false"
              << ",\"localization_resolved\":false"
              << ",\"digest_hash\":\"" << Digest.DigestHash << "\""
              << ",\"digest\":{\"digest_hash\":\"" << Digest.DigestHash << "\""
              << ",\"lua_release_num\":" << Digest.LuaReleaseNumber
              << ",\"repository_content_hash\":\"" << Digest.RepositoryContentHash << "\""
              << ",\"seed\":" << Digest.Seed
              << ",\"executed_commands_count\":" << Digest.ExecutedCommandsCount
              << ",\"success\":" << (Digest.bSuccess ? "true" : "false")
              << ",\"final_screen_id\":\"" << Digest.FinalScreenId << "\""
              << ",\"state_hash\":\"" << Digest.StateHash << "\""
              << ",\"fault_code\":\"" << Digest.FaultCode << "\"}"
              << "}\n";
    return 0;
}

int RunCheckScripts(
    const std::vector<GV2RuntimeCore::FRuntimeSource>& RuntimeSources,
    const std::optional<std::filesystem::path>& ContentRoot)
{
    if (!ContentRoot)
    {
        std::cerr << "content_root_not_found\n";
        return 9;
    }
    const GV2ContentCore::FBuildResult RepositoryBuild = BuildRepositoryFromDirectory(*ContentRoot);
    if (RepositoryBuild.IsFailure())
    {
        PrintRepositoryDiagnostics(RepositoryBuild.GetDiagnostics());
        std::cerr << "repository_build_failed\n";
        return 9;
    }
    const GV2ContentCore::FRepositoryReadHandle RepositoryHandle =
        RepositoryBuild.GetCandidate().GetReadHandle();

    GV2RuntimeCore::FRuntimeSession Runtime;
    GV2RuntimeCore::FRuntimeFault Fault;
    std::size_t ModuleCount = 0;
    if (!Runtime.CheckScripts(1, RepositoryHandle, RuntimeSources, &ModuleCount, Fault))
    {
        std::cerr << "gv2-headless: script check failed: [" << Fault.Code << "] " << Fault.Message << "\n";
        return 1;
    }

    std::cout << "{\"ok\":true,\"status\":\"ok\",\"modules_checked\":" << ModuleCount
              << ",\"repository_content_hash\":\"" << RepositoryHandle.GetContentHash() << "\"}\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    std::int64_t CommandCount = 1000;
    std::int64_t Seed = 1;
    bool bSelfTest = false;
    bool bCheckScripts = false;
    std::optional<std::string> ExplicitContentRoot;
    std::optional<std::string> ManifestPath;
    std::optional<std::string> OutputManifestPath;
    std::optional<std::string> OutputDigestPath;

    for (int Index = 1; Index < argc; ++Index)
    {
        const std::string Argument = argv[Index];
        if (Argument == "--self-test")
        {
            bSelfTest = true;
        }
        else if (Argument == "--check-scripts")
        {
            bCheckScripts = true;
        }
        else if (Argument.rfind("--commands=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(11), CommandCount))
            {
                std::cerr << "invalid --commands value\n";
                return 64;
            }
        }
        else if (Argument.rfind("--seed=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(7), Seed))
            {
                std::cerr << "invalid --seed value\n";
                return 64;
            }
        }
        else if (Argument.rfind("--content-root=", 0) == 0)
        {
            ExplicitContentRoot = Argument.substr(15);
        }
        else if (Argument.rfind("--manifest=", 0) == 0)
        {
            ManifestPath = Argument.substr(11);
        }
        else if (Argument.rfind("--output-manifest=", 0) == 0)
        {
            OutputManifestPath = Argument.substr(18);
        }
        else if (Argument.rfind("--output-digest=", 0) == 0)
        {
            OutputDigestPath = Argument.substr(16);
        }
        else
        {
            std::cerr << "unknown argument: " << Argument << '\n';
            return 64;
        }
    }

    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    if (!LoadRuntimeSources(argv[0], RuntimeSources))
    {
        std::cerr << "unable to locate a non-empty Scripts module tree\n";
        return 66;
    }
    const std::optional<std::filesystem::path> ContentRoot = LoadContentRoot(argv[0], ExplicitContentRoot);
    if (bCheckScripts)
    {
        return RunCheckScripts(RuntimeSources, ContentRoot);
    }
    return Run(CommandCount, Seed, bSelfTest, RuntimeSources, ContentRoot, ManifestPath, OutputManifestPath, OutputDigestPath);
}
