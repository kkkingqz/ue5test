#pragma once

#include "Support/CliOutput.h"
#include <string>
#include <vector>

namespace GV2ContentCli
{

// PCC-32: `gv2-content inspect <package-root> <definition-id> [--provenance] [--format=text|json]`.
int RunInspect(const std::vector<std::string>& Positional, EOutputFormat Format, bool bProvenance);

} // namespace GV2ContentCli
