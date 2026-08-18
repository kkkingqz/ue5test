#pragma once

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCli
{

bool IsValidJson5Identifier(std::string_view Key);

void FormatJson5Value(std::ostream& Out, const GV2ContentCore::FValue& Value, int IndentLevel);

GV2ContentCore::FValue GeneratePlaceholderValue(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const std::string& PackageId,
    const std::string& DefinitionType,
    const std::string& DefinitionIdPath,
    const std::string& FieldName);

std::string FormatDefinitionEntry(
    const std::string& DefinitionId,
    const GV2ContentCore::FValue& Data,
    int IndentLevel = 2);

std::string CreateNewDefinitionFileContent(
    std::int64_t SchemaVersion,
    const std::string& DefinitionType,
    const std::string& FormattedEntry);

bool InsertDefinitionEntryIntoJson5(
    const std::string& OriginalContent,
    const std::string& FormattedEntry,
    std::string& OutNewContent,
    std::string& OutErrorMessage);

struct FRenameStringTokenResult
{
    bool bSuccess = false;
    std::string UpdatedContent;
    std::size_t ReplacementsCount = 0;
    std::string ErrorMessage;
};

FRenameStringTokenResult ReplaceStringTokens(
    const std::string& RawContent,
    const std::string& OldValue,
    const std::string& NewValue,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

bool SourcePositionToByteOffset(
    std::string_view RawInput,
    std::uint32_t Line,
    std::uint32_t Column,
    std::size_t& OutByteOffset);

bool SourceSpanToByteRange(
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

struct FSetFieldValueResult
{
    ESetFieldValueStatus Status = ESetFieldValueStatus::Success;
    std::string UpdatedContent;
    std::string ErrorCode;
    std::string ErrorMessage;
};

FSetFieldValueResult SetFieldValue(
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

struct FRemoveDefinitionResult
{
    ERemoveDefinitionStatus Status = ERemoveDefinitionStatus::Success;
    std::string UpdatedContent;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::size_t RemovedIndex = 0;
};

FRemoveDefinitionResult RemoveDefinitionEntry(
    const std::string& OriginalContent,
    const std::string& DefinitionId,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

} // namespace GV2ContentCli
