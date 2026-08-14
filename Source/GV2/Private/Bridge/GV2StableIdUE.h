#pragma once

#include "GV2RuntimeCore/GV2StableId.h"

#include "Containers/StringConv.h"
#include "Containers/StringView.h"

#include <string>

namespace GV2StableIdUE
{
inline std::string ToUtf8(const FStringView Value)
{
    const FString Owned(Value);
    const FTCHARToUTF8 Converted(*Owned);
    return std::string(Converted.Get(), Converted.Length());
}

inline bool IsValidSegment(const FStringView Value)
{
    return GV2RuntimeCore::FStableId::IsValidSegment(ToUtf8(Value));
}

inline bool IsOfKind(const FStringView Value, const std::string_view ExpectedKind)
{
    return GV2RuntimeCore::FStableId::IsOfKind(ToUtf8(Value), ExpectedKind);
}
}
