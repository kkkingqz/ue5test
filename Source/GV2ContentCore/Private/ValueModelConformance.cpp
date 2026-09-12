#include "GV2ContentCore/Testing/ValueModelConformance.h"

#include "GV2ContentCore/CanonicalHash.h"
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

    // 3. Integer vs Double distinction and Negative Zero Normalization
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

    // Zero distinction: Integer 0 != Number 0.0, different kinds
    FValue IntZero(static_cast<std::int64_t>(0));
    FValue PosZero(0.0);
    FValue NegZero(-0.0);
    if (IntZero.GetKind() != EValueKind::Integer || PosZero.GetKind() != EValueKind::Number || NegZero.GetKind() != EValueKind::Number)
    {
        return "value_model.zero_kind_distinction";
    }
    if (IntZero == PosZero || IntZero == NegZero)
    {
        return "value_model.integer_zero_not_equal_number_zero";
    }

    // Negative zero normalizes to positive zero: signbit is false, values equal, canonical hashes identical
    if (!(PosZero == NegZero) || PosZero != NegZero)
    {
        return "value_model.positive_and_negative_zero_must_equal";
    }
    if (std::signbit(PosZero.AsNumber()) || std::signbit(NegZero.AsNumber()))
    {
        return "value_model.negative_zero_signbit_not_cleared";
    }

    const std::string PosZeroHash = ComputeCanonicalHash(PosZero);
    const std::string NegZeroHash = ComputeCanonicalHash(NegZero);
    if (PosZeroHash.empty() || PosZeroHash != NegZeroHash)
    {
        return "value_model.zero_canonical_hash_must_match";
    }

    // Factory method MakeNumber normalization
    FValue PosZeroMake = FValue::MakeNumber(0.0);
    FValue NegZeroMake = FValue::MakeNumber(-0.0);
    if (!(PosZeroMake == NegZeroMake) || std::signbit(NegZeroMake.AsNumber()))
    {
        return "value_model.make_number_negative_zero_normalization";
    }
    if (ComputeCanonicalHash(PosZeroMake) != ComputeCanonicalHash(NegZeroMake))
    {
        return "value_model.make_number_zero_hash_must_match";
    }

    // Nested array/object zero normalization
    FValue ArrayPosZero = FValue::MakeArray({ FValue(0.0) });
    FValue ArrayNegZero = FValue::MakeArray({ FValue(-0.0) });
    if (ComputeCanonicalHash(ArrayPosZero) != ComputeCanonicalHash(ArrayNegZero))
    {
        return "value_model.array_zero_hash_must_match";
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

    // 4. Non-finite double exceptions (quiet_NaN, signaling_NaN, +Infinity, -Infinity)
    for (const double NonFinite : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::signaling_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()
         })
    {
        bool bCaughtNonFinite = false;
        try
        {
            FValue InvalidDouble(NonFinite);
        }
        catch (const std::invalid_argument&)
        {
            bCaughtNonFinite = true;
        }
        if (!bCaughtNonFinite)
        {
            return "value_model.non_finite_throws_invalid_argument";
        }

        bool bCaughtMakeNonFinite = false;
        try
        {
            FValue::MakeNumber(NonFinite);
        }
        catch (const std::invalid_argument&)
        {
            bCaughtMakeNonFinite = true;
        }
        if (!bCaughtMakeNonFinite)
        {
            return "value_model.make_number_non_finite_throws_invalid_argument";
        }
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

    // 8. Canonical SHA-256 validation
    const std::string ValidSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    if (!IsCanonicalSha256(ValidSha))
    {
        return "value_model.valid_sha256_must_pass";
    }
    if (!IsCanonicalSha256(std::string(64, '0')) || !IsCanonicalSha256(std::string(64, 'f')))
    {
        return "value_model.boundary_sha256_must_pass";
    }
    // Invalid cases
    if (IsCanonicalSha256(""))
    {
        return "value_model.empty_sha256_must_fail";
    }
    if (IsCanonicalSha256(std::string(63, 'a')))
    {
        return "value_model.sha256_length_63_must_fail";
    }
    if (IsCanonicalSha256(std::string(65, 'a')))
    {
        return "value_model.sha256_length_65_must_fail";
    }
    if (IsCanonicalSha256("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"))
    {
        return "value_model.uppercase_sha256_must_fail";
    }
    if (IsCanonicalSha256(std::string(64, 'z')))
    {
        return "value_model.non_hex_sha256_must_fail";
    }
    if (IsCanonicalSha256("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"))
    {
        return "value_model.char_g_sha256_must_fail";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
