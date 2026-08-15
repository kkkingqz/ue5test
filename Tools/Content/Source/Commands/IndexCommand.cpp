#include "Commands/IndexCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace GV2ContentCli
{

int RunIndex(
    const std::vector<std::string>& Positional,
    const EOutputFormat Format)
{
    if (Positional.size() != 1)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"index requires <package-root>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: index requires <package-root>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path PackageRoot = Positional[0];
    FRootBuildOutcome Outcome = BuildFromPackageRoot(PackageRoot);
    if (Outcome.bToolFailure)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":";
            WriteJsonEscapedString(std::cout, Outcome.ToolFailureMessage);
            std::cout << "}\n";
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
        return EmitDiagnosticsFailure(Result.GetDiagnostics(), Format);
    }

    const GV2ContentCore::FCandidate& Candidate = Result.GetCandidate();
    const GV2ContentCore::FRepositoryReadHandle Handle = Candidate.GetReadHandle();
    const std::string ContentHash = Handle.GetContentHash();
    const std::string PackageId = Outcome.Descriptor ? Outcome.Descriptor->GetPackageId() : "";

    std::map<std::string, std::vector<std::string>> ActiveByKind;
    std::size_t TotalActiveIds = 0;

    const GV2ContentCore::FValue& RootVal = Candidate.GetRootValue();
    if (RootVal.IsObject())
    {
        const GV2ContentCore::FValue* Definitions = RootVal.FindField("definitions");
        if (Definitions && Definitions->IsArray())
        {
            for (const GV2ContentCore::FValue& Def : Definitions->AsArray())
            {
                if (Def.IsObject())
                {
                    const GV2ContentCore::FValue* IdField = Def.FindField("id");
                    if (IdField && IdField->IsString())
                    {
                        const std::string& Id = IdField->AsString();
                        GV2ContentCore::FStableIdView IdView;
                        GV2ContentCore::EStableIdError IdError = GV2ContentCore::EStableIdError::None;
                        if (GV2ContentCore::FStableId::Parse(Id, IdView, &IdError))
                        {
                            ActiveByKind[std::string(IdView.Kind)].push_back(Id);
                            TotalActiveIds++;
                        }
                    }
                }
            }
        }
    }

    for (auto& [Kind, Ids] : ActiveByKind)
    {
        std::sort(Ids.begin(), Ids.end());
    }

    std::vector<std::pair<std::string, std::string>> Redirects;
    std::vector<std::string> Tombstones;
    if (Outcome.Descriptor)
    {
        for (const GV2ContentCore::FRedirectDescriptor& R : Outcome.Descriptor->GetRedirects())
        {
            Redirects.emplace_back(R.GetSourceId(), R.GetTargetId());
        }
        std::sort(Redirects.begin(), Redirects.end());

        Tombstones = Outcome.Descriptor->GetTombstones();
        std::sort(Tombstones.begin(), Tombstones.end());
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"package_id\":";
        WriteJsonEscapedString(std::cout, PackageId);
        std::cout << ",\"content_hash\":";
        WriteJsonEscapedString(std::cout, ContentHash);
        std::cout << ",\"total_active_ids\":" << TotalActiveIds;
        std::cout << ",\"kinds_count\":" << ActiveByKind.size();
        std::cout << ",\"kinds\":{";
        bool bFirstKind = true;
        for (const auto& [Kind, Ids] : ActiveByKind)
        {
            if (!bFirstKind)
            {
                std::cout << ",";
            }
            bFirstKind = false;
            WriteJsonEscapedString(std::cout, Kind);
            std::cout << ":[";
            for (std::size_t Idx = 0; Idx < Ids.size(); ++Idx)
            {
                if (Idx > 0)
                {
                    std::cout << ",";
                }
                WriteJsonEscapedString(std::cout, Ids[Idx]);
            }
            std::cout << "]";
        }
        std::cout << "},\"redirects\":[";
        for (std::size_t Idx = 0; Idx < Redirects.size(); ++Idx)
        {
            if (Idx > 0)
            {
                std::cout << ",";
            }
            std::cout << "{\"source_id\":";
            WriteJsonEscapedString(std::cout, Redirects[Idx].first);
            std::cout << ",\"target_id\":";
            WriteJsonEscapedString(std::cout, Redirects[Idx].second);
            std::cout << "}";
        }
        std::cout << "],\"tombstones\":[";
        for (std::size_t Idx = 0; Idx < Tombstones.size(); ++Idx)
        {
            if (Idx > 0)
            {
                std::cout << ",";
            }
            WriteJsonEscapedString(std::cout, Tombstones[Idx]);
        }
        std::cout << "]}\n";
    }
    else
    {
        std::cout << "package_id: " << PackageId << "\n";
        std::cout << "content_hash: " << ContentHash << "\n";
        std::cout << "active_ids: " << TotalActiveIds << "\n";

        for (const auto& [Kind, Ids] : ActiveByKind)
        {
            std::cout << "\n[" << Kind << "] (" << Ids.size() << ")\n";
            for (const std::string& Id : Ids)
            {
                std::cout << "  " << Id << "\n";
            }
        }

        std::cout << "\n[redirects] (" << Redirects.size() << ")\n";
        for (const auto& [SourceId, TargetId] : Redirects)
        {
            std::cout << "  " << SourceId << " -> " << TargetId << "\n";
        }

        std::cout << "\n[tombstones] (" << Tombstones.size() << ")\n";
        for (const std::string& TombstoneId : Tombstones)
        {
            std::cout << "  " << TombstoneId << "\n";
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
