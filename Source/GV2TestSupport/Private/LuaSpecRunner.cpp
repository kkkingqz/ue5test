#include "GV2TestSupport/LuaSpecRunner.h"

#include "GV2ContentHostSupport/LuaSpecDiscovery.h"

namespace GV2TestSupport
{
bool RunLuaSpecs(
    const std::filesystem::path& SpecRoot,
    GV2RuntimeCore::FRuntimeSession& Session,
    FLuaSpecRunResult& OutResult)
{
    OutResult = FLuaSpecRunResult{};

    const std::vector<GV2ContentHostSupport::FLuaSpecFile> Files =
        GV2ContentHostSupport::DiscoverLuaSpecFiles(SpecRoot);

    for (const GV2ContentHostSupport::FLuaSpecFile& File : Files)
    {
        const std::string SpecId = GV2ContentHostSupport::DeriveLuaSpecId(File.RelativePath);
        const std::string ChunkName = "@Tests/Lua/" + File.RelativePath;

        std::vector<GV2RuntimeCore::FLuaSpecCaseResult> CaseResults;
        GV2RuntimeCore::FRuntimeFault Fault;
        if (!Session.RunLuaSpec(ChunkName, File.Source, CaseResults, Fault))
        {
            OutResult.Failures.push_back(
                GV2ContentHostSupport::MakeLuaSpecFault(SpecId, Fault.Code, Fault.Message));
            continue;
        }

        ++OutResult.SpecsExecuted;
        for (const GV2RuntimeCore::FLuaSpecCaseResult& CaseResult : CaseResults)
        {
            ++OutResult.CasesExecuted;
            if (!CaseResult.Success)
            {
                OutResult.Failures.push_back(GV2ContentHostSupport::MakeLuaSpecCaseFailure(
                    SpecId, CaseResult.CaseId, CaseResult.ErrorMessage));
            }
        }
    }

    return OutResult.Failures.empty();
}
} // namespace GV2TestSupport
