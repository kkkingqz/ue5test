#include "Application/GV2PackageClosure.h"

#include "Misc/Paths.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace GV2PackageClosure
{
// PAH-04: pre_ready_discovery -- today's production callers,
// UGV2ScreenRegistry::GetPackageLoadOrderFromGameData() and (PAH-05)
// ResolveContentRootOwnershipFromGameData(), only ever run from Build(), from
// LoadScreenRegistry(), from Initialize(), before any session exists.
TArray<FEntry> DiscoverFromGameData()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = TCHAR_TO_UTF8(*GameDataDir);
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::vector<std::filesystem::path> OrderedRoots;
    const std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Descriptors =
        GV2ContentHostSupport::DiscoverPackagesFromContainer(
            std::filesystem::path(GameDataDirUtf8),
            Diagnostics,
            &OrderedRoots);
    if (!Descriptors.has_value())
    {
        return {};
    }

    TArray<FEntry> Result;
    Result.SetNum(Descriptors->size());
    for (std::size_t Index = 0; Index < Descriptors->size(); ++Index)
    {
        const GV2ContentCore::FPackageDescriptor& Descriptor = (*Descriptors)[Index];
        const int32 LoadIndex = static_cast<int32>(Descriptor.GetLoadIndex());
        if (!Result.IsValidIndex(LoadIndex))
        {
            continue;
        }
        Result[LoadIndex].PackageId = UTF8_TO_TCHAR(Descriptor.GetPackageId().c_str());
        if (Index < OrderedRoots.size())
        {
            Result[LoadIndex].RootDirectory = UTF8_TO_TCHAR(OrderedRoots[Index].string().c_str());
        }
    }
    return Result;
}

TArray<FEntry> FromResolvedPackageSet(const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet)
{
    TArray<FEntry> Result;
    Result.Reserve(static_cast<int32>(ResolvedPackageSet.OrderedSources.size()));
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedPackageSet.OrderedSources)
    {
        FEntry Entry;
        Entry.PackageId = UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str());
        Entry.RootDirectory = UTF8_TO_TCHAR(Source.Root.string().c_str());
        Result.Add(MoveTemp(Entry));
    }
    return Result;
}
}
