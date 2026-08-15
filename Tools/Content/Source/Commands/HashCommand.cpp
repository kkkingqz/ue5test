#include "Commands/HashCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"

#include <iostream>

namespace GV2ContentCli
{

int RunHash(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 1)
    {
        std::cerr << "usage: gv2-content hash <package-root> [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FRootBuildOutcome Outcome = BuildFromPackageRoot(Positional[0]);
    if (Outcome.bToolFailure)
    {
        std::cerr << "gv2-content: " << Outcome.ToolFailureMessage << "\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const GV2ContentCore::FBuildResult& Result = *Outcome.Result;
    if (Result.IsFailure())
    {
        return EmitDiagnosticsFailure(Result.GetDiagnostics(), Format);
    }

    const std::string Hash = Result.GetCandidate().GetReadHandle().GetContentHash();
    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"content_hash\":";
        WriteJsonEscapedString(std::cout, Hash);
        std::cout << "}\n";
    }
    else
    {
        std::cout << Hash << "\n";
    }
    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
