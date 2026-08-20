#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include <string>

namespace GV2ContentEditor::Testing
{

/**
 * Conformance test runner for the GV2 Write Surface layer (M4: CED-13..16).
 * Returns empty string on success, or failure description if any test fails.
 */
GV2_CONTENT_EDITOR_API std::string RunWriteSurfaceConformance();

} // namespace GV2ContentEditor::Testing
