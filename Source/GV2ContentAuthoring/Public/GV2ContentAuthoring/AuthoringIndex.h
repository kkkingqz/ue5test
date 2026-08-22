#pragma once

#include "GV2ContentAuthoring/GV2ContentAuthoring.h"
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentAuthoring
{

/**
 * Portable physical locator for a definition in a specific package and source file (CEH-01).
 * Completely independent of Slate and Unreal UObjects.
 */
struct GV2_CONTENT_AUTHORING_API FAuthoringLocator final
{
    std::string PackageId;
    std::string RelativeSource;
    std::string DefinitionId;
    std::string DefinitionType;
    std::size_t LoadIndex = 0;
    bool bIsWinner = true;
    bool bIsShadowed = false;
    bool bDeprecated = false;
    std::vector<std::string> Tags;

    std::string ToDebugString() const
    {
        return PackageId + "@" + RelativeSource + "#" + DefinitionId;
    }

    bool operator==(const FAuthoringLocator& Other) const
    {
        return PackageId == Other.PackageId
            && RelativeSource == Other.RelativeSource
            && DefinitionId == Other.DefinitionId;
    }

    bool operator!=(const FAuthoringLocator& Other) const
    {
        return !(*this == Other);
    }
};

/**
 * Portable index of all physical definition entries across discovered packages (CEH-01).
 * Resolves exactly one effective winner per canonical Stable ID based on package load order.
 */
class GV2_CONTENT_AUTHORING_API FAuthoringIndex final
{
public:
    FAuthoringIndex() = default;
    ~FAuthoringIndex() = default;

    void Clear();

    /** Adds a physical definition entry discovered during package indexing. */
    void AddEntry(const FAuthoringLocator& Entry);

    /**
     * Resolves winners and shadowed state for all indexed definitions.
     * For each unique Stable ID, the entry with highest LoadIndex (or latest discovery order)
     * is marked bIsWinner = true; all other entries are marked bIsWinner = false, bIsShadowed = true.
     */
    void Finalize();

    /** Returns all physical definition entries across all packages (winners + shadowed). */
    const std::vector<FAuthoringLocator>& GetAllEntries() const { return AllEntries; }

    /** Returns only effective winning definitions (one per unique Stable ID), optionally filtered by type. */
    std::vector<FAuthoringLocator> GetEffectiveDefinitions(
        const std::optional<std::string>& FilterType = std::nullopt) const;

    /** Returns all physical provider entries for a given Stable ID (winner + shadowed). */
    std::vector<FAuthoringLocator> GetEntriesForDefinition(const std::string& DefinitionId) const;

    /** Returns the effective winning locator for a given Stable ID. */
    std::optional<FAuthoringLocator> GetEffectiveWinner(const std::string& DefinitionId) const;

    /** Finds a specific locator by PackageId and DefinitionId. */
    std::optional<FAuthoringLocator> FindLocator(
        const std::string& PackageId,
        const std::string& DefinitionId) const;

    /** Finds a specific locator by PackageId, RelativeSource, and DefinitionId. */
    std::optional<FAuthoringLocator> FindLocator(
        const std::string& PackageId,
        const std::string& RelativeSource,
        const std::string& DefinitionId) const;

    /** Returns all available definition types across indexed entries. */
    std::vector<std::string> GetAvailableDefinitionTypes() const;

    /** Returns the count of unique effective definitions. */
    std::size_t NumEffective() const { return EffectiveWinners.size(); }

    /** Returns the count of all physical entries. */
    std::size_t NumTotal() const { return AllEntries.size(); }

private:
    std::vector<FAuthoringLocator> AllEntries;
    std::map<std::string, std::vector<std::size_t>> ByDefinitionId;
    std::map<std::string, std::size_t> EffectiveWinners;
};

} // namespace GV2ContentAuthoring
