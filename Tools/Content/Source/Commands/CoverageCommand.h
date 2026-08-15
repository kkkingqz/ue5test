#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

int RunCoverage(
    const std::vector<std::string>& Positional,
    EOutputFormat Format,
    const std::string& SpecificLocale);

} // namespace GV2ContentCli
