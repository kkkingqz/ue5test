#pragma once

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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

// Discovers a single-package FPackageDescriptor from a directory laid out as
// <root>/definitions/*.json5 and <root>/schemas/*.json5.
struct FRootBuildOutcome
{
    bool bToolFailure = false;
    std::string ToolFailureMessage;
    std::unique_ptr<GV2ContentCore::FPackageDescriptor> Descriptor;
    std::optional<GV2ContentCore::FBuildResult> Result;
    // Kept alive for the lifetime of Result, which holds a raw pointer into it via FBuildOptions.
    std::unique_ptr<FFilesystemContentSourceProvider> Provider;
};

FRootBuildOutcome BuildFromPackageRoot(const std::filesystem::path& RawRoot);

} // namespace GV2ContentCli
