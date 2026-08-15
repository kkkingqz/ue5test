#include "GV2ContentCore/Testing/ValueModelConformance.h"

#include "GV2ContentCore/Value.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace GV2ContentCore::Testing
{
std::string RunValueModelConformance()
{
    // 1. Default & Null constructor
    FValue NullVal;
    if (NullVal.GetKind() != EValueKind::Null || !NullVal.IsNull())
    {
        return "value_model.default_constructor_is_null";
    }

    FValue ExplicitNull(nullptr);
    if (!ExplicitNull.IsNull() || NullVal != ExplicitNull)
    {
        return "value_model.explicit_null_is_null";
    }

    // 2. Boolean
    FValue BoolVal(true);
    if (BoolVal.GetKind() != EValueKind::Boolean || !BoolVal.AsBoolean() || BoolVal == NullVal)
    {
        return "value_model.boolean_kind_and_value";
    }

    // 3. Integer vs Double distinction
    FValue IntVal(static_cast<std::int64_t>(42));
    FValue DoubleVal(42.0);
    if (IntVal.GetKind() != EValueKind::Integer || DoubleVal.GetKind() != EValueKind::Number)
    {
        return "value_model.integer_vs_number_kind";
    }
    if (IntVal == DoubleVal)
    {
        return "value_model.integer_vs_number_not_equal";
    }
    if (IntVal.AsInteger() != 42 || DoubleVal.AsNumber() != 42.0)
    {
        return "value_model.as_integer_and_as_number";
    }

    // Type mismatch exceptions
    bool bCaughtLogicError = false;
    try
    {
        IntVal.AsBoolean();
    }
    catch (const std::logic_error&)
    {
        bCaughtLogicError = true;
    }
    if (!bCaughtLogicError)
    {
        return "value_model.type_mismatch_throws_logic_error";
    }

    // 4. Non-finite double exception
    bool bCaughtInvalidArgument = false;
    try
    {
        FValue InvalidDouble(std::numeric_limits<double>::quiet_NaN());
    }
    catch (const std::invalid_argument&)
    {
        bCaughtInvalidArgument = true;
    }
    if (!bCaughtInvalidArgument)
    {
        return "value_model.nan_throws_invalid_argument";
    }

    // 5. String
    FValue StrVal(std::string("Hello GV2"));
    if (StrVal.GetKind() != EValueKind::String || StrVal.AsString() != "Hello GV2")
    {
        return "value_model.string_kind_and_value";
    }

    FValue Utf8Str(std::string(reinterpret_cast<const char*>(u8"Привет, GV2")));
    if (!Utf8Str.IsString())
    {
        return "value_model.valid_utf8_accepted";
    }

    bool bRejectedInvalidUtf8 = false;
    try
    {
        FValue InvalidUtf8(std::string("\xc0\xaf", 2));
    }
    catch (const std::invalid_argument&)
    {
        bRejectedInvalidUtf8 = true;
    }
    if (!bRejectedInvalidUtf8)
    {
        return "value_model.invalid_utf8_rejected";
    }

    // 6. Array & Object
    FValue ArrayVal = FValue::MakeArray({ FValue(1), FValue(2), FValue("three") });
    if (ArrayVal.GetKind() != EValueKind::Array || ArrayVal.AsArray().size() != 3)
    {
        return "value_model.array_kind_and_size";
    }

    FValue ObjectVal = FValue::MakeObject({
        { "name", FValue("Hero") },
        { "level", FValue(static_cast<std::int64_t>(10)) },
        { "stats", FValue::MakeObject({ { "hp", FValue(100.0) } }) }
    });
    if (ObjectVal.GetKind() != EValueKind::Object
        || ObjectVal.FindField("name") == nullptr
        || ObjectVal.FindField("name")->AsString() != "Hero")
    {
        return "value_model.object_find_field_and_nested";
    }

    const FValue* NestedHp = ObjectVal.FindField("stats") ? ObjectVal.FindField("stats")->FindField("hp") : nullptr;
    if (NestedHp == nullptr || NestedHp->AsNumber() != 100.0)
    {
        return "value_model.object_find_field_and_nested";
    }

    // 7. Copy and Move Semantics
    FValue CopyVal = ObjectVal;
    if (CopyVal != ObjectVal)
    {
        return "value_model.copy_semantics";
    }

    FValue MoveVal = std::move(CopyVal);
    if (MoveVal != ObjectVal || !CopyVal.IsNull())
    {
        return "value_model.move_semantics_and_moved_from_null";
    }
    CopyVal = FValue("reused");
    if (CopyVal.AsString() != "reused")
    {
        return "value_model.moved_from_reused";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
