#include "Commands/DeleteCommand.h"
#include "Support/CliOutput.h"
#include "Support/Json5AstRewriter.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

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

struct FReferenceOccurrence
{
    std::string RelativeSource;
    std::string JsonPointer;
    std::string SourceDefinitionId;
    std::size_t Line = 0;
    std::size_t Column = 0;
};

void ScanValueForReferences(
    const GV2ContentCore::FValue& Value,
    const std::string& CurrentPointer,
    const std::string& RelativeSource,
    const std::string& CurrentDefinitionId,
    const std::string& TargetId,
    const GV2ContentCore::FParsedDocument& Document,
    std::vector<FReferenceOccurrence>& OutOccurrences)
{
    if (Value.IsString())
    {
        if (Value.AsString() == TargetId && CurrentDefinitionId != TargetId)
        {
            FReferenceOccurrence Occ;
            Occ.RelativeSource = RelativeSource;
            Occ.JsonPointer = CurrentPointer;
            Occ.SourceDefinitionId = CurrentDefinitionId;
            if (const auto* Loc = Document.FindLocation(CurrentPointer))
            {
                Occ.Line = Loc->ValueSpan.StartLine;
                Occ.Column = Loc->ValueSpan.StartColumn;
            }
            OutOccurrences.push_back(std::move(Occ));
        }
        return;
    }
    if (Value.IsArray())
    {
        const auto& Arr = Value.AsArray();
        for (std::size_t Index = 0; Index < Arr.size(); ++Index)
        {
            const std::string ChildPointer = CurrentPointer + "/" + std::to_string(Index);
            std::string DefId = CurrentDefinitionId;
            if (CurrentPointer == "/definitions" && Arr[Index].IsObject())
            {
                const auto& Obj = Arr[Index].AsObject();
                for (const auto& [Key, FieldVal] : Obj)
                {
                    if (Key == "id" && FieldVal.IsString())
                    {
                        DefId = FieldVal.AsString();
                        break;
                    }
                }
            }
            ScanValueForReferences(Arr[Index], ChildPointer, RelativeSource, DefId, TargetId, Document, OutOccurrences);
        }
        return;
    }
    if (Value.IsObject())
    {
        const auto& Obj = Value.AsObject();
        for (const auto& [Key, ChildVal] : Obj)
        {
            const std::string ChildPointer = CurrentPointer + "/" + Key;
            ScanValueForReferences(ChildVal, ChildPointer, RelativeSource, CurrentDefinitionId, TargetId, Document, OutOccurrences);
        }
        return;
    }
}

} // namespace

