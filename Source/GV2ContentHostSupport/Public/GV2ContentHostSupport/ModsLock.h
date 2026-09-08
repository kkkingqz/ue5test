#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"

#include <string>
#include <vector>

namespace GV2ContentHostSupport
{
// PSC-03 (ADR-0043 D1/D5, PAH-R6): identity (package_id, load_index -- load position is
// external context, not manifest content) plus the package's full-content
// CanonicalManifestHash (see FResolvedPackageSource, PackageDiscovery.h). Any semantic
// field in package.json5, known or not yet known to FPackageDescriptor, changes
// CanonicalManifestHash and therefore this fingerprint -- there is no separate list of
// "fields this hash covers" to keep in sync by hand.
GV2_CONTENT_HOST_SUPPORT_API std::string ComputePackageFingerprint(
    const GV2ContentCore::FPackageDescriptor& Descriptor,
    const std::string& CanonicalManifestHash);

/**
 * Generates canonical, byte-for-byte deterministic mods.lock.json5 document content
 * from an ordered list of resolved package sources.
 */
GV2_CONTENT_HOST_SUPPORT_API std::string GenerateModsLockContent(
    const std::vector<FResolvedPackageSource>& Sources);

/**
 * Verifies that an existing mods.lock.json5 content matches the discovered package sources.
 * Returns true if valid and matching. On mismatch, returns false and populates OutDiagnostics
 * with typed diagnostics ("core:diagnostic.package.lock.mismatch", "core:diagnostic.package.lock.invalid", etc.).
 */
GV2_CONTENT_HOST_SUPPORT_API bool VerifyModsLock(
    const std::string& LockFileContent,
    const std::vector<FResolvedPackageSource>& Sources,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);
}
