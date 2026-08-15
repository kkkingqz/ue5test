#pragma once

#include "Support/CliOutput.h"
#include <cstdint>
#include <string>
#include <vector>

namespace GV2ContentCli
{

// PCC-31: `gv2-content validate <package-root> [--watch] [--poll-interval=MS] [--max-iterations=N] [--format=text|json]`.
int RunValidate(
    const std::vector<std::string>& Positional,
    EOutputFormat Format,
    bool bWatch = false,
    std::uint32_t PollIntervalMs = 500,
    std::uint32_t MaxIterations = 0);

} // namespace GV2ContentCli
