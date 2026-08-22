#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/ReferenceScanner.h"
#include "GV2ContentEditor/SchemaFormModel.h"
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
     * Selects the effective winning provider if multiple packages contain this ID.
     * Clears any dirty field state.
     */
    std::optional<FGV2LoadedDefinition> LoadDefinition(
        const std::string& DefinitionId,
        std::vector<FGV2EditorDiagnostic>& OutDiagnostics);

    /**
     * Loads a specific physical definition provider into memory (CEH-02).
     * Clears any dirty field state.
     */
    std::optional<FGV2LoadedDefinition> LoadDefinition(
        const GV2ContentAuthoring::FAuthoringLocator& Locator,
        std::vector<FGV2EditorDiagnostic>& OutDiagnostics);

    /** Gets the currently loaded definition, or nullptr if none loaded. */
    const FGV2LoadedDefinition* GetCurrentDefinition() const;

    /** Gets the active authoring locator if a definition is loaded (CEH-02). */
    std::optional<GV2ContentAuthoring::FAuthoringLocator> GetCurrentLocator() const;

    /** Gets all physical provider locators for a Stable ID (CEH-01, CEH-05). */
    std::vector<GV2ContentAuthoring::FAuthoringLocator> GetProvidersForDefinition(
        const std::string& DefinitionId) const;

    /** Gets the authoring index (CEH-01). */
    const GV2ContentAuthoring::FAuthoringIndex& GetAuthoringIndex() const { return AuthoringIndex; }

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

    /**
     * Gets the property presence for a given JSON pointer (CEH-07).
     */
    EPropertyPresence GetPropertyPresence(
        const std::string& JsonPointer,
        const GV2ContentCore::FCompiledFieldSpec* Spec = nullptr,
        bool bRequired = false) const;

    /**
     * Materializes an absent optional property into the current definition candidate (CEH-09).
     */
    void AddCurrentOptionalProperty(const std::string& JsonPointer);

    /**
     * Removes an explicit object property from the current definition candidate (CEH-09).
     */
    void RemoveCurrentProperty(const std::string& JsonPointer);

    /**
     * Resets an explicit property back to schema default or removes it (CEH-09).
     */
    void ResetCurrentFieldToDefault(const std::string& JsonPointer);

    /**
     * Inserts an element into an array at the given index (CEH-08, CEH-11).
     */
    void InsertCurrentArrayElement(
        const std::string& JsonPointer,
        std::size_t Index,
        const GV2ContentCore::FValue& Value);

    /**
     * Removes an element from an array at the given index (CEH-08, CEH-11).
     */
    void RemoveCurrentArrayElement(
        const std::string& JsonPointer,
        std::size_t Index);

    /**
     * Reorders array elements by moving an element from one index to another (CEH-08, CEH-11).
     */
    void MoveCurrentArrayElement(
        const std::string& JsonPointer,
        std::size_t FromIndex,
        std::size_t ToIndex);

    /**
     * Returns schema-declared absent optional fields for the currently loaded definition.
     */
    std::vector<FGV2FormFieldDescriptor> GetAbsentOptionalFieldsForCategory(
        const std::string& CategoryName) const;
    std::vector<FGV2FormFieldDescriptor> GetAllAbsentOptionalFields() const;

    /** Returns true if there are unsaved field modifications. */
    bool IsDirty() const;

    /** Returns the map of dirty JSON pointers and their modified values. */
    const std::map<std::string, GV2ContentCore::FValue>& GetDirtyFields() const { return DirtyFields; }
    const std::vector<GV2ContentAuthoring::FFieldOp>& GetPendingOperations() const { return PendingOperations; }

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
     * Calculates pre-rename impact assessment (CEH-16).
     */
    FRenameImpactReport CalculateRenameImpact(
        const std::string& OldDefinitionId,
        const std::string& NewDefinitionId) const;

    /**
     * Renames an existing definition across all typed references in the owning package (CED-04, CEH-17).
     */
    FGV2EditorAuthoringResult RenameDefinition(
        const std::string& OldDefinitionId,
        const std::string& NewDefinitionId,
        bool bCreateRedirect = false);

    /**
     * Deletes an existing definition atomically (CED-03).
     * Rejects deletion if referenced by other definitions.
     */
    FGV2EditorAuthoringResult DeleteDefinition(
        const std::string& DefinitionId);

    /**
     * Runs authoritative repository validation across the entire content set.
     */
    std::vector<FGV2EditorDiagnostic> ValidateRepository() const;

    /** Returns the schema form model built dynamically for a definition type (CED-11). */
    std::optional<FGV2SchemaFormModel> GetFormModelForDefinitionType(
        const std::string& DefinitionType,
        const std::optional<std::string>& OwningPackageId = std::nullopt) const;

    /** Scans outgoing references for the active loaded definition ("Uses", CED-12, CEH-15). */
    std::vector<FGV2ReferenceItem> GetOutgoingReferences() const;

    /** Scans incoming references targeting a definition ("Used by", CED-12, CEH-15). */
    std::vector<FGV2ReferenceItem> GetIncomingReferences(
        const std::string& DefinitionId) const;

    /** Returns compatible target definition IDs for a given expected kind (CED-12, CEH-14). */
    std::vector<std::string> GetCompatibleReferenceTargets(
        const std::string& ExpectedKind) const;

    /** Returns resource definition IDs filtered by canonical resource_class (CEH-14). */
    std::vector<std::string> GetCompatibleResourceTargets(
        const std::string& ResourceClass) const;

    /**
     * Unified navigation gate preventing silent loss of unsaved edits (CEH-18).
     */
    ENavigationGateResult RequestNavigateTo(
        const GV2ContentAuthoring::FAuthoringLocator& TargetLocator,
        ENavigationDirtyResolution Resolution,
        std::vector<FGV2EditorDiagnostic>& OutDiags);

    ENavigationGateResult RequestNavigateTo(
        const std::string& DefinitionId,
        ENavigationDirtyResolution Resolution,
        std::vector<FGV2EditorDiagnostic>& OutDiags);

    /**
     * Returns the 4-state session lifecycle status for the active loaded definition (CEH-19).
     */
    EDefinitionSessionState GetSessionState() const;

    /**
     * Exports active dirty edits to an offline JSON5 draft (CEH-20).
     */
    bool ExportCurrentDraft(
        const std::filesystem::path& DraftFilePath,
        std::string& OutError) const;

    /**
     * Imports an offline JSON5 draft and reapplies pending edits over the current definition (CEH-20).
     */
    bool ImportAndApplyDraft(
        const std::filesystem::path& DraftFilePath,
        std::vector<FGV2EditorDiagnostic>& OutDiags,
        std::string& OutError);

    /**
     * Discards local in-memory edits and reloads baseline from disk (CEH-20).
     */
    bool DiscardAndReload(std::vector<FGV2EditorDiagnostic>& OutDiags);

    /**
     * Returns initialization diagnostics retained after Initialize() (CEH-22).
     */
    const std::vector<FGV2EditorDiagnostic>& GetInitializationDiagnostics() const
    {
        return InitializationDiagnostics;
    }

