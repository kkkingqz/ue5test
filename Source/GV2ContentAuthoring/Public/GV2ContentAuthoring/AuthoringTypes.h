#pragma once

#include "GV2ContentAuthoring/GV2ContentAuthoring.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/Value.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentAuthoring
{

enum class EAuthoringStatus
{
    Success = 0,
    InvalidArguments,
    InvalidDefinitionId,
    IdKindMismatch,
    PackageNotFound,
    DefinitionNotFound,
    DuplicateDefinitionId,
    TargetIsContainer,
    PointerNotFound,
    InvalidValue,
    SchemaNotFound,
    ValidationFailed,
    StaleFileState,
    FileWriteFailed,
    SpanMappingFailed,
    ReferencedDefinition,
    ToolFailure,
};

struct GV2_CONTENT_AUTHORING_API FFileStateStamp
{
    std::string ContentHash;

    bool IsEmpty() const { return ContentHash.empty(); }
    bool Matches(const FFileStateStamp& Other) const { return ContentHash == Other.ContentHash; }
    bool Matches(std::string_view ActualContent) const;

    static FFileStateStamp FromContent(std::string_view Content);
    static FFileStateStamp FromFile(const std::filesystem::path& FilePath);
};

struct FFieldChange
{
    std::string JsonPointer;
    GV2ContentCore::FValue NewValue;
};

struct GV2_CONTENT_AUTHORING_API FAuthoringResult
{
    EAuthoringStatus Status = EAuthoringStatus::Success;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    FFileStateStamp NewStamp;
    std::filesystem::path TargetFilePath;
    std::string UpdatedFileContent;
    std::size_t AffectedDefinitionsCount = 0;

    bool IsSuccess() const { return Status == EAuthoringStatus::Success; }
};

struct FCreateDefinitionParams
{
    std::filesystem::path PackageRoot;
    std::string DefinitionType;
    std::string DefinitionId;
    std::optional<GV2ContentCore::FValue> InitialData;
    std::vector<std::string> Tags;
    std::optional<FFileStateStamp> ExpectedStamp;
};

struct FSetFieldParams
{
    std::filesystem::path PackageRoot;
    std::string DefinitionId;
    std::string JsonPointer;
    GV2ContentCore::FValue NewValue;
    std::optional<FFileStateStamp> ExpectedStamp;
};

struct FBatchSetFieldsParams
{
    std::filesystem::path PackageRoot;
    std::string DefinitionId;
    std::vector<FFieldChange> Changes;
    std::optional<FFileStateStamp> ExpectedStamp;
};

struct FDeleteDefinitionParams
{
    std::filesystem::path PackageRoot;
    std::string DefinitionId;
    bool bCheckReferences = true;
    std::optional<FFileStateStamp> ExpectedStamp;
};

struct FRenameDefinitionParams
{
    std::filesystem::path PackageRoot;
    std::string OldDefinitionId;
    std::string NewDefinitionId;
    std::optional<FFileStateStamp> ExpectedStamp;
};

struct FDuplicateDefinitionParams
{
    std::filesystem::path PackageRoot;
    std::string SourceDefinitionId;
    std::string TargetDefinitionId;
    std::optional<FFileStateStamp> ExpectedStamp;
};

} // namespace GV2ContentAuthoring
