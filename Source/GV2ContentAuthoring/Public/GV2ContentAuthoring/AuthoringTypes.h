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

enum class EFieldOpType : std::uint8_t
{
    Set,
    RemoveProperty,
    InsertArrayElement,
    RemoveArrayElement,
    MoveArrayElement
};

struct GV2_CONTENT_AUTHORING_API FFieldOp final
{
    EFieldOpType OpType = EFieldOpType::Set;
    std::string JsonPointer;
    GV2ContentCore::FValue Value;
    std::size_t TargetIndex = 0;
    std::size_t SourceIndex = 0;

    static FFieldOp MakeSet(std::string JsonPointer, GV2ContentCore::FValue Value)
    {
        FFieldOp Op;
        Op.OpType = EFieldOpType::Set;
        Op.JsonPointer = std::move(JsonPointer);
        Op.Value = std::move(Value);
        return Op;
    }

    static FFieldOp MakeRemoveProperty(std::string JsonPointer)
    {
        FFieldOp Op;
        Op.OpType = EFieldOpType::RemoveProperty;
        Op.JsonPointer = std::move(JsonPointer);
        return Op;
    }

    static FFieldOp MakeInsertArrayElement(std::string JsonPointer, std::size_t Index, GV2ContentCore::FValue Value)
    {
        FFieldOp Op;
        Op.OpType = EFieldOpType::InsertArrayElement;
        Op.JsonPointer = std::move(JsonPointer);
        Op.TargetIndex = Index;
        Op.Value = std::move(Value);
        return Op;
    }

    static FFieldOp MakeRemoveArrayElement(std::string JsonPointer, std::size_t Index)
    {
        FFieldOp Op;
        Op.OpType = EFieldOpType::RemoveArrayElement;
        Op.JsonPointer = std::move(JsonPointer);
        Op.TargetIndex = Index;
        return Op;
    }

    static FFieldOp MakeMoveArrayElement(std::string JsonPointer, std::size_t FromIndex, std::size_t ToIndex)
    {
        FFieldOp Op;
        Op.OpType = EFieldOpType::MoveArrayElement;
        Op.JsonPointer = std::move(JsonPointer);
        Op.SourceIndex = FromIndex;
        Op.TargetIndex = ToIndex;
        return Op;
    }
};

struct FApplyOperationsParams
{
    std::filesystem::path PackageRoot;
    std::string DefinitionId;
    std::vector<FFieldOp> Operations;
    std::optional<FFileStateStamp> ExpectedStamp;
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
    std::size_t AffectedFilesCount = 0;
    std::size_t ReplacementsCount = 0;
    std::vector<std::filesystem::path> AffectedFilePaths;

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