private:
    void UpdatePendingReferenceOverlay();
    std::filesystem::path FindPackageRootById(const std::string& PackageId) const;
    std::filesystem::path FindPackageRootForDefinition(const std::string& DefinitionId) const;
    static FGV2EditorAuthoringResult ConvertAuthoringResult(
        const GV2ContentAuthoring::FAuthoringResult& InResult);

private:
    std::filesystem::path ContentRoot;
    std::vector<GV2ContentCore::FPackageDescriptor> DiscoveredPackages;
    std::vector<std::filesystem::path> PackageRoots;
    GV2ContentAuthoring::FAuthoringIndex AuthoringIndex;
    FGV2AuthoringReferenceIndex AuthoringReferenceIndex;
    std::vector<FGV2DefinitionSummary> IndexedDefinitions;
    std::vector<FGV2EditorDiagnostic> InitializationDiagnostics;

    std::optional<FGV2LoadedDefinition> CurrentDefinition;
    GV2ContentCore::FValue CandidateDefinitionValue;
    std::vector<GV2ContentAuthoring::FFieldOp> PendingOperations;
    std::map<std::string, GV2ContentCore::FValue> CurrentValues;
    std::map<std::string, GV2ContentCore::FValue> DirtyFields;
};

} // namespace GV2ContentEditor
