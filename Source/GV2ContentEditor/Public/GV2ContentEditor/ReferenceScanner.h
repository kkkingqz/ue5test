#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/SchemaFormModel.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/Value.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GV2ContentEditor
{

/**
 * Single typed reference relationship between definitions (CED-12, CEH-13).
 */
struct GV2_CONTENT_EDITOR_API FGV2ReferenceItem final
{
    GV2ContentAuthoring::FAuthoringLocator SourceLocator;
    std::string SourceDefinitionId;
    std::string TargetDefinitionId;
    std::string TargetKind;
    GV2ContentCore::EFieldKind ReferenceKind = GV2ContentCore::EFieldKind::Reference;
    std::string RelativeSource;
    std::string JsonPointer;
    std::size_t Line = 0;
    std::size_t Column = 0;
    bool bIsExternal = false;
};

/**
 * Combined incoming and outgoing reference report for an active definition.
 */
struct GV2_CONTENT_EDITOR_API FGV2ReferenceReport final
{
    std::string DefinitionId;
    std::vector<FGV2ReferenceItem> OutgoingReferences; // "Uses"
    std::vector<FGV2ReferenceItem> IncomingReferences; // "Used by"
};

/**
 * Pre-rename impact assessment report (CEH-16).
 */
struct GV2_CONTENT_EDITOR_API FRenameImpactReport final
{
    std::string OldDefinitionId;
    std::string NewDefinitionId;
    std::string SourcePackageId;
    std::filesystem::path SourceFilePath;
    std::vector<FGV2ReferenceItem> OwnPackageReferences;
    std::vector<FGV2ReferenceItem> ExternalPackageReferences;
    std::vector<std::string> RedirectConflicts;
    std::vector<std::string> TombstoneConflicts;
    std::size_t TotalFilesAffected = 0;
    std::size_t TotalReplacements = 0;
    bool bCanRenameDirectly = true;
    bool bHasExternalReferences = false;
};

/**
 * High-performance typed authoring reference index (CEH-13, CEH-14, CEH-15).
 */
class GV2_CONTENT_EDITOR_API FGV2AuthoringReferenceIndex final
{
public:
    void Clear();

    void BuildIndex(
        const std::vector<std::filesystem::path>& PackageRoots,
        const std::vector<GV2ContentCore::FPackageDescriptor>& Packages,
        const std::vector<FGV2DefinitionSummary>& AllDefinitions);

    std::vector<FGV2ReferenceItem> GetOutgoingReferences(const std::string& SourceDefId) const;
    std::vector<FGV2ReferenceItem> GetIncomingReferences(const std::string& TargetDefId) const;
    std::vector<std::string> GetCompatibleReferenceTargets(const std::string& ExpectedKind) const;
    std::vector<std::string> GetCompatibleResourceTargets(const std::string& ResourceClass) const;

    void SetPendingDefinitionReferences(
        const std::string& DefinitionId,
        const std::vector<FGV2ReferenceItem>& PendingOutgoing);
    void ClearPendingDefinitionReferences(const std::string& DefinitionId);

private:
    std::vector<FGV2ReferenceItem> AllReferences;
    std::unordered_map<std::string, std::vector<FGV2ReferenceItem>> OutgoingMap;
    std::unordered_map<std::string, std::vector<FGV2ReferenceItem>> IncomingMap;
    std::unordered_map<std::string, std::vector<std::string>> CompatibleTargetsMap;
    std::unordered_map<std::string, std::vector<std::string>> ResourceTargetsMap;

    std::unordered_map<std::string, std::vector<FGV2ReferenceItem>> PendingOutgoingOverlay;
};

/**
 * Reference scanner and resolution engine (CED-12, CEH-13).
 */
class GV2_CONTENT_EDITOR_API FGV2ReferenceScanner final
{
public:
    /**
     * Extracts typed references from a definition value given its compiled schema spec.
     */
    static std::vector<FGV2ReferenceItem> ExtractTypedReferences(
        const GV2ContentCore::FValue& DataValue,
        const GV2ContentCore::FCompiledFieldSpec* RootSpec,
        const GV2ContentCore::FValue& ExtensionsValue,
        const std::vector<GV2ContentCore::FExtensionSchemaResource>& ExtensionSchemas,
        const std::string& SourceDefId,
        const std::string& PackageId,
        const std::string& RelativeSource,
        const GV2ContentCore::FParsedDocument* Doc = nullptr);

    /**
     * Scans an in-memory loaded definition for all outgoing Stable ID references ("Uses").
     */
    static std::vector<FGV2ReferenceItem> FindOutgoingReferences(
        const FGV2LoadedDefinition& LoadedDef,
        const FGV2SchemaFormModel* FormModel = nullptr);

    /**
     * Scans all indexed definition files to find references targeting TargetDefinitionId ("Used by").
     */
    static std::vector<FGV2ReferenceItem> FindIncomingReferences(
        const std::string& TargetDefinitionId,
        const std::vector<FGV2DefinitionSummary>& AllDefinitions,
        const std::vector<std::filesystem::path>& PackageRoots,
        const std::vector<GV2ContentCore::FPackageDescriptor>& Packages);

    /**
     * Returns all indexed definition IDs that match the expected kind (for typed reference pickers).
     */
    static std::vector<std::string> FindCompatibleTargets(
        const std::string& ExpectedKind,
        const std::vector<FGV2DefinitionSummary>& AllDefinitions);
};

} // namespace GV2ContentEditor
