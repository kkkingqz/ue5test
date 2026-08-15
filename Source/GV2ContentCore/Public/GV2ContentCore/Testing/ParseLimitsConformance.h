#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for UTF-8 byte validation, BOM handling,
 * nesting depth ceilings, file/string/container limits, and parse-stage limit enforcement across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunParseLimitsConformance();
}
