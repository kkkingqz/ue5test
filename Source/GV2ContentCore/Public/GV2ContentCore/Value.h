#pragma once

#include "GV2ContentCore/GV2ContentCore.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace GV2ContentCore
{
    enum class EValueKind : std::uint8_t
    {
        Null,
        Boolean,
        Integer,
        Number,
        String,
        Array,
        Object
    };

    class GV2_CONTENT_CORE_API FValue final
    {
    public:
        using FArray = std::vector<FValue>;
        using FObjectField = std::pair<std::string, FValue>;
        using FObject = std::vector<FObjectField>;

        FValue();
        FValue(std::nullptr_t);
        FValue(bool bInValue);
        FValue(std::int64_t InValue);
        FValue(int InValue);
        FValue(double InValue);
        FValue(std::string InValue);
        FValue(const char* InValue);
        FValue(FArray InValue);
        FValue(FObject InValue);

        FValue(const FValue& Other) = default;
        FValue(FValue&& Other) noexcept;
        ~FValue() = default;

        FValue& operator=(const FValue& Other);
        FValue& operator=(FValue&& Other) noexcept;

        bool operator==(const FValue& Other) const;
        bool operator!=(const FValue& Other) const;

        EValueKind GetKind() const;

        bool IsNull() const { return std::holds_alternative<std::monostate>(Storage); }
        bool IsBoolean() const { return std::holds_alternative<bool>(Storage); }
        bool IsInteger() const { return std::holds_alternative<std::int64_t>(Storage); }
        bool IsNumber() const { return std::holds_alternative<double>(Storage); }
        bool IsString() const { return std::holds_alternative<std::string>(Storage); }
        bool IsArray() const { return std::holds_alternative<FArray>(Storage); }
        bool IsObject() const { return std::holds_alternative<FObject>(Storage); }

        bool AsBoolean() const;
        std::int64_t AsInteger() const;
        double AsNumber() const;
        const std::string& AsString() const;
        const FArray& AsArray() const;
        const FObject& AsObject() const;

        FArray& AsArray();
        FObject& AsObject();

        const FValue* FindField(std::string_view FieldName) const;
        FValue* FindField(std::string_view FieldName);

        static FValue MakeNull();
        static FValue MakeBoolean(bool bInValue);
        static FValue MakeInteger(std::int64_t InValue);
        static FValue MakeNumber(double InValue);
        static FValue MakeString(std::string InValue);
        static FValue MakeArray(FArray InValue = {});
        static FValue MakeObject(FObject InValue = {});

    private:
        using FStorage = std::variant<
            std::monostate,
            bool,
            std::int64_t,
            double,
            std::string,
            FArray,
            FObject>;

        FStorage Storage;
    };
}
