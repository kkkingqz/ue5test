#pragma once

#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include "Containers/UnrealString.h"

// Discovers ordered package descriptors from a list of package root directories
// and builds the repository via the shared GV2ContentCore::BuildRepository() path.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectories(const TArray<FString>& PackageRootDirs);

// Discovers a single-package FPackageDescriptor from
// <PackageRootDir>/definitions/*.json5 and <PackageRootDir>/schemas/*.json5
// and builds the repository via the shared GV2ContentCore::BuildRepository() path.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir);

// PSC-02 (ADR-0043 D1/D5): builds the repository directly from an already-resolved
// FResolvedPackageSet's descriptors -- no discovery of its own. The one production path
// (UGV2RuntimeSubsystem::Initialize) resolves the package set exactly once and feeds it
// here, to the Screen Registry, and to FGV2SessionCoordinator::StartSession alike,
// instead of each independently re-discovering the same package.json5 files.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromResolvedPackageSet(
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet);
