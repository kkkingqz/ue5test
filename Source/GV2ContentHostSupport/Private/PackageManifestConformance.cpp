#include "GV2ContentHostSupport/Testing/PackageManifestConformance.h"

#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <chrono>
#include <fstream>

namespace GV2ContentHostSupport::Testing
{
namespace
{
std::filesystem::path MakeUniqueTempDir()
{
    const auto Now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path Dir = std::filesystem::temp_directory_path()
        / ("gv2_package_manifest_conformance_" + std::to_string(Now));
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

void WriteManifest(const std::filesystem::path& PackageRoot, const std::string& ManifestBody)
{
    std::error_code Ec;
    std::filesystem::create_directories(PackageRoot, Ec);
    std::ofstream Stream(PackageRoot / "package.json5", std::ios::binary | std::ios::trunc);
    Stream << ManifestBody;
}

// Discovers PackageRoot. Returns "" if discovery succeeded. On failure,
// returns the first diagnostic code that matches ExpectedCode if present
// (a manifest can trigger more than one diagnostic at once; sort order
// among unrelated codes at the same RelativeSource is not semantics this
// suite pins), otherwise the first diagnostic's code.
std::string DiscoverAndGetFirstCode(const std::filesystem::path& PackageRoot, const std::string& ExpectedCode = {})
{
    using namespace GV2ContentCore;
    std::vector<FDiagnostic> Diagnostics;
    const std::optional<FPackageDescriptor> Result = DiscoverPackageFromDirectory(PackageRoot, Diagnostics);
    if (Result.has_value())
    {
        return "";
    }
    if (Diagnostics.empty())
    {
        return "core:diagnostic.<none>";
    }
    if (!ExpectedCode.empty())
    {
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == ExpectedCode)
            {
                return ExpectedCode;
            }
        }
    }
    return Diagnostics.front().Code;
}
}

