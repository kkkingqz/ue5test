#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// CEP-07: `gv2-content delete <package-root> <definition-id> [--format=text|json]`
int RunDelete(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
