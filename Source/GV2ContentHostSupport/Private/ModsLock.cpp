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

std::string ComputePackageFingerprint(
    const GV2ContentCore::FPackageDescriptor& Descriptor,
    const std::string& CanonicalManifestHash)
{
    using namespace GV2ContentCore;

    // PSC-03: identity + full-content hash, not a hand-maintained re-listing of manifest
    // fields. load_index is load-order context assigned by discovery, not manifest
    // content, so it stays a separate field alongside the hash rather than folded into
    // it; every semantic manifest field (known or not) is already covered by
    // CanonicalManifestHash.
    std::vector<std::pair<std::string, FValue>> Fields;
    Fields.emplace_back("package_id", FValue::MakeString(Descriptor.GetPackageId()));
    Fields.emplace_back("load_index", FValue::MakeInteger(static_cast<std::int64_t>(Descriptor.GetLoadIndex())));
    Fields.emplace_back("canonical_manifest_hash", FValue::MakeString(CanonicalManifestHash));
    return ComputeCanonicalHash(FValue::MakeObject(std::move(Fields)));
}

std::string GenerateModsLockContent(const std::vector<FResolvedPackageSource>& Sources)
{
    std::ostringstream Output;
    Output << "{\n";
    Output << "  schema_version: 1,\n";
    Output << "  packages: [\n";

    for (const auto& Source : Sources)
    {
        const std::string Fingerprint = ComputePackageFingerprint(Source.Descriptor, Source.CanonicalManifestHash);
        Output << "    {\n";
        Output << "      package_id: \"" << Source.Descriptor.GetPackageId() << "\",\n";
        Output << "      version: \"" << Source.Descriptor.GetVersion() << "\",\n";
        Output << "      load_index: " << Source.Descriptor.GetLoadIndex() << ",\n";
        Output << "      fingerprint: \"" << Fingerprint << "\",\n";
        Output << "    },\n";
    }

    Output << "  ],\n";
    Output << "}\n";
    return Output.str();
}

bool VerifyModsLock(
    const std::string& LockFileContent,
    const std::vector<FResolvedPackageSource>& Sources,
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
    if (LockPackages.size() != Sources.size())
    {
        OutDiagnostics.push_back(MakeLockDiagnostic(
            "core:diagnostic.package.lock.mismatch",
            "mods.lock.json5 package count (" + std::to_string(LockPackages.size())
                + ") does not match actual package count (" + std::to_string(Sources.size()) + ")"));
        return false;
    }

    bool bAllMatch = true;
    for (std::size_t Index = 0; Index < Sources.size(); ++Index)
    {
        const FPackageDescriptor& Descriptor = Sources[Index].Descriptor;
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

        const std::string ActualFingerprint = ComputePackageFingerprint(Descriptor, Sources[Index].CanonicalManifestHash);
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
