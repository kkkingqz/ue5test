#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include <string>

namespace GV2ContentHostSupport::Testing
{
/**
 * Executes the portable cross-host conformance suite for mandatory
 * package manifests (PKG-01/02/03, plan PackageSupport). Roots itself in
 * temporary directories it creates and removes internally, so both hosts
 * call this with no arguments and no host-specific setup.
 *
 * Covers: missing manifest, invalid/mismatched package_id and namespace,
 * missing/malformed version, incompatible game/api/schema compatibility
 * ranges (and that an absent range is always compatible), malformed and
 * well-formed dependency entries (including load_after), and that a fully
 * valid manifest is accepted and its fields (version, dependencies) are
 * carried through to the resulting FPackageDescriptor unchanged.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_CONTENT_HOST_SUPPORT_API std::string RunPackageManifestConformance();
}
