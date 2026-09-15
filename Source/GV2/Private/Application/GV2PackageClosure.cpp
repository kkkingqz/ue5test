#include "Application/GV2PackageClosure.h"

#include "Misc/Paths.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace GV2PackageClosure
{
#if WITH_DEV_AUTOMATION_TESTS
// Test-only projection fixture. Production receives FResolvedPackageSet from its host.
// PAH-04: pre_ready_discovery callers=none
// Automation tests use this projection helper only while arranging content fixtures
// before starting a session; it has no production callers and is absent from non-automation builds.
TArray<FEntry> DiscoverFromGameData()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = TCHAR_TO_UTF8(*GameDataDir);
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
        GV2ContentHostSupport::ResolvePackageSetFromContainer(
            std::filesystem::path(GameDataDirUtf8),
            Diagnostics);
    if (!ResolvedSet.has_value())
    {
        return {};
    }
    return FromResolvedPackageSet(*ResolvedSet);
}
#endif

TArray<FEntry> FromResolvedPackageSet(const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet)
{
    TArray<FEntry> Result;
    Result.Reserve(static_cast<int32>(ResolvedPackageSet.OrderedSources.size()));
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedPackageSet.OrderedSources)
    {
        FEntry Entry;
        Entry.PackageId = UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str());
        Entry.RootDirectory = UTF8_TO_TCHAR(Source.Root.string().c_str());
        Entry.UeContentRoots.Reserve(static_cast<int32>(Source.UeContentRoots.size()));
        for (const std::string& Root : Source.UeContentRoots)
        {
            Entry.UeContentRoots.Add(UTF8_TO_TCHAR(Root.c_str()));
        }
        Result.Add(MoveTemp(Entry));
    }
    return Result;
}
}
