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

GV2ContentCore::FDiagnostic MakeManifestDiagnostic(
    const std::string& Code,
    std::string Message,
    const std::optional<std::string>& PackageId = std::nullopt)
{
    using namespace GV2ContentCore;
    FDiagnostic Diagnostic;
    Diagnostic.Code = Code;
    Diagnostic.Severity = EDiagnosticSeverity::Error;
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = PackageId;
    Diagnostic.RelativeSource = "package.json5";
    return Diagnostic;
}

// PKG-02: one axis of the manifest's optional "compatibility" object.
// Absent axis is compatible by construction (Done: "отсутствие диапазона
// у core не является ошибкой"). Present axis must be an object with
// integer min <= max, and the current build's version for that axis must
// fall inside [min, max] inclusive.
bool CheckCompatibilityAxis(
    const GV2ContentCore::FValue* CompatibilityObject,
    const char* AxisName,
    std::int64_t CurrentVersion,
    const std::optional<std::string>& PackageId,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    using namespace GV2ContentCore;
    if (CompatibilityObject == nullptr)
    {
        return true;
    }
    const FValue* AxisValue = CompatibilityObject->FindField(AxisName);
    if (AxisValue == nullptr)
    {
        return true;
    }
    const FValue* MinValue = AxisValue->IsObject() ? AxisValue->FindField("min") : nullptr;
    const FValue* MaxValue = AxisValue->IsObject() ? AxisValue->FindField("max") : nullptr;
    if (!AxisValue->IsObject() || MinValue == nullptr || !MinValue->IsInteger()
        || MaxValue == nullptr || !MaxValue->IsInteger() || MinValue->AsInteger() > MaxValue->AsInteger())
    {
        OutDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_compatibility",
            std::string("compatibility.") + AxisName + " must be an object with integer min <= max",
            PackageId));
        return false;
    }

    const std::int64_t Min = MinValue->AsInteger();
    const std::int64_t Max = MaxValue->AsInteger();
    if (CurrentVersion < Min || CurrentVersion > Max)
    {
        OutDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.incompatible_range",
            std::string("compatibility.") + AxisName + " requires [" + std::to_string(Min) + ", "
                + std::to_string(Max) + "], this build is " + std::to_string(CurrentVersion),
            PackageId));
        return false;
    }
    return true;
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
        OutDiagnostics.push_back(std::move(Diagnostic));
        return std::nullopt;
    }

    // PKG-01: the manifest is mandatory and is the sole source of identity
    // — no fallback to a directory-name-derived package_id/namespace.
    const std::filesystem::path PackageDescriptorPath = PackageRoot / "package.json5";
    if (!std::filesystem::exists(PackageDescriptorPath, Ec))
    {
        OutDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.missing",
            "package root has no package.json5"));
        return std::nullopt;
    }

    const std::optional<std::string> ManifestContent = ReadFileToString(PackageDescriptorPath);
    if (!ManifestContent)
    {
        OutDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.unreadable",
            "package.json5 could not be read"));
        return std::nullopt;
    }

    const FParseLimits Limits;
    std::vector<FDiagnostic> ManifestParseDiagnostics;
    // PackageId is not known yet here (that is what the manifest is about
    // to tell us), so parse diagnostics carry no PackageId context.
    const std::optional<FValue> ParsedManifest = ParseJson5(
        *ManifestContent, Limits, ManifestParseDiagnostics, std::nullopt, 0u, "package.json5");
    if (!ParsedManifest)
    {
        OutDiagnostics.insert(
            OutDiagnostics.end(), ManifestParseDiagnostics.begin(), ManifestParseDiagnostics.end());
        return std::nullopt;
    }
    if (!ParsedManifest->IsObject())
    {
        OutDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid",
            "package.json5 must be a JSON5 object"));
        return std::nullopt;
    }

    const FValue* PackageIdField = ParsedManifest->FindField("package_id");
    if (PackageIdField == nullptr || !PackageIdField->IsString())
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_package_id",
            "package.json5 must declare a string 'package_id'"));
    }
    else if (!FStableId::IsValidSegment(PackageIdField->AsString()))
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_package_id",
            "package_id is not a canonical lowercase ASCII segment: " + PackageIdField->AsString()));
    }
    const std::optional<std::string> PackageId = (PackageIdField && PackageIdField->IsString())
        ? std::optional<std::string>(PackageIdField->AsString())
        : std::nullopt;

    const FValue* NamespaceField = ParsedManifest->FindField("namespace");
    if (NamespaceField == nullptr || !NamespaceField->IsString())
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_namespace",
            "package.json5 must declare a string 'namespace'", PackageId));
    }
    else if (!FStableId::IsValidSegment(NamespaceField->AsString()))
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_namespace",
            "namespace is not a canonical lowercase ASCII segment: " + NamespaceField->AsString(), PackageId));
    }
    else if (PackageId && NamespaceField->AsString() != *PackageId)
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.namespace_mismatch",
            "namespace must equal package_id (single-package-per-namespace v1 rule): '"
                + NamespaceField->AsString() + "' != '" + *PackageId + "'",
            PackageId));
    }

    const FValue* VersionField = ParsedManifest->FindField("version");
    static const std::string VersionGrammarMessage =
        "version must be a string matching <major>.<minor>.<patch> (non-negative integers)";
    if (VersionField == nullptr || !VersionField->IsString())
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_version", VersionGrammarMessage, PackageId));
    }
    else
    {
        const std::string& VersionText = VersionField->AsString();
        const auto IsDigits = [](std::string_view Segment)
        {
            return !Segment.empty() && Segment.find_first_not_of("0123456789") == std::string_view::npos;
        };
        const std::size_t FirstDot = VersionText.find('.');
        const std::size_t SecondDot = FirstDot == std::string::npos
            ? std::string::npos
            : VersionText.find('.', FirstDot + 1);
        const bool bValidGrammar = FirstDot != std::string::npos && SecondDot != std::string::npos
            && VersionText.find('.', SecondDot + 1) == std::string::npos
            && IsDigits(std::string_view(VersionText).substr(0, FirstDot))
            && IsDigits(std::string_view(VersionText).substr(FirstDot + 1, SecondDot - FirstDot - 1))
            && IsDigits(std::string_view(VersionText).substr(SecondDot + 1));
        if (!bValidGrammar)
        {
            LocalDiagnostics.push_back(MakeManifestDiagnostic(
                "core:diagnostic.package.manifest.invalid_version", VersionGrammarMessage, PackageId));
        }
    }

    // PKG-02: compatibility ranges — checked here, before any definitions/
    // schemas are read, regardless of whether identity above was valid
    // (both classes of manifest problem are reported together).
    const FValue* CompatibilityField = ParsedManifest->FindField("compatibility");
    if (CompatibilityField != nullptr && !CompatibilityField->IsObject())
    {
        LocalDiagnostics.push_back(MakeManifestDiagnostic(
            "core:diagnostic.package.manifest.invalid_compatibility",
            "compatibility must be an object", PackageId));
    }
    else
    {
        CheckCompatibilityAxis(CompatibilityField, "game", CurrentGameVersion, PackageId, LocalDiagnostics);
        CheckCompatibilityAxis(CompatibilityField, "api", CurrentApiVersion, PackageId, LocalDiagnostics);
        CheckCompatibilityAxis(CompatibilityField, "schema", CurrentSchemaVersion, PackageId, LocalDiagnostics);
    }

    // PKG-03: dependencies — parsed and form-validated only here; presence
    // in an actual package set and cycles are M2's job (Discovery and Order).
    std::vector<FPackageDependency> Dependencies;
    const FValue* DependenciesField = ParsedManifest->FindField("dependencies");
    if (DependenciesField != nullptr)
    {
        if (!DependenciesField->IsArray())
        {
            LocalDiagnostics.push_back(MakeManifestDiagnostic(
                "core:diagnostic.package.manifest.invalid_dependency",
                "dependencies must be an array", PackageId));
        }
        else
        {
            for (const FValue& Entry : DependenciesField->AsArray())
            {
                const FValue* EntryPackageId = Entry.IsObject() ? Entry.FindField("package_id") : nullptr;
                if (EntryPackageId == nullptr || !EntryPackageId->IsString()
                    || !FStableId::IsValidSegment(EntryPackageId->AsString()))
                {
                    LocalDiagnostics.push_back(MakeManifestDiagnostic(
                        "core:diagnostic.package.manifest.invalid_dependency",
                        "each dependencies[] entry must be an object with a canonical string package_id",
                        PackageId));
                    continue;
                }
                const FValue* LoadAfterField = Entry.FindField("load_after");
                if (LoadAfterField != nullptr && !LoadAfterField->IsBoolean())
                {
                    LocalDiagnostics.push_back(MakeManifestDiagnostic(
                        "core:diagnostic.package.manifest.invalid_dependency",
                        "dependencies[].load_after must be a boolean when present",
                        PackageId));
                    continue;
                }
                Dependencies.emplace_back(
                    EntryPackageId->AsString(),
                    LoadAfterField != nullptr && LoadAfterField->AsBoolean());
            }
        }
    }

    // PKG-01/02/03 done: reject before reading definitions/schemas at all.
    if (!LocalDiagnostics.empty())
    {
        std::sort(LocalDiagnostics.begin(), LocalDiagnostics.end());
        OutDiagnostics.insert(
            OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
        return std::nullopt;
    }

    const std::string ResolvedPackageId = *PackageId;

    std::vector<std::string> RelativeSources;
    for (const auto& Path : ListSortedJson5Files(PackageRoot / "definitions"))
    {
        RelativeSources.push_back("definitions/" + Path.filename().string());
    }

    std::vector<FSchemaBinding> SchemaBindings;
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
            Diagnostic.PackageId = ResolvedPackageId;
            Diagnostic.RelativeSource = RelativePath;
            LocalDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        std::vector<FDiagnostic> ParseDiagnostics;
        const std::optional<FValue> Parsed = ParseJson5(
            *Content, Limits, ParseDiagnostics, ResolvedPackageId, 0u, RelativePath);
        if (!Parsed)
        {
            LocalDiagnostics.insert(
                LocalDiagnostics.end(), ParseDiagnostics.begin(), ParseDiagnostics.end());
            continue;
        }

        const FValue* IdField = Parsed->IsObject() ? Parsed->FindField("id") : nullptr;
        const FValue* TypeField = Parsed->IsObject() ? Parsed->FindField("definition_type") : nullptr;
        const FValue* SchemaVersionField = Parsed->IsObject() ? Parsed->FindField("schema_version") : nullptr;
        if (!IdField || !IdField->IsString()
            || !TypeField || !TypeField->IsString()
            || !SchemaVersionField || !SchemaVersionField->IsInteger())
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.package.discovery.schema_missing_fields";
            Diagnostic.Severity = EDiagnosticSeverity::Error;
            Diagnostic.Message =
                "schema resource must declare a string 'id', string 'definition_type' and integer 'schema_version'";
            Diagnostic.PackageId = ResolvedPackageId;
            Diagnostic.RelativeSource = RelativePath;
            LocalDiagnostics.push_back(std::move(Diagnostic));
            continue;
        }

        SchemaBindings.emplace_back(
            TypeField->AsString(), SchemaVersionField->AsInteger(), IdField->AsString(), RelativePath);
    }

    std::vector<FRedirectDescriptor> Redirects;
    std::vector<std::string> Tombstones;
    std::vector<FExtensionSchemaBinding> ExtensionSchemaBindings;

    const FValue* RedirectsVal = ParsedManifest->FindField("redirects");
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

    const FValue* TombstonesVal = ParsedManifest->FindField("tombstones");
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

    if (!LocalDiagnostics.empty())
    {
        std::sort(LocalDiagnostics.begin(), LocalDiagnostics.end());
        OutDiagnostics.insert(
            OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
        return std::nullopt;
    }

    return FPackageDescriptor(
        ResolvedPackageId,
        ResolvedPackageId,
        0u,
        std::move(RelativeSources),
        std::move(SchemaBindings),
        std::move(ExtensionSchemaBindings),
        std::move(Redirects),
        std::move(Tombstones),
        VersionField->AsString(),
        std::move(Dependencies));
}

