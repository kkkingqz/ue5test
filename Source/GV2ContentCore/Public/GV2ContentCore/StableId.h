#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace GV2ContentCore
{
enum class EStableIdError
{
    None,
    Empty,
    TooLong,
    NonAscii,
    InvalidCharacter,
    InvalidSegmentStart,
    EmptySegment,
    InvalidSeparator,
    WrongKind,
    InvalidInstanceId,
};

struct FStableIdView
{
    std::string_view Namespace;
    std::string_view Kind;
    std::string_view Path;
};

struct FInstanceIdView
{
    std::string_view Kind;
    uint64_t Counter = 0;
};

class GV2_CONTENT_CORE_API FStableId
{
public:
    static constexpr std::size_t MaxSegmentLength = 64;
    static constexpr std::size_t MaxLength = 192;

    static bool IsValidSegment(
        std::string_view Value,
        EStableIdError* OutError = nullptr);

    static bool Parse(
        std::string_view Value,
        FStableIdView& OutId,
        EStableIdError* OutError = nullptr);

    static bool IsValid(
        std::string_view Value,
        EStableIdError* OutError = nullptr);

    static bool IsOfKind(
        std::string_view Value,
        std::string_view ExpectedKind,
        EStableIdError* OutError = nullptr);

    static bool ParseInstanceId(
        std::string_view Value,
        FInstanceIdView& OutId,
        EStableIdError* OutError = nullptr);

    static bool IsValidInstanceId(
        std::string_view Value,
        EStableIdError* OutError = nullptr);
};
}
