#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// CAT-02: `gv2-content new <package-root> <definition-type> <definition-id> [--format=text|json]`.
int RunNew(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
