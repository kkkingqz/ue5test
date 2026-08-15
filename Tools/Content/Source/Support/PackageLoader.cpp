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

FRootBuildOutcome BuildFromPackageRoot(const std::filesystem::path& RawRoot)
{
    FRootBuildOutcome Outcome;

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        Outcome.bToolFailure = true;
        Outcome.ToolFailureMessage = "package root not found or not a directory";
        return Outcome;
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> DiscoveredDescriptor =
        GV2ContentHostSupport::DiscoverPackageFromDirectory(Root, Diagnostics);
    if (!DiscoveredDescriptor)
    {
        Outcome.Result.emplace(GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics)));
        return Outcome;
    }

    Outcome.Descriptor = std::make_unique<GV2ContentCore::FPackageDescriptor>(std::move(*DiscoveredDescriptor));
    Outcome.Provider = std::make_unique<FFilesystemContentSourceProvider>(Root, Outcome.Descriptor->GetPackageId());
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = Outcome.Provider.get();

    Outcome.Result.emplace(GV2ContentCore::BuildRepository({*Outcome.Descriptor}, Options));
    return Outcome;
}

} // namespace GV2ContentCli
