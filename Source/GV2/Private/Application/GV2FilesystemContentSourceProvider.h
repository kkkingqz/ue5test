#pragma once

#include "GV2ContentCore/RepositoryBuilder.h"

#include "Containers/UnrealString.h"

// PCC-36: UE-filesystem-backed IContentSourceProvider for a single package
// root directory. This is the only UE-specific piece of the repository
// build path - schema/resolution logic stays entirely inside the shared
// GV2ContentCore::BuildRepository() reference path, matching
// Content/Source/main.cpp's FFilesystemContentSourceProvider and
// Headless/Source/main.cpp's FHeadlessFilesystemContentSourceProvider.
class FGV2FilesystemContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FGV2FilesystemContentSourceProvider(FString InPackageRootDir, std::string InPackageId);

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override;

private:
    FString PackageRootDir;
    std::string PackageId;
};

// Discovers a single-package FPackageDescriptor from
// <PackageRootDir>/definitions/*.json5 and <PackageRootDir>/schemas/*.json5
// (each schema resource self-declares "id"/"definition_type"/
// "schema_version") and builds the repository via the shared
// GV2ContentCore::BuildRepository() path. Does not resolve mods, dependency
// order or extension schemas - PackageRootDir is always exactly one package,
// matching the CLI/headless discovery convention (PCC-38 parity).
GV2ContentCore::FBuildResult BuildGV2RepositoryFromDirectory(const FString& PackageRootDir);
