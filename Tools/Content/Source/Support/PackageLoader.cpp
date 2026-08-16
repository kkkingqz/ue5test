#include "Support/PackageLoader.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace GV2ContentCli
{

FFilesystemContentSourceProvider::FFilesystemContentSourceProvider(
    std::filesystem::path InPackageRoot,
    std::string InPackageId)
    : PackageRoot(std::move(InPackageRoot))
    , PackageId(std::move(InPackageId))
{
}

std::optional<std::string> FFilesystemContentSourceProvider::ReadSource(
    std::string_view RequestedPackageId,
    std::string_view RelativeSource) const
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

FRootBuildOutcome BuildFromPackageRoots(const std::vector<std::filesystem::path>& RawRoots)
{
    FRootBuildOutcome Outcome;
    if (RawRoots.empty())
    {
        Outcome.bToolFailure = true;
        Outcome.ToolFailureMessage = "no package roots specified";
        return Outcome;
    }

    std::vector<std::filesystem::path> Roots;
    Roots.reserve(RawRoots.size());
    for (const auto& RawRoot : RawRoots)
    {
        std::error_code Ec;
        const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
        const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

        if (!std::filesystem::is_directory(Root, Ec) || Ec)
        {
            Outcome.bToolFailure = true;
            Outcome.ToolFailureMessage = "package root not found or not a directory: " + Root.string();
            return Outcome;
        }
        Roots.push_back(Root);
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> DiscoveredDescriptors =
        GV2ContentHostSupport::DiscoverPackagesFromDirectories(Roots, Diagnostics);
    if (!DiscoveredDescriptors)
    {
        Outcome.Result.emplace(GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics)));
        return Outcome;
    }

    Outcome.Descriptors = std::move(*DiscoveredDescriptors);
    if (!Outcome.Descriptors.empty())
    {
        Outcome.Descriptor = std::make_unique<GV2ContentCore::FPackageDescriptor>(Outcome.Descriptors.front());
    }

    auto MultiProvider = std::make_unique<GV2ContentHostSupport::FMultiPackageSourceProvider>();
    for (std::size_t Index = 0; Index < Outcome.Descriptors.size(); ++Index)
    {
        MultiProvider->RegisterPackage(Outcome.Descriptors[Index].GetPackageId(), Roots[Index]);
    }

    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = MultiProvider.get();
    Outcome.Provider = std::move(MultiProvider);

    Outcome.Result.emplace(GV2ContentCore::BuildRepository(Outcome.Descriptors, Options));
    return Outcome;
}

FRootBuildOutcome BuildFromPackageRoot(const std::filesystem::path& RawRoot)
{
    return BuildFromPackageRoots({RawRoot});
}

} // namespace GV2ContentCli
