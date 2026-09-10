#pragma once

#include "CoreMinimal.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

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
// PSC-02: retained as a general-purpose convenience (e.g. a test building its own,
// genuinely independent oracle) -- the production registry-build path no longer calls
// this itself (see FromResolvedPackageSet below).
#if WITH_DEV_AUTOMATION_TESTS
TArray<FEntry> DiscoverFromGameData();
#endif

// PSC-02 (ADR-0043 D1/D5): pure projection of an already-resolved package set into this
// UE-friendly (PackageId, RootDirectory) shape -- no discovery of its own. This is what
// the production path (UGV2RuntimeSubsystem::Initialize, resolving its package set once)
// feeds UGV2ScreenRegistry::Build() with, instead of a second independent discovery.
TArray<FEntry> FromResolvedPackageSet(const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet);
}
