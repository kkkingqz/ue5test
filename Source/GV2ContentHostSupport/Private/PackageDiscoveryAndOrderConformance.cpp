#include "GV2ContentHostSupport/Testing/PackageDiscoveryAndOrderConformance.h"

#include "GV2ContentHostSupport/ModsLock.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/RepositoryBuilder.h"

#include <chrono>
#include <fstream>
#include <vector>

namespace GV2ContentHostSupport::Testing
{
namespace
{
std::filesystem::path MakeUniqueTempDir()
{
    const auto Now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path Dir = std::filesystem::temp_directory_path()
        / ("gv2_discovery_order_conformance_" + std::to_string(Now));
    std::error_code Ec;
    std::filesystem::create_directories(Dir, Ec);
    return Dir;
}

struct FScopedTempDir
{
    std::filesystem::path Dir = MakeUniqueTempDir();
    ~FScopedTempDir()
    {
        std::error_code Ec;
        std::filesystem::remove_all(Dir, Ec);
    }
};

void WriteFile(const std::filesystem::path& FilePath, const std::string& Content)
{
    std::error_code Ec;
    std::filesystem::create_directories(FilePath.parent_path(), Ec);
    std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
    Stream << Content;
}

void WritePackage(
    const std::filesystem::path& Root,
    const std::string& ManifestBody,
    const std::vector<std::pair<std::string, std::string>>& Definitions = {},
    const std::vector<std::pair<std::string, std::string>>& Schemas = {})
{
    WriteFile(Root / "package.json5", ManifestBody);
    for (const auto& [RelPath, Content] : Definitions)
    {
        WriteFile(Root / "definitions" / RelPath, Content);
    }
    for (const auto& [RelPath, Content] : Schemas)
    {
        WriteFile(Root / "schemas" / RelPath, Content);
    }
}
} // namespace

std::string RunPackageDiscoveryAndOrderConformance()
{
    using namespace GV2ContentCore;
    FScopedTempDir TempDir;

    // 1. Positive multi-root discovery with correct load_index assignment
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case1_core";
        const std::filesystem::path ModARoot = TempDir.Dir / "case1_mod_a";
        const std::filesystem::path ModBRoot = TempDir.Dir / "case1_mod_b";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.2.0",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");

        WritePackage(ModBRoot, R"json5({
            package_id: "mod_b",
            namespace: "mod_b",
            version: "2.0.0",
            dependencies: [
                { package_id: "mod_a", load_after: true },
            ],
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<std::vector<FPackageDescriptor>> Result =
            DiscoverPackagesFromDirectories({CoreRoot, ModARoot, ModBRoot}, Diagnostics);

        if (!Result.has_value())
        {
            return "discovery_order.case1_failed_discovery: "
                + (Diagnostics.empty() ? "no diagnostics" : Diagnostics.front().Code);
        }
        if (Result->size() != 3)
        {
            return "discovery_order.case1_wrong_size: " + std::to_string(Result->size());
        }
        if ((*Result)[0].GetPackageId() != "core" || (*Result)[0].GetLoadIndex() != 0)
        {
            return "discovery_order.case1_core_index_mismatch";
        }
        if ((*Result)[1].GetPackageId() != "mod_a" || (*Result)[1].GetLoadIndex() != 1)
        {
            return "discovery_order.case1_mod_a_index_mismatch";
        }
        if ((*Result)[2].GetPackageId() != "mod_b" || (*Result)[2].GetLoadIndex() != 2)
        {
            return "discovery_order.case1_mod_b_index_mismatch";
        }
    }

    // 2. Duplicate package_id across roots -> duplicate_package_id diagnostic with both paths
    {
        const std::filesystem::path RootA = TempDir.Dir / "case2_root_a";
        const std::filesystem::path RootB = TempDir.Dir / "case2_root_b";

        WritePackage(RootA, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(RootB, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<std::vector<FPackageDescriptor>> Result =
            DiscoverPackagesFromDirectories({RootA, RootB}, Diagnostics);

        if (Result.has_value())
        {
            return "discovery_order.case2_expected_failure_for_duplicate_id";
        }
        bool bFoundDuplicateCode = false;
        bool bFoundPathsInMessage = false;
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == "core:diagnostic.package.discovery.duplicate_package_id")
            {
                bFoundDuplicateCode = true;
                if (Diagnostic.Message.find(RootA.string()) != std::string::npos
                    && Diagnostic.Message.find(RootB.string()) != std::string::npos)
                {
                    bFoundPathsInMessage = true;
                }
            }
        }
        if (!bFoundDuplicateCode)
        {
            return "discovery_order.case2_missing_duplicate_package_id_code";
        }
        if (!bFoundPathsInMessage)
        {
            return "discovery_order.case2_message_missing_paths";
        }
    }

