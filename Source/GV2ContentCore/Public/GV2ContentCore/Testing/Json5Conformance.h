#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
struct GV2_CONTENT_CORE_API FJson5Fixture final
{
    std::string RelativePath;
    std::string Source;
    std::optional<std::string> ExpectedDiagnosticCode;
};

/** Runs the same parser fixture assertions in standalone and Unreal hosts. */
GV2_CONTENT_CORE_API bool RunJson5FixtureConformance(
    const std::vector<FJson5Fixture>& Fixtures,
    std::string& OutFailure);
}
