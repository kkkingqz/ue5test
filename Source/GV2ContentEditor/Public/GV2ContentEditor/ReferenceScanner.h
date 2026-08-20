#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/SchemaFormModel.h"
#include "GV2ContentCore/Value.h"

#include <filesystem>
#include <string>
#include <vector>

namespace GV2ContentEditor
{

/**
 * Single reference relationship between definitions (CED-12).
 */
struct GV2_CONTENT_EDITOR_API FGV2ReferenceItem final
{
    std::string SourceDefinitionId;
    std::string TargetDefinitionId;
    std::string TargetKind;
    std::string RelativeSource;
    std::string JsonPointer;
    std::size_t Line = 0;
    std::size_t Column = 0;
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
 * Reference scanner and resolution engine (CED-12).
 */
class GV2_CONTENT_EDITOR_API FGV2ReferenceScanner final
{
public:
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
