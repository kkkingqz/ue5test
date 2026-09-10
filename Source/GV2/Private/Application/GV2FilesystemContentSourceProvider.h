#pragma once

#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include "Containers/UnrealString.h"

#if WITH_DEV_AUTOMATION_TESTS
// Test-only conveniences. Production callers receive an already resolved package set;
// these declarations do not exist in non-test builds.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectories(const TArray<FString>& PackageRootDirs);
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir);
#endif

// PSC-02 (ADR-0043 D1/D5): builds the repository directly from an already-resolved
// FResolvedPackageSet's descriptors -- no discovery of its own. The one production path
// (UGV2RuntimeSubsystem::Initialize) resolves the package set exactly once and feeds it
// here, to the Screen Registry, and to FGV2SessionCoordinator::StartSession alike,
// instead of each independently re-discovering the same package.json5 files.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromResolvedPackageSet(
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet);
