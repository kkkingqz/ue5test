#include "GV2ContentCore/StableId.h"

namespace GV2ContentCore
{
namespace
{
bool Fail(const EStableIdError Error, EStableIdError* OutError)
{
    if (OutError != nullptr)
    {
        *OutError = Error;
    }
    return false;
}

void Succeed(EStableIdError* OutError)
{
    if (OutError != nullptr)
    {
        *OutError = EStableIdError::None;
    }
}
}

bool FStableId::IsValidSegment(const std::string_view Value, EStableIdError* OutError)
{
    if (Value.empty())
    {
        return Fail(EStableIdError::EmptySegment, OutError);
    }
    if (Value.size() > MaxSegmentLength)
    {
        return Fail(EStableIdError::TooLong, OutError);
    }
    if (Value.front() < 'a' || Value.front() > 'z')
    {
        return Fail(
            static_cast<unsigned char>(Value.front()) > 0x7f
                ? EStableIdError::NonAscii
                : EStableIdError::InvalidSegmentStart,
            OutError);
    }
    for (const unsigned char Character : Value)
    {
        if (Character > 0x7f)
        {
            return Fail(EStableIdError::NonAscii, OutError);
        }
        const bool bLowercaseLetter = Character >= 'a' && Character <= 'z';
        const bool bDigit = Character >= '0' && Character <= '9';
        if (!bLowercaseLetter && !bDigit && Character != '_')
        {
            return Fail(EStableIdError::InvalidCharacter, OutError);
        }
    }
    Succeed(OutError);
    return true;
}

bool FStableId::Parse(
    const std::string_view Value,
    FStableIdView& OutId,
    EStableIdError* OutError)
{
    OutId = {};
    if (Value.empty())
    {
        return Fail(EStableIdError::Empty, OutError);
    }
    if (Value.size() > MaxLength)
    {
        return Fail(EStableIdError::TooLong, OutError);
    }

    const std::size_t Colon = Value.find(':');
    if (Colon == std::string_view::npos
        || Value.find(':', Colon + 1) != std::string_view::npos)
    {
        return Fail(EStableIdError::InvalidSeparator, OutError);
    }
    const std::size_t FirstDot = Value.find('.', Colon + 1);
    if (FirstDot == std::string_view::npos)
    {
        return Fail(EStableIdError::InvalidSeparator, OutError);
    }

    FStableIdView Candidate{
        Value.substr(0, Colon),
        Value.substr(Colon + 1, FirstDot - Colon - 1),
        Value.substr(FirstDot + 1),
    };
    if (!IsValidSegment(Candidate.Namespace, OutError)
        || !IsValidSegment(Candidate.Kind, OutError))
    {
        return false;
    }

    std::size_t SegmentStart = FirstDot + 1;
    while (SegmentStart <= Value.size())
    {
        const std::size_t Dot = Value.find('.', SegmentStart);
        const std::size_t SegmentEnd = Dot == std::string_view::npos ? Value.size() : Dot;
        if (!IsValidSegment(Value.substr(SegmentStart, SegmentEnd - SegmentStart), OutError))
        {
            return false;
        }
        if (Dot == std::string_view::npos)
        {
            break;
        }
        SegmentStart = Dot + 1;
    }

    OutId = Candidate;
    Succeed(OutError);
    return true;
}

bool FStableId::IsValid(const std::string_view Value, EStableIdError* OutError)
{
    FStableIdView Parsed;
    return Parse(Value, Parsed, OutError);
}

bool FStableId::IsOfKind(
    const std::string_view Value,
    const std::string_view ExpectedKind,
    EStableIdError* OutError)
{
    FStableIdView Parsed;
    if (!Parse(Value, Parsed, OutError))
    {
        return false;
    }
    if (Parsed.Kind != ExpectedKind)
    {
        return Fail(EStableIdError::WrongKind, OutError);
    }
    Succeed(OutError);
    return true;
}
}
