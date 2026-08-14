#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Value.h"
#include "Misc/AutomationTest.h"
#include <cmath>
#include <limits>
#include <stdexcept>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreValueModelTest,
    "GV2.Runtime.ContentCore.ValueModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreValueModelTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // 1. Default & Null constructor
    FValue NullVal;
    TestEqual(TEXT("Default constructor creates Null"), NullVal.GetKind(), EValueKind::Null);
    TestTrue(TEXT("IsNull returns true for Null"), NullVal.IsNull());

    FValue ExplicitNull(nullptr);
    TestTrue(TEXT("Explicit nullptr constructor creates Null"), ExplicitNull.IsNull());
    TestEqual(TEXT("Null values are equal"), NullVal, ExplicitNull);

    // 2. Boolean
    FValue BoolVal(true);
    TestEqual(TEXT("Boolean constructor sets kind"), BoolVal.GetKind(), EValueKind::Boolean);
    TestTrue(TEXT("AsBoolean returns true"), BoolVal.AsBoolean());
    TestNotEqual(TEXT("Boolean not equal to Null"), BoolVal, NullVal);

    // 3. Integer vs Double distinction
    FValue IntVal(static_cast<std::int64_t>(42));
    FValue DoubleVal(42.0);
    TestEqual(TEXT("IntVal kind is Integer"), IntVal.GetKind(), EValueKind::Integer);
    TestEqual(TEXT("DoubleVal kind is Number"), DoubleVal.GetKind(), EValueKind::Number);
    TestNotEqual(TEXT("Integer and Number are not equal despite same numeric value"), IntVal, DoubleVal);
    TestEqual(TEXT("AsInteger returns correct value"), IntVal.AsInteger(), static_cast<std::int64_t>(42));
    TestEqual(TEXT("AsNumber returns correct value"), DoubleVal.AsNumber(), 42.0);

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
    TestTrue(TEXT("Type mismatch throws std::logic_error"), bCaughtLogicError);

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
    TestTrue(TEXT("NaN double throws std::invalid_argument"), bCaughtInvalidArgument);

    // 5. String
    FValue StrVal(std::string("Hello GV2"));
    TestEqual(TEXT("String constructor sets kind"), StrVal.GetKind(), EValueKind::String);
    TestEqual(TEXT("AsString returns correct string"), FString(UTF8_TO_TCHAR(StrVal.AsString().c_str())), TEXT("Hello GV2"));

    FValue Utf8Str(std::string(reinterpret_cast<const char*>(u8"Привет, GV2")));
    TestTrue(TEXT("Valid UTF-8 is accepted"), Utf8Str.IsString());
    bool bRejectedInvalidUtf8 = false;
    try
    {
        FValue InvalidUtf8(std::string("\xc0\xaf", 2));
    }
    catch (const std::invalid_argument&)
    {
        bRejectedInvalidUtf8 = true;
    }
    TestTrue(TEXT("Invalid UTF-8 is rejected"), bRejectedInvalidUtf8);

    // 6. Array & Object
    FValue ArrayVal = FValue::MakeArray({ FValue(1), FValue(2), FValue("three") });
    TestEqual(TEXT("Array kind is Array"), ArrayVal.GetKind(), EValueKind::Array);
    TestEqual(TEXT("Array size is 3"), ArrayVal.AsArray().size(), static_cast<size_t>(3));

    FValue ObjectVal = FValue::MakeObject({
        { "name", FValue("Hero") },
        { "level", FValue(static_cast<std::int64_t>(10)) },
        { "stats", FValue::MakeObject({ { "hp", FValue(100.0) } }) }
    });
    TestEqual(TEXT("Object kind is Object"), ObjectVal.GetKind(), EValueKind::Object);
    TestNotNull(TEXT("FindField finds name"), ObjectVal.FindField("name"));
    TestEqual(TEXT("name value matches"), FString(UTF8_TO_TCHAR(ObjectVal.FindField("name")->AsString().c_str())), TEXT("Hero"));

    const FValue* NestedHp = ObjectVal.FindField("stats") ? ObjectVal.FindField("stats")->FindField("hp") : nullptr;
    TestNotNull(TEXT("Nested hp field exists"), NestedHp);
    if (NestedHp)
    {
        TestEqual(TEXT("Nested hp is 100.0"), NestedHp->AsNumber(), 100.0);
    }

    // 7. Copy and Move Semantics
    FValue CopyVal = ObjectVal;
    TestEqual(TEXT("Copy constructor creates equal value"), CopyVal, ObjectVal);

    FValue MoveVal = std::move(CopyVal);
    TestEqual(TEXT("Move value equals original ObjectVal"), MoveVal, ObjectVal);
    TestTrue(TEXT("Moved-from value becomes Null"), CopyVal.IsNull());
    CopyVal = FValue("reused");
    TestEqual(TEXT("Moved-from value can be safely reused"), CopyVal.AsString(), std::string("reused"));

    return true;
}

#endif
