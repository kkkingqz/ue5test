#include "UI/GV2PreparedUiValue.h"

// --- FGV2PreparedUiObject ---

FGV2PreparedUiObject::FGV2PreparedUiObject(TArray<TPair<FString, FGV2PreparedUiValue>> InFields)
    : Fields(MoveTemp(InFields))
{
    // Canonical sort: strictly lexicographical by property name
    Fields.Sort([](const TPair<FString, FGV2PreparedUiValue>& A, const TPair<FString, FGV2PreparedUiValue>& B)
    {
        return A.Key < B.Key;
    });
}

TSharedRef<const FGV2PreparedUiObject> FGV2PreparedUiObject::Create(TArray<TPair<FString, FGV2PreparedUiValue>> InFields)
{
    return MakeShareable(new FGV2PreparedUiObject(MoveTemp(InFields)));
}

TSharedRef<const FGV2PreparedUiObject> FGV2PreparedUiObject::Create(const TMap<FString, FGV2PreparedUiValue>& InFields)
{
    TArray<TPair<FString, FGV2PreparedUiValue>> Pairs;
    Pairs.Reserve(InFields.Num());
    for (const auto& Entry : InFields)
    {
        Pairs.Emplace(Entry.Key, Entry.Value);
    }
    return Create(MoveTemp(Pairs));
}

const FGV2PreparedUiValue* FGV2PreparedUiObject::FindField(const FString& InName) const
{
    for (const auto& Field : Fields)
    {
        if (Field.Key == InName)
        {
            return &Field.Value;
        }
    }
    return nullptr;
}

bool FGV2PreparedUiObject::operator==(const FGV2PreparedUiObject& Other) const
{
    if (Fields.Num() != Other.Fields.Num())
    {
        return false;
    }

    for (int32 Index = 0; Index < Fields.Num(); ++Index)
    {
        if (Fields[Index].Key != Other.Fields[Index].Key
            || Fields[Index].Value != Other.Fields[Index].Value)
        {
            return false;
        }
    }
    return true;
}

FString FGV2PreparedUiObject::ToDebugString(const FString& PropertyPath) const
{
    TArray<FString> Lines;
    for (const auto& Field : Fields)
    {
        const FString ChildPath = PropertyPath.IsEmpty()
            ? Field.Key
            : FString::Printf(TEXT("%s.%s"), *PropertyPath, *Field.Key);
        Lines.Add(Field.Value.ToDebugString(ChildPath));
    }
    return FString::Join(Lines, TEXT("\n"));
}

// --- FGV2PreparedUiArray ---

FGV2PreparedUiArray::FGV2PreparedUiArray(TArray<FGV2PreparedUiValue> InElements)
    : Elements(MoveTemp(InElements))
{
}

TSharedRef<const FGV2PreparedUiArray> FGV2PreparedUiArray::Create(TArray<FGV2PreparedUiValue> InElements)
{
    return MakeShareable(new FGV2PreparedUiArray(MoveTemp(InElements)));
}

bool FGV2PreparedUiArray::operator==(const FGV2PreparedUiArray& Other) const
{
    if (Elements.Num() != Other.Elements.Num())
    {
        return false;
    }

    for (int32 Index = 0; Index < Elements.Num(); ++Index)
    {
        if (Elements[Index] != Other.Elements[Index])
        {
            return false;
        }
    }
    return true;
}

FString FGV2PreparedUiArray::ToDebugString(const FString& PropertyPath) const
{
    TArray<FString> Lines;
    for (int32 Index = 0; Index < Elements.Num(); ++Index)
    {
        const FString ChildPath = PropertyPath.IsEmpty()
            ? FString::Printf(TEXT("[%d]"), Index)
            : FString::Printf(TEXT("%s[%d]"), *PropertyPath, Index);
        Lines.Add(Elements[Index].ToDebugString(ChildPath));
    }
    return FString::Join(Lines, TEXT("\n"));
}

// --- FGV2PreparedUiValue ---

