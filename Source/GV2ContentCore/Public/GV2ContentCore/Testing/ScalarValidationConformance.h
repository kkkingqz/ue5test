#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for scalar field specifications (integer, double,
 * boolean, string, enum, stable_id) and constraint validation across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunScalarValidationConformance();
}
