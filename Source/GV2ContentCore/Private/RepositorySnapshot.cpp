#include "GV2ContentCore/RepositorySnapshot.h"

#include "GV2ContentCore/StableId.h"

#include <stdexcept>

namespace GV2ContentCore
{
std::optional<FDefinitionId> FDefinitionId::Parse(const std::string_view Value)
{
    if (!FStableId::IsValid(Value)) return std::nullopt;
    return FDefinitionId(std::string(Value));
}

FDefinitionId FDefinitionId::Require(std::string Value)
{
    if (!FStableId::IsValid(Value))
    {
        throw std::invalid_argument("Definition ID must be a canonical Stable ID");
    }
    return FDefinitionId(std::move(Value));
}

FRepositorySnapshot::FRepositorySnapshot(
    FValue InRootValue,
    std::map<std::string, std::size_t> InById,
    std::map<std::string, std::vector<std::string>> InByKind,
    std::map<std::string, FDefinitionProvenance> InProvenanceById,
    std::map<std::string, std::string> InRedirects,
    std::set<std::string> InTombstones,
    std::string InContentHash)
    : RootValue(std::move(InRootValue))
    , ById(std::move(InById))
    , ByKind(std::move(InByKind))
    , ProvenanceById(std::move(InProvenanceById))
    , Redirects(std::move(InRedirects))
    , Tombstones(std::move(InTombstones))
    , ContentHash(std::move(InContentHash))
{
}

const FValue* FRepositoryReadHandle::Find(const FDefinitionId& DefinitionId) const
{
    if (Snapshot == nullptr) return nullptr;
    std::string CanonicalId = DefinitionId.ToString();
    if (const auto Redirect = Snapshot->Redirects.find(CanonicalId); Redirect != Snapshot->Redirects.end())
    {
        CanonicalId = Redirect->second;
    }
    const auto Found = Snapshot->ById.find(CanonicalId);
    if (Found == Snapshot->ById.end()) return nullptr;
    const FValue* Definitions = Snapshot->RootValue.FindField("definitions");
    if (Definitions == nullptr || Found->second >= Definitions->AsArray().size()) return nullptr;
    return &Definitions->AsArray()[Found->second];
}

FRepositoryQueryResult FRepositoryReadHandle::Require(const FDefinitionId& DefinitionId) const
{
    if (const FValue* Definition = Find(DefinitionId))
    {
        return FRepositoryQueryResult{ Definition, std::nullopt };
    }
    FRepositoryQueryError Error;
    Error.RequestedId = DefinitionId.ToString();
    if (Snapshot == nullptr)
    {
        Error.Code = "core:diagnostic.repository.read.invalid_handle";
    }
    else if (Snapshot->Tombstones.contains(DefinitionId.ToString()))
    {
        Error.Code = "core:diagnostic.repository.read.tombstoned";
    }
    else
    {
        Error.Code = "core:diagnostic.repository.read.not_found";
        if (const auto Redirect = Snapshot->Redirects.find(DefinitionId.ToString());
            Redirect != Snapshot->Redirects.end())
        {
            Error.CanonicalId = Redirect->second;
        }
    }
    return FRepositoryQueryResult{ nullptr, std::move(Error) };
}

std::vector<FDefinitionId> FRepositoryReadHandle::List(const std::string_view Kind) const
{
    std::vector<FDefinitionId> Result;
    if (Snapshot == nullptr || !FStableId::IsValidSegment(Kind)) return Result;
    const auto Found = Snapshot->ByKind.find(std::string(Kind));
    if (Found == Snapshot->ByKind.end()) return Result;
    Result.reserve(Found->second.size());
    for (const std::string& Id : Found->second) Result.push_back(FDefinitionId::Require(Id));
    return Result;
}

const FDefinitionProvenance* FRepositoryReadHandle::GetProvenance(
    const FDefinitionId& DefinitionId) const
{
    if (Snapshot == nullptr) return nullptr;
    const auto Found = Snapshot->ProvenanceById.find(DefinitionId.ToString());
    return Found == Snapshot->ProvenanceById.end() ? nullptr : &Found->second;
}

const std::string& FRepositoryReadHandle::GetContentHash() const
{
    if (Snapshot == nullptr) throw std::logic_error("Invalid repository read handle");
    return Snapshot->ContentHash;
}
}
