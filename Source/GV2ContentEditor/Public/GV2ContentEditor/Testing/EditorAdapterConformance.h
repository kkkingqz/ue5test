#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include <string>

namespace GV2ContentEditor::Testing
{

/**
 * Conformance test runner for the GV2 Editor Adapter layer (M2: CED-05..08).
 * Returns empty string on success, or failure description if any test fails.
 */
GV2_CONTENT_EDITOR_API std::string RunEditorAdapterConformance();

} // namespace GV2ContentEditor::Testing
