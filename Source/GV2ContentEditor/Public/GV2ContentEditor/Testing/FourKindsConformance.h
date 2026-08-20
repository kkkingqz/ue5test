#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include <string>

namespace GV2ContentEditor::Testing
{

/**
 * Conformance test runner for the Four Kinds authoring layer (M5: CED-17..20).
 * Returns empty string on success, or failure description if any test fails.
 */
GV2_CONTENT_EDITOR_API std::string RunFourKindsConformance();

} // namespace GV2ContentEditor::Testing
