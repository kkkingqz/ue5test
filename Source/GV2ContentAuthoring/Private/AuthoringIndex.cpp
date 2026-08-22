#include "GV2ContentAuthoring/AuthoringIndex.h"

#include <algorithm>
#include <set>

namespace GV2ContentAuthoring
{

void FAuthoringIndex::Clear()
{
    AllEntries.clear();
    ByDefinitionId.clear();
    EffectiveWinners.clear();
}

void FAuthoringIndex::AddEntry(const FAuthoringLocator& Entry)
{
    const std::size_t Index = AllEntries.size();
    AllEntries.push_back(Entry);
    ByDefinitionId[Entry.DefinitionId].push_back(Index);
}

void FAuthoringIndex::Finalize()
{
    EffectiveWinners.clear();

    for (auto& [DefId, Indices] : ByDefinitionId)
    {
        if (Indices.empty()) continue;

        // Pick the winning entry: entry with highest LoadIndex, or latest discovery order if equal
        std::size_t WinnerIndex = Indices[0];
        for (std::size_t I = 1; I < Indices.size(); ++I)
        {
            const std::size_t CandidateIdx = Indices[I];
            if (AllEntries[CandidateIdx].LoadIndex >= AllEntries[WinnerIndex].LoadIndex)
            {
                WinnerIndex = CandidateIdx;
            }
        }

        for (const std::size_t Idx : Indices)
        {
            if (Idx == WinnerIndex)
            {
                AllEntries[Idx].bIsWinner = true;
                AllEntries[Idx].bIsShadowed = false;
            }
            else
            {
                AllEntries[Idx].bIsWinner = false;
                AllEntries[Idx].bIsShadowed = true;
            }
        }

        EffectiveWinners[DefId] = WinnerIndex;
    }
}

std::vector<FAuthoringLocator> FAuthoringIndex::GetEffectiveDefinitions(
    const std::optional<std::string>& FilterType) const
{
    std::vector<FAuthoringLocator> Result;
    Result.reserve(EffectiveWinners.size());

    for (const auto& [DefId, WinnerIndex] : EffectiveWinners)
    {
        if (WinnerIndex < AllEntries.size())
        {
            const auto& Entry = AllEntries[WinnerIndex];
            if (!FilterType.has_value() || FilterType->empty() || Entry.DefinitionType == *FilterType)
            {
                Result.push_back(Entry);
            }
        }
    }

    return Result;
}

std::vector<FAuthoringLocator> FAuthoringIndex::GetEntriesForDefinition(
    const std::string& DefinitionId) const
{
    std::vector<FAuthoringLocator> Result;
    auto It = ByDefinitionId.find(DefinitionId);
    if (It != ByDefinitionId.end())
    {
        Result.reserve(It->second.size());
        for (const std::size_t Idx : It->second)
        {
            if (Idx < AllEntries.size())
            {
                Result.push_back(AllEntries[Idx]);
            }
        }
    }
    return Result;
}

std::optional<FAuthoringLocator> FAuthoringIndex::GetEffectiveWinner(
    const std::string& DefinitionId) const
{
    auto It = EffectiveWinners.find(DefinitionId);
    if (It != EffectiveWinners.end() && It->second < AllEntries.size())
    {
        return AllEntries[It->second];
    }
    return std::nullopt;
}

std::optional<FAuthoringLocator> FAuthoringIndex::FindLocator(
    const std::string& PackageId,
    const std::string& DefinitionId) const
{
    auto It = ByDefinitionId.find(DefinitionId);
    if (It != ByDefinitionId.end())
    {
        for (const std::size_t Idx : It->second)
        {
            if (Idx < AllEntries.size() && AllEntries[Idx].PackageId == PackageId)
            {
                return AllEntries[Idx];
            }
        }
    }
    return std::nullopt;
}

std::optional<FAuthoringLocator> FAuthoringIndex::FindLocator(
    const std::string& PackageId,
    const std::string& RelativeSource,
    const std::string& DefinitionId) const
{
    auto It = ByDefinitionId.find(DefinitionId);
    if (It != ByDefinitionId.end())
    {
        for (const std::size_t Idx : It->second)
        {
            if (Idx < AllEntries.size()
                && AllEntries[Idx].PackageId == PackageId
                && AllEntries[Idx].RelativeSource == RelativeSource)
            {
                return AllEntries[Idx];
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> FAuthoringIndex::GetAvailableDefinitionTypes() const
{
    std::set<std::string> Types;
    for (const auto& Entry : AllEntries)
    {
        if (!Entry.DefinitionType.empty())
        {
            Types.insert(Entry.DefinitionType);
        }
    }
    return std::vector<std::string>(Types.begin(), Types.end());
}

} // namespace GV2ContentAuthoring
