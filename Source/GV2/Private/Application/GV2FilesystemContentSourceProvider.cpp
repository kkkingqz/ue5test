#include "Application/GV2FilesystemContentSourceProvider.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/PackageDiscovery.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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

FGV2FilesystemContentSourceProvider::FGV2FilesystemContentSourceProvider(
    FString InPackageRootDir,
    std::string InPackageId)
    : PackageRootDir(MoveTemp(InPackageRootDir))
    , PackageId(std::move(InPackageId))
{
}

std::optional<std::string> FGV2FilesystemContentSourceProvider::ReadSource(
    const std::string_view RequestedPackageId,
    const std::string_view RelativeSource) const
{
    if (RequestedPackageId != PackageId)
    {
        return std::nullopt;
    }

    const FString RelativeSourceStr = UTF8_TO_TCHAR(std::string(RelativeSource).c_str());
    const FString FullPath = FPaths::Combine(PackageRootDir, RelativeSourceStr);
    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FullPath))
    {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(FileBytes.GetData()), FileBytes.Num());
}

GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir)
{
    FString Normalized = PackageRootDir;
    FPaths::NormalizeDirectoryName(Normalized);

    const std::filesystem::path PackageRootPath(ToUtf8(Normalized));
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> Descriptor =
        GV2ContentCore::DiscoverPackageFromDirectory(PackageRootPath, Diagnostics);
    if (!Descriptor)
    {
        return GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics));
    }

    FGV2FilesystemContentSourceProvider Provider(Normalized, Descriptor->GetPackageId());
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;

    return GV2ContentCore::BuildRepository({*Descriptor}, Options);
}
