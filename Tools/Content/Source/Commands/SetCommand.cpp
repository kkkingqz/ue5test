#include "Commands/SetCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace GV2ContentCli
{

namespace
{

GV2ContentCore::FValue ParseCliValue(const std::string& Raw)
{
    if (Raw == "true")
    {
        return GV2ContentCore::FValue(true);
    }
    if (Raw == "false")
    {
        return GV2ContentCore::FValue(false);
    }
    if (Raw == "null")
    {
        return GV2ContentCore::FValue();
    }

    // Try parsing as integer
    std::int64_t IntVal = 0;
    const auto [IntPtr, IntEc] = std::from_chars(Raw.data(), Raw.data() + Raw.size(), IntVal);
    if (IntEc == std::errc{} && IntPtr == Raw.data() + Raw.size())
    {
        return GV2ContentCore::FValue(IntVal);
    }

    // Try parsing as floating point number
    if (Raw.find('.') != std::string::npos || Raw.find('e') != std::string::npos || Raw.find('E') != std::string::npos)
    {
        try
        {
            std::size_t Processed = 0;
            double DoubleVal = std::stod(Raw, &Processed);
            if (Processed == Raw.size())
            {
                return GV2ContentCore::FValue(DoubleVal);
            }
        }
        catch (...)
        {
        }
    }

    // Quoted string check
    if (Raw.size() >= 2)
    {
        if ((Raw.front() == '"' && Raw.back() == '"') || (Raw.front() == '\'' && Raw.back() == '\''))
        {
            return GV2ContentCore::FValue(Raw.substr(1, Raw.size() - 2));
        }
    }

    return GV2ContentCore::FValue(Raw);
}

} // namespace

int RunSet(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 4)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"set requires <package-root>, <definition-id>, <json-pointer>, and <value>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: set requires <package-root>, <definition-id>, <json-pointer>, and <value>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path RawRoot = Positional[0];
    const std::string& DefinitionIdStr = Positional[1];
    const std::string& JsonPointerArg = Positional[2];
    const std::string& RawValue = Positional[3];

    GV2ContentCore::FStableIdView StableIdView;
    GV2ContentCore::EStableIdError IdError = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(DefinitionIdStr, StableIdView, &IdError))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_definition_id\",\"message\":\"'"
                      << DefinitionIdStr << "' is not a valid definition id\",\"definition_id\":\""
                      << DefinitionIdStr << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: set failed: '" << DefinitionIdStr << "' is not a valid definition id\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"package root not found or not a directory\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: package root not found or not a directory: " << Root.string() << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const FPackageSetDiscovery PackageSet = DiscoverPackageSet({ Root });
    if (PackageSet.bToolFailure)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\""
                      << PackageSet.ToolFailureMessage << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: " << PackageSet.ToolFailureMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (PackageSet.bDiscoveryFailed)
    {
        return EmitDiagnosticsFailure(PackageSet.Diagnostics, Format);
    }

    const std::size_t TargetIndex = PackageSet.TargetIndex();
    const GV2ContentCore::FPackageDescriptor& TargetDescriptor = PackageSet.Descriptors[TargetIndex];
    const std::filesystem::path& TargetRoot = PackageSet.Roots[TargetIndex];

    // Check if target package is frozen
    const std::filesystem::path ManifestPath = TargetRoot / "package.json5";
    if (std::filesystem::is_regular_file(ManifestPath))
    {
        std::ifstream ManifestFile(ManifestPath);
        std::string ManifestContent(
            (std::istreambuf_iterator<char>(ManifestFile)),
            std::istreambuf_iterator<char>());
        std::vector<GV2ContentCore::FDiagnostic> ManifestDiags;
        GV2ContentCore::FParseLimits Limits;
        auto ParsedManifest = GV2ContentCore::ParseJson5Document(ManifestContent, Limits, ManifestDiags);
        if (ParsedManifest.has_value() && ParsedManifest->GetRootValue().IsObject())
        {
            const auto* FrozenField = ParsedManifest->GetRootValue().FindField("frozen");
            if (FrozenField != nullptr && FrozenField->IsBoolean() && FrozenField->AsBoolean())
            {
                if (Format == EOutputFormat::Json)
                {
                    std::cout << "{\"status\":\"error\",\"code\":\"package_frozen\",\"message\":\"cannot set field in frozen package '"
                              << TargetDescriptor.GetPackageId() << "'\",\"package_id\":\"" << TargetDescriptor.GetPackageId() << "\"}\n";
                }
                else
                {
                    std::cerr << "gv2-content: set failed: cannot set field in frozen package '" << TargetDescriptor.GetPackageId() << "'\n";
                }
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
    }

    GV2ContentAuthoring::FSetFieldParams Params;
    Params.PackageRoot = TargetRoot;
    Params.DefinitionId = DefinitionIdStr;
    Params.JsonPointer = JsonPointerArg;
    Params.NewValue = ParseCliValue(RawValue);
    const GV2ContentAuthoring::FAuthoringResult Result =
        GV2ContentAuthoring::FAuthoringService::SetField(Params);

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
            WriteJsonEscapedString(std::cout, DefinitionIdStr);
            std::cout << ",\"json_pointer\":";
            WriteJsonEscapedString(std::cout, JsonPointerArg);
            std::cout << "}\n";
        }
        else
        {
            std::cerr << "gv2-content: set failed: " << Result.ErrorMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code RelativeError;
    const std::filesystem::path RelativeFile =
        std::filesystem::relative(Result.TargetFilePath, TargetRoot, RelativeError);
    const std::string FileText = RelativeError
        ? Result.TargetFilePath.generic_string()
        : RelativeFile.generic_string();

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_id\":\"" << DefinitionIdStr
                  << "\",\"json_pointer\":\"" << JsonPointerArg
                  << "\",\"file\":\"" << FileText << "\"}\n";
    }
    else
    {
        std::cout << "updated " << DefinitionIdStr << " (" << JsonPointerArg << ") in " << FileText << "\n";
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
