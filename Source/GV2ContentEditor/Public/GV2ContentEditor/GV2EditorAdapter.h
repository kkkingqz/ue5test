#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentAuthoring/AuthoringTypes.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/Value.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentEditor
{

/**
 * Editor content adapter (CED-06).
 * Single point of access for Unreal Editor to read, edit, validate, and write
 * GV2 game definitions. Maintains only editing UI state in-memory.
 */
class GV2_CONTENT_EDITOR_API FGV2EditorAdapter final
{
public:
    FGV2EditorAdapter() = default;
    ~FGV2EditorAdapter() = default;

    // Non-copyable
    FGV2EditorAdapter(const FGV2EditorAdapter&) = delete;
    FGV2EditorAdapter& operator=(const FGV2EditorAdapter&) = delete;

    // Moveable
    FGV2EditorAdapter(FGV2EditorAdapter&&) noexcept = default;
    FGV2EditorAdapter& operator=(FGV2EditorAdapter&&) noexcept = default;

    /**
     * Initializes the adapter with the project's content directory (e.g. GameData/).
     * Discovers all packages and indexes definitions.
     */
    bool Initialize(
        const std::filesystem::path& InContentRoot,
        std::vector<FGV2EditorDiagnostic>& OutDiagnostics);

    /** Gets the root content directory. */
    const std::filesystem::path& GetContentRoot() const { return ContentRoot; }

    /** Refreshes definition index and package metadata. */
    bool RefreshIndex(std::vector<FGV2EditorDiagnostic>& OutDiagnostics);

    /** Lists all indexed definitions, optionally filtered by definition type. */
    std::vector<FGV2DefinitionSummary> ListDefinitions(
        const std::optional<std::string>& FilterType = std::nullopt) const;

    /** Returns all available definition types across discovered schema bindings. */
    std::vector<std::string> GetAvailableDefinitionTypes() const;

    /**
     * Loads a definition by ID into memory as current active definition.
     * Clears any dirty field state.
     */
    std::optional<FGV2LoadedDefinition> LoadDefinition(
        const std::string& DefinitionId,
        std::vector<FGV2EditorDiagnostic>& OutDiagnostics);

    /** Gets the currently loaded definition, or nullptr if none loaded. */
    const FGV2LoadedDefinition* GetCurrentDefinition() const;

    /**
     * Sets an edited field value for the current definition at the given JSON pointer.
     * Marks the field as dirty if it differs from the canonical baseline.
     */
    void SetCurrentFieldValue(
        const std::string& JsonPointer,
        const GV2ContentCore::FValue& NewValue);

    /**
     * Gets the effective value of a field (current edited value if dirty, else baseline).
     */
    std::optional<GV2ContentCore::FValue> GetCurrentFieldValue(
        const std::string& JsonPointer) const;

    /** Returns true if there are unsaved field modifications. */
    bool IsDirty() const;

    /** Returns the map of dirty JSON pointers and their modified values. */
    const std::map<std::string, GV2ContentCore::FValue>& GetDirtyFields() const { return DirtyFields; }

    /** Discards all unsaved field modifications and reverts to canonical baseline. */
    void DiscardCurrentChanges();

    /**
     * Checks whether the file containing the current definition has been modified
     * externally on disk since it was loaded (CED-07).
     * Returns true if file matches loaded stamp, false if stale / modified.
     */
    bool CheckFileState() const;

    /**
     * Saves the current definition by atomically applying all dirty fields (CED-03, CED-07).
     * Rejects save if file is stale on disk (returns StaleFileState).
     * On success, reloads definition and clears dirty state.
     */
    FGV2EditorAuthoringResult SaveCurrentDefinition();

    /**
     * Creates a new definition in a target package.
     */
    FGV2EditorAuthoringResult CreateDefinition(
        const std::string& PackageId,
        const std::string& DefinitionId,
        const std::string& DefinitionType,
        const std::optional<GV2ContentCore::FValue>& InitialData = std::nullopt);

    /**
     * Duplicates an existing definition atomically (CED-03).
     */
    FGV2EditorAuthoringResult DuplicateDefinition(
        const std::string& SourceDefinitionId,
        const std::string& TargetDefinitionId);

    /**
     * Deletes a definition.
     */
    FGV2EditorAuthoringResult DeleteDefinition(
        const std::string& DefinitionId);

    /**
     * Renames a definition across all references.
     */
    FGV2EditorAuthoringResult RenameDefinition(
        const std::string& OldDefinitionId,
        const std::string& NewDefinitionId);

    /**
     * Runs authoritative repository validation across the entire content set.
     */
    std::vector<FGV2EditorDiagnostic> ValidateRepository() const;

private:
    std::filesystem::path FindPackageRootById(const std::string& PackageId) const;
    std::filesystem::path FindPackageRootForDefinition(const std::string& DefinitionId) const;
    static FGV2EditorAuthoringResult ConvertAuthoringResult(
        const GV2ContentAuthoring::FAuthoringResult& InResult);

private:
    std::filesystem::path ContentRoot;
    std::vector<GV2ContentCore::FPackageDescriptor> DiscoveredPackages;
    std::vector<std::filesystem::path> PackageRoots;
    std::vector<FGV2DefinitionSummary> IndexedDefinitions;

    std::optional<FGV2LoadedDefinition> CurrentDefinition;
    std::map<std::string, GV2ContentCore::FValue> CurrentValues;
    std::map<std::string, GV2ContentCore::FValue> DirtyFields;
};

} // namespace GV2ContentEditor
