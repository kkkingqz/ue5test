#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"
#include "Containers/StringConv.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace
{
static_assert(!std::is_constructible_v<
    GV2ContentCore::FRepositoryReadHandle,
    std::shared_ptr<const GV2ContentCore::FRepositorySnapshot>>);
static_assert(!std::is_constructible_v<
    GV2ContentCore::FRepositorySnapshot,
    GV2ContentCore::FValue,
    std::map<std::string, std::size_t>,
    std::map<std::string, std::vector<std::string>>,
    std::map<std::string, GV2ContentCore::FDefinitionProvenance>,
    std::map<std::string, std::string>,
    std::set<std::string>,
    std::string>);

class FResolutionFixtureProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, FString> PackageRoots;
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
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *FPaths::Combine(
                Root->second, UTF8_TO_TCHAR(std::string(RelativeSource).c_str()))))
        {
            return std::nullopt;
        }
        FTCHARToUTF8 Utf8(*Source);
        return std::string(Utf8.Get(), Utf8.Length());
    }
};

class FDuplicateCoreValidator final : public GV2ContentCore::ISemanticValidator
{
public:
    std::string_view GetId() const override
    {
        return "core:validator.item.positive_price";
    }

    void Validate(
        const GV2ContentCore::FValue&,
        const GV2ContentCore::FSemanticCandidateView&,
        const GV2ContentCore::FSemanticValidationContext&,
        std::vector<GV2ContentCore::FDiagnostic>&) const override
    {
    }
};

const GV2ContentCore::FValue* FindDefinition(
    const GV2ContentCore::FBuildResult& Result,
    const std::string_view Id)
{
    const GV2ContentCore::FValue* Definitions =
        Result.GetCandidate().GetRootValue().FindField("definitions");
    if (Definitions == nullptr) return nullptr;
    for (const GV2ContentCore::FValue& Definition : Definitions->AsArray())
    {
        if (Definition.FindField("id")->AsString() == Id) return &Definition;
    }
    return nullptr;
}

