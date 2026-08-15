#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

int RunRefs(const std::vector<std::string>& Positional, EOutputFormat Format);

} // namespace GV2ContentCli
