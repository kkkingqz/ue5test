#include "Commands/CoverageCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/LocalizationDiscovery.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace GV2ContentCli
{

namespace
{

struct FLocaleStats
{
    std::string Locale;
    std::size_t TotalDefinitions = 0;
    std::vector<std::string> TranslatedKeys;
    std::vector<std::string> EmptyKeys;
    std::vector<std::string> MissingKeys;
    std::vector<std::string> ExtraKeys;
};

} // namespace

int RunCoverage(
    const std::vector<std::string>& Positional,
    EOutputFormat Format,
    const std::string& SpecificLocale)
{
    if (Positional.empty())
    {
        std::cerr << "gv2-content: 'coverage' requires a package root path\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }
    if (Positional.size() > 1)
    {
        std::cerr << "gv2-content: 'coverage' takes only one positional argument (package-root)\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path PackagePath(Positional[0]);
    FRootBuildOutcome Outcome = BuildFromPackageRoot(PackagePath);
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

    const GV2ContentCore::FBuildResult& BuildRes = *Outcome.Result;
    if (BuildRes.IsFailure())
    {
        return EmitDiagnosticsFailure(BuildRes.GetDiagnostics(), Format);
    }

    const auto ReadHandle = BuildRes.GetCandidate().GetReadHandle();
    const auto TextIds = ReadHandle.List("text");
    const std::string PackageId = Outcome.Descriptor ? Outcome.Descriptor->GetPackageId() : "";
    std::set<std::string> DefinedTextIds;
    for (const auto& DefId : TextIds)
    {
        GV2ContentCore::FStableIdView View;
        if (GV2ContentCore::FStableId::Parse(DefId.ToString(), View))
        {
            if (PackageId.empty() || View.Namespace == PackageId)
            {
                DefinedTextIds.insert(DefId.ToString());
            }
        }
    }

    std::vector<std::string> Locales;
    if (!SpecificLocale.empty())
    {
        Locales.push_back(SpecificLocale);
    }
    else
    {
        Locales = GV2ContentHostSupport::DiscoverPackageLocales(PackagePath);
    }

    std::vector<FLocaleStats> AllStats;
    for (const std::string& Loc : Locales)
    {
        FLocaleStats Stats;
        Stats.Locale = Loc;
        Stats.TotalDefinitions = DefinedTextIds.size();

        std::vector<GV2ContentCore::FDiagnostic> LoadDiags;
        auto CatalogOpt = GV2ContentHostSupport::LoadPackageLocalization(PackagePath, Loc, LoadDiags);
        if (!CatalogOpt)
        {
            for (const auto& TextId : DefinedTextIds)
            {
                Stats.MissingKeys.push_back(TextId);
            }
        }
        else
        {
            for (const auto& Entry : CatalogOpt->Entries)
            {
                const std::string& Key = !Entry.MsgCtxt.empty() ? Entry.MsgCtxt : Entry.MsgId;
                if (Key.empty()) continue;
                if (!DefinedTextIds.contains(Key))
                {
                    Stats.ExtraKeys.push_back(Key);
                }
            }

            for (const auto& TextId : DefinedTextIds)
            {
                const auto* Entry = CatalogOpt->FindByContext(TextId);
                if (!Entry) Entry = CatalogOpt->FindById(TextId);

                if (!Entry)
                {
                    Stats.MissingKeys.push_back(TextId);
                }
                else if (Entry->MsgStr.empty())
                {
                    Stats.EmptyKeys.push_back(TextId);
                }
                else
                {
                    Stats.TranslatedKeys.push_back(TextId);
                }
            }
        }

        std::sort(Stats.TranslatedKeys.begin(), Stats.TranslatedKeys.end());
        std::sort(Stats.EmptyKeys.begin(), Stats.EmptyKeys.end());
        std::sort(Stats.MissingKeys.begin(), Stats.MissingKeys.end());
        std::sort(Stats.ExtraKeys.begin(), Stats.ExtraKeys.end());
        AllStats.push_back(std::move(Stats));
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"package_id\":";
        WriteJsonEscapedString(std::cout, PackageId);
        std::cout << ",\"locales\":{";
        for (std::size_t Idx = 0; Idx < AllStats.size(); ++Idx)
        {
            if (Idx > 0) std::cout << ",";
            const auto& St = AllStats[Idx];
            const double Pct = St.TotalDefinitions > 0
                ? (static_cast<double>(St.TranslatedKeys.size()) / static_cast<double>(St.TotalDefinitions) * 100.0)
                : 100.0;
            WriteJsonEscapedString(std::cout, St.Locale);
            std::cout << ":{\"total_definitions\":" << St.TotalDefinitions
                      << ",\"translated_count\":" << St.TranslatedKeys.size()
                      << ",\"empty_count\":" << St.EmptyKeys.size()
                      << ",\"missing_count\":" << St.MissingKeys.size()
                      << ",\"extra_count\":" << St.ExtraKeys.size()
                      << ",\"coverage_percentage\":" << Pct
                      << ",\"translated_keys\":[";
            for (std::size_t K = 0; K < St.TranslatedKeys.size(); ++K)
            {
                if (K > 0) std::cout << ",";
                WriteJsonEscapedString(std::cout, St.TranslatedKeys[K]);
            }
            std::cout << "],\"empty_keys\":[";
            for (std::size_t K = 0; K < St.EmptyKeys.size(); ++K)
            {
                if (K > 0) std::cout << ",";
                WriteJsonEscapedString(std::cout, St.EmptyKeys[K]);
            }
            std::cout << "],\"missing_keys\":[";
            for (std::size_t K = 0; K < St.MissingKeys.size(); ++K)
            {
                if (K > 0) std::cout << ",";
                WriteJsonEscapedString(std::cout, St.MissingKeys[K]);
            }
            std::cout << "],\"extra_keys\":[";
            for (std::size_t K = 0; K < St.ExtraKeys.size(); ++K)
            {
                if (K > 0) std::cout << ",";
                WriteJsonEscapedString(std::cout, St.ExtraKeys[K]);
            }
            std::cout << "]}";
        }
        std::cout << "}}\n";
    }
    else
    {
        std::cout << "package_id: " << PackageId << "\n";
        if (AllStats.empty())
        {
            std::cout << "No localization catalogs found.\n";
        }
        for (const auto& St : AllStats)
        {
            const double Pct = St.TotalDefinitions > 0
                ? (static_cast<double>(St.TranslatedKeys.size()) / static_cast<double>(St.TotalDefinitions) * 100.0)
                : 100.0;
            std::cout << "\nlocale: " << St.Locale << "\n";
            std::cout << "  total_definitions: " << St.TotalDefinitions << "\n";
            std::cout << "  translated: " << St.TranslatedKeys.size() << " (" << Pct << "%)\n";
            std::cout << "  empty: " << St.EmptyKeys.size() << "\n";
            for (const auto& K : St.EmptyKeys)
            {
                std::cout << "    - " << K << "\n";
            }
            std::cout << "  missing: " << St.MissingKeys.size() << "\n";
            for (const auto& K : St.MissingKeys)
            {
                std::cout << "    - " << K << "\n";
            }
            std::cout << "  extra: " << St.ExtraKeys.size() << "\n";
            for (const auto& K : St.ExtraKeys)
            {
                std::cout << "    - " << K << "\n";
            }
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
