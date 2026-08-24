#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2PreparedUiValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PreparedUiValueTest,
    "GV2.UI.PreparedUiValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PreparedUiValueTest::RunTest(const FString& Parameters)
{
    // 1. All kinds creation and type checking
    {
        const FGV2PreparedUiValue NullVal = FGV2PreparedUiValue::MakeNull();
        TestTrue(TEXT("Null kind"), NullVal.IsNull() && NullVal.GetKind() == EGV2PreparedUiValueKind::Null);

        const FGV2PreparedUiValue BoolVal = FGV2PreparedUiValue::MakeBoolean(true);
        TestTrue(TEXT("Bool kind"), BoolVal.IsBoolean() && BoolVal.AsBoolean() == true);

        const FGV2PreparedUiValue IntVal = FGV2PreparedUiValue::MakeInteger(42);
        TestTrue(TEXT("Integer kind"), IntVal.IsInteger() && IntVal.AsInteger() == 42);

        const FGV2PreparedUiValue NumVal = FGV2PreparedUiValue::MakeNumber(3.1415);
        TestTrue(TEXT("Number kind"), NumVal.IsNumber() && FMath::IsNearlyEqual(NumVal.AsNumber(), 3.1415));

        const FGV2PreparedUiValue StrVal = FGV2PreparedUiValue::MakeString(TEXT("hello"));
        TestTrue(TEXT("String kind"), StrVal.IsString() && StrVal.AsString() == TEXT("hello"));

        const FGV2PreparedUiValue KeyVal = FGV2PreparedUiValue::MakeKey(TEXT("button_key"));
        TestTrue(TEXT("Key kind"), KeyVal.IsKey() && KeyVal.AsKey() == TEXT("button_key"));

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("Sample Text"));
        TextModel.StyleToken = FName(TEXT("title"));
        const FGV2PreparedUiValue TextVal = FGV2PreparedUiValue::MakeText(TextModel);
        TestTrue(TEXT("Text kind"), TextVal.IsText() && TextVal.AsText().Text.EqualTo(FText::FromString(TEXT("Sample Text"))));

        const FGV2PreparedUiValue StableIdVal = FGV2PreparedUiValue::MakeStableId(TEXT("core:resource.icon"), TEXT("resource"));
        TestTrue(TEXT("StableId kind"), StableIdVal.IsStableId() && StableIdVal.AsStableId().Id == TEXT("core:resource.icon") && StableIdVal.AsStableId().TargetKind == TEXT("resource"));

        const FGV2PreparedUiValue BindingVal = FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("runtime@1:10")));
        TestTrue(TEXT("Binding kind"), BindingVal.IsBinding() && BindingVal.AsBinding().ToString() == TEXT("runtime@1:10"));
    }

    // 2. Key and String are strictly distinct kinds without coercion
    {
        const FGV2PreparedUiValue StrVal = FGV2PreparedUiValue::MakeString(TEXT("my_identity"));
        const FGV2PreparedUiValue KeyVal = FGV2PreparedUiValue::MakeKey(TEXT("my_identity"));

        TestTrue(TEXT("String is not Key"), !StrVal.IsKey() && StrVal.IsString());
        TestTrue(TEXT("Key is not String"), !KeyVal.IsString() && KeyVal.IsKey());
        TestTrue(TEXT("Key != String even with same string payload"), StrVal != KeyVal);
    }

    // 3. Canonical object property iteration order independent of insertion order
    {
        TArray<TPair<FString, FGV2PreparedUiValue>> FieldsOrder1;
        FieldsOrder1.Emplace(TEXT("zebra"), FGV2PreparedUiValue::MakeInteger(1));
        FieldsOrder1.Emplace(TEXT("alpha"), FGV2PreparedUiValue::MakeInteger(2));
        FieldsOrder1.Emplace(TEXT("middle"), FGV2PreparedUiValue::MakeInteger(3));

        TArray<TPair<FString, FGV2PreparedUiValue>> FieldsOrder2;
        FieldsOrder2.Emplace(TEXT("alpha"), FGV2PreparedUiValue::MakeInteger(2));
        FieldsOrder2.Emplace(TEXT("middle"), FGV2PreparedUiValue::MakeInteger(3));
        FieldsOrder2.Emplace(TEXT("zebra"), FGV2PreparedUiValue::MakeInteger(1));

        const TSharedRef<const FGV2PreparedUiObject> Obj1 = FGV2PreparedUiObject::Create(MoveTemp(FieldsOrder1));
        const TSharedRef<const FGV2PreparedUiObject> Obj2 = FGV2PreparedUiObject::Create(MoveTemp(FieldsOrder2));

        TestEqual(TEXT("Obj1 == Obj2"), *Obj1, *Obj2);

        TArray<FString> KeysTraversed1;
        for (const auto& Field : *Obj1)
        {
            KeysTraversed1.Add(Field.Key);
        }

        TArray<FString> KeysTraversed2;
        for (const auto& Field : *Obj2)
        {
            KeysTraversed2.Add(Field.Key);
        }

        TestEqual(TEXT("Keys count"), KeysTraversed1.Num(), 3);
        TestEqual(TEXT("Keys canonical order 1"), KeysTraversed1[0], TEXT("alpha"));
        TestEqual(TEXT("Keys canonical order 2"), KeysTraversed1[1], TEXT("middle"));
        TestEqual(TEXT("Keys canonical order 3"), KeysTraversed1[2], TEXT("zebra"));
        TestEqual(TEXT("Both traversal orders match"), KeysTraversed1, KeysTraversed2);
    }

    // 4. Array creation and access
    {
        TArray<FGV2PreparedUiValue> Items;
        Items.Add(FGV2PreparedUiValue::MakeInteger(100));
        Items.Add(FGV2PreparedUiValue::MakeInteger(200));

        const TSharedRef<const FGV2PreparedUiArray> Arr = FGV2PreparedUiArray::Create(MoveTemp(Items));
        TestEqual(TEXT("Array count"), Arr->Num(), 2);
        TestEqual(TEXT("Array item 0"), (*Arr)[0].AsInteger(), int64(100));
        TestEqual(TEXT("Array item 1"), (*Arr)[1].AsInteger(), int64(200));

        const FGV2PreparedUiValue ArrVal = FGV2PreparedUiValue::MakeArray(Arr);
        TestTrue(TEXT("Array value kind"), ArrVal.IsArray());
    }

    // 5. ToDebugString with full property_path
    {
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("name"), FGV2PreparedUiValue::MakeString(TEXT("Hero")));
        Fields.Emplace(TEXT("level"), FGV2PreparedUiValue::MakeInteger(10));
        Fields.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("hero_1")));

        const TSharedRef<const FGV2PreparedUiObject> RootObj = FGV2PreparedUiObject::Create(MoveTemp(Fields));
        const FGV2PreparedUiValue RootVal = FGV2PreparedUiValue::MakeObject(RootObj);

        const FString DebugStr = RootVal.ToDebugString(TEXT("screen.player"));
        TestTrue(TEXT("Debug string contains hero_1 key"), DebugStr.Contains(TEXT("screen.player.key = (Key)\"hero_1\"")));
        TestTrue(TEXT("Debug string contains level"), DebugStr.Contains(TEXT("screen.player.level = 10")));
        TestTrue(TEXT("Debug string contains name"), DebugStr.Contains(TEXT("screen.player.name = \"Hero\"")));
    }

    return true;
}

#endif
