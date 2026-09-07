#include "Application/GV2FilesystemContentSourceProvider.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include "Misc/Paths.h"

#include <filesystem>

namespace
{
std::string ToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}
}

// PAH-04: pre_ready_discovery -- only called from Initialize() (directly, and via
// BuildGV2RepositoryFromDirectory, itself a production-unused convenience wrapper),
// before any session exists.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectories(const TArray<FString>& PackageRootDirs)
{
    std::vector<std::filesystem::path> Roots;
    Roots.reserve(PackageRootDirs.Num());
    for (const FString& Dir : PackageRootDirs)
    {
        FString Normalized = Dir;
        FPaths::NormalizeDirectoryName(Normalized);
        Roots.emplace_back(ToUtf8(Normalized));
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Descriptors =
        GV2ContentHostSupport::DiscoverPackagesFromDirectories(Roots, Diagnostics);
    if (!Descriptors)
    {
        return GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics));
    }

    GV2ContentHostSupport::FMultiPackageSourceProvider Provider;
    for (std::size_t Index = 0; Index < Descriptors->size(); ++Index)
    {
        Provider.RegisterPackage((*Descriptors)[Index].GetPackageId(), Roots[Index]);
    }

    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;

    return GV2ContentCore::BuildRepository(*Descriptors, Options);
}

GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir)
{
    return BuildGV2RepositoryFromDirectories({PackageRootDir});
}
