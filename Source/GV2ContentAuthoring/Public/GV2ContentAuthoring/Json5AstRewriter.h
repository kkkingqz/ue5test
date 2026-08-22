#pragma once

#include "GV2ContentAuthoring/GV2ContentAuthoring.h"
#include "GV2ContentAuthoring/AuthoringTypes.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentAuthoring
{

GV2_CONTENT_AUTHORING_API bool IsValidJson5Identifier(std::string_view Key);

GV2_CONTENT_AUTHORING_API void FormatJson5Value(std::ostream& Out, const GV2ContentCore::FValue& Value, int IndentLevel);

GV2_CONTENT_AUTHORING_API GV2ContentCore::FValue GeneratePlaceholderValue(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const std::string& PackageId,
    const std::string& DefinitionType,
    const std::string& DefinitionIdPath,
    const std::string& FieldName);

GV2_CONTENT_AUTHORING_API std::string FormatDefinitionEntry(
    const std::string& DefinitionId,
    const GV2ContentCore::FValue& Data,
    int IndentLevel = 2);

GV2_CONTENT_AUTHORING_API std::string CreateNewDefinitionFileContent(
    std::int64_t SchemaVersion,
    const std::string& DefinitionType,
    const std::string& FormattedEntry);

GV2_CONTENT_AUTHORING_API bool InsertDefinitionEntryIntoJson5(
    const std::string& OriginalContent,
    const std::string& FormattedEntry,
    std::string& OutNewContent,
    std::string& OutErrorMessage);

struct GV2_CONTENT_AUTHORING_API FRenameStringTokenResult
{
    bool bSuccess = false;
    std::string UpdatedContent;
    std::size_t ReplacementsCount = 0;
    std::string ErrorMessage;
};

GV2_CONTENT_AUTHORING_API FRenameStringTokenResult ReplaceStringTokens(
    const std::string& RawContent,
    const std::string& OldValue,
    const std::string& NewValue,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

GV2_CONTENT_AUTHORING_API bool SourcePositionToByteOffset(
    std::string_view RawInput,
    std::uint32_t Line,
    std::uint32_t Column,
    std::size_t& OutByteOffset);

GV2_CONTENT_AUTHORING_API bool SourceSpanToByteRange(
    std::string_view RawInput,
    const GV2ContentCore::FSourceSpan& Span,
    std::size_t& OutStartByte,
    std::size_t& OutEndByte);

enum class ESetFieldValueStatus
{
    Success,
    InvalidJson5,
    PointerNotFound,
    TargetIsContainer,
    InvalidValue,
    SpanMappingFailed,
};

struct GV2_CONTENT_AUTHORING_API FSetFieldValueResult
{
    ESetFieldValueStatus Status = ESetFieldValueStatus::Success;
    std::string UpdatedContent;
    std::string ErrorCode;
    std::string ErrorMessage;
};

GV2_CONTENT_AUTHORING_API FSetFieldValueResult SetFieldValue(
    const std::string& OriginalContent,
    const std::string& JsonPointer,
    const GV2ContentCore::FValue& NewValue,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

enum class ERemoveDefinitionStatus
{
    Success,
    InvalidJson5,
    DefinitionNotFound,
    SpanMappingFailed,
};

struct GV2_CONTENT_AUTHORING_API FRemoveDefinitionResult
{
    ERemoveDefinitionStatus Status = ERemoveDefinitionStatus::Success;
    std::string UpdatedContent;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::size_t RemovedIndex = 0;
};

GV2_CONTENT_AUTHORING_API FRemoveDefinitionResult RemoveDefinitionEntry(
    const std::string& OriginalContent,
    const std::string& DefinitionId,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

GV2_CONTENT_AUTHORING_API bool ApplyFieldOpToDefinitionValue(
    GV2ContentCore::FValue& DefValue,
    const std::string& RawPointer,
    const FFieldOp& Op,
    std::string& OutError);

} // namespace GV2ContentAuthoring
