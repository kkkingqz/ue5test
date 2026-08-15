#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for JSON5 AST parsing, determinism, numeric normalization,
 * duplicate key detection with related spans, and source location tracking across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunJson5ParserConformance();
}
