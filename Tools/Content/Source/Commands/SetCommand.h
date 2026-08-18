#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// CEP-07: `gv2-content set <package-root> <definition-id> <json-pointer> <value> [--format=text|json]`
int RunSet(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
