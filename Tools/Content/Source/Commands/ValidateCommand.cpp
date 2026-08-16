#include "Commands/ValidateCommand.h"
#include "Support/CliOutput.h"
#include "Support/DirectoryWatcher.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/BuildResult.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace GV2ContentCli
{

int RunValidate(
    const std::vector<std::string>& Positional,
    const EOutputFormat Format,
    const bool bWatch,
    const std::uint32_t PollIntervalMs,
    const std::uint32_t MaxIterations)
{
    if (Positional.empty())
    {
        std::cerr << "usage: gv2-content validate <package-root>... [--watch] [--poll-interval=MS] [--max-iterations=N] [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::vector<std::filesystem::path> PackageRoots;
    PackageRoots.reserve(Positional.size());
    for (const auto& Pos : Positional)
    {
        PackageRoots.emplace_back(Pos);
    }

    auto ExecuteSinglePass = [&](std::size_t Iteration) -> int {
        try
        {
            FRootBuildOutcome Outcome = BuildFromPackageRoots(PackageRoots);
            if (Outcome.bToolFailure)
            {
                if (Format == EOutputFormat::Json)
                {
                    std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":";
                    WriteJsonEscapedString(std::cout, Outcome.ToolFailureMessage);
                    if (bWatch)
                    {
                        std::cout << ",\"iteration\":" << Iteration;
                    }
                    std::cout << "}\n";
                    std::cout.flush();
                }
                else
                {
                    std::cerr << "gv2-content: " << Outcome.ToolFailureMessage << "\n";
                }
                return static_cast<int>(EExitCode::ToolFailure);
            }

            const GV2ContentCore::FBuildResult& Result = *Outcome.Result;
            if (Result.IsFailure())
            {
                if (Format == EOutputFormat::Json)
                {
                    std::cout << "{\"status\":\"invalid\",\"diagnostics\":[";
                    for (std::size_t Index = 0; Index < Result.GetDiagnostics().size(); ++Index)
                    {
                        if (Index != 0)
                        {
                            std::cout << ",";
                        }
                        WriteDiagnosticJson(std::cout, Result.GetDiagnostics()[Index]);
                    }
                    std::cout << "]";
                    if (bWatch)
                    {
                        std::cout << ",\"iteration\":" << Iteration;
                    }
                    std::cout << "}\n";
                    std::cout.flush();
                }
                else
                {
                    for (const auto& Diagnostic : Result.GetDiagnostics())
                    {
                        WriteDiagnosticText(std::cout, Diagnostic);
                    }
                    std::cout.flush();
                }
                return static_cast<int>(EExitCode::InvalidContent);
            }

            const GV2ContentCore::FRepositoryReadHandle Handle = Result.GetCandidate().GetReadHandle();
            if (Format == EOutputFormat::Json)
            {
                std::cout << "{\"status\":\"ok\",\"content_hash\":";
                WriteJsonEscapedString(std::cout, Handle.GetContentHash());
                if (bWatch)
                {
                    std::cout << ",\"iteration\":" << Iteration;
                }
                std::cout << "}\n";
                std::cout.flush();
            }
            else
            {
                std::cout << "ok content_hash=" << Handle.GetContentHash() << "\n";
                std::cout.flush();
            }
            return static_cast<int>(EExitCode::Success);
        }
        catch (const std::exception& Ex)
        {
            if (Format == EOutputFormat::Json)
            {
                std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":";
                WriteJsonEscapedString(std::cout, Ex.what());
                if (bWatch)
                {
                    std::cout << ",\"iteration\":" << Iteration;
                }
                std::cout << "}\n";
                std::cout.flush();
            }
            else
            {
                std::cerr << "gv2-content: exception during validation: " << Ex.what() << "\n";
            }
            return static_cast<int>(EExitCode::ToolFailure);
        }
    };

    if (!bWatch)
    {
        return ExecuteSinglePass(1);
    }

    return RunDirectoryWatchLoop(PackageRoots[0], {PollIntervalMs, MaxIterations}, ExecuteSinglePass);
}

} // namespace GV2ContentCli
