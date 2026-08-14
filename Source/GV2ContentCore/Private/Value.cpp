#include "GV2ContentCore/Value.h"

#include <cmath>
#include <stdexcept>

namespace GV2ContentCore
{
namespace
{
bool IsValidUtf8(const std::string_view Value)
{
    std::size_t Index = 0;
    while (Index < Value.size())
    {
        const auto Lead = static_cast<unsigned char>(Value[Index]);
        if (Lead <= 0x7f)
        {
            ++Index;
            continue;
        }

        std::size_t ContinuationCount = 0;
        std::uint32_t CodePoint = 0;
        std::uint32_t MinimumCodePoint = 0;
        if (Lead >= 0xc2 && Lead <= 0xdf)
        {
            ContinuationCount = 1;
            CodePoint = Lead & 0x1f;
            MinimumCodePoint = 0x80;
        }
        else if (Lead >= 0xe0 && Lead <= 0xef)
        {
            ContinuationCount = 2;
            CodePoint = Lead & 0x0f;
            MinimumCodePoint = 0x800;
        }
        else if (Lead >= 0xf0 && Lead <= 0xf4)
        {
            ContinuationCount = 3;
            CodePoint = Lead & 0x07;
            MinimumCodePoint = 0x10000;
        }
        else
        {
            return false;
        }

        if (Index + ContinuationCount >= Value.size())
        {
            return false;
        }
        for (std::size_t Offset = 1; Offset <= ContinuationCount; ++Offset)
        {
            const auto Continuation = static_cast<unsigned char>(Value[Index + Offset]);
            if ((Continuation & 0xc0) != 0x80)
            {
                return false;
            }
            CodePoint = (CodePoint << 6) | (Continuation & 0x3f);
        }
        if (CodePoint < MinimumCodePoint
            || CodePoint > 0x10ffff
            || (CodePoint >= 0xd800 && CodePoint <= 0xdfff))
        {
            return false;
        }
        Index += ContinuationCount + 1;
    }
    return true;
}

void RequireValidUtf8(const std::string_view Value)
{
    if (!IsValidUtf8(Value))
    {
        throw std::invalid_argument("FValue string must be valid UTF-8");
    }
}
}

    FValue::FValue()
        : Storage(std::monostate{})
    {
    }

    FValue::FValue(std::nullptr_t)
        : Storage(std::monostate{})
    {
    }

    FValue::FValue(bool bInValue)
        : Storage(bInValue)
    {
    }

    FValue::FValue(std::int64_t InValue)
        : Storage(InValue)
    {
    }

    FValue::FValue(int InValue)
        : Storage(static_cast<std::int64_t>(InValue))
    {
    }

    FValue::FValue(double InValue)
        : Storage(InValue)
    {
        if (!std::isfinite(InValue))
        {
            throw std::invalid_argument("FValue double must be finite");
        }
    }

    FValue::FValue(std::string InValue)
        : Storage(std::move(InValue))
    {
        RequireValidUtf8(std::get<std::string>(Storage));
    }

    FValue::FValue(const char* InValue)
        : FValue(std::string(InValue != nullptr ? InValue : ""))
    {
    }

    FValue::FValue(FArray InValue)
        : Storage(std::move(InValue))
    {
    }

    FValue::FValue(FObject InValue)
        : Storage(std::move(InValue))
    {
        for (const FObjectField& Field : std::get<FObject>(Storage))
        {
            RequireValidUtf8(Field.first);
        }
    }

    FValue::FValue(FValue&& Other) noexcept
        : Storage(std::move(Other.Storage))
    {
        Other.Storage = std::monostate{};
    }

    FValue& FValue::operator=(const FValue& Other)
    {
        if (this != &Other)
        {
            Storage = Other.Storage;
        }
        return *this;
    }

    FValue& FValue::operator=(FValue&& Other) noexcept
    {
        if (this != &Other)
        {
            Storage = std::move(Other.Storage);
            Other.Storage = std::monostate{};
        }
        return *this;
    }

    bool FValue::operator==(const FValue& Other) const
    {
        return Storage == Other.Storage;
    }

    bool FValue::operator!=(const FValue& Other) const
    {
        return !(*this == Other);
    }

    EValueKind FValue::GetKind() const
    {
        return static_cast<EValueKind>(Storage.index());
    }

    bool FValue::AsBoolean() const
    {
        if (!IsBoolean())
        {
            throw std::logic_error("FValue is not a Boolean");
        }
        return std::get<bool>(Storage);
    }

    std::int64_t FValue::AsInteger() const
    {
        if (!IsInteger())
        {
            throw std::logic_error("FValue is not an Integer");
        }
        return std::get<std::int64_t>(Storage);
    }

    double FValue::AsNumber() const
    {
        if (!IsNumber())
        {
            throw std::logic_error("FValue is not a Number");
        }
        return std::get<double>(Storage);
    }

    const std::string& FValue::AsString() const
    {
        if (!IsString())
        {
            throw std::logic_error("FValue is not a String");
        }
        return std::get<std::string>(Storage);
    }

    const FValue::FArray& FValue::AsArray() const
    {
        if (!IsArray())
        {
            throw std::logic_error("FValue is not an Array");
        }
        return std::get<FArray>(Storage);
    }

    const FValue::FObject& FValue::AsObject() const
    {
        if (!IsObject())
        {
            throw std::logic_error("FValue is not an Object");
        }
        return std::get<FObject>(Storage);
    }

    FValue::FArray& FValue::AsArray()
    {
        if (!IsArray())
        {
            throw std::logic_error("FValue is not an Array");
        }
        return std::get<FArray>(Storage);
    }

    FValue::FObject& FValue::AsObject()
    {
        if (!IsObject())
        {
            throw std::logic_error("FValue is not an Object");
        }
        return std::get<FObject>(Storage);
    }

    const FValue* FValue::FindField(std::string_view FieldName) const
    {
        if (!IsObject())
        {
            return nullptr;
        }
        for (const auto& Field : std::get<FObject>(Storage))
        {
            if (Field.first == FieldName)
            {
                return &Field.second;
            }
        }
        return nullptr;
    }

    FValue* FValue::FindField(std::string_view FieldName)
    {
        if (!IsObject())
        {
            return nullptr;
        }
        for (auto& Field : std::get<FObject>(Storage))
        {
            if (Field.first == FieldName)
            {
                return &Field.second;
            }
        }
        return nullptr;
    }

    FValue FValue::MakeNull()
    {
        return FValue();
    }

    FValue FValue::MakeBoolean(bool bInValue)
    {
        return FValue(bInValue);
    }

    FValue FValue::MakeInteger(std::int64_t InValue)
    {
        return FValue(InValue);
    }

    FValue FValue::MakeNumber(double InValue)
    {
        return FValue(InValue);
    }

    FValue FValue::MakeString(std::string InValue)
    {
        return FValue(std::move(InValue));
    }

    FValue FValue::MakeArray(FArray InValue)
    {
        return FValue(std::move(InValue));
    }

    FValue FValue::MakeObject(FObject InValue)
    {
        return FValue(std::move(InValue));
    }

}