int RunDelete(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 2)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"delete requires <package-root> and <definition-id>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: delete requires <package-root> and <definition-id>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path RawRoot = Positional[0];
    const std::string& DefinitionIdStr = Positional[1];

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
            std::cerr << "gv2-content: delete failed: '" << DefinitionIdStr << "' is not a valid definition id\n";
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
                    std::cout << "{\"status\":\"error\",\"code\":\"package_frozen\",\"message\":\"cannot delete definition from frozen package '"
                              << TargetDescriptor.GetPackageId() << "'\",\"package_id\":\"" << TargetDescriptor.GetPackageId() << "\"}\n";
                }
                else
                {
                    std::cerr << "gv2-content: delete failed: cannot delete definition from frozen package '" << TargetDescriptor.GetPackageId() << "'\n";
                }
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
    }

    // Find which file contains DefinitionIdStr
    std::string FoundRelativeSource;
    std::string FoundFileContent;
    std::size_t FoundEntryIndex = std::string::npos;
    GV2ContentCore::FParseLimits Limits;

    for (const std::string& RelSource : TargetDescriptor.GetRelativeSources())
    {
        std::filesystem::path FullPath = TargetRoot / RelSource;
        if (!std::filesystem::is_regular_file(FullPath))
        {
            continue;
        }

        std::ifstream InFile(FullPath);
        std::string Content(
            (std::istreambuf_iterator<char>(InFile)),
            std::istreambuf_iterator<char>());

        std::vector<GV2ContentCore::FDiagnostic> Diags;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diags, TargetDescriptor.GetPackageId(), 0, RelSource);
        if (!ParsedDoc.has_value() || !ParsedDoc->GetRootValue().IsObject())
        {
            continue;
        }

        const auto* DefsVal = ParsedDoc->GetRootValue().FindField("definitions");
        if (DefsVal != nullptr && DefsVal->IsArray())
        {
            const auto& DefsArr = DefsVal->AsArray();
            for (std::size_t i = 0; i < DefsArr.size(); ++i)
            {
                if (DefsArr[i].IsObject())
                {
                    const auto* IdField = DefsArr[i].FindField("id");
                    if (IdField != nullptr && IdField->IsString() && IdField->AsString() == DefinitionIdStr)
                    {
                        FoundRelativeSource = RelSource;
                        FoundFileContent = std::move(Content);
                        FoundEntryIndex = i;
                        break;
                    }
                }
            }
        }

        if (FoundEntryIndex != std::string::npos)
        {
            break;
        }
    }

    if (FoundEntryIndex == std::string::npos)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"definition_not_found\",\"message\":\"definition '"
                      << DefinitionIdStr << "' not found in package '" << TargetDescriptor.GetPackageId()
                      << "'\",\"definition_id\":\"" << DefinitionIdStr << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: delete failed: definition '" << DefinitionIdStr
                      << "' not found in package '" << TargetDescriptor.GetPackageId() << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    // Check incoming references across all packages in the set
    std::vector<FReferenceOccurrence> IncomingReferences;
    for (std::size_t PkgIdx = 0; PkgIdx < PackageSet.Descriptors.size(); ++PkgIdx)
    {
        const auto& PkgDesc = PackageSet.Descriptors[PkgIdx];
        const auto& PkgRoot = PackageSet.Roots[PkgIdx];

        for (const std::string& RelSource : PkgDesc.GetRelativeSources())
        {
            std::filesystem::path FullPath = PkgRoot / RelSource;
            if (!std::filesystem::is_regular_file(FullPath))
            {
                continue;
            }

            std::ifstream InFile(FullPath);
            std::string Content(
                (std::istreambuf_iterator<char>(InFile)),
                std::istreambuf_iterator<char>());

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto ParsedDoc = GV2ContentCore::ParseJson5Document(
                Content, Limits, Diags, PkgDesc.GetPackageId(), 0, RelSource);
            if (!ParsedDoc.has_value())
            {
                continue;
            }

            ScanValueForReferences(
                ParsedDoc->GetRootValue(), "", RelSource, "", DefinitionIdStr, *ParsedDoc, IncomingReferences);
        }
    }

    if (!IncomingReferences.empty())
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"referenced_by_definitions\",\"message\":\"cannot delete definition '"
                      << DefinitionIdStr << "': referenced by " << IncomingReferences.size() << " definition(s)\",\"definition_id\":\""
                      << DefinitionIdStr << "\",\"references\":[";
            for (std::size_t i = 0; i < IncomingReferences.size(); ++i)
            {
                if (i > 0) std::cout << ",";
                std::cout << "{\"source_definition_id\":\"" << IncomingReferences[i].SourceDefinitionId
                          << "\",\"file\":\"" << IncomingReferences[i].RelativeSource
                          << "\",\"line\":" << IncomingReferences[i].Line
                          << ",\"column\":" << IncomingReferences[i].Column
                          << ",\"pointer\":\"" << IncomingReferences[i].JsonPointer << "\"}";
            }
            std::cout << "]}\n";
        }
        else
        {
            std::cerr << "gv2-content: cannot delete " << DefinitionIdStr << ": referenced by "
                      << IncomingReferences.size() << " definition(s):\n";
            for (const auto& Ref : IncomingReferences)
            {
                std::cerr << "  " << Ref.SourceDefinitionId << " in " << Ref.RelativeSource
                          << ":" << Ref.Line << ":" << Ref.Column << " (" << Ref.JsonPointer << ")\n";
            }
        }
        return static_cast<int>(EExitCode::InvalidContent);
    }

    FRemoveDefinitionResult RemoveResult = RemoveDefinitionEntry(
        FoundFileContent, DefinitionIdStr, TargetDescriptor.GetPackageId(), FoundRelativeSource);

    if (RemoveResult.Status != ERemoveDefinitionStatus::Success)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"" << RemoveResult.ErrorCode
                      << "\",\"message\":\"" << RemoveResult.ErrorMessage
                      << "\",\"definition_id\":\"" << DefinitionIdStr << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: delete failed: " << RemoveResult.ErrorMessage << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    // Write updated content
    std::filesystem::path TargetFilePath = TargetRoot / FoundRelativeSource;
    std::ofstream OutFile(TargetFilePath, std::ios::trunc);
    if (!OutFile.is_open())
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"io_failure\",\"message\":\"failed to write to file "
                      << FoundRelativeSource << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: failed to write to file " << FoundRelativeSource << "\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }
    OutFile << RemoveResult.UpdatedContent;
    OutFile.close();

    // Validate the package set after edit
    const FRootBuildOutcome BuildOutcome = BuildFromPackageRoots({ TargetRoot });
    if (BuildOutcome.bToolFailure)
    {
        return EmitToolFailure(BuildOutcome.ToolFailureMessage, Format);
    }
    if (BuildOutcome.Result.has_value() && BuildOutcome.Result->IsFailure())
    {
        return EmitDiagnosticsFailure(BuildOutcome.Result->GetDiagnostics(), Format);
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_id\":\"" << DefinitionIdStr
                  << "\",\"file\":\"" << FoundRelativeSource << "\"}\n";
    }
    else
    {
        std::cout << "deleted definition " << DefinitionIdStr << " from " << FoundRelativeSource << "\n";
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
