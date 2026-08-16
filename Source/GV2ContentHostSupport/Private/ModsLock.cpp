#include "GV2ContentHostSupport/ModsLock.h"

#include "GV2ContentCore/CanonicalHash.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/Value.h"

#include <algorithm>
#include <sstream>

namespace GV2ContentHostSupport
{
namespace
{
GV2ContentCore::FDiagnostic MakeLockDiagnostic(
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
    Diagnostic.RelativeSource = "mods.lock.json5";
    return Diagnostic;
}
} // namespace

std::string ComputePackageFingerprint(const GV2ContentCore::FPackageDescriptor& Descriptor)
{
    using namespace GV2ContentCore;

    std::vector<std::pair<std::string, FValue>> Fields;
    Fields.emplace_back("package_id", FValue::MakeString(Descriptor.GetPackageId()));
    Fields.emplace_back("namespace", FValue::MakeString(Descriptor.GetNamespace()));
    Fields.emplace_back("version", FValue::MakeString(Descriptor.GetVersion()));
    Fields.emplace_back("load_index", FValue::MakeInteger(static_cast<std::int64_t>(Descriptor.GetLoadIndex())));

    std::vector<FValue> SourcesArray;
    for (const std::string& Source : Descriptor.GetRelativeSources())
    {
        SourcesArray.push_back(FValue::MakeString(Source));
    }
    Fields.emplace_back("relative_sources", FValue::MakeArray(std::move(SourcesArray)));

    std::vector<FValue> SchemasArray;
    for (const FSchemaBinding& Binding : Descriptor.GetSchemaBindings())
    {
        std::vector<std::pair<std::string, FValue>> BindingFields;
        BindingFields.emplace_back("definition_type", FValue::MakeString(Binding.GetDefinitionType()));
        BindingFields.emplace_back("schema_version", FValue::MakeInteger(Binding.GetSchemaVersion()));
        BindingFields.emplace_back("schema_id", FValue::MakeString(Binding.GetSchemaId()));
        BindingFields.emplace_back("relative_path", FValue::MakeString(Binding.GetRelativePath()));
        SchemasArray.push_back(FValue::MakeObject(std::move(BindingFields)));
    }
    Fields.emplace_back("schema_bindings", FValue::MakeArray(std::move(SchemasArray)));

    std::vector<FValue> ExtensionSchemasArray;
    for (const FExtensionSchemaBinding& Binding : Descriptor.GetExtensionSchemaBindings())
    {
        std::vector<std::pair<std::string, FValue>> BindingFields;
        BindingFields.emplace_back("definition_type", FValue::MakeString(Binding.GetDefinitionType()));
        BindingFields.emplace_back("schema_version", FValue::MakeInteger(Binding.GetSchemaVersion()));
        BindingFields.emplace_back("extension_site", FValue::MakeString(Binding.GetExtensionSite()));
        BindingFields.emplace_back("extension_namespace", FValue::MakeString(Binding.GetExtensionNamespace()));
        BindingFields.emplace_back("schema_id", FValue::MakeString(Binding.GetSchemaId()));
        BindingFields.emplace_back("relative_path", FValue::MakeString(Binding.GetRelativePath()));
        ExtensionSchemasArray.push_back(FValue::MakeObject(std::move(BindingFields)));
    }
    Fields.emplace_back("extension_schema_bindings", FValue::MakeArray(std::move(ExtensionSchemasArray)));

    std::vector<FValue> RedirectsArray;
    for (const FRedirectDescriptor& Redirect : Descriptor.GetRedirects())
    {
        std::vector<std::pair<std::string, FValue>> RedirectFields;
        RedirectFields.emplace_back("source_id", FValue::MakeString(Redirect.GetSourceId()));
        RedirectFields.emplace_back("target_id", FValue::MakeString(Redirect.GetTargetId()));
        RedirectsArray.push_back(FValue::MakeObject(std::move(RedirectFields)));
    }
    Fields.emplace_back("redirects", FValue::MakeArray(std::move(RedirectsArray)));

    std::vector<FValue> TombstonesArray;
    for (const std::string& Tombstone : Descriptor.GetTombstones())
    {
        TombstonesArray.push_back(FValue::MakeString(Tombstone));
    }
    Fields.emplace_back("tombstones", FValue::MakeArray(std::move(TombstonesArray)));

    std::vector<FValue> DependenciesArray;
    for (const FPackageDependency& Dep : Descriptor.GetDependencies())
    {
        std::vector<std::pair<std::string, FValue>> DepFields;
        DepFields.emplace_back("package_id", FValue::MakeString(Dep.GetPackageId()));
        DepFields.emplace_back("load_after", FValue::MakeBoolean(Dep.GetLoadAfter()));
        DependenciesArray.push_back(FValue::MakeObject(std::move(DepFields)));
    }
    Fields.emplace_back("dependencies", FValue::MakeArray(std::move(DependenciesArray)));

    return ComputeCanonicalHash(FValue::MakeObject(std::move(Fields)));
}

std::string GenerateModsLockContent(const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors)
{
    std::ostringstream Output;
    Output << "{\n";
    Output << "  schema_version: 1,\n";
    Output << "  packages: [\n";

    for (const auto& Descriptor : Descriptors)
    {
        const std::string Fingerprint = ComputePackageFingerprint(Descriptor);
        Output << "    {\n";
        Output << "      package_id: \"" << Descriptor.GetPackageId() << "\",\n";
        Output << "      version: \"" << Descriptor.GetVersion() << "\",\n";
        Output << "      load_index: " << Descriptor.GetLoadIndex() << ",\n";
        Output << "      fingerprint: \"" << Fingerprint << "\",\n";
        Output << "    },\n";
    }

    Output << "  ],\n";
    Output << "}\n";
    return Output.str();
}

bool VerifyModsLock(
    const std::string& LockFileContent,
    const std::vector<GV2ContentCore::FPackageDescriptor>& Descriptors,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    using namespace GV2ContentCore;

    const FParseLimits Limits;
    std::vector<FDiagnostic> ParseDiagnostics;
    const std::optional<FValue> Parsed = ParseJson5(
        LockFileContent, Limits, ParseDiagnostics, std::nullopt, 0u, "mods.lock.json5");
    if (!Parsed)
    {
        OutDiagnostics.insert(OutDiagnostics.end(), ParseDiagnostics.begin(), ParseDiagnostics.end());
        return false;
    }
    if (!Parsed->IsObject())
    {
        OutDiagnostics.push_back(MakeLockDiagnostic(
            "core:diagnostic.package.lock.invalid",
            "mods.lock.json5 must be a JSON5 object"));
        return false;
    }

    const FValue* SchemaVersionVal = Parsed->FindField("schema_version");
    if (SchemaVersionVal == nullptr || !SchemaVersionVal->IsInteger() || SchemaVersionVal->AsInteger() != 1)
    {
        OutDiagnostics.push_back(MakeLockDiagnostic(
            "core:diagnostic.package.lock.invalid",
            "mods.lock.json5 must declare integer schema_version = 1"));
        return false;
    }

    const FValue* PackagesVal = Parsed->FindField("packages");
    if (PackagesVal == nullptr || !PackagesVal->IsArray())
    {
        OutDiagnostics.push_back(MakeLockDiagnostic(
            "core:diagnostic.package.lock.invalid",
            "mods.lock.json5 must declare 'packages' array"));
        return false;
    }

    const auto& LockPackages = PackagesVal->AsArray();
    if (LockPackages.size() != Descriptors.size())
    {
        OutDiagnostics.push_back(MakeLockDiagnostic(
            "core:diagnostic.package.lock.mismatch",
            "mods.lock.json5 package count (" + std::to_string(LockPackages.size())
                + ") does not match actual package count (" + std::to_string(Descriptors.size()) + ")"));
        return false;
    }

    bool bAllMatch = true;
    for (std::size_t Index = 0; Index < Descriptors.size(); ++Index)
    {
        const FPackageDescriptor& Descriptor = Descriptors[Index];
        const FValue& LockPkg = LockPackages[Index];
        if (!LockPkg.IsObject())
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.invalid",
                "mods.lock.json5 packages[" + std::to_string(Index) + "] must be an object",
                Descriptor.GetPackageId()));
            bAllMatch = false;
            continue;
        }

        const FValue* PkgIdVal = LockPkg.FindField("package_id");
        const FValue* VersionVal = LockPkg.FindField("version");
        const FValue* LoadIndexVal = LockPkg.FindField("load_index");
        const FValue* FingerprintVal = LockPkg.FindField("fingerprint");

        if (PkgIdVal == nullptr || !PkgIdVal->IsString()
            || VersionVal == nullptr || !VersionVal->IsString()
            || LoadIndexVal == nullptr || !LoadIndexVal->IsInteger()
            || FingerprintVal == nullptr || !FingerprintVal->IsString())
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.invalid",
                "mods.lock.json5 packages[" + std::to_string(Index) + "] is missing required fields (package_id, version, load_index, fingerprint)",
                Descriptor.GetPackageId()));
            bAllMatch = false;
            continue;
        }

        if (PkgIdVal->AsString() != Descriptor.GetPackageId())
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.mismatch",
                "mods.lock.json5 packages[" + std::to_string(Index) + "] package_id '" + PkgIdVal->AsString()
                    + "' does not match expected package_id '" + Descriptor.GetPackageId() + "'",
                Descriptor.GetPackageId()));
            bAllMatch = false;
        }

        if (VersionVal->AsString() != Descriptor.GetVersion())
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.mismatch",
                "mods.lock.json5 package '" + Descriptor.GetPackageId() + "' version '" + VersionVal->AsString()
                    + "' does not match actual version '" + Descriptor.GetVersion() + "'",
                Descriptor.GetPackageId()));
            bAllMatch = false;
        }

        if (LoadIndexVal->AsInteger() != static_cast<std::int64_t>(Descriptor.GetLoadIndex()))
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.mismatch",
                "mods.lock.json5 package '" + Descriptor.GetPackageId() + "' load_index " + std::to_string(LoadIndexVal->AsInteger())
                    + " does not match actual load_index " + std::to_string(Descriptor.GetLoadIndex()),
                Descriptor.GetPackageId()));
            bAllMatch = false;
        }

        const std::string ActualFingerprint = ComputePackageFingerprint(Descriptor);
        if (FingerprintVal->AsString() != ActualFingerprint)
        {
            OutDiagnostics.push_back(MakeLockDiagnostic(
                "core:diagnostic.package.lock.mismatch",
                "mods.lock.json5 package '" + Descriptor.GetPackageId() + "' fingerprint '" + FingerprintVal->AsString()
                    + "' does not match actual fingerprint '" + ActualFingerprint + "'",
                Descriptor.GetPackageId()));
            bAllMatch = false;
        }
    }

    return bAllMatch;
}
} // namespace GV2ContentHostSupport
