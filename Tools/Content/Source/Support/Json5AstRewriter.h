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

} // namespace GV2ContentCli
