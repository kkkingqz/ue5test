#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for the `ui_field`/`ui_value` schema
 * node compiler: schema_domain parsing and every standard UI FieldSpec kind
 * (scalar, key, text, ref, binding, object, array, screen_fields), each with
 * a positive and a negative case, across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunUiSchemaConformance();
}
