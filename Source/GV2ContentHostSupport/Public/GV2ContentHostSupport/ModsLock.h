#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/PackageDescriptor.h"

#include <string>
#include <vector>

namespace GV2ContentHostSupport
{
/**
 * Computes a deterministic canonical SHA-256 fingerprint for a package descriptor.
 */
GV2_CONTENT_HOST_SUPPORT_API std::string ComputePackageFingerprint(
    const GV2ContentCore::FPackageDescriptor& Descriptor);

/**
 * Generates canonical, byte-for-byte deterministic mods.lock.json5 document content
 * from an ordered list of package descriptors.
 */
GV2_CONTENT_HOST_SUPPORT_API std::string GenerateModsLockContent(
    const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors);

/**
 * Verifies that an existing mods.lock.json5 content matches the discovered package descriptors.
 * Returns true if valid and matching. On mismatch, returns false and populates OutDiagnostics
 * with typed diagnostics ("core:diagnostic.package.lock.mismatch", "core:diagnostic.package.lock.invalid", etc.).
 */
GV2_CONTENT_HOST_SUPPORT_API bool VerifyModsLock(
    const std::string& LockFileContent,
    const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);
}
