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
        std::optional<std::vector<FPackageDescriptor>> Descriptors =
            DiscoverPackagesFromDirectories({CoreRoot, ModARoot}, Diagnostics);
        if (!Descriptors)
        {
            return "discovery_order.case6_discovery_failed";
        }

        const std::string LockContent1 = GenerateModsLockContent(*Descriptors);
        const std::string LockContent2 = GenerateModsLockContent(*Descriptors);
        if (LockContent1 != LockContent2)
        {
            return "discovery_order.case6_lock_generation_not_deterministic";
        }

        std::vector<FDiagnostic> LockDiagnostics;
        if (!VerifyModsLock(LockContent1, *Descriptors, LockDiagnostics))
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
            if (VerifyModsLock(TamperedContent, *Descriptors, TamperDiagnostics))
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

        std::vector<FPackageDescriptor> Descs;
        std::vector<FDiagnostic> TempDiags;
        Descs.push_back(*DiscoverPackageFromDirectory(CoreRoot, TempDiags));
        Descs.push_back((*DiscoverPackageFromDirectory(RhRoot, TempDiags)).WithLoadIndex(1));

        std::ofstream LockOut(Container / "mods.lock.json5");
        LockOut << GenerateModsLockContent(Descs);
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

    return "";
}
} // namespace GV2ContentHostSupport::Testing