GV2ContentCore::FPackageDescriptor MakeSingleSourceMod(
    const std::string& RelativeSource)
{
    return GV2ContentCore::FPackageDescriptor(
        "test_mod", "test_mod", 1, { RelativeSource });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreRepositoryResolutionM4Test,
    "GV2.Runtime.ContentCore.RepositoryResolutionM4",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreRepositoryResolutionM4Test::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;
    const FString FixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore")));
    const FPackageDescriptor Core = Testing::MakeRepresentativeCorePackageDescriptor();
    const FPackageDescriptor TestMod = Testing::MakeRepresentativeTestModPackageDescriptor();

    FResolutionFixtureProvider ValidProvider;
    ValidProvider.PackageRoots.emplace("core", FPaths::Combine(FixtureRoot, TEXT("valid/core")));
    ValidProvider.PackageRoots.emplace("test_mod", FPaths::Combine(FixtureRoot, TEXT("valid/test_mod")));
    FBuildOptions Options;
    Options.SourceProvider = &ValidProvider;
    const FBuildResult Valid = BuildRepository({ Core, TestMod }, Options);
    TestTrue(TEXT("core plus test mod resolves"), Valid.IsSuccess());
    if (!Valid.IsSuccess()) return false;
    TestTrue(TEXT("M4 artifact is publishable"),
        Valid.GetCandidate().GetStage() == ECandidateStage::RepositoryResolved
        && Valid.GetCandidate().IsPublishable());
    const FValue* Definitions = Valid.GetCandidate().GetRootValue().FindField("definitions");
    TestTrue(TEXT("winner set has eleven definitions"),
        Definitions != nullptr && Definitions->AsArray().size() == 11);
    for (std::size_t Index = 1; Definitions != nullptr && Index < Definitions->AsArray().size(); ++Index)
    {
        TestTrue(TEXT("winner definitions use canonical ID order"),
            Definitions->AsArray()[Index - 1].FindField("id")->AsString()
                < Definitions->AsArray()[Index].FindField("id")->AsString());
    }
    const FValue* Inventory = FindDefinition(Valid, "core:screen.inventory");
    TestTrue(TEXT("mod entry is the complete inventory winner"), Inventory != nullptr
        && Inventory->FindField("tags")->AsArray().size() == 1
        && Inventory->FindField("tags")->AsArray()[0].AsString() == "test_mod_override"
        && !Inventory->FindField("deprecated")->AsBoolean());

    const FRepositoryReadHandle Handle = Valid.GetCandidate().GetReadHandle();
    TestTrue(TEXT("resolved candidate returns a valid pinned handle"), Handle.IsValid());
    TestTrue(TEXT("content hash is lowercase SHA-256 hex"),
        Handle.GetContentHash().size() == 64
        && std::all_of(Handle.GetContentHash().begin(), Handle.GetContentHash().end(),
            [](const char Character)
            {
                return (Character >= '0' && Character <= '9')
                    || (Character >= 'a' && Character <= 'f');
            }));
    const FDefinitionId InventoryId = FDefinitionId::Require("core:screen.inventory");
    const FDefinitionId RedirectId = FDefinitionId::Require("test_mod:screen.codex_archive");
    const FDefinitionId CanonicalId = FDefinitionId::Require("test_mod:screen.codex_lab");
    TestTrue(TEXT("Find resolves redirect to final canonical definition"),
        Handle.Find(RedirectId) == Handle.Find(CanonicalId));
    const FDefinitionProvenance* InventoryProvenance = Handle.GetProvenance(InventoryId);
    TestTrue(TEXT("override provenance keeps winner and ordered shadowed provider"),
        InventoryProvenance != nullptr
        && InventoryProvenance->Winner.PackageId == "test_mod"
        && InventoryProvenance->ShadowedProviders.size() == 1
        && InventoryProvenance->ShadowedProviders[0].PackageId == "core"
        && InventoryProvenance->Winner.SchemaId == "core:schema.definition.screen.v1"
        && InventoryProvenance->Winner.SchemaVersion == 1
        && InventoryProvenance->Winner.SourceSpan.StartLine > 1
        && !InventoryProvenance->Winner.RelativeSource.starts_with('/'));
    const FDefinitionProvenance* RedirectProvenance = Handle.GetProvenance(RedirectId);
    TestTrue(TEXT("redirect provenance preserves original, final ID and complete chain"),
        RedirectProvenance != nullptr
        && RedirectProvenance->OriginalId == "test_mod:screen.codex_archive"
        && RedirectProvenance->CanonicalId == "test_mod:screen.codex_lab"
        && RedirectProvenance->RedirectChain == std::vector<std::string>({
            "test_mod:screen.codex_archive",
            "test_mod:screen.codex_legacy",
            "test_mod:screen.codex_lab" }));
    const FValue* Location = Handle.Find(FDefinitionId::Require("core:location.city.market"));
    TestTrue(TEXT("reference payload is normalized to final redirect target"),
        Location != nullptr
        && Location->FindField("data")->FindField("screen_ids")->AsArray()[0].AsString()
            == "test_mod:screen.codex_lab");
    const std::vector<FDefinitionId> Screens = Handle.List("screen");
    TestTrue(TEXT("ByKind contains active definitions only in canonical order"),
        Screens.size() == 3
        && Screens[0].ToString() == "core:screen.inventory"
        && Screens[1].ToString() == "core:screen.main"
        && Screens[2].ToString() == "test_mod:screen.codex_lab");
    const FRepositoryQueryResult Tombstoned = Handle.Require(
        FDefinitionId::Require("test_mod:screen.retired"));
    TestTrue(TEXT("Require returns a structured tombstone error"),
        !Tombstoned
        && Tombstoned.Error.has_value()
        && Tombstoned.Error->Code == "core:diagnostic.repository.read.tombstoned");
    const FRepositoryQueryResult Missing = Handle.Require(
        FDefinitionId::Require("core:screen.missing"));
    TestTrue(TEXT("Require returns a structured missing-ID error"),
        !Missing && Missing.Error.has_value()
        && Missing.Error->Code == "core:diagnostic.repository.read.not_found");
    TestFalse(TEXT("typed DefinitionId rejects malformed input"),
        FDefinitionId::Parse("Core:screen.invalid").has_value());
    const FRepositoryReadHandle InvalidHandle;
    const FRepositoryQueryResult InvalidHandleResult = InvalidHandle.Require(InventoryId);
    TestTrue(TEXT("invalid handle returns its structured query error"),
        !InvalidHandleResult && InvalidHandleResult.Error.has_value()
        && InvalidHandleResult.Error->Code
            == "core:diagnostic.repository.read.invalid_handle");
    bool bInvalidHashThrows = false;
    try
    {
        (void)InvalidHandle.GetContentHash();
    }
    catch (const std::logic_error&)
    {
        bInvalidHashThrows = true;
    }
    TestTrue(TEXT("invalid handle cannot expose a fabricated content hash"),
        bInvalidHashThrows);
    const FRepositoryReadHandle IndependentlyPinned = [&]()
    {
        const FBuildResult Temporary = BuildRepository({ Core, TestMod }, Options);
        return Temporary.GetCandidate().GetReadHandle();
    }();
    TestTrue(TEXT("read handle pins snapshot beyond candidate lifetime"),
        IndependentlyPinned.Find(InventoryId) != nullptr
        && IndependentlyPinned.GetContentHash() == Handle.GetContentHash());

    const FPackageDescriptor PermutedMod(
        "test_mod", "test_mod", 1,
        { "definitions/texts.json5", "definitions/screens.json5", "definitions/locations.json5" }, {}, {},
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const FBuildResult Permuted = BuildRepository({ PermutedMod, Core }, Options);
    TestTrue(TEXT("package/file permutation still succeeds"), Permuted.IsSuccess());
    if (Permuted.IsSuccess())
    {
        TestTrue(TEXT("package/file permutation produces the same winner artifact"),
            Permuted.GetCandidate() == Valid.GetCandidate());
        TestEqual(TEXT("package/file permutation preserves content hash"),
            Permuted.GetCandidate().GetReadHandle().GetContentHash(), Handle.GetContentHash());
    }

    FResolutionFixtureProvider EntryPermutedProvider = ValidProvider;
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
    FBuildOptions EntryPermutedOptions;
    EntryPermutedOptions.SourceProvider = &EntryPermutedProvider;
    const FBuildResult EntryPermuted = BuildRepository({ Core, TestMod }, EntryPermutedOptions);
    TestTrue(TEXT("entry permutation still succeeds"), EntryPermuted.IsSuccess());
    if (EntryPermuted.IsSuccess())
    {
        TestTrue(TEXT("entry permutation produces the same winner artifact"),
            EntryPermuted.GetCandidate() == Valid.GetCandidate());
    }

    FResolutionFixtureProvider NumericSpellingProvider = ValidProvider;
    NumericSpellingProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { // comments and raw numeric spelling are not snapshot identity
      type: "item", definitions: [{
        extensions: {}, deprecated: false, tags: ["weapon", "melee"],
        data: {
          icon_resource_id: "core:resource.item.iron_sword.icon",
          price: 0xa,
          label_text_id: "core:text.item.iron_sword.name",
        },
        id: "core:item.weapon.iron_sword",
      }], schema_version: 1, extensions: {},
    })json5";
    FBuildOptions NumericSpellingOptions;
    NumericSpellingOptions.SourceProvider = &NumericSpellingProvider;
    const FBuildResult NumericSpelling = BuildRepository({ Core, TestMod }, NumericSpellingOptions);
    TestTrue(TEXT("comments, object order and raw numeric spelling preserve hash"),
        NumericSpelling.IsSuccess()
        && NumericSpelling.GetCandidate().GetReadHandle().GetContentHash() == Handle.GetContentHash());

    TestEqual(TEXT("representative snapshot pins canonical SHA-256 output"),
        Handle.GetContentHash(),
        std::string("7e74328e50a01e9dc44982a86c37a63eaae751b40e288eecb6c364d4124121a1"));

    FResolutionFixtureProvider ChangedActiveProvider = ValidProvider;
    ChangedActiveProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { schema_version: 1, type: "item", definitions: [{
      id: "core:item.weapon.iron_sword",
      data: {
        label_text_id: "core:text.item.iron_sword.name",
        price: 11,
        icon_resource_id: "core:resource.item.iron_sword.icon",
      },
      tags: ["weapon", "melee"], deprecated: false, extensions: {},
    }], extensions: {} })json5";
    FBuildOptions ChangedActiveOptions;
    ChangedActiveOptions.SourceProvider = &ChangedActiveProvider;
    const FBuildResult ChangedActive = BuildRepository(
        { Core, TestMod }, ChangedActiveOptions);
    TestTrue(TEXT("active normalized content changes hash"),
        ChangedActive.IsSuccess()
        && ChangedActive.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash());

    const FPackageDescriptor ReindexedMod(
        "test_mod", "test_mod", 2, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const FBuildResult Reindexed = BuildRepository({ Core, ReindexedMod }, Options);
    TestTrue(TEXT("provider identity/order metadata changes hash"),
        Reindexed.IsSuccess()
        && Reindexed.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash());

    std::vector<std::string> ExtendedTombstones = TestMod.GetTombstones();
    ExtendedTombstones.push_back("test_mod:item.retired");
    const FPackageDescriptor AdditionalRetirementMod(
        "test_mod", "test_mod", 1, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), std::move(ExtendedTombstones));
    const FBuildResult AdditionalRetirement = BuildRepository(
        { Core, AdditionalRetirementMod }, Options);
    TestTrue(TEXT("retirement table changes hash"),
        AdditionalRetirement.IsSuccess()
        && AdditionalRetirement.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash());

    const FBuildResult Repeated = BuildRepository({ Core, TestMod }, Options);
    TestTrue(TEXT("repeated single-thread build is equivalent"),
        Repeated.IsSuccess()
        && Repeated.GetCandidate() == Valid.GetCandidate());

    FDuplicateCoreValidator DuplicateCoreValidator;
    FSemanticValidatorRegistry HostValidators;
    std::vector<FDiagnostic> RegistryDiagnostics;
    TestTrue(TEXT("standalone host registry accepts its first validator"),
        HostValidators.Register(DuplicateCoreValidator, RegistryDiagnostics));
    FBuildOptions DuplicateValidatorOptions = Options;
    DuplicateValidatorOptions.SemanticValidatorRegistry = &HostValidators;
    const FBuildResult DuplicateValidatorBuild = BuildRepository(
        { Core, TestMod }, DuplicateValidatorOptions);
    TestTrue(TEXT("core/host semantic validator conflict rejects candidate"),
        DuplicateValidatorBuild.IsFailure());
    if (DuplicateValidatorBuild.IsFailure())
    {
        TestEqual(TEXT("core/host semantic validator conflict code"),
            DuplicateValidatorBuild.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.semantic.validator.duplicate_id"));
    }

    const auto RunInvalidFixture = [&](const TCHAR* Group, const FPackageDescriptor& Mod)
    {
        FResolutionFixtureProvider Provider;
        Provider.PackageRoots.emplace("core", FPaths::Combine(FixtureRoot, TEXT("valid/core")));
        Provider.PackageRoots.emplace("test_mod", FPaths::Combine(FixtureRoot, Group, TEXT("test_mod")));
        FBuildOptions InvalidOptions;
        InvalidOptions.SourceProvider = &Provider;
        return BuildRepository({ Core, Mod }, InvalidOptions);
    };

    const FBuildResult ForeignNew = RunInvalidFixture(
        TEXT("invalid/foreign_new_id"), MakeSingleSourceMod("definitions/screens.json5"));
    TestTrue(TEXT("foreign new ID is rejected"), ForeignNew.IsFailure());
    if (ForeignNew.IsFailure()) TestEqual(TEXT("foreign new ID code"),
        ForeignNew.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.identity.foreign_new_id"));

    const FBuildResult BrokenOverride = RunInvalidFixture(
        TEXT("invalid/broken_override"), MakeSingleSourceMod("definitions/screens.json5"));
    TestTrue(TEXT("broken winning override rejects the whole artifact"), BrokenOverride.IsFailure());
    if (BrokenOverride.IsFailure()) TestEqual(TEXT("broken override keeps schema failure"),
        BrokenOverride.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.schema.value.missing_required_field"));

    const FBuildResult MissingReference = RunInvalidFixture(
        TEXT("invalid/missing_reference"), MakeSingleSourceMod("definitions/screens.json5"));
    TestTrue(TEXT("missing winner reference is rejected"), MissingReference.IsFailure());
    if (MissingReference.IsFailure()) TestEqual(TEXT("missing reference code"),
        MissingReference.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.reference.target_missing"));

    const FBuildResult SemanticFailure = RunInvalidFixture(
        TEXT("invalid/semantic_failure"), MakeSingleSourceMod("definitions/items.json5"));
    TestTrue(TEXT("semantic validator failure rejects artifact"), SemanticFailure.IsFailure());
    if (SemanticFailure.IsFailure()) TestEqual(TEXT("semantic failure code"),
        SemanticFailure.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.semantic.item.price_not_positive"));

    const FPackageDescriptor ClassMod(
        "test_mod", "test_mod", 1,
        { "definitions/items.json5", "definitions/resources.json5" },
        { FSchemaBinding(
            "resource", 2, "test_mod:schema.definition.resource.v2",
            "schemas/resource_v2.schema.json5") });
    const FBuildResult WrongClass = RunInvalidFixture(
        TEXT("invalid/resource_class_mismatch"), ClassMod);
    TestTrue(TEXT("resource class mismatch is rejected"), WrongClass.IsFailure());
    if (WrongClass.IsFailure()) TestEqual(TEXT("resource class mismatch code"),
        WrongClass.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.reference.resource_class_mismatch"));

    const FPackageDescriptor CycleMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        {
            FRedirectDescriptor("test_mod:screen.a", "test_mod:screen.b"),
            FRedirectDescriptor("test_mod:screen.b", "test_mod:screen.a"),
        });
    const FBuildResult Cycle = RunInvalidFixture(TEXT("invalid/redirect_cycle"), CycleMod);
    TestTrue(TEXT("redirect cycle rejects snapshot"), Cycle.IsFailure());
    if (Cycle.IsFailure()) TestEqual(TEXT("redirect cycle code"),
        Cycle.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.redirect.cycle"));

    const FPackageDescriptor MissingRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { FRedirectDescriptor("test_mod:screen.old", "test_mod:screen.missing") });
    const FBuildResult MissingRedirectTarget = RunInvalidFixture(
        TEXT("invalid/redirect_cycle"), MissingRedirectTargetMod);
    TestTrue(TEXT("missing redirect final target rejects snapshot"),
        MissingRedirectTarget.IsFailure());
    if (MissingRedirectTarget.IsFailure()) TestEqual(TEXT("missing redirect target code"),
        MissingRedirectTarget.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.redirect.target_missing"));

    const FPackageDescriptor TombstonedRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { FRedirectDescriptor("test_mod:screen.old", "test_mod:screen.removed") },
        { "test_mod:screen.removed" });
    const FBuildResult TombstonedRedirectTarget = RunInvalidFixture(
        TEXT("invalid/redirect_cycle"), TombstonedRedirectTargetMod);
    TestTrue(TEXT("tombstoned redirect final target rejects snapshot"),
        TombstonedRedirectTarget.IsFailure());
    if (TombstonedRedirectTarget.IsFailure()) TestEqual(TEXT("tombstoned redirect target code"),
        TombstonedRedirectTarget.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.redirect.target_tombstoned"));

    const FPackageDescriptor ActiveRedirectMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { FRedirectDescriptor("test_mod:screen.old_name", "core:screen.main") });
    const FBuildResult ActiveRedirect = RunInvalidFixture(
        TEXT("invalid/active_redirect_source"), ActiveRedirectMod);
    TestTrue(TEXT("active redirect source rejects snapshot"), ActiveRedirect.IsFailure());
    if (ActiveRedirect.IsFailure()) TestEqual(TEXT("active redirect source code"),
        ActiveRedirect.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.redirect.active_source_conflict"));

    const FPackageDescriptor ActiveTombstoneMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {}, {},
        { "test_mod:screen.old_name" });
    const FBuildResult ActiveTombstone = RunInvalidFixture(
        TEXT("invalid/active_redirect_source"), ActiveTombstoneMod);
    TestTrue(TEXT("active tombstone rejects snapshot"), ActiveTombstone.IsFailure());
    if (ActiveTombstone.IsFailure()) TestEqual(TEXT("active tombstone code"),
        ActiveTombstone.GetDiagnostics()[0].Code,
        std::string("core:diagnostic.repository.tombstone.active_definition_conflict"));

    return true;
}

#endif