std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> DiscoverPackagesFromDirectories(
    const std::vector<std::filesystem::path>& PackageRoots,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    using namespace GV2ContentCore;
    std::vector<FDiagnostic> LocalDiagnostics;
    std::vector<FPackageDescriptor> Descriptors;
    Descriptors.reserve(PackageRoots.size());

    std::map<std::string, std::filesystem::path> SeenPackageRoots;

    for (std::size_t Index = 0; Index < PackageRoots.size(); ++Index)
    {
        const std::filesystem::path& Root = PackageRoots[Index];
        std::vector<FDiagnostic> SingleDiagnostics;
        std::optional<FPackageDescriptor> Discovered = DiscoverPackageFromDirectory(Root, SingleDiagnostics);
        if (!Discovered)
        {
            LocalDiagnostics.insert(LocalDiagnostics.end(), SingleDiagnostics.begin(), SingleDiagnostics.end());
            continue;
        }

        const std::string& PackageId = Discovered->GetPackageId();
        const auto PrevIt = SeenPackageRoots.find(PackageId);
        if (PrevIt != SeenPackageRoots.end())
        {
            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.package.discovery.duplicate_package_id";
            Diagnostic.Severity = EDiagnosticSeverity::Error;
            Diagnostic.Message = "Duplicate package_id '" + PackageId + "' discovered in '"
                + PrevIt->second.string() + "' and '" + Root.string() + "'";
            Diagnostic.PackageId = PackageId;
            LocalDiagnostics.push_back(std::move(Diagnostic));
        }
        else
        {
            SeenPackageRoots.emplace(PackageId, Root);
        }

        Descriptors.push_back(Discovered->WithLoadIndex(static_cast<std::uint32_t>(Index)));
    }

    if (!LocalDiagnostics.empty())
    {
        std::sort(LocalDiagnostics.begin(), LocalDiagnostics.end());
        OutDiagnostics.insert(OutDiagnostics.end(), LocalDiagnostics.begin(), LocalDiagnostics.end());
        return std::nullopt;
    }

    std::vector<FDiagnostic> ValidationDiagnostics = ValidatePackageDescriptors(Descriptors);
    if (!ValidationDiagnostics.empty())
    {
        std::sort(ValidationDiagnostics.begin(), ValidationDiagnostics.end());
        OutDiagnostics.insert(OutDiagnostics.end(), ValidationDiagnostics.begin(), ValidationDiagnostics.end());
        return std::nullopt;
    }

    return Descriptors;
}

