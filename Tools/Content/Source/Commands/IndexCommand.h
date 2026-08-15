#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

int RunIndex(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
