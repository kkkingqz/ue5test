#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// PCC-33: `gv2-content hash <package-root> [--format=text|json]`.
int RunHash(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
