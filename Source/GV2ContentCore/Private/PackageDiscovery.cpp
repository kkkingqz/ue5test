#include "GV2ContentCore/PackageDiscovery.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <fstream>

namespace GV2ContentCore
{
namespace
{
std::vector<std::filesystem::path> ListSortedJson5Files(const std::filesystem::path& Directory)
{
    std::vector<std::filesystem::path> Entries;
    std::error_code Ec;
    if (!std::filesystem::is_directory(Directory, Ec) || Ec)
    {
        return Entries;
    }

    for (const auto& Entry : std::filesystem::directory_iterator(Directory, Ec))
    {
        if (Ec) break;
        if (Entry.is_regular_file(Ec) && Entry.path().extension() == ".json5")
        {
            Entries.push_back(Entry.path());
        }
    }
    std::sort(Entries.begin(), Entries.end());
    return Entries;
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

std::optional<FPackageDescriptor> DiscoverPackageFromDirectory(
    const std::filesystem::path& PackageRoot,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    std::error_code Ec;
    if (!std::filesystem::is_directory(PackageRoot, Ec) || Ec)
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.package.discovery.root_not_found";
        Diagnostic.Severity = EDiagnosticSeverity::Error;
        Diagnostic.Message = "package root not found or not a directory";
        OutDiagnostics.push_back(std::move(Diagnostic));
        return std::nullopt;
    }

    const std::filesystem::path Normalized = PackageRoot.lexically_normal();
    std::string PackageId = Normalized.filename().string();
    if (PackageId.empty())
    {
        PackageId = Normalized.parent_path().filename().string();
    }
    if (PackageId.empty() || !FStableId::IsValidSegment(PackageId))
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.package.discovery.invalid_package_id";
        Diagnostic.Severity = EDiagnosticSeverity::Error;
        Diagnostic.Message = "derived package id is not a valid lowercase ASCII segment";
        OutDiagnostics.push_back(std::move(Diagnostic));
        return std::nullopt;
    }

    std::vector<std::string> RelativeSources;
    for (const auto& Path : ListSortedJson5Files(PackageRoot / "definitions"))
    {
        RelativeSources.push_back("definitions/" + Path.filename().string());
    }

    std::vector<FSchemaBinding> SchemaBindings;
    const FParseLimits Limits;
    for (const auto& Path : ListSortedJson5Files(PackageRoot / "schemas"))
    {
        const std::string RelativePath = "schemas/" + Path.filename().string();
        const std::optional<std::string> Content = ReadFileToString(Path);
        if (!Content)
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.package.discovery.schema_unreadable";
            Diagnostic.Severity = EDiagnosticSeverity::Error;
            Diagnostic.Message = "schema resource could not be read";
            Diagnostic.PackageId = PackageId;
            Diagnostic.RelativeSource = RelativePath;
            OutDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        std::vector<FDiagnostic> ParseDiagnostics;
        const std::optional<FValue> Parsed = ParseJson5(
            *Content, Limits, ParseDiagnostics, PackageId, 0u, RelativePath);
        if (!Parsed)
        {
            OutDiagnostics.insert(
                OutDiagnostics.end(), ParseDiagnostics.begin(), ParseDiagnostics.end());
            continue;
        }

        const FValue* IdField = Parsed->IsObject() ? Parsed->FindField("id") : nullptr;
        const FValue* TypeField = Parsed->IsObject() ? Parsed->FindField("definition_type") : nullptr;
        const FValue* VersionField = Parsed->IsObject() ? Parsed->FindField("schema_version") : nullptr;
        if (!IdField || !IdField->IsString()
            || !TypeField || !TypeField->IsString()
            || !VersionField || !VersionField->IsInteger())
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.package.discovery.schema_missing_fields";
            Diagnostic.Severity = EDiagnosticSeverity::Error;
            Diagnostic.Message =
                "schema resource must declare a string 'id', string 'definition_type' and integer 'schema_version'";
            Diagnostic.PackageId = PackageId;
            Diagnostic.RelativeSource = RelativePath;
            OutDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        SchemaBindings.emplace_back(
            TypeField->AsString(), VersionField->AsInteger(), IdField->AsString(), RelativePath);
    }

    if (!OutDiagnostics.empty())
    {
        std::sort(OutDiagnostics.begin(), OutDiagnostics.end());
        return std::nullopt;
    }

    return FPackageDescriptor(
        PackageId, PackageId, 0u, std::move(RelativeSources), std::move(SchemaBindings));
}
} // namespace GV2ContentCore
