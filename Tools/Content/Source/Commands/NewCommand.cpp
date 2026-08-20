#include "Commands/NewCommand.h"
#include "Support/CliOutput.h"
#include "GV2ContentAuthoring/AuthoringService.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace GV2ContentCli
{

int RunNew(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 3)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"new requires <package-root>, <definition-type>, and <definition-id>\"}\n";
        }
        else
        {
            std::cerr << "usage: gv2-content new <package-root> <definition-type> <definition-id> [--format=text|json]\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code Ec;
    const std::filesystem::path RawRoot = Positional[0];
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;
    const std::string& DefinitionType = Positional[1];
    const std::string& DefinitionId = Positional[2];

    GV2ContentAuthoring::FCreateDefinitionParams Params;
    Params.PackageRoot = Root;
    Params.DefinitionType = DefinitionType;
    Params.DefinitionId = DefinitionId;
    const GV2ContentAuthoring::FAuthoringResult Result =
        GV2ContentAuthoring::FAuthoringService::CreateDefinition(Params);

    if (!Result.IsSuccess())
    {
        if (!Result.Diagnostics.empty())
        {
            return EmitDiagnosticsFailure(Result.Diagnostics, Format);
        }
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":";
            WriteJsonEscapedString(std::cout, Result.ErrorCode.empty() ? "tool_failure" : Result.ErrorCode);
            std::cout << ",\"message\":";
            WriteJsonEscapedString(std::cout, Result.ErrorMessage);
            std::cout << ",\"definition_id\":";
            WriteJsonEscapedString(std::cout, DefinitionId);
            std::cout << ",\"definition_type\":";
            WriteJsonEscapedString(std::cout, DefinitionType);
            std::cout << "}\n";
        }
        else
        {
            std::cerr << "gv2-content: " << Result.ErrorMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code RelativeError;
    const std::filesystem::path RelativeFile =
        std::filesystem::relative(Result.TargetFilePath, Root, RelativeError);
    const std::string RelativeSource = RelativeError
        ? Result.TargetFilePath.generic_string()
        : RelativeFile.generic_string();

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_id\":";
        WriteJsonEscapedString(std::cout, DefinitionId);
        std::cout << ",\"definition_type\":";
        WriteJsonEscapedString(std::cout, DefinitionType);
        std::cout << ",\"relative_source\":";
        WriteJsonEscapedString(std::cout, RelativeSource);
        std::cout << "}\n";
    }
    else
    {
        std::cout << "created definition " << DefinitionId << " in " << RelativeSource << "\n";
    }
    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
