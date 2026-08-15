#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// GV2ContentHostSupport is a host-side utility library, deliberately kept
// outside GV2ContentCore: ADR-0018 requires GV2ContentCore's public API and
// shared sources to have no filesystem ownership ("Host отвечает за
// discovery и чтение source bytes; Content Core получает resolved values").
// This module performs that host-side discovery once, shared by the CLI
// (Tools/Content), gv2-headless and the UE host adapter, instead of each
// host duplicating (and risking drift in) the same directory-scan logic.
// See ADR-0019 for the dependency-direction decision.
namespace GV2ContentHostSupport
{
/**
 * Discovers a single package descriptor from a package root directory by scanning:
 * - <root>/definitions/ for .json5 files
 * - <root>/schemas/ for .json5 files (extracting self-describing 'id', 'definition_type', and 'schema_version')
 *
 * Emits canonical diagnostics with the "core:diagnostic.package.discovery." prefix.
 */
GV2_CONTENT_HOST_SUPPORT_API std::optional<GV2ContentCore::FPackageDescriptor> DiscoverPackageFromDirectory(
    const std::filesystem::path& PackageRoot,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);
}
