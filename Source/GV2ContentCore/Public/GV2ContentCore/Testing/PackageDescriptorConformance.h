#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for FPackageDescriptor validation,
 * namespace rules, schema/extension bindings, and redirect/tombstone integrity across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunPackageDescriptorConformance();
}
