#pragma once

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCli
{

// Filesystem-backed IContentSourceProvider for a single package root directory.
class FFilesystemContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FFilesystemContentSourceProvider(std::filesystem::path InPackageRoot, std::string InPackageId);

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override;

private:
    std::filesystem::path PackageRoot;
    std::string PackageId;
};

// Ordered package set behind one or more root directories, without building a repository.
// A single non-core root picks up a sibling "core" package, so a command pointed at the game
// package still sees the schemas that package depends on.
struct FPackageSetDiscovery
{
    bool bToolFailure = false;
    std::string ToolFailureMessage;
    // Set when discovery itself produced diagnostics; Descriptors is empty in that case.
    bool bDiscoveryFailed = false;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    // Parallel arrays in load order: Roots[i] is the directory Descriptors[i] was discovered in.
    std::vector<std::filesystem::path> Roots;
    std::vector<GV2ContentCore::FPackageDescriptor> Descriptors;

    // Index of the package the command was pointed at: always the last one in load order.
    std::size_t TargetIndex() const { return Descriptors.empty() ? 0 : Descriptors.size() - 1; }
};

FPackageSetDiscovery DiscoverPackageSet(const std::vector<std::filesystem::path>& RawRoots);

// Locates the schema binding for a definition type anywhere in the set, in load order.
// Returns false when no package in the set binds the type.
bool FindSchemaBindingInSet(
    const FPackageSetDiscovery& Set,
    std::string_view DefinitionType,
    std::size_t& OutPackageIndex,
    const GV2ContentCore::FSchemaBinding*& OutBinding);

// Discovers package descriptors and builds a repository from one or more package root directories.
struct FRootBuildOutcome
{
    bool bToolFailure = false;
    std::string ToolFailureMessage;
    std::unique_ptr<GV2ContentCore::FPackageDescriptor> Descriptor;
    std::vector<GV2ContentCore::FPackageDescriptor> Descriptors;
    std::optional<GV2ContentCore::FBuildResult> Result;
    // Kept alive for the lifetime of Result, which holds a raw pointer into it via FBuildOptions.
    std::unique_ptr<GV2ContentCore::IContentSourceProvider> Provider;
};

FRootBuildOutcome BuildFromPackageRoots(const std::vector<std::filesystem::path>& RawRoots);
FRootBuildOutcome BuildFromPackageRoot(const std::filesystem::path& RawRoot);

} // namespace GV2ContentCli
