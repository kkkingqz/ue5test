#include "GV2ContentHostSupport/LocalizationDiscovery.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace GV2ContentHostSupport
{
namespace
{
std::string EscapeCsvField(const std::string& Text)
{
    std::string Escaped = "\"";
    for (const char C : Text)
    {
        if (C == '"')
        {
            Escaped += "\"\"";
        }
        else
        {
            Escaped += C;
        }
    }
    Escaped += "\"";
    return Escaped;
}

std::optional<std::string> ReadFileToString(const std::filesystem::path& FilePath)
{
    std::ifstream Stream(FilePath, std::ios::binary);
    if (!Stream)
    {
        return std::nullopt;
    }
    return std::string(
        (std::istreambuf_iterator<char>(Stream)),
        std::istreambuf_iterator<char>());
}
} // namespace

std::vector<std::string> DiscoverPackageLocales(const std::filesystem::path& PackageRoot)
{
    std::vector<std::string> Locales;
    std::error_code Ec;
    const std::filesystem::path LocDir = PackageRoot / "localization";

    if (!std::filesystem::is_directory(LocDir, Ec) || Ec)
    {
        return Locales;
    }

    for (const auto& Entry : std::filesystem::directory_iterator(LocDir, Ec))
    {
        if (Ec) break;
        if (Entry.is_regular_file(Ec) && Entry.path().extension() == ".po")
        {
            Locales.push_back(Entry.path().stem().string());
        }
    }

    std::sort(Locales.begin(), Locales.end());
    return Locales;
}

std::optional<GV2ContentCore::FPoCatalog> LoadPackageLocalization(
    const std::filesystem::path& PackageRoot,
    const std::string& Locale,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    const std::filesystem::path PoPath = PackageRoot / "localization" / (Locale + ".po");
    const std::optional<std::string> Content = ReadFileToString(PoPath);

    if (!Content)
    {
        GV2ContentCore::FDiagnostic Diag;
        Diag.Code = "core:diagnostic.package.discovery.localization_not_found";
        Diag.Severity = GV2ContentCore::EDiagnosticSeverity::Error;
        Diag.Message = "localization catalog file could not be read: " + PoPath.string();
        Diag.PackageId = PackageRoot.lexically_normal().filename().string();
        Diag.RelativeSource = "localization/" + Locale + ".po";
        OutDiagnostics.push_back(std::move(Diag));
        return std::nullopt;
    }

    GV2ContentCore::FPoParseOptions Options;
    Options.PackageId = PackageRoot.lexically_normal().filename().string();
    Options.RelativeSource = "localization/" + Locale + ".po";

    return GV2ContentCore::ParsePo(*Content, OutDiagnostics, Options);
}

std::string ExportPoToStringTableCsv(const GV2ContentCore::FPoCatalog& Catalog)
{
    std::map<std::string, std::string> SortedRows;

    for (const auto& Entry : Catalog.Entries)
    {
        const std::string& Key = !Entry.MsgCtxt.empty() ? Entry.MsgCtxt : Entry.MsgId;
        if (Key.empty()) continue;

        const std::string& Value = !Entry.MsgStr.empty() ? Entry.MsgStr : Entry.MsgId;
        SortedRows[Key] = Value;
    }

    std::ostringstream Out;
    Out << "Key,SourceString\n";

    for (const auto& [Key, Value] : SortedRows)
    {
        Out << EscapeCsvField(Key) << "," << EscapeCsvField(Value) << "\n";
    }

    return Out.str();
}
} // namespace GV2ContentHostSupport