std::string RunPackageManifestConformance()
{
    using namespace GV2ContentCore;
    FScopedTempDir TempDir;

    // 1. PKG-01: missing manifest entirely.
    {
        const std::filesystem::path Root = TempDir.Dir / "missing_manifest";
        std::error_code Ec;
        std::filesystem::create_directories(Root, Ec);
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.missing");
        if (Code != "core:diagnostic.package.manifest.missing")
        {
            return "package_manifest_conformance.missing_manifest_wrong_code: " + Code;
        }
    }

    // 2. PKG-01: invalid package_id grammar.
    {
        const std::filesystem::path Root = TempDir.Dir / "invalid_package_id";
        WriteManifest(Root, R"json5({
            package_id: "Not-Valid",
            namespace: "Not-Valid",
            version: "1.0.0",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.invalid_package_id");
        if (Code != "core:diagnostic.package.manifest.invalid_package_id")
        {
            return "package_manifest_conformance.invalid_package_id_wrong_code: " + Code;
        }
    }

    // 3. PKG-01: namespace must equal package_id.
    {
        const std::filesystem::path Root = TempDir.Dir / "namespace_mismatch";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "other_ns",
            version: "1.0.0",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.namespace_mismatch");
        if (Code != "core:diagnostic.package.manifest.namespace_mismatch")
        {
            return "package_manifest_conformance.namespace_mismatch_wrong_code: " + Code;
        }
    }

    // 4. PKG-01: missing version.
    {
        const std::filesystem::path Root = TempDir.Dir / "missing_version";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.invalid_version");
        if (Code != "core:diagnostic.package.manifest.invalid_version")
        {
            return "package_manifest_conformance.missing_version_wrong_code: " + Code;
        }
    }

    // 5. PKG-01: malformed version grammar.
    {
        const std::filesystem::path Root = TempDir.Dir / "malformed_version";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "not-a-version",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.invalid_version");
        if (Code != "core:diagnostic.package.manifest.invalid_version")
        {
            return "package_manifest_conformance.malformed_version_wrong_code: " + Code;
        }
    }

    // 6. PKG-02: incompatible game range (this build's CurrentGameVersion
    //    falls outside [min, max]).
    {
        const std::filesystem::path Root = TempDir.Dir / "incompatible_game";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
            compatibility: { game: { min: 999, max: 999 } },
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.incompatible_range");
        if (Code != "core:diagnostic.package.manifest.incompatible_range")
        {
            return "package_manifest_conformance.incompatible_game_wrong_code: " + Code;
        }
    }

    // 7. PKG-02: incompatible api range (separate axis, same code).
    {
        const std::filesystem::path Root = TempDir.Dir / "incompatible_api";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
            compatibility: { api: { min: 0, max: 0 } },
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.incompatible_range");
        if (Code != "core:diagnostic.package.manifest.incompatible_range")
        {
            return "package_manifest_conformance.incompatible_api_wrong_code: " + Code;
        }
    }

    // 8. PKG-02: an absent compatibility range is always compatible
    //    ("core" never declares ranges and must never be rejected).
    {
        const std::filesystem::path Root = TempDir.Dir / "no_compatibility_declared";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root);
        if (!Code.empty())
        {
            return "package_manifest_conformance.absent_compatibility_rejected: " + Code;
        }
    }

    // 9. PKG-02: a range that covers the current version is accepted.
    {
        const std::filesystem::path Root = TempDir.Dir / "compatible_range";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
            compatibility: {
                game: { min: 1, max: 1 },
                api: { min: 1, max: 1 },
                schema: { min: 1, max: 1 },
            },
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root);
        if (!Code.empty())
        {
            return "package_manifest_conformance.compatible_range_rejected: " + Code;
        }
    }

    // 10. PKG-03: a dependency entry missing package_id is rejected.
    {
        const std::filesystem::path Root = TempDir.Dir / "invalid_dependency";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
            dependencies: [ { load_after: true } ],
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.package.manifest.invalid_dependency");
        if (Code != "core:diagnostic.package.manifest.invalid_dependency")
        {
            return "package_manifest_conformance.invalid_dependency_wrong_code: " + Code;
        }
    }

    // 11. PKG-03: a well-formed dependencies list (with and without
    //     load_after) is accepted and carried through to the descriptor —
    //     PKG-03 is parse/form-validation only, not presence-in-set
    //     checking (that is M2).
    {
        const std::filesystem::path Root = TempDir.Dir / "valid_dependencies";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
            dependencies: [
                { package_id: "other_pkg" },
                { package_id: "hint_only_pkg", load_after: true },
            ],
        })json5");
        std::vector<FDiagnostic> Diagnostics;
        const std::optional<FPackageDescriptor> Result = DiscoverPackageFromDirectory(Root, Diagnostics);
        if (!Result.has_value())
        {
            const std::string Code = Diagnostics.empty() ? "<none>" : Diagnostics.front().Code;
            return "package_manifest_conformance.valid_dependencies_rejected: " + Code;
        }
        if (Result->GetDependencies().size() != 2)
        {
            return "package_manifest_conformance.valid_dependencies_count_mismatch";
        }
        if (Result->GetDependencies()[0].GetPackageId() != "other_pkg" || Result->GetDependencies()[0].GetLoadAfter())
        {
            return "package_manifest_conformance.valid_dependencies_first_entry_mismatch";
        }
        if (Result->GetDependencies()[1].GetPackageId() != "hint_only_pkg" || !Result->GetDependencies()[1].GetLoadAfter())
        {
            return "package_manifest_conformance.valid_dependencies_second_entry_mismatch";
        }
        if (Result->GetVersion() != "1.0.0")
        {
            return "package_manifest_conformance.version_not_carried_through";
        }
    }

    // 12. Duplicate key inside package.json5 itself propagates the
    //     generic JSON5 diagnostic (manifest parsing reuses ParseJson5,
    //     no separate duplicate-key detection needed).
    {
        const std::filesystem::path Root = TempDir.Dir / "duplicate_key_manifest";
        WriteManifest(Root, R"json5({
            package_id: "some_pkg",
            package_id: "some_pkg",
            namespace: "some_pkg",
            version: "1.0.0",
        })json5");
        const std::string Code = DiscoverAndGetFirstCode(Root, "core:diagnostic.json5.duplicate_key");
        if (Code != "core:diagnostic.json5.duplicate_key")
        {
            return "package_manifest_conformance.duplicate_key_wrong_code: " + Code;
        }
    }

    return "";
}
}