std::vector<FDiscoveredScriptSource> DiscoverPackageScripts(
    const std::filesystem::path& PackageRoot,
    const std::string& PackageId)
{
    std::vector<FDiscoveredScriptSource> Results;
    std::vector<std::filesystem::path> CandidateDirs = {
        PackageRoot / "scripts",
        PackageRoot / "Scripts"
    };

    if (PackageId == "core")
    {
        CandidateDirs.push_back(PackageRoot / ".." / ".." / "Scripts");
        CandidateDirs.push_back(PackageRoot / ".." / "Scripts");
        CandidateDirs.push_back(std::filesystem::current_path() / "Scripts");
        CandidateDirs.push_back(std::filesystem::current_path() / ".." / "Scripts");
    }

    std::filesystem::path ScriptsDir;
    std::error_code Ec;
    for (const auto& Candidate : CandidateDirs)
    {
        if (std::filesystem::is_directory(Candidate, Ec) && !Ec)
        {
            ScriptsDir = Candidate;
            break;
        }
    }

    if (ScriptsDir.empty())
    {
        return Results;
    }

    std::vector<std::filesystem::path> SourcePaths;
    for (std::filesystem::recursive_directory_iterator It(ScriptsDir, Ec), End;
         !Ec && It != End;
         It.increment(Ec))
    {
        if (It->is_regular_file(Ec) && It->path().extension() == ".lua")
        {
            SourcePaths.push_back(It->path());
        }
    }

    std::sort(SourcePaths.begin(), SourcePaths.end());

    for (const auto& Path : SourcePaths)
    {
        std::ifstream Stream(Path, std::ios::binary);
        if (!Stream)
        {
            continue;
        }
        std::string Text{
            (std::istreambuf_iterator<char>(Stream)),
            std::istreambuf_iterator<char>()};
        if (Text.starts_with("\xef\xbb\xbf"))
        {
            Text.erase(0, 3);
        }
        std::string Relative = std::filesystem::relative(Path, ScriptsDir).generic_string();
        Results.push_back({"@" + PackageId + "/" + Relative, std::move(Text)});
    }

    return Results;
}

std::vector<FDiscoveredScriptSource> DiscoverPackagesScripts(
    const std::vector<std::pair<std::string, std::filesystem::path>>& Packages)
{
    std::vector<FDiscoveredScriptSource> AllSources;
    for (const auto& [PkgId, PkgRoot] : Packages)
    {
        auto PkgSources = DiscoverPackageScripts(PkgRoot, PkgId);
        AllSources.insert(AllSources.end(),
            std::make_move_iterator(PkgSources.begin()),
            std::make_move_iterator(PkgSources.end()));
    }
    return AllSources;
}

void FMultiPackageSourceProvider::RegisterPackage(std::string PackageId, std::filesystem::path PackageRoot)
{
    PackageRoots.insert_or_assign(std::move(PackageId), std::move(PackageRoot));
}

std::optional<std::string> FMultiPackageSourceProvider::ReadSource(
    const std::string_view RequestedPackageId,
    const std::string_view RelativeSource) const
{
    const auto It = PackageRoots.find(RequestedPackageId);
    if (It == PackageRoots.end())
    {
        return std::nullopt;
    }
    const std::filesystem::path FilePath = It->second / std::string(RelativeSource);
    return ReadFileToString(FilePath);
}
} // namespace GV2ContentHostSupport
