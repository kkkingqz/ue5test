#pragma once

#include "GV2ContentCore/CanonicalHash.h"
#include "GV2ContentCore/Value.h"

#include <string>
#include <string_view>

namespace GV2RuntimeCore::Internal
{

inline bool ReadRequiredCanonicalSha256Field(
    const GV2ContentCore::FValue& Root,
    std::string_view FieldName,
    std::string& OutHash,
    std::string& OutError,
    std::string_view ErrorCode)
{
    const auto* FieldVal = Root.FindField(FieldName);
    if (FieldVal == nullptr || !FieldVal->IsString() || !GV2ContentCore::IsCanonicalSha256(FieldVal->AsString()))
    {
        OutError = std::string(ErrorCode);
        return false;
    }
    OutHash = FieldVal->AsString();
    return true;
}

inline bool ReadOptionalCanonicalSha256Field(
    const GV2ContentCore::FValue& Root,
    std::string_view FieldName,
    std::string& OutHash,
    std::string& OutError,
    std::string_view ErrorCode)
{
    const auto* FieldVal = Root.FindField(FieldName);
    if (FieldVal == nullptr)
    {
        OutHash.clear();
        return true;
    }
    if (!FieldVal->IsString())
    {
        OutError = std::string(ErrorCode);
        return false;
    }
    const std::string& Str = FieldVal->AsString();
    if (!Str.empty() && !GV2ContentCore::IsCanonicalSha256(Str))
    {
        OutError = std::string(ErrorCode);
        return false;
    }
    OutHash = Str;
    return true;
}

} // namespace GV2RuntimeCore::Internal