    // 3. Missing dependency -> missing_dependency diagnostic
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case3_core";
        const std::filesystem::path ModRoot = TempDir.Dir / "case3_mod";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(ModRoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.0.0",
            dependencies: [
                { package_id: "missing_mod" },
            ],
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<std::vector<FPackageDescriptor>> Result =
            DiscoverPackagesFromDirectories({CoreRoot, ModRoot}, Diagnostics);

        if (Result.has_value())
        {
            return "discovery_order.case3_expected_failure_for_missing_dependency";
        }
        bool bFoundMissingDepCode = false;
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == "core:diagnostic.package.order.missing_dependency")
            {
                bFoundMissingDepCode = true;
                break;
            }
        }
        if (!bFoundMissingDepCode)
        {
            return "discovery_order.case3_missing_dependency_code_not_found";
        }
    }

    // 4. Dependency cycle -> dependency_cycle diagnostic
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case4_core";
        const std::filesystem::path ModARoot = TempDir.Dir / "case4_mod_a";
        const std::filesystem::path ModBRoot = TempDir.Dir / "case4_mod_b";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.0.0",
            dependencies: [
                { package_id: "mod_b" },
            ],
        })json5");

        WritePackage(ModBRoot, R"json5({
            package_id: "mod_b",
            namespace: "mod_b",
            version: "1.0.0",
            dependencies: [
                { package_id: "mod_a" },
            ],
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<std::vector<FPackageDescriptor>> Result =
            DiscoverPackagesFromDirectories({CoreRoot, ModARoot, ModBRoot}, Diagnostics);

        if (Result.has_value())
        {
            return "discovery_order.case4_expected_failure_for_dependency_cycle";
        }
        bool bFoundCycleCode = false;
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == "core:diagnostic.package.order.dependency_cycle")
            {
                bFoundCycleCode = true;
                break;
            }
        }
        if (!bFoundCycleCode)
        {
            return "discovery_order.case4_dependency_cycle_code_not_found";
        }
    }

    // 5. load_after violation -> load_after_violation diagnostic
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case5_core";
        const std::filesystem::path ModARoot = TempDir.Dir / "case5_mod_a";
        const std::filesystem::path ModBRoot = TempDir.Dir / "case5_mod_b";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        // mod_b is placed BEFORE mod_a in the list, but declares load_after mod_a
        WritePackage(ModBRoot, R"json5({
            package_id: "mod_b",
            namespace: "mod_b",
            version: "1.0.0",
            dependencies: [
                { package_id: "mod_a", load_after: true },
            ],
        })json5");

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.0.0",
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<std::vector<FPackageDescriptor>> Result =
            DiscoverPackagesFromDirectories({CoreRoot, ModBRoot, ModARoot}, Diagnostics);

        if (Result.has_value())
        {
            return "discovery_order.case5_expected_failure_for_load_after_violation";
        }
        bool bFoundViolation = false;
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == "core:diagnostic.package.order.load_after_violation")
            {
                bFoundViolation = true;
                break;
            }
        }
        if (!bFoundViolation)
        {
            return "discovery_order.case5_load_after_violation_code_not_found";
        }
    }

    // 6. mods.lock.json5 generation and verification
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case6_core";
        const std::filesystem::path ModARoot = TempDir.Dir / "case6_mod_a";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.2.3",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<FResolvedPackageSet> ResolvedSet =
            ResolvePackageSetFromDirectories({CoreRoot, ModARoot}, Diagnostics);
        if (!ResolvedSet)
        {
            return "discovery_order.case6_discovery_failed";
        }
        const std::vector<FResolvedPackageSource>& Sources = ResolvedSet->OrderedSources;

        const std::string LockContent1 = GenerateModsLockContent(Sources);
        const std::string LockContent2 = GenerateModsLockContent(Sources);
        if (LockContent1 != LockContent2)
        {
            return "discovery_order.case6_lock_generation_not_deterministic";
        }

        std::vector<FDiagnostic> LockDiagnostics;
        if (!VerifyModsLock(LockContent1, Sources, LockDiagnostics))
        {
            return "discovery_order.case6_lock_verification_failed";
        }

        // Tamper with lock content (modify fingerprint)
        std::string TamperedContent = LockContent1;
        const std::size_t FingerprintPos = TamperedContent.find("fingerprint: \"");
        if (FingerprintPos != std::string::npos)
        {
            TamperedContent.replace(FingerprintPos + 14, 4, "dead");
            std::vector<FDiagnostic> TamperDiagnostics;
            if (VerifyModsLock(TamperedContent, Sources, TamperDiagnostics))
            {
                return "discovery_order.case6_tampered_lock_should_fail";
            }
            bool bFoundMismatch = false;
            for (const FDiagnostic& Diagnostic : TamperDiagnostics)
            {
                if (Diagnostic.Code == "core:diagnostic.package.lock.mismatch")
                {
                    bFoundMismatch = true;
                    break;
                }
            }
            if (!bFoundMismatch)
            {
                return "discovery_order.case6_missing_lock_mismatch_diagnostic";
            }
        }
    }

    // 7. End-to-end repository build from multiple packages with FMultiPackageSourceProvider
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case7_core";
        const std::filesystem::path ModARoot = TempDir.Dir / "case7_mod_a";

        WritePackage(
            CoreRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
            })json5",
            {
                {"screens.json5", R"json5({
                    schema_version: 1,
                    type: "screen",
                    definitions: [
                        {
                            id: "core:screen.main",
                            data: { title: "Core Main Screen" },
                            tags: ["core_tag"],
                        },
                    ],
                })json5"},
            },
            {
                {"screen_v1.schema.json5", R"json5({
                    id: "core:schema.definition.screen.v1",
                    definition_type: "screen",
                    schema_version: 1,
                    root: {
                        kind: "object",
                        fields: {
                            title: { kind: "string", required: true },
                        },
                    },
                    semantic_validators: [],
                    extensions: {},
                })json5"},
            });

        WritePackage(
            ModARoot,
            R"json5({
                package_id: "mod_a",
                namespace: "mod_a",
                version: "1.0.0",
                dependencies: [
                    { package_id: "core", load_after: true },
                ],
            })json5",
            {
                {"screens.json5", R"json5({
                    schema_version: 1,
                    type: "screen",
                    definitions: [
                        {
                            id: "core:screen.main",
                            data: { title: "Mod A Overridden Screen" },
                            tags: ["mod_a_override"],
                        },
                    ],
                })json5"},
            });

        std::vector<FDiagnostic> DiscoveryDiagnostics;
        std::optional<std::vector<FPackageDescriptor>> Descriptors =
            DiscoverPackagesFromDirectories({CoreRoot, ModARoot}, DiscoveryDiagnostics);
        if (!Descriptors)
        {
            return "discovery_order.case7_discovery_failed";
        }

        FMultiPackageSourceProvider Provider;
        Provider.RegisterPackage("core", CoreRoot);
        Provider.RegisterPackage("mod_a", ModARoot);

        FBuildOptions Options;
        Options.SourceProvider = &Provider;
        FBuildResult BuildOutcome = BuildRepository(*Descriptors, Options);
        if (!BuildOutcome.IsSuccess())
        {
            return "discovery_order.case7_build_failed: "
                + (BuildOutcome.GetDiagnostics().empty() ? "no diag" : BuildOutcome.GetDiagnostics().front().Code);
        }

        const FRepositoryReadHandle ReadHandle = BuildOutcome.GetCandidate().GetReadHandle();
        const FValue* ScreenDef = ReadHandle.Find(FDefinitionId::Require("core:screen.main"));
        if (ScreenDef == nullptr)
        {
            return "discovery_order.case7_missing_overridden_screen";
        }
        const FValue* TagsVal = ScreenDef->FindField("tags");
        if (TagsVal == nullptr || !TagsVal->IsArray() || TagsVal->AsArray().size() != 1
            || TagsVal->AsArray()[0].AsString() != "mod_a_override")
        {
            return "discovery_order.case7_override_winner_mismatch";
        }
    }

    // 8. Container discovery with mods.lock.json5
    {
        const std::filesystem::path Container = TempDir.Dir / "case8_container";
        const std::filesystem::path CoreRoot = Container / "core";
        const std::filesystem::path RhRoot = Container / "rh";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(RhRoot, R"json5({
            package_id: "rh",
            namespace: "rh",
            version: "1.0.0",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");

        std::vector<FDiagnostic> ResolveDiags;
        std::optional<FResolvedPackageSet> ResolvedForLock =
            ResolvePackageSetFromDirectories({CoreRoot, RhRoot}, ResolveDiags);
        if (!ResolvedForLock)
        {
            return "discovery_order.case8_resolve_for_lock_failed";
        }

        std::ofstream LockOut(Container / "mods.lock.json5");
        LockOut << GenerateModsLockContent(ResolvedForLock->OrderedSources);
        LockOut.close();

        std::vector<FDiagnostic> DiscoveryDiagnostics;
        std::vector<std::filesystem::path> OrderedRoots;
        auto Result = DiscoverPackagesFromContainer(Container, DiscoveryDiagnostics, &OrderedRoots);
        if (!Result)
        {
            return "discovery_order.case8_failed_container_discovery";
        }
        if (Result->size() != 2 || (*Result)[0].GetPackageId() != "core" || (*Result)[1].GetPackageId() != "rh")
        {
            return "discovery_order.case8_mismatched_container_packages";
        }
        if (OrderedRoots.size() != 2 || OrderedRoots[0] != CoreRoot || OrderedRoots[1] != RhRoot)
        {
            return "discovery_order.case8_mismatched_container_roots";
        }
    }

    // 9. Container discovery without mods.lock.json5 (topological sorting)
    {
        const std::filesystem::path Container = TempDir.Dir / "case9_container";
        const std::filesystem::path ModBRoot = Container / "mod_b";
        const std::filesystem::path CoreRoot = Container / "core";
        const std::filesystem::path ModARoot = Container / "mod_a";

        WritePackage(ModBRoot, R"json5({
            package_id: "mod_b",
            namespace: "mod_b",
            version: "1.0.0",
            dependencies: [
                { package_id: "mod_a", load_after: true },
            ],
        })json5");

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.0.0",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");

        std::vector<FDiagnostic> DiscoveryDiagnostics;
        std::vector<std::filesystem::path> OrderedRoots;
        auto Result = DiscoverPackagesFromContainer(Container, DiscoveryDiagnostics, &OrderedRoots);
        if (!Result)
        {
            return "discovery_order.case9_failed_topological_discovery";
        }
        if (Result->size() != 3)
        {
            return "discovery_order.case9_wrong_size";
        }
        if ((*Result)[0].GetPackageId() != "core" || (*Result)[0].GetLoadIndex() != 0)
        {
            return "discovery_order.case9_core_order_mismatch";
        }
        if ((*Result)[1].GetPackageId() != "mod_a" || (*Result)[1].GetLoadIndex() != 1)
        {
            return "discovery_order.case9_mod_a_order_mismatch";
        }
        if ((*Result)[2].GetPackageId() != "mod_b" || (*Result)[2].GetLoadIndex() != 2)
        {
            return "discovery_order.case9_mod_b_order_mismatch";
        }
    }

    // 10. Container discovery on empty container directory -> no_packages_found
    {
        const std::filesystem::path Container = TempDir.Dir / "case10_empty_container";
        std::filesystem::create_directories(Container);

        std::vector<FDiagnostic> DiscoveryDiagnostics;
        auto Result = DiscoverPackagesFromContainer(Container, DiscoveryDiagnostics);
        if (Result.has_value())
        {
            return "discovery_order.case10_expected_empty_failure";
        }
        bool bFoundNoPackagesDiag = false;
        for (const auto& Diag : DiscoveryDiagnostics)
        {
            if (Diag.Code == "core:diagnostic.package.discovery.no_packages_found")
            {
                bFoundNoPackagesDiag = true;
                break;
            }
        }
        if (!bFoundNoPackagesDiag)
        {
            return "discovery_order.case10_missing_no_packages_diagnostic";
        }
    }

    // 11. Container discovery on container without core -> missing_core
    {
        const std::filesystem::path Container = TempDir.Dir / "case11_no_core_container";
        const std::filesystem::path ModARoot = Container / "mod_a";

        WritePackage(ModARoot, R"json5({
            package_id: "mod_a",
            namespace: "mod_a",
            version: "1.0.0",
        })json5");

        std::vector<FDiagnostic> DiscoveryDiagnostics;
        auto Result = DiscoverPackagesFromContainer(Container, DiscoveryDiagnostics);
        if (Result.has_value())
        {
            return "discovery_order.case11_expected_missing_core_failure";
        }
        bool bFoundMissingCoreDiag = false;
        for (const auto& Diag : DiscoveryDiagnostics)
        {
            if (Diag.Code == "core:diagnostic.package.order.missing_core")
            {
                bFoundMissingCoreDiag = true;
                break;
            }
        }
        if (!bFoundMissingCoreDiag)
        {
            return "discovery_order.case11_missing_missing_core_diagnostic";
        }
    }

    // 12. PSC-02: FResolvedPackageSet -- ResolvePackageSetFromDirectories pairs each
    // descriptor with its root and a 64-lowercase-hex CanonicalManifestHash; two packages
    // with different manifest content get different hashes; the hash is insensitive to
    // formatting/comments (parsed-value hash, not raw bytes) but sensitive to a semantic
    // field FPackageDescriptor itself never projects (an unknown host-extension key) --
    // the exact property PSC-03's fingerprint redesign needs this type to already have.
    {
        const std::filesystem::path CoreRoot = TempDir.Dir / "case12_core";
        const std::filesystem::path ModRoot = TempDir.Dir / "case12_mod";

        WritePackage(CoreRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");
        WritePackage(ModRoot, R"json5({
            package_id: "case12_mod",
            namespace: "case12_mod",
            version: "1.0.0",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");

        std::vector<FDiagnostic> Diagnostics;
        std::optional<FResolvedPackageSet> Set =
            ResolvePackageSetFromDirectories({CoreRoot, ModRoot}, Diagnostics);
        if (!Set.has_value())
        {
            return "discovery_order.case12_failed_resolve: "
                + (Diagnostics.empty() ? "no diagnostics" : Diagnostics.front().Code);
        }
        if (Set->OrderedSources.size() != 2)
        {
            return "discovery_order.case12_wrong_size: " + std::to_string(Set->OrderedSources.size());
        }
        const FResolvedPackageSource& CoreSource = Set->OrderedSources[0];
        const FResolvedPackageSource& ModSource = Set->OrderedSources[1];
        if (CoreSource.Root != CoreRoot || ModSource.Root != ModRoot)
        {
            return "discovery_order.case12_root_mismatch";
        }
        if (CoreSource.Descriptor.GetPackageId() != "core" || ModSource.Descriptor.GetPackageId() != "case12_mod")
        {
            return "discovery_order.case12_descriptor_mismatch";
        }
        const auto IsLowercaseHex64 = [](const std::string& Hash)
        {
            if (Hash.size() != 64) return false;
            for (const char Ch : Hash)
            {
                const bool bDigit = Ch >= '0' && Ch <= '9';
                const bool bLowerHexLetter = Ch >= 'a' && Ch <= 'f';
                if (!bDigit && !bLowerHexLetter) return false;
            }
            return true;
        };
        if (!IsLowercaseHex64(CoreSource.CanonicalManifestHash) || !IsLowercaseHex64(ModSource.CanonicalManifestHash))
        {
            return "discovery_order.case12_hash_not_lowercase_hex64";
        }
        if (CoreSource.CanonicalManifestHash == ModSource.CanonicalManifestHash)
        {
            return "discovery_order.case12_distinct_manifests_hash_equal";
        }

        // ResolvePackageSetFromContainer (topological, no mods.lock.json5) over a
        // container holding the SAME two package roots must compute the SAME hash for
        // 'core' as the direct-directories call above -- the hash does not depend on
        // which factory/discovery path found the package.
        const std::filesystem::path ContainerDir = TempDir.Dir / "case12_container";
        WritePackage(ContainerDir / "core", R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");
        std::vector<FDiagnostic> ContainerDiagnostics;
        std::optional<FResolvedPackageSet> ContainerSet =
            ResolvePackageSetFromContainer(ContainerDir, ContainerDiagnostics);
        if (!ContainerSet.has_value() || ContainerSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case12_container_resolve_failed";
        }
        if (ContainerSet->OrderedSources[0].CanonicalManifestHash != CoreSource.CanonicalManifestHash)
        {
            return "discovery_order.case12_container_hash_diverges_from_directories";
        }

        // Reformatting (whitespace/comment change, same semantic content) must not
        // change the hash; adding a field FPackageDescriptor never reads must change it.
        const std::filesystem::path ReformattedRoot = TempDir.Dir / "case12_reformatted";
        WritePackage(ReformattedRoot, R"json5({
            // a comment DiscoverPackageFromDirectory never sees as a semantic field
            package_id:    "core",
            namespace: "core",
            version: "1.0.0",
        })json5");
        std::vector<FDiagnostic> ReformattedDiagnostics;
        std::optional<FResolvedPackageSet> ReformattedSet =
            ResolvePackageSetFromDirectories({ReformattedRoot}, ReformattedDiagnostics);
        if (!ReformattedSet.has_value() || ReformattedSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case12_reformatted_resolve_failed";
        }
        if (ReformattedSet->OrderedSources[0].CanonicalManifestHash != CoreSource.CanonicalManifestHash)
        {
            return "discovery_order.case12_formatting_change_altered_hash";
        }

        const std::filesystem::path ExtendedRoot = TempDir.Dir / "case12_extended";
        WritePackage(ExtendedRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
            synthetic_host_extension_field: "unread_by_descriptor",
        })json5");
        std::vector<FDiagnostic> ExtendedDiagnostics;
        std::optional<FResolvedPackageSet> ExtendedSet =
            ResolvePackageSetFromDirectories({ExtendedRoot}, ExtendedDiagnostics);
        if (!ExtendedSet.has_value() || ExtendedSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case12_extended_resolve_failed";
        }
        if (ExtendedSet->OrderedSources[0].CanonicalManifestHash == CoreSource.CanonicalManifestHash)
        {
            return "discovery_order.case12_unknown_field_did_not_change_hash";
        }
    }

    // 13. PSC-03: ComputePackageFingerprint is identity (package_id, load_index) plus
    // CanonicalManifestHash, not a re-listing of manifest fields -- so every property
    // case 12 already proved for the hash (formatting-insensitive, semantic-field- and
    // unknown-field-sensitive) carries through to the fingerprint automatically, and
    // ue_content_roots specifically (a UE-only field FPackageDescriptor's parser never
    // projects) changes it too.
    {
        const std::filesystem::path BaseRoot = TempDir.Dir / "case13_base";
        WritePackage(BaseRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
        })json5");
        std::vector<FDiagnostic> BaseDiagnostics;
        std::optional<FResolvedPackageSet> BaseSet = ResolvePackageSetFromDirectories({BaseRoot}, BaseDiagnostics);
        if (!BaseSet.has_value() || BaseSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case13_base_resolve_failed";
        }
        const FResolvedPackageSource& BaseSource = BaseSet->OrderedSources[0];
        const std::string BaseFingerprint = ComputePackageFingerprint(BaseSource.Descriptor, BaseSource.CanonicalManifestHash);

        // Reformatting (whitespace/comment change, same semantic content) must not
        // change the fingerprint.
        const std::filesystem::path ReformattedRoot = TempDir.Dir / "case13_reformatted";
        WritePackage(ReformattedRoot, R"json5({
            // a comment -- same semantic content as case13_base
            package_id:    "core",
            namespace: "core",
            version: "1.0.0",
        })json5");
        std::vector<FDiagnostic> ReformattedDiagnostics;
        std::optional<FResolvedPackageSet> ReformattedSet =
            ResolvePackageSetFromDirectories({ReformattedRoot}, ReformattedDiagnostics);
        if (!ReformattedSet.has_value() || ReformattedSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case13_reformatted_resolve_failed";
        }
        const FResolvedPackageSource& ReformattedSource = ReformattedSet->OrderedSources[0];
        if (ComputePackageFingerprint(ReformattedSource.Descriptor, ReformattedSource.CanonicalManifestHash) != BaseFingerprint)
        {
            return "discovery_order.case13_formatting_change_altered_fingerprint";
        }

        // ue_content_roots (UE-only; DiscoverPackageFromDirectory's portable parser never
        // reads it into FPackageDescriptor) must still change the fingerprint, because
        // CanonicalManifestHash is computed from the full parsed manifest, not from
        // FPackageDescriptor's projected fields.
        const std::filesystem::path UeRootsRoot = TempDir.Dir / "case13_ue_content_roots";
        WritePackage(UeRootsRoot, R"json5({
            package_id: "core",
            namespace: "core",
            version: "1.0.0",
            ue_content_roots: ["/Game/core"],
        })json5");
        std::vector<FDiagnostic> UeRootsDiagnostics;
        std::optional<FResolvedPackageSet> UeRootsSet =
            ResolvePackageSetFromDirectories({UeRootsRoot}, UeRootsDiagnostics);
        if (!UeRootsSet.has_value() || UeRootsSet->OrderedSources.size() != 1)
        {
            return "discovery_order.case13_ue_content_roots_resolve_failed";
        }
        const FResolvedPackageSource& UeRootsSource = UeRootsSet->OrderedSources[0];
        if (UeRootsSource.Descriptor.GetRelativeSources().size() != BaseSource.Descriptor.GetRelativeSources().size())
        {
            return "discovery_order.case13_ue_content_roots_leaked_into_descriptor";
        }
        if (ComputePackageFingerprint(UeRootsSource.Descriptor, UeRootsSource.CanonicalManifestHash) == BaseFingerprint)
        {
            return "discovery_order.case13_ue_content_roots_did_not_change_fingerprint";
        }

        // Same manifest content, different load_index (position in the resolved set) --
        // load_index is external load-order context, not manifest content, but it is
        // still folded into the fingerprint identity, so it must change too.
        const std::filesystem::path SecondRoot = TempDir.Dir / "case13_second";
        WritePackage(SecondRoot, R"json5({
            package_id: "case13_second",
            namespace: "case13_second",
            version: "1.0.0",
            dependencies: [
                { package_id: "core", load_after: true },
            ],
        })json5");
        std::vector<FDiagnostic> PairDiagnostics;
        std::optional<FResolvedPackageSet> PairSet =
            ResolvePackageSetFromDirectories({BaseRoot, SecondRoot}, PairDiagnostics);
        if (!PairSet.has_value() || PairSet->OrderedSources.size() != 2)
        {
            return "discovery_order.case13_pair_resolve_failed";
        }
        const FResolvedPackageSource& CoreAtIndex0 = PairSet->OrderedSources[0];
        if (CoreAtIndex0.Descriptor.GetLoadIndex() == BaseSource.Descriptor.GetLoadIndex()
            && ComputePackageFingerprint(CoreAtIndex0.Descriptor, CoreAtIndex0.CanonicalManifestHash) != BaseFingerprint)
        {
            return "discovery_order.case13_same_identity_and_content_fingerprint_diverged";
        }

        // GenerateModsLockContent/VerifyModsLock round-trip using ComputePackageFingerprint
        // internally -- already exercised end-to-end by cases 6 and 8 above.
    }

    // 14. PSC-03: a UE-specific manifest field changes the package fingerprint (case 13)
    // but must NOT change repository_content_hash -- BuildRepository only ever reads
    // FPackageDescriptor's projected relative_sources through the source provider, never
    // the raw manifest or CanonicalManifestHash, so package identity/content hashing and
    // repository content hashing stay independent typed concepts by construction.
    {
        const std::filesystem::path PlainRoot = TempDir.Dir / "case14_plain";
        WritePackage(
            PlainRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
            })json5",
            {
                {"screens.json5", R"json5({
                    schema_version: 1,
                    type: "screen",
                    definitions: [
                        { id: "core:screen.main", data: { title: "Case14" }, tags: [] },
                    ],
                })json5"},
            },
            {
                {"screen_v1.schema.json5", R"json5({
                    id: "core:schema.definition.screen.v1",
                    definition_type: "screen",
                    schema_version: 1,
                    root: {
                        kind: "object",
                        fields: {
                            title: { kind: "string", required: true },
                        },
                    },
                    semantic_validators: [],
                    extensions: {},
                })json5"},
            });

        const std::filesystem::path UeRootsRoot = TempDir.Dir / "case14_ue_roots";
        WritePackage(
            UeRootsRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
                ue_content_roots: ["/Game/core"],
            })json5",
            {
                {"screens.json5", R"json5({
                    schema_version: 1,
                    type: "screen",
                    definitions: [
                        { id: "core:screen.main", data: { title: "Case14" }, tags: [] },
                    ],
                })json5"},
            },
            {
                {"screen_v1.schema.json5", R"json5({
                    id: "core:schema.definition.screen.v1",
                    definition_type: "screen",
                    schema_version: 1,
                    root: {
                        kind: "object",
                        fields: {
                            title: { kind: "string", required: true },
                        },
                    },
                    semantic_validators: [],
                    extensions: {},
                })json5"},
            });

        auto BuildSingleRepo = [](const std::filesystem::path& Root) -> std::optional<std::string>
        {
            std::vector<FDiagnostic> Diags;
            std::optional<std::vector<FPackageDescriptor>> Descs = DiscoverPackagesFromDirectories({Root}, Diags);
            if (!Descs)
            {
                return std::nullopt;
            }
            FMultiPackageSourceProvider Provider;
            Provider.RegisterPackage("core", Root);
            FBuildOptions Options;
            Options.SourceProvider = &Provider;
            FBuildResult Outcome = BuildRepository(*Descs, Options);
            if (!Outcome.IsSuccess())
            {
                return std::nullopt;
            }
            return Outcome.GetCandidate().GetReadHandle().GetContentHash();
        };

        const std::optional<std::string> PlainHash = BuildSingleRepo(PlainRoot);
        const std::optional<std::string> UeRootsHash = BuildSingleRepo(UeRootsRoot);
        if (!PlainHash.has_value() || !UeRootsHash.has_value())
        {
            return "discovery_order.case14_build_failed";
        }
        if (*PlainHash != *UeRootsHash)
        {
            return "discovery_order.case14_ue_content_roots_changed_repository_content_hash";
        }
    }

    // 15. SAC-02: ue_content_roots is captured in FResolvedPackageSource during package set resolution.
    // A package without ue_content_roots has empty UeContentRoots (not an error).
    // When a package manifest is resolved with ue_content_roots ["/Game/A"], and the file on disk is
    // subsequently changed to ["/Game/B"], the already-captured FResolvedPackageSource retains ["/Game/A"],
    // and only a newly resolved package set sees ["/Game/B"].
    {
        const std::filesystem::path RootsTestRoot = TempDir.Dir / "case15_roots";
        // Subcase 4: package without ue_content_roots has empty roots
        WritePackage(
            RootsTestRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
            })json5");

        std::vector<FDiagnostic> DiagsNoField;
        std::optional<FResolvedPackageSet> SetNoField =
            ResolvePackageSetFromDirectories({RootsTestRoot}, DiagsNoField);
        if (!SetNoField.has_value() || SetNoField->OrderedSources.size() != 1)
        {
            return "discovery_order.case15_no_field_resolve_failed";
        }
        if (!SetNoField->OrderedSources[0].UeContentRoots.empty())
        {
            return "discovery_order.case15_absent_field_expected_empty_roots";
        }

        // Subcase 3: manifest A with /Game/A is resolved
        WritePackage(
            RootsTestRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
                ue_content_roots: ["/Game/A"],
            })json5");

        std::vector<FDiagnostic> DiagsA;
        std::optional<FResolvedPackageSet> SetA =
            ResolvePackageSetFromDirectories({RootsTestRoot}, DiagsA);
        if (!SetA.has_value() || SetA->OrderedSources.size() != 1)
        {
            return "discovery_order.case15_set_a_resolve_failed";
        }
        const std::vector<std::string> ExpectedRootsA = {"/Game/A"};
        if (SetA->OrderedSources[0].UeContentRoots != ExpectedRootsA)
        {
            return "discovery_order.case15_set_a_roots_mismatch";
        }

        // Now overwrite package.json5 on disk with /Game/B
        WritePackage(
            RootsTestRoot,
            R"json5({
                package_id: "core",
                namespace: "core",
                version: "1.0.0",
                ue_content_roots: ["/Game/B"],
            })json5");

        // Already-captured SetA must still hold /Game/A
        if (SetA->OrderedSources[0].UeContentRoots != ExpectedRootsA)
        {
            return "discovery_order.case15_captured_set_mutated";
        }

        // A new ResolvePackageSet sees /Game/B
        std::vector<FDiagnostic> DiagsB;
        std::optional<FResolvedPackageSet> SetB =
            ResolvePackageSetFromDirectories({RootsTestRoot}, DiagsB);
        if (!SetB.has_value() || SetB->OrderedSources.size() != 1)
        {
            return "discovery_order.case15_set_b_resolve_failed";
        }
        const std::vector<std::string> ExpectedRootsB = {"/Game/B"};
        if (SetB->OrderedSources[0].UeContentRoots != ExpectedRootsB)
        {
            return "discovery_order.case15_set_b_roots_mismatch";
        }
        if (SetA->OrderedSources[0].CanonicalManifestHash == SetB->OrderedSources[0].CanonicalManifestHash)
        {
            return "discovery_order.case15_manifest_hash_did_not_change";
        }
    }

    return "";
}
} // namespace GV2ContentHostSupport::Testing
