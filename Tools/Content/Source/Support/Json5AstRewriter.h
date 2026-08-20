#pragma once

#include "GV2ContentAuthoring/Json5AstRewriter.h"

namespace GV2ContentCli
{
using GV2ContentAuthoring::IsValidJson5Identifier;
using GV2ContentAuthoring::FormatJson5Value;
using GV2ContentAuthoring::GeneratePlaceholderValue;
using GV2ContentAuthoring::FormatDefinitionEntry;
using GV2ContentAuthoring::CreateNewDefinitionFileContent;
using GV2ContentAuthoring::InsertDefinitionEntryIntoJson5;
using GV2ContentAuthoring::FRenameStringTokenResult;
using GV2ContentAuthoring::ReplaceStringTokens;
using GV2ContentAuthoring::SourcePositionToByteOffset;
using GV2ContentAuthoring::SourceSpanToByteRange;
using GV2ContentAuthoring::ESetFieldValueStatus;
using GV2ContentAuthoring::FSetFieldValueResult;
using GV2ContentAuthoring::SetFieldValue;
using GV2ContentAuthoring::ERemoveDefinitionStatus;
using GV2ContentAuthoring::FRemoveDefinitionResult;
using GV2ContentAuthoring::RemoveDefinitionEntry;
} // namespace GV2ContentCli
