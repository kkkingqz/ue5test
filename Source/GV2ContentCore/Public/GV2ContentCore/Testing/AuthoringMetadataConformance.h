#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for Schema Authoring UI Metadata across hosts:
 * valid metadata parsing, field lookup, order, label, description, category, widget_hint,
 * and negative error diagnostics (unresolved fields, unknown root properties,
 * unknown field properties, invalid types).
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunAuthoringMetadataConformance();
}
