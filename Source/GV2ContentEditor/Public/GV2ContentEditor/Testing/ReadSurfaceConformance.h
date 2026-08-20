#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include <string>

namespace GV2ContentEditor::Testing
{

/**
 * Conformance test runner for the GV2 Read Surface layer (M3: CED-09..12).
 * Returns empty string on success, or failure description if any test fails.
 */
GV2_CONTENT_EDITOR_API std::string RunReadSurfaceConformance();

} // namespace GV2ContentEditor::Testing
