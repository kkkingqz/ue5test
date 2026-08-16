#include "GV2TestSupport/LuaSpecRunner.h"

#include "GV2ContentHostSupport/LuaSpecDiscovery.h"

#include <algorithm>

namespace GV2TestSupport
{
const std::vector<std::string>& GetFixtureSessionSubtreeNames()
{
    // TAS-13: Tests/Lua/commands needs test-scoped validators registered
    // before the global registry freezes, so it cannot run on the shared
    // production session (which is already frozen and empty by the time
    // any spec runs on it).
    static const std::vector<std::string> Names = {"commands"};
    return Names;
}

std::vector<std::string> DiscoverProductionSessionSubtreeNames(const std::filesystem::path& TestsLuaRoot)
{
    std::vector<std::string> Names;
    std::error_code Ec;
    if (!std::filesystem::is_directory(TestsLuaRoot, Ec) || Ec)
    {
        return Names;
    }

    const std::vector<std::string>& FixtureNames = GetFixtureSessionSubtreeNames();
    for (const auto& Entry : std::filesystem::directory_iterator(TestsLuaRoot, Ec))
    {
        if (Ec) break;
        if (!Entry.is_directory(Ec) || Ec)
        {
            continue;
        }
        const std::string Name = Entry.path().filename().string();
        if (std::find(FixtureNames.begin(), FixtureNames.end(), Name) != FixtureNames.end())
        {
            continue;
        }
        Names.push_back(Name);
    }
    std::sort(Names.begin(), Names.end());
    return Names;
}

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
