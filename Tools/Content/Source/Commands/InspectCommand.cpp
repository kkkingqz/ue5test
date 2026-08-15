#include "Commands/InspectCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/StableId.h"

#include <iostream>

namespace GV2ContentCli
{

int RunInspect(const std::vector<std::string>& Positional, EOutputFormat Format, bool bProvenance)
{
    if (Positional.size() != 2)
    {
        std::cerr << "usage: gv2-content inspect <package-root> <definition-id> [--provenance] [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::string& RawId = Positional[1];
    const std::optional<GV2ContentCore::FDefinitionId> DefinitionId = GV2ContentCore::FDefinitionId::Parse(RawId);
    if (!DefinitionId)
    {
        std::cerr << "gv2-content: '" << RawId << "' is not a valid definition id\n";
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

    const GV2ContentCore::FRepositoryReadHandle Handle = Result.GetCandidate().GetReadHandle();
    const GV2ContentCore::FRepositoryQueryResult Query = Handle.Require(*DefinitionId);
    if (!Query)
    {
        const GV2ContentCore::FRepositoryQueryError& Error = *Query.Error;
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"not_found\",\"code\":";
            WriteJsonEscapedString(std::cout, Error.Code);
            std::cout << ",\"requested_id\":";
            WriteJsonEscapedString(std::cout, Error.RequestedId);
            if (Error.CanonicalId)
            {
                std::cout << ",\"canonical_id\":";
                WriteJsonEscapedString(std::cout, *Error.CanonicalId);
            }
            std::cout << "}\n";
        }
        else
        {
            std::cout << "not_found " << Error.Code << " " << Error.RequestedId;
            if (Error.CanonicalId)
            {
                std::cout << " (canonical=" << *Error.CanonicalId << ")";
            }
            std::cout << "\n";
        }
        return static_cast<int>(EExitCode::InvalidContent);
    }

    const GV2ContentCore::FDefinitionProvenance* Provenance =
        bProvenance ? Handle.GetProvenance(*DefinitionId) : nullptr;

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"id\":";
        WriteJsonEscapedString(std::cout, DefinitionId->ToString());
        std::cout << ",\"definition\":";
        WriteJsonValue(std::cout, *Query.Definition);
        if (bProvenance)
        {
            std::cout << ",\"provenance\":";
            if (Provenance)
            {
                WriteDefinitionProvenanceJson(std::cout, *Provenance);
            }
            else
            {
                std::cout << "null";
            }
        }
        std::cout << "}\n";
    }
    else
    {
        std::cout << "id: " << DefinitionId->ToString() << "\n";
        std::cout << "definition: ";
        WriteJsonValue(std::cout, *Query.Definition);
        std::cout << "\n";
        if (bProvenance)
        {
            if (Provenance)
            {
                WriteProvenanceText(std::cout, *Provenance);
            }
            else
            {
                std::cout << "provenance: unavailable\n";
            }
        }
    }
    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
