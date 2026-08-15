#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// CAT-01: `gv2-content describe <package-root> <definition-type> [--format=text|json]`.
int RunDescribe(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
