#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for FSchemaRegistry registration,
 * schema binding resolution, duplicate checks, and missing schema diagnostics across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunSchemaRegistryConformance();
}
