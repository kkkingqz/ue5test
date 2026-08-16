#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include <string>

namespace GV2ContentHostSupport::Testing
{
/**
 * Executes the portable cross-host conformance suite for Package Discovery and Order
 * (PKG-05…09, Milestone 2 of plan PackageSupport).
 *
 * Covers:
 * - Multi-root package discovery with correct LoadIndex assignment
 * - Duplicate package_id detection across roots with both paths in diagnostic
 * - Missing dependency detection
 * - Dependency cycle detection
 * - load_after ordering violation detection
 * - mods.lock.json5 byte-for-byte generation and mismatch verification
 * - End-to-end repository building from multiple packages using FMultiPackageSourceProvider
 *
 * Returns empty string on success, or an error string describing the failure.
 */
GV2_CONTENT_HOST_SUPPORT_API std::string RunPackageDiscoveryAndOrderConformance();
}
