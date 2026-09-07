#pragma once

#include "GV2ContentCore/RepositoryBuilder.h"

#include "Containers/UnrealString.h"

// Discovers ordered package descriptors from a list of package root directories
// and builds the repository via the shared GV2ContentCore::BuildRepository() path.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectories(const TArray<FString>& PackageRootDirs);

// Discovers a single-package FPackageDescriptor from
// <PackageRootDir>/definitions/*.json5 and <PackageRootDir>/schemas/*.json5
// and builds the repository via the shared GV2ContentCore::BuildRepository() path.
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir);
