#include "Commands/RenameCommand.h"
#include "Support/CliOutput.h"
#include "GV2ContentAuthoring/AuthoringService.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace GV2ContentCli
{

int RunRename(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 3)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"rename requires <package-root>, <old-id>, and <new-id>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename requires <package-root>, <old-id>, and <new-id>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code Ec;
    const std::filesystem::path RawRoot = Positional[0];
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;
    const std::string& OldId = Positional[1];
    const std::string& NewId = Positional[2];

    GV2ContentAuthoring::FRenameDefinitionParams Params;
    Params.PackageRoot = Root;
    Params.OldDefinitionId = OldId;
    Params.NewDefinitionId = NewId;
    const GV2ContentAuthoring::FAuthoringResult Result =
        GV2ContentAuthoring::FAuthoringService::RenameDefinition(Params);

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
            std::cout << ",\"old_id\":";
            WriteJsonEscapedString(std::cout, OldId);
            std::cout << ",\"new_id\":";
            WriteJsonEscapedString(std::cout, NewId);
            std::cout << "}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: " << Result.ErrorMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::vector<std::string> ModifiedFiles;
    ModifiedFiles.reserve(Result.AffectedFilePaths.size());
    for (const auto& FilePath : Result.AffectedFilePaths)
    {
        std::error_code RelativeError;
        const auto Relative = std::filesystem::relative(FilePath, Root, RelativeError);
        ModifiedFiles.push_back(RelativeError ? FilePath.generic_string() : Relative.generic_string());
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"old_id\":";
        WriteJsonEscapedString(std::cout, OldId);
        std::cout << ",\"new_id\":";
        WriteJsonEscapedString(std::cout, NewId);
        std::cout << ",\"files_modified_count\":" << Result.AffectedFilesCount
                  << ",\"replacements_count\":" << Result.ReplacementsCount
                  << ",\"modified_files\":[";
        for (std::size_t Index = 0; Index < ModifiedFiles.size(); ++Index)
        {
            if (Index > 0) std::cout << ',';
            std::cout << "{\"source_file\":";
            WriteJsonEscapedString(std::cout, ModifiedFiles[Index]);
            std::cout << "}";
        }
        std::cout << "]}\n";
    }
    else
    {
        std::cout << "renamed " << OldId << " -> " << NewId
                  << "\nfiles modified: " << Result.AffectedFilesCount
                  << "\nreplacements: " << Result.ReplacementsCount << "\n";
        for (const auto& File : ModifiedFiles)
        {
            std::cout << "  " << File << "\n";
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
