#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// GV2ContentHostSupport is a host-side utility library, deliberately kept
// outside GV2ContentCore: ADR-0018 requires GV2ContentCore's public API and
// shared sources to have no filesystem ownership ("Host отвечает за
// discovery и чтение source bytes; Content Core получает resolved values").
// This module performs that host-side discovery once, shared by the CLI
// (Tools/Content), gv2-headless and the UE host adapter, instead of each
// host duplicating (and risking drift in) the same directory-scan logic.
// See ADR-0019 for the dependency-direction decision.
namespace GV2ContentHostSupport
{
// PKG-02 (plan PackageSupport, M1 Package Manifest): this build's own
// compatibility versions, checked against each package manifest's
// optional "compatibility.{game,api,schema}" ranges. Bumped manually
// whenever a corresponding breaking change ships; a package that declares
// no range for an axis is compatible with any build on that axis (core
// never declares ranges, by design — the engine is always compatible with
// itself).
inline constexpr std::int64_t CurrentGameVersion = 1;
inline constexpr std::int64_t CurrentApiVersion = 1;
inline constexpr std::int64_t CurrentSchemaVersion = 1;

/**
 * Discovers a single package descriptor from a package root directory.
 *
 * PKG-01 (plan PackageSupport): `<root>/package.json5` is mandatory and is
 * the sole source of package_id/namespace/version identity — a package
 * root with no manifest, an unparseable one, or one missing/misdeclaring
 * package_id/namespace/version is rejected before any of the following is
 * read. PKG-02: an incompatible `compatibility` range on the manifest is
 * rejected next, still before any of the following is read. Once the
 * manifest is fully valid:
 * - <root>/definitions/ for .json5 files
 * - <root>/schemas/ for .json5 files (extracting self-describing 'id', 'definition_type', and 'schema_version')
 *
 * Emits canonical diagnostics with the "core:diagnostic.package.manifest."
 * or "core:diagnostic.package.discovery." prefix.
 */
GV2_CONTENT_HOST_SUPPORT_API std::optional<GV2ContentCore::FPackageDescriptor> DiscoverPackageFromDirectory(
    const std::filesystem::path& PackageRoot,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);

/**
 * Discovers a sequence of package descriptors from an ordered list of package root directories.
 *
 * PKG-05 (plan PackageSupport, M2 Discovery and Order):
 * - Discovers each package in the configured order
 * - Assigns LoadIndex = 0, 1, 2, ...
 * - Detects duplicate package_id across roots (reporting both paths)
 * - Validates dependencies, load_after, and cycles via ValidatePackageDescriptors
 */
GV2_CONTENT_HOST_SUPPORT_API std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> DiscoverPackagesFromDirectories(
    const std::vector<std::filesystem::path>& PackageRoots,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);

/**
 * Discovered Lua script source belonging to a package.
 * PKG-14/15: Name is '@<package_id>/<relative_path>'.
 */
struct FDiscoveredScriptSource
{
    std::string Name;
    std::string Text;
};

/**
 * Discovers all .lua script files inside a package root directory (scripts/ or Scripts/ directory).
 * For core, falls back to repository Scripts/ directory if needed.
 */
GV2_CONTENT_HOST_SUPPORT_API std::vector<FDiscoveredScriptSource> DiscoverPackageScripts(
    const std::filesystem::path& PackageRoot,
    const std::string& PackageId);

/**
 * Discovers all .lua script files across an ordered sequence of packages.
 */
GV2_CONTENT_HOST_SUPPORT_API std::vector<FDiscoveredScriptSource> DiscoverPackagesScripts(
    const std::vector<std::pair<std::string, std::filesystem::path>>& Packages);

/**
 * Filesystem-backed IContentSourceProvider that routes ReadSource calls by package_id
 * to their respective package root directories. Shared by CLI, Headless and UE.
 */
class GV2_CONTENT_HOST_SUPPORT_API FMultiPackageSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FMultiPackageSourceProvider() = default;

    void RegisterPackage(std::string PackageId, std::filesystem::path PackageRoot);

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override;

private:
    std::map<std::string, std::filesystem::path, std::less<>> PackageRoots;
};
}

