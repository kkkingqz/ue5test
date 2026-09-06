#pragma once

#include "CoreMinimal.h"

// DCA-19: the single place that reads GameData/mods.lock.json5 (via
// GV2ContentHostSupport's package discovery, the same mechanism
// UGV2RuntimeSubsystem uses to resolve its repository roots and
// UGV2ScreenRegistry::GetPackageLoadOrderFromGameData uses for layer
// ordering, DCA-18) to answer "what packages does this session's pinned
// closure actually contain, and where do they live on disk". A package
// physically present under GameData/ but absent from mods.lock.json5 (e.g.
// a leftover "sample" directory) is never returned -- the closure, not the
// filesystem, decides what a session sees.
namespace GV2PackageClosure
{
struct FEntry
{
    FString PackageId;
    FString RootDirectory;
};

// Ordered by load_index. Empty on discovery failure (missing/invalid
// mods.lock.json5, a listed package not found on disk, etc.) -- callers
// treat an empty result as "nothing to scan", never as "scan everything".
TArray<FEntry> DiscoverFromGameData();
}
