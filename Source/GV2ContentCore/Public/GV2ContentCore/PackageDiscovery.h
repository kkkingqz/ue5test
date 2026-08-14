#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"

#include <filesystem>
#include <optional>
#include <vector>
#include <string>

namespace GV2ContentCore
{
/**
 * Discovers a single package descriptor from a package root directory by scanning:
 * - <root>/definitions/*.json5
 * - <root>/schemas/*.json5 (extracting self-describing 'id', 'definition_type', and 'schema_version')
 *
 * Emits canonical diagnostics with prefix 'core:diagnostic.package.discovery.*'.
 */
GV2_CONTENT_CORE_API std::optional<FPackageDescriptor> DiscoverPackageFromDirectory(
    const std::filesystem::path& PackageRoot,
    std::vector<FDiagnostic>& OutDiagnostics);
}
