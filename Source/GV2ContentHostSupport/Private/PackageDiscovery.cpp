#include "GV2ContentHostSupport/PackageDiscovery.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <fstream>

namespace GV2ContentHostSupport
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

std::optional<GV2ContentCore::FPackageDescriptor> DiscoverPackageFromDirectory(
    const std::filesystem::path& PackageRoot,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    using namespace GV2ContentCore;
    std::vector<FDiagnostic> LocalDiagnostics;

    std::error_code Ec;
    if (!std::filesystem::is_directory(PackageRoot, Ec) || Ec)
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.package.discovery.root_not_found";
        Diagnostic.Severity = EDiagnosticSeverity::Error;
        Diagnostic.Message = "package root not found or not a directory";
        LocalDiagnostics.push_back(std::move(Diagnostic));
        OutDiagnostics.insert(
            OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
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
        LocalDiagnostics.push_back(std::move(Diagnostic));
        OutDiagnostics.insert(
            OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
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
            LocalDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        std::vector<FDiagnostic> ParseDiagnostics;
        const std::optional<FValue> Parsed = ParseJson5(
            *Content, Limits, ParseDiagnostics, PackageId, 0u, RelativePath);
        if (!Parsed)
        {
            LocalDiagnostics.insert(
                LocalDiagnostics.end(), ParseDiagnostics.begin(), ParseDiagnostics.end());
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
            LocalDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        SchemaBindings.emplace_back(
            TypeField->AsString(), VersionField->AsInteger(), IdField->AsString(), RelativePath);
    }

    std::vector<FRedirectDescriptor> Redirects;
    std::vector<std::string> Tombstones;
    std::vector<FExtensionSchemaBinding> ExtensionSchemaBindings;

    const std::filesystem::path PackageDescriptorPath = PackageRoot / "package.json5";
    if (std::filesystem::exists(PackageDescriptorPath))
    {
        const std::optional<std::string> PkgContent = ReadFileToString(PackageDescriptorPath);
        if (PkgContent)
        {
            std::vector<FDiagnostic> PkgDiagnostics;
            const auto ParsedPkg = ParseJson5(
                *PkgContent, Limits, PkgDiagnostics, PackageId, 0u, "package.json5");
            if (ParsedPkg && ParsedPkg->IsObject())
            {
                const FValue* RedirectsVal = ParsedPkg->FindField("redirects");
                if (RedirectsVal && RedirectsVal->IsObject())
                {
                    for (const auto& [SourceId, TargetIdVal] : RedirectsVal->AsObject())
                    {
                        if (TargetIdVal.IsString())
                        {
                            Redirects.emplace_back(SourceId, TargetIdVal.AsString());
                        }
                    }
                }

                const FValue* TombstonesVal = ParsedPkg->FindField("tombstones");
                if (TombstonesVal && TombstonesVal->IsArray())
                {
                    for (const FValue& TombstoneVal : TombstonesVal->AsArray())
                    {
                        if (TombstoneVal.IsString())
                        {
                            Tombstones.push_back(TombstoneVal.AsString());
                        }
                    }
                }
            }
            else if (!PkgDiagnostics.empty())
            {
                LocalDiagnostics.insert(
                    LocalDiagnostics.end(), PkgDiagnostics.begin(), PkgDiagnostics.end());
            }
        }
    }

    if (!LocalDiagnostics.empty())
    {
        std::sort(LocalDiagnostics.begin(), LocalDiagnostics.end());
        OutDiagnostics.insert(
            OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
        return std::nullopt;
    }

    return FPackageDescriptor(
        PackageId,
        PackageId,
        0u,
        std::move(RelativeSources),
        std::move(SchemaBindings),
        std::move(ExtensionSchemaBindings),
        std::move(Redirects),
        std::move(Tombstones));
}
} // namespace GV2ContentHostSupport
