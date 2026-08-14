#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
struct FPackageDescriptor;
struct FBuildOptions;
class FBuildResult;
class FCandidate;
GV2_CONTENT_CORE_API FBuildResult BuildRepository(
    const std::vector<FPackageDescriptor>& PackageSet,
    const FBuildOptions& Options);

class GV2_CONTENT_CORE_API FDefinitionId final
{
public:
    static std::optional<FDefinitionId> Parse(std::string_view Value);
    static FDefinitionId Require(std::string Value);

    const std::string& ToString() const { return Value; }
    bool operator==(const FDefinitionId&) const = default;
    bool operator<(const FDefinitionId& Other) const { return Value < Other.Value; }

private:
    explicit FDefinitionId(std::string InValue) : Value(std::move(InValue)) {}
    std::string Value;
};

struct GV2_CONTENT_CORE_API FProviderProvenance final
{
    std::string PackageId;
    std::uint32_t PackageLoadIndex = 0;
    std::string RelativeSource;
    FSourceSpan SourceSpan;
    std::string SchemaId;
    std::int64_t SchemaVersion = 0;

    bool operator==(const FProviderProvenance&) const = default;
};

struct GV2_CONTENT_CORE_API FDefinitionProvenance final
{
    std::string OriginalId;
    std::string CanonicalId;
    std::vector<std::string> RedirectChain;
    FProviderProvenance Winner;
    std::vector<FProviderProvenance> ShadowedProviders;

    bool operator==(const FDefinitionProvenance&) const = default;
};

struct GV2_CONTENT_CORE_API FRepositoryQueryError final
{
    std::string Code;
    std::string RequestedId;
    std::optional<std::string> CanonicalId;
};

struct GV2_CONTENT_CORE_API FRepositoryQueryResult final
{
    const FValue* Definition = nullptr;
    std::optional<FRepositoryQueryError> Error;
    explicit operator bool() const { return Definition != nullptr; }
};

class GV2_CONTENT_CORE_API FRepositorySnapshot final
{
public:
    FRepositorySnapshot(const FRepositorySnapshot&) = delete;
    FRepositorySnapshot(FRepositorySnapshot&&) = delete;
    FRepositorySnapshot& operator=(const FRepositorySnapshot&) = delete;
    FRepositorySnapshot& operator=(FRepositorySnapshot&&) = delete;

    const FValue& GetRootValue() const { return RootValue; }
    const std::string& GetContentHash() const { return ContentHash; }

private:
    friend FBuildResult BuildRepository(
        const std::vector<FPackageDescriptor>&,
        const FBuildOptions&);
    friend class FRepositoryReadHandle;
    FRepositorySnapshot(
        FValue InRootValue,
        std::map<std::string, std::size_t> InById,
        std::map<std::string, std::vector<std::string>> InByKind,
        std::map<std::string, FDefinitionProvenance> InProvenanceById,
        std::map<std::string, std::string> InRedirects,
        std::set<std::string> InTombstones,
        std::string InContentHash);
    FValue RootValue;
    std::map<std::string, std::size_t> ById;
    std::map<std::string, std::vector<std::string>> ByKind;
    std::map<std::string, FDefinitionProvenance> ProvenanceById;
    std::map<std::string, std::string> Redirects;
    std::set<std::string> Tombstones;
    std::string ContentHash;
};

class GV2_CONTENT_CORE_API FRepositoryReadHandle final
{
public:
    FRepositoryReadHandle() = default;

    bool IsValid() const { return Snapshot != nullptr; }
    const FValue* Find(const FDefinitionId& DefinitionId) const;
    FRepositoryQueryResult Require(const FDefinitionId& DefinitionId) const;
    std::vector<FDefinitionId> List(std::string_view Kind) const;
    const FDefinitionProvenance* GetProvenance(const FDefinitionId& DefinitionId) const;
    const std::string& GetContentHash() const;

private:
    friend class FCandidate;
    explicit FRepositoryReadHandle(std::shared_ptr<const FRepositorySnapshot> InSnapshot)
        : Snapshot(std::move(InSnapshot)) {}
    std::shared_ptr<const FRepositorySnapshot> Snapshot;
};
}
