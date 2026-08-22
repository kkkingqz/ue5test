#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentAuthoring/AuthoringIndex.h"
#include "GV2ContentAuthoring/AuthoringTypes.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/Value.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentEditor
{

/**
 * Lossless structured diagnostic for editor presentation (CED-08).
 * Directly preserves all diagnostic fields from GV2ContentCore / GV2ContentAuthoring
 * without string regex manipulation.
 */
struct GV2_CONTENT_EDITOR_API FGV2EditorDiagnostic final
{
    GV2ContentCore::EDiagnosticSeverity Severity = GV2ContentCore::EDiagnosticSeverity::Error;
    std::string Code;
    std::string Message;
    std::string PackageId;
    std::string RelativeSource;
    std::size_t Line = 0;
    std::size_t Column = 0;
    std::string StableId;
    std::string JsonPointer;

    static FGV2EditorDiagnostic FromDiagnostic(const GV2ContentCore::FDiagnostic& InDiag)
    {
        FGV2EditorDiagnostic Out;
        Out.Severity = InDiag.Severity;
        Out.Code = InDiag.Code;
        Out.Message = InDiag.Message;
        Out.PackageId = InDiag.PackageId.value_or("");
        Out.RelativeSource = InDiag.RelativeSource.value_or("");
        if (InDiag.Span.has_value())
        {
            Out.Line = InDiag.Span->StartLine;
            Out.Column = InDiag.Span->StartColumn;
        }
        Out.StableId = InDiag.DefinitionId.value_or("");
        Out.JsonPointer = InDiag.JsonPointer.value_or("");
        return Out;
    }
};

/**
 * Lightweight definition summary for browser / navigation list.
 */
struct GV2_CONTENT_EDITOR_API FGV2DefinitionSummary final
{
    std::string Id;
    std::string Type;
    std::string PackageId;
    std::string RelativeSource;
    bool bDeprecated = false;
    std::vector<std::string> Tags;
    GV2ContentAuthoring::FAuthoringLocator Locator;

    static FGV2DefinitionSummary FromLocator(const GV2ContentAuthoring::FAuthoringLocator& Loc)
    {
        FGV2DefinitionSummary Out;
        Out.Id = Loc.DefinitionId;
        Out.Type = Loc.DefinitionType;
        Out.PackageId = Loc.PackageId;
        Out.RelativeSource = Loc.RelativeSource;
        Out.bDeprecated = Loc.bDeprecated;
        Out.Tags = Loc.Tags;
        Out.Locator = Loc;
        return Out;
    }
};

/**
 * Fully loaded definition in memory for inspection and editing (CED-06).
 * Holds the canonical baseline and the file state stamp at load time.
 */
struct GV2_CONTENT_EDITOR_API FGV2LoadedDefinition final
{
    std::string Id;
    std::string Type;
    std::string PackageId;
    std::string RelativeSource;
    std::filesystem::path AbsolutePath;
    GV2ContentAuthoring::FAuthoringLocator Locator;
    GV2ContentAuthoring::FFileStateStamp Stamp;
    GV2ContentCore::FValue CanonicalData;
    std::vector<std::string> Tags;
    bool bDeprecated = false;
    GV2ContentCore::FValue Extensions;
};

/**
 * Session lifecycle state of the currently loaded definition (CEH-19).
 */
enum class EDefinitionSessionState : std::uint8_t
{
    Clean = 0,        // Not modified in memory, file on disk matches load stamp
    Dirty,            // Modified in memory, file on disk matches load stamp
    Stale,            // Not modified in memory, file on disk changed externally
    DirtyAndStale     // Modified in memory AND file on disk changed externally (conflict)
};

/**
 * Resolution policy when navigating away from a dirty definition (CEH-18).
 */
enum class ENavigationDirtyResolution : std::uint8_t
{
    CancelIfDirty = 0, // Abort navigation if currently dirty (preserve current selection and edits)
    SaveIfDirty,       // Attempt to save current definition before navigating (stay if save fails)
    DiscardIfDirty     // Discard in-memory modifications and navigate immediately
};

/**
 * Result of requesting navigation through the session navigation gate (CEH-18).
 */
enum class ENavigationGateResult : std::uint8_t
{
    Navigated = 0,            // Navigation succeeded (target definition loaded)
    Cancelled,                // Navigation was cancelled (dirty definition preserved)
    SaveFailedStayOnCurrent,  // Save attempted but failed validation/IO; stayed on current definition
    TargetNotFound            // Target definition was not found in package index
};

/**
 * Portable JSON5 draft for preserving unsaved modifications during external conflicts (CEH-20).
 */
struct GV2_CONTENT_EDITOR_API FGV2EditorDraft final
{
    GV2ContentAuthoring::FAuthoringLocator Locator;
    std::string BaseStamp;
    std::string CreatedAtIso;
    GV2ContentCore::FValue CandidateValue;
    std::vector<GV2ContentAuthoring::FFieldOp> PendingOperations;
    std::map<std::string, GV2ContentCore::FValue> DirtyFields;
};

/**
 * Distinct outcome categories for authoring operations in the editor (CED-07).
 * Specifically separates StaleFileState (external modification) from ValidationFailed.
 */
enum class EEditorAuthoringOutcome : std::uint8_t
{
    Success = 0,
    StaleFileState,       // File on disk changed externally since definition was loaded (CED-07)
    ValidationFailed,     // Authoritative schema/JSON5 validation error
    DefinitionNotFound,   // Definition ID does not exist in target package
    DuplicateDefinitionId,// Definition ID already exists
    InvalidDefinitionId,  // Definition ID violates Stable ID grammar
    IdKindMismatch,       // Definition ID kind does not match definition type
    PackageNotFound,      // Target package root or manifest missing
    SchemaNotFound,       // Schema for definition type not found
    PointerNotFound,      // Target JSON pointer not found in document
    TargetIsContainer,    // JSON pointer targets object/array instead of leaf
    IOError               // File read/write failure
};

/**
 * Result envelope for editor authoring operations.
 */
struct GV2_CONTENT_EDITOR_API FGV2EditorAuthoringResult final
{
    EEditorAuthoringOutcome Outcome = EEditorAuthoringOutcome::Success;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::vector<FGV2EditorDiagnostic> Diagnostics;
    std::filesystem::path AffectedFile;
    std::vector<std::filesystem::path> AffectedFilePaths;
    std::size_t AffectedFilesCount = 0;
    std::size_t ReplacementsCount = 0;
    GV2ContentAuthoring::FFileStateStamp NewStamp;
    std::size_t AffectedDefinitionsCount = 0;

    bool IsSuccess() const { return Outcome == EEditorAuthoringOutcome::Success; }
};

} // namespace GV2ContentEditor
