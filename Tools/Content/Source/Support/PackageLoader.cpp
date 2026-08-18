#include "Support/PackageLoader.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <algorithm>
#include <iterator>
#include <set>
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

FPackageSetDiscovery DiscoverPackageSet(const std::vector<std::filesystem::path>& RawRoots)
{
    FPackageSetDiscovery Set;
    if (RawRoots.empty())
    {
        Set.bToolFailure = true;
        Set.ToolFailureMessage = "no package roots specified";
        return Set;
    }

    std::vector<std::filesystem::path> Roots;
    Roots.reserve(RawRoots.size() + 1);
    for (const auto& RawRoot : RawRoots)
    {
        std::error_code Ec;
        const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
        const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

        if (!std::filesystem::is_directory(Root, Ec) || Ec)
        {
            Set.bToolFailure = true;
            Set.ToolFailureMessage = "package root not found or not a directory: " + Root.string();
            return Set;
        }
        Roots.push_back(Root);
    }

    // Case 1: Single root that is a container directory
    if (Roots.size() == 1 && !std::filesystem::exists(Roots.front() / "package.json5"))
    {
        std::vector<std::filesystem::path> ContainerRoots;
        std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Discovered =
            GV2ContentHostSupport::DiscoverPackagesFromContainer(Roots.front(), Set.Diagnostics, &ContainerRoots);
        if (!Discovered)
        {
            Set.bDiscoveryFailed = true;
            return Set;
        }
        Set.Roots = std::move(ContainerRoots);
        Set.Descriptors = std::move(*Discovered);
        return Set;
    }

    // Case 2: Single package root (resolve sibling dependencies if present)
    if (Roots.size() == 1)
    {
        std::error_code Ec;
        const std::filesystem::path Parent = Roots.front().parent_path();
        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto TargetDesc = GV2ContentHostSupport::DiscoverPackageFromDirectory(Roots.front(), Diags);
        if (TargetDesc && TargetDesc->GetPackageId() != "core" && std::filesystem::is_directory(Parent, Ec))
        {
            std::vector<std::string> DepQueue;
            std::set<std::string> VisitedDeps;
            for (const auto& Dep : TargetDesc->GetDependencies())
            {
                if (VisitedDeps.insert(Dep.GetPackageId()).second)
                {
                    DepQueue.push_back(Dep.GetPackageId());
                }
            }

            std::size_t Head = 0;
            while (Head < DepQueue.size())
            {
                const std::string DepId = DepQueue[Head++];
                const std::filesystem::path SiblingPkgDir = Parent / DepId;
                if (std::filesystem::is_directory(SiblingPkgDir, Ec) && std::filesystem::exists(SiblingPkgDir / "package.json5", Ec))
                {
                    std::vector<GV2ContentCore::FDiagnostic> SubDiags;
                    auto SiblingDesc = GV2ContentHostSupport::DiscoverPackageFromDirectory(SiblingPkgDir, SubDiags);
                    if (SiblingDesc)
                    {
                        for (const auto& SubDep : SiblingDesc->GetDependencies())
                        {
                            if (VisitedDeps.insert(SubDep.GetPackageId()).second)
                            {
                                DepQueue.push_back(SubDep.GetPackageId());
                            }
                        }
                    }
                }
            }

            VisitedDeps.insert("core");
            std::vector<std::filesystem::path> OrderedPackageRoots;
            if (std::filesystem::exists(Parent / "core" / "package.json5", Ec))
            {
                OrderedPackageRoots.push_back(Parent / "core");
            }
            for (const auto& DepId : DepQueue)
            {
                if (DepId != "core" && DepId != TargetDesc->GetPackageId())
                {
                    const std::filesystem::path SiblingPkgDir = Parent / DepId;
                    if (std::filesystem::exists(SiblingPkgDir / "package.json5", Ec))
                    {
                        if (std::find(OrderedPackageRoots.begin(), OrderedPackageRoots.end(), SiblingPkgDir) == OrderedPackageRoots.end())
                        {
                            OrderedPackageRoots.push_back(SiblingPkgDir);
                        }
                    }
                }
            }
            OrderedPackageRoots.push_back(Roots.front());
            Roots = std::move(OrderedPackageRoots);
        }
    }

    std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> DiscoveredDescriptors =
        GV2ContentHostSupport::DiscoverPackagesFromDirectories(Roots, Set.Diagnostics);
    if (!DiscoveredDescriptors)
    {
        Set.bDiscoveryFailed = true;
        return Set;
    }

    Set.Roots = std::move(Roots);
    Set.Descriptors = std::move(*DiscoveredDescriptors);
    return Set;
}

bool FindSchemaBindingInSet(
    const FPackageSetDiscovery& Set,
    std::string_view DefinitionType,
    std::size_t& OutPackageIndex,
    const GV2ContentCore::FSchemaBinding*& OutBinding)
{
    // Load order: a package binding a type its dependency already binds is fatal at repository
    // build, so the first match is the only match.
    for (std::size_t Index = 0; Index < Set.Descriptors.size(); ++Index)
    {
        for (const GV2ContentCore::FSchemaBinding& Binding : Set.Descriptors[Index].GetSchemaBindings())
        {
            if (Binding.GetDefinitionType() == DefinitionType)
            {
                OutPackageIndex = Index;
                OutBinding = &Binding;
                return true;
            }
        }
    }
    return false;
}

FRootBuildOutcome BuildFromPackageRoots(const std::vector<std::filesystem::path>& RawRoots)
{
    FRootBuildOutcome Outcome;

    FPackageSetDiscovery Set = DiscoverPackageSet(RawRoots);
    if (Set.bToolFailure)
    {
        Outcome.bToolFailure = true;
        Outcome.ToolFailureMessage = std::move(Set.ToolFailureMessage);
        return Outcome;
    }
    if (Set.bDiscoveryFailed)
    {
        Outcome.Result.emplace(GV2ContentCore::FBuildResult::Failure(std::move(Set.Diagnostics)));
        return Outcome;
    }

    Outcome.Descriptors = std::move(Set.Descriptors);
    if (!Outcome.Descriptors.empty())
    {
        Outcome.Descriptor = std::make_unique<GV2ContentCore::FPackageDescriptor>(Outcome.Descriptors.back());
    }

    auto MultiProvider = std::make_unique<GV2ContentHostSupport::FMultiPackageSourceProvider>();
    for (std::size_t Index = 0; Index < Outcome.Descriptors.size(); ++Index)
    {
        MultiProvider->RegisterPackage(Outcome.Descriptors[Index].GetPackageId(), Set.Roots[Index]);
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
