#include "GV2TestSupport/LuaSpecRunner.h"

#include "GV2ContentHostSupport/LuaSpecDiscovery.h"

#include <algorithm>

namespace GV2TestSupport
{
const std::vector<FSubtreeTierConfig>& GetAllSubtreeTierConfigs()
{
    static const std::vector<FSubtreeTierConfig> Configs = {
        // Core Tier (engine only, no textsystem, no rh)
        { "actions", ELuaSpecTier::Core },
        { "actors", ELuaSpecTier::Core },
        { "events", ELuaSpecTier::Core },
        { "lifecycle", ELuaSpecTier::Core },
        { "resources", ELuaSpecTier::Core },
        { "save", ELuaSpecTier::Core },

        // TextSystem Tier (core + textsystem + sample, no rh)
        { "world", ELuaSpecTier::TextSystem },

        // FullGame Tier (core + textsystem + rh)
        { "authoring", ELuaSpecTier::FullGame },
        { "economy", ELuaSpecTier::FullGame },
        { "presentation", ELuaSpecTier::FullGame },

        // Fixture Commands Tier (test-scoped validator fixture session)
        { "commands", ELuaSpecTier::FixtureCommands },
    };
    return Configs;
}

std::optional<ELuaSpecTier> GetSubtreeTier(const std::string& SubtreeName)
{
    for (const auto& Cfg : GetAllSubtreeTierConfigs())
    {
        if (Cfg.SubtreeName == SubtreeName)
        {
            return Cfg.Tier;
        }
    }
    return std::nullopt;
}

std::vector<std::string> GetSubtreesForTier(ELuaSpecTier Tier)
{
    std::vector<std::string> Names;
    for (const auto& Cfg : GetAllSubtreeTierConfigs())
    {
        if (Cfg.Tier == Tier)
        {
            Names.push_back(Cfg.SubtreeName);
        }
    }
    std::sort(Names.begin(), Names.end());
    return Names;
}

std::vector<std::string> GetPackageNamesForTier(ELuaSpecTier Tier)
{
    switch (Tier)
    {
    case ELuaSpecTier::Core:
        return { "core" };
    case ELuaSpecTier::TextSystem:
        return { "core", "textsystem", "sample" };
    case ELuaSpecTier::FullGame:
        return { "core", "textsystem", "rh" };
    case ELuaSpecTier::FixtureCommands:
        return { "core" };
    }
    return { "core" };
}

bool ValidateAllSubtreesRegistered(
    const std::filesystem::path& TestsLuaRoot,
    std::vector<std::string>& OutUnregisteredSubtrees)
{
    OutUnregisteredSubtrees.clear();
    std::error_code Ec;
    if (!std::filesystem::is_directory(TestsLuaRoot, Ec) || Ec)
    {
        return true;
    }

    for (const auto& Entry : std::filesystem::directory_iterator(TestsLuaRoot, Ec))
    {
        if (Ec) break;
        if (!Entry.is_directory(Ec) || Ec)
        {
            continue;
        }
        const std::string Name = Entry.path().filename().string();
        if (!GetSubtreeTier(Name).has_value())
        {
            OutUnregisteredSubtrees.push_back(Name);
        }
    }
    std::sort(OutUnregisteredSubtrees.begin(), OutUnregisteredSubtrees.end());
    return OutUnregisteredSubtrees.empty();
}

const std::vector<std::string>& GetFixtureSessionSubtreeNames()
{
    static const std::vector<std::string> Names = { "commands" };
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
