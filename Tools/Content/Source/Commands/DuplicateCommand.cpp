#include "Commands/DuplicateCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentCore/StableId.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace GV2ContentCli
{

int RunDuplicate(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 3)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"duplicate requires <package-root>, <source-id>, and <target-id>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: duplicate requires <package-root>, <source-id>, and <target-id>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path RawRoot = Positional[0];
    const std::string& SourceIdStr = Positional[1];
    const std::string& TargetIdStr = Positional[2];

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"package_root_not_found\",\"message\":\"package root not found or not a directory\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: package root not found or not a directory: " << Root.string() << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    GV2ContentAuthoring::FDuplicateDefinitionParams Params;
    Params.PackageRoot = Root;
    Params.SourceDefinitionId = SourceIdStr;
    Params.TargetDefinitionId = TargetIdStr;

    GV2ContentAuthoring::FAuthoringResult Result = GV2ContentAuthoring::FAuthoringService::DuplicateDefinition(Params);
    if (!Result.IsSuccess())
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"" << Result.ErrorCode
                      << "\",\"message\":\"" << Result.ErrorMessage
                      << "\",\"source_id\":\"" << SourceIdStr
                      << "\",\"target_id\":\"" << TargetIdStr << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: duplicate failed: " << Result.ErrorMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"source_id\":\"" << SourceIdStr
                  << "\",\"target_id\":\"" << TargetIdStr
                  << "\",\"file\":\"" << Result.TargetFilePath.filename().string() << "\"}\n";
    }
    else
    {
        std::cout << "duplicated " << SourceIdStr << " -> " << TargetIdStr << "\n";
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
