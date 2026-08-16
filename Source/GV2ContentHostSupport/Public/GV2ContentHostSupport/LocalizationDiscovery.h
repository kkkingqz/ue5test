#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PoParser.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentHostSupport
{
/**
 * Discovers available locales within a package root by scanning <package-root>/localization/ for .po files.
 * Returns sorted list of locale codes (e.g. ["en", "ru"]).
 */
std::vector<std::string> DiscoverPackageLocales(const std::filesystem::path& PackageRoot);

/**
 * Loads and parses a PO translation catalog for a specific locale from <package-root>/localization/<locale>.po.
 * Fails atomically if the file is unreadable or malformed.
 */
std::optional<GV2ContentCore::FPoCatalog> LoadPackageLocalization(
    const std::filesystem::path& PackageRoot,
    const std::string& Locale,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);

/**
 * Compiles an FPoCatalog into a deterministic Unreal Engine String Table CSV format.
 * Format: Key,SourceString (sorted alphabetically by Key, standard RFC 4180 CSV escaping).
 */
std::string ExportPoToStringTableCsv(const GV2ContentCore::FPoCatalog& Catalog);
}
