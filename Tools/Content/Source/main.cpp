#include "Commands/CoverageCommand.h"
#include "Commands/DeleteCommand.h"
#include "Commands/DescribeCommand.h"
#include "Commands/HashCommand.h"
#include "Commands/IndexCommand.h"
#include "Commands/InspectCommand.h"
#include "Commands/NewCommand.h"
#include "Commands/RefsCommand.h"
#include "Commands/RenameCommand.h"
#include "Commands/SetCommand.h"
#include "Commands/ValidateCommand.h"
#include "Support/CliOutput.h"
#include "Support/Json5AstRewriterConformance.h"
#include "GV2ContentCore/Testing/AuthoringMetadataConformance.h"

#include <iostream>
#include <string>
#include <vector>

using namespace GV2ContentCli;

int main(int argc, char** argv)
{
    const std::vector<std::string> Args(argv + 1, argv + argc);
    if (Args.empty())
    {
        PrintUsage(std::cerr);
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::string& Command = Args[0];
    if (Command == "--help" || Command == "-h" || Command == "help")
    {
        PrintUsage(std::cout);
        return static_cast<int>(EExitCode::Success);
    }

    if (Command == "--self-test")
    {
        const std::string Error = GV2ContentCli::Testing::RunJson5AstRewriterConformance();
        if (!Error.empty())
        {
            std::cerr << "json5_ast_rewriter_conformance_failed: " << Error << "\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        const std::string MetaError = GV2ContentCore::Testing::RunAuthoringMetadataConformance();
        if (!MetaError.empty())
        {
            std::cerr << "authoring_metadata_conformance_failed: " << MetaError << "\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        std::cout << "gv2-content self-test passed.\n";
        return static_cast<int>(EExitCode::Success);
    }

    EOutputFormat Format = EOutputFormat::Text;
    bool bProvenance = false;
    bool bWatch = false;
    std::string Locale;
    std::uint32_t PollIntervalMs = 500;
    std::uint32_t MaxIterations = 0;
    std::vector<std::string> Positional;
    for (std::size_t Index = 1; Index < Args.size(); ++Index)
    {
        const std::string& Arg = Args[Index];
        if (Arg.rfind("--format=", 0) == 0)
        {
            const std::string Value = Arg.substr(9);
            if (Value == "text")
            {
                Format = EOutputFormat::Text;
            }
            else if (Value == "json")
            {
                Format = EOutputFormat::Json;
            }
            else
            {
                std::cerr << "gv2-content: unknown --format value '" << Value << "'\n";
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
        else if (Arg.rfind("--locale=", 0) == 0)
        {
            Locale = Arg.substr(9);
        }
        else if (Arg == "--provenance")
        {
            bProvenance = true;
        }
        else if (Arg == "--watch")
        {
            bWatch = true;
        }
        else if (Arg.rfind("--poll-interval=", 0) == 0)
        {
            try
            {
                PollIntervalMs = static_cast<std::uint32_t>(std::stoul(Arg.substr(16)));
            }
            catch (...)
            {
                std::cerr << "gv2-content: invalid --poll-interval value\n";
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
        else if (Arg.rfind("--max-iterations=", 0) == 0)
        {
            try
            {
                MaxIterations = static_cast<std::uint32_t>(std::stoul(Arg.substr(17)));
            }
            catch (...)
            {
                std::cerr << "gv2-content: invalid --max-iterations value\n";
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
        else if (!Arg.empty() && Arg[0] == '-' && !(Arg.size() >= 2 && Arg[1] >= '0' && Arg[1] <= '9'))
        {
            std::cerr << "gv2-content: unknown option '" << Arg << "'\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        else
        {
            Positional.push_back(Arg);
        }
    }

    if (bProvenance && Command != "inspect")
    {
        std::cerr << "gv2-content: --provenance is only supported for 'inspect'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if ((bWatch || PollIntervalMs != 500 || MaxIterations != 0) && Command != "validate")
    {
        std::cerr << "gv2-content: --watch, --poll-interval, and --max-iterations are only supported for 'validate'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (!Locale.empty() && Command != "coverage")
    {
        std::cerr << "gv2-content: --locale is only supported for 'coverage'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (Command == "validate")
    {
        return RunValidate(Positional, Format, bWatch, PollIntervalMs, MaxIterations);
    }
    if (Command == "inspect")
    {
        return RunInspect(Positional, Format, bProvenance);
    }
    if (Command == "describe")
    {
        return RunDescribe(Positional, Format);
    }
    if (Command == "new")
    {
        return RunNew(Positional, Format);
    }
    if (Command == "refs")
    {
        return RunRefs(Positional, Format);
    }
    if (Command == "rename")
    {
        return RunRename(Positional, Format);
    }
    if (Command == "index")
    {
        return RunIndex(Positional, Format);
    }
    if (Command == "hash")
    {
        return RunHash(Positional, Format);
    }
    if (Command == "coverage")
    {
        return RunCoverage(Positional, Format, Locale);
    }
    if (Command == "set")
    {
        return RunSet(Positional, Format);
    }
    if (Command == "delete")
    {
        return RunDelete(Positional, Format);
    }

    std::cerr << "gv2-content: unknown command '" << Command << "'\n";
    PrintUsage(std::cerr);
    return static_cast<int>(EExitCode::ToolFailure);
}