EGV2PreparedUiValueKind FGV2PreparedUiValue::GetKind() const
{
    if (IsNull()) return EGV2PreparedUiValueKind::Null;
    if (IsBoolean()) return EGV2PreparedUiValueKind::Boolean;
    if (IsInteger()) return EGV2PreparedUiValueKind::Integer;
    if (IsNumber()) return EGV2PreparedUiValueKind::Number;
    if (IsString()) return EGV2PreparedUiValueKind::String;
    if (IsKey()) return EGV2PreparedUiValueKind::Key;
    if (IsText()) return EGV2PreparedUiValueKind::Text;
    if (IsStableId()) return EGV2PreparedUiValueKind::StableId;
    if (IsBinding()) return EGV2PreparedUiValueKind::Binding;
    if (IsObject()) return EGV2PreparedUiValueKind::Object;
    if (IsArray()) return EGV2PreparedUiValueKind::Array;

    return EGV2PreparedUiValueKind::Null;
}

bool FGV2PreparedUiValue::operator==(const FGV2PreparedUiValue& Other) const
{
    if (GetKind() != Other.GetKind())
    {
        return false;
    }

    switch (GetKind())
    {
    case EGV2PreparedUiValueKind::Null:
        return true;
    case EGV2PreparedUiValueKind::Boolean:
        return AsBoolean() == Other.AsBoolean();
    case EGV2PreparedUiValueKind::Integer:
        return AsInteger() == Other.AsInteger();
    case EGV2PreparedUiValueKind::Number:
        return FMath::IsNearlyEqual(AsNumber(), Other.AsNumber());
    case EGV2PreparedUiValueKind::String:
        return AsString() == Other.AsString();
    case EGV2PreparedUiValueKind::Key:
        return AsKeyStruct() == Other.AsKeyStruct();
    case EGV2PreparedUiValueKind::Text:
        return AsText() == Other.AsText();
    case EGV2PreparedUiValueKind::StableId:
        return AsStableId() == Other.AsStableId();
    case EGV2PreparedUiValueKind::Binding:
        return AsBinding() == Other.AsBinding();
    case EGV2PreparedUiValueKind::Object:
        return AsObject() == Other.AsObject();
    case EGV2PreparedUiValueKind::Array:
        return AsArray() == Other.AsArray();
    default:
        return false;
    }
}

FString FGV2PreparedUiValue::ToDebugString(const FString& PropertyPath) const
{
    const FString Prefix = PropertyPath.IsEmpty() ? TEXT("") : FString::Printf(TEXT("%s = "), *PropertyPath);

    switch (GetKind())
    {
    case EGV2PreparedUiValueKind::Null:
        return Prefix + TEXT("<null>");
    case EGV2PreparedUiValueKind::Boolean:
        return Prefix + (AsBoolean() ? TEXT("true") : TEXT("false"));
    case EGV2PreparedUiValueKind::Integer:
        return Prefix + FString::Printf(TEXT("%lld"), AsInteger());
    case EGV2PreparedUiValueKind::Number:
        return Prefix + FString::Printf(TEXT("%f"), AsNumber());
    case EGV2PreparedUiValueKind::String:
        return Prefix + FString::Printf(TEXT("\"%s\""), *AsString());
    case EGV2PreparedUiValueKind::Key:
        return Prefix + FString::Printf(TEXT("(Key)\"%s\""), *AsKey());
    case EGV2PreparedUiValueKind::Text:
        return Prefix + FString::Printf(TEXT("(Text)\"%s\""), *AsText().Text.ToString());
    case EGV2PreparedUiValueKind::StableId:
        return Prefix + FString::Printf(TEXT("(StableId:%s)\"%s\""), *AsStableId().TargetKind, *AsStableId().Id);
    case EGV2PreparedUiValueKind::Binding:
        return Prefix + FString::Printf(TEXT("(Binding)\"%s\""), *AsBinding().ToString());
    case EGV2PreparedUiValueKind::Object:
        if (PropertyPath.IsEmpty())
        {
            return AsObject().ToDebugString(TEXT("root"));
        }
        return AsObject().ToDebugString(PropertyPath);
    case EGV2PreparedUiValueKind::Array:
        if (PropertyPath.IsEmpty())
        {
            return AsArray().ToDebugString(TEXT("root"));
        }
        return AsArray().ToDebugString(PropertyPath);
    default:
        return Prefix + TEXT("<unknown>");
    }
}
