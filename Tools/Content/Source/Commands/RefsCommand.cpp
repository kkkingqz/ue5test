#include "Commands/RefsCommand.h"
#include "Support/CliOutput.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCli
{

namespace
{

std::string EscapeJsonPointerSegment(const std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~')
        {
            Escaped += "~0";
        }
        else if (Character == '/')
        {
            Escaped += "~1";
        }
        else
        {
            Escaped.push_back(Character);
        }
    }
    return Escaped;
}

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
        if (Value.AsString() == TargetId)
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
            const std::string ChildPointer = CurrentPointer + "/" + EscapeJsonPointerSegment(Key);
            // Skip the definition entry's own identity declaration at /definitions/N/id
            if (Key == "id" && !CurrentDefinitionId.empty() && CurrentPointer.rfind("/definitions/", 0) == 0 && CurrentPointer.find('/', 13) == std::string::npos)
            {
                continue;
            }
            ScanValueForReferences(ChildVal, ChildPointer, RelativeSource, CurrentDefinitionId, TargetId, Document, OutOccurrences);
        }
        return;
    }
}

} // namespace

int RunRefs(
    const std::vector<std::string>& Positional,
    const EOutputFormat Format)
{
    if (Positional.size() != 2)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"refs requires <package-root> and <definition-id>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: refs requires <package-root> and <definition-id>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path PackageRoot = Positional[0];
    const std::string& TargetId = Positional[1];

    GV2ContentCore::FStableIdView IdView;
    GV2ContentCore::EStableIdError IdError = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(TargetId, IdView, &IdError))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_definition_id\",\"message\":\"not a valid definition id '"
                      << TargetId << "'\",\"definition_id\":\"" << TargetId << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: refs failed: not a valid definition id '" << TargetId << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code RootEc;
    if (!std::filesystem::exists(PackageRoot, RootEc) || !std::filesystem::is_directory(PackageRoot, RootEc))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"package root not found or not a directory\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: refs failed: package root not found or not a directory\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::vector<GV2ContentCore::FDiagnostic> DiscoveryDiagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> PackageDescriptor =
        GV2ContentHostSupport::DiscoverPackageFromDirectory(PackageRoot, DiscoveryDiagnostics);
    if (!PackageDescriptor)
    {
        return EmitDiagnosticsFailure(DiscoveryDiagnostics, Format);
    }

    std::vector<FReferenceOccurrence> Occurrences;
    const GV2ContentCore::FParseLimits Limits;

    for (const std::string& RelativeSource : PackageDescriptor->GetRelativeSources())
    {
        const std::filesystem::path SourcePath = PackageRoot / RelativeSource;
        std::ifstream Stream(SourcePath, std::ios::binary);
        if (!Stream)
        {
            continue;
        }
        const std::string Content((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        const auto Doc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diagnostics, PackageDescriptor->GetPackageId(), 0, RelativeSource);
        if (Doc.has_value())
        {
            ScanValueForReferences(Doc->GetRootValue(), "", RelativeSource, "", TargetId, *Doc, Occurrences);
        }
    }

    for (const GV2ContentCore::FRedirectDescriptor& Redirect : PackageDescriptor->GetRedirects())
    {
        if (Redirect.GetTargetId() == TargetId)
        {
            FReferenceOccurrence Occ;
            Occ.RelativeSource = "package.json5";
            Occ.JsonPointer = "/redirects/" + EscapeJsonPointerSegment(Redirect.GetSourceId());
            Occ.SourceDefinitionId = Redirect.GetSourceId();
            Occurrences.push_back(std::move(Occ));
        }
    }

    std::sort(Occurrences.begin(), Occurrences.end(), [](const FReferenceOccurrence& A, const FReferenceOccurrence& B) {
        if (A.RelativeSource != B.RelativeSource) return A.RelativeSource < B.RelativeSource;
        if (A.Line != B.Line) return A.Line < B.Line;
        if (A.Column != B.Column) return A.Column < B.Column;
        return A.JsonPointer < B.JsonPointer;
    });

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_id\":";
        WriteJsonEscapedString(std::cout, TargetId);
        std::cout << ",\"references_count\":" << Occurrences.size()
                  << ",\"references\":[";
        for (std::size_t Index = 0; Index < Occurrences.size(); ++Index)
        {
            if (Index > 0)
            {
                std::cout << ",";
            }
            const auto& Occ = Occurrences[Index];
            std::cout << "{\"source_file\":";
            WriteJsonEscapedString(std::cout, Occ.RelativeSource);
            std::cout << ",\"line\":" << Occ.Line
                      << ",\"column\":" << Occ.Column
                      << ",\"json_pointer\":";
            WriteJsonEscapedString(std::cout, Occ.JsonPointer);
            std::cout << ",\"source_definition_id\":";
            WriteJsonEscapedString(std::cout, Occ.SourceDefinitionId);
            std::cout << "}";
        }
        std::cout << "]}\n";
    }
    else
    {
        std::cout << "definition_id: " << TargetId << "\n";
        std::cout << "references: " << Occurrences.size() << "\n";
        for (const auto& Occ : Occurrences)
        {
            std::cout << "  " << Occ.RelativeSource << ":" << Occ.Line << ":" << Occ.Column
                      << " (" << Occ.JsonPointer;
            if (!Occ.SourceDefinitionId.empty())
            {
                std::cout << ", definition: " << Occ.SourceDefinitionId;
            }
            std::cout << ")\n";
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
