#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <string>

namespace GV2ContentCore::Testing
{
/**
 * Executes portable conformance tests for special schema fields: Stable ID references (`ref`),
 * localizable text keys (`text_id`), and resource bindings (`resource_ref`) across hosts.
 *
 * Returns empty string on success, or a stable case identifier on failure.
 */
GV2_CONTENT_CORE_API std::string RunSpecialFieldValidationConformance();
}
