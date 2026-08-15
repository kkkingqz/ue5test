#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for GNU gettext PO catalog parsing across hosts:
 * valid entries, multi-line string concatenation, escape sequences, context IDs, headers,
 * comments, and negative error diagnostics (unterminated strings, invalid escapes,
 * duplicate context IDs, missing fields).
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunPoParserConformance();
}
