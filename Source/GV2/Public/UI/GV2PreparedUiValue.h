#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "Bridge/GV2BridgeTypes.h"

enum class EGV2PreparedUiValueKind : uint8
{
    Null,
    Boolean,
    Integer,
    Number,
    String,
    Key,
    Text,
    StableId,
    Binding,
    Object,
    Array,
    Count
};

struct GV2_API FGV2PreparedUiKey
{
    FString Value;

    bool operator==(const FGV2PreparedUiKey& Other) const
    {
        return Value == Other.Value;
    }
    bool operator!=(const FGV2PreparedUiKey& Other) const
    {
        return Value != Other.Value;
    }
};

struct GV2_API FGV2PreparedUiStableId
{
    FString Id;
    FString TargetKind;

    bool operator==(const FGV2PreparedUiStableId& Other) const
    {
        return Id == Other.Id && TargetKind == Other.TargetKind;
    }
    bool operator!=(const FGV2PreparedUiStableId& Other) const
    {
        return !(*this == Other);
    }
};

// FGV2PreparedUiObject holds FGV2PreparedUiValue by value (TArray<TPair<...>>),
// and FGV2PreparedUiValue holds FGV2PreparedUiObject/FGV2PreparedUiArray only through
// TSharedRef (pointer-like, complete type not required). Breaking the cycle this way
// means FGV2PreparedUiValue must be defined first; the two container classes are
// defined below it once it is a complete type. AsObject()/AsArray(), the only members
// that dereference the shared containers, are declared here and defined in the .cpp
// after both container classes are complete.
class FGV2PreparedUiObject;
class FGV2PreparedUiArray;

/**
 * Immutable prepared UI value node based on TVariant.
 * Kinds: Null, Boolean, Integer, Number, String, Key, Text, StableId, Binding, Object, Array.
 * Key and String are distinct kinds with no implicit coercion.
 */
class GV2_API FGV2PreparedUiValue final
{
public:
    using FVariantType = TVariant<
        FEmptyVariantState,
        bool,
        int64,
        double,
        FString,
        FGV2PreparedUiKey,
        FGV2TextViewModel,
        FGV2PreparedUiStableId,
        FGV2UiBindingHandle,
        TSharedRef<const FGV2PreparedUiObject>,
        TSharedRef<const FGV2PreparedUiArray>
    >;

    FGV2PreparedUiValue() : Storage(TInPlaceType<FEmptyVariantState>()) {}

    static FGV2PreparedUiValue MakeNull() { return FGV2PreparedUiValue(); }
    static FGV2PreparedUiValue MakeBoolean(bool InValue) { return FGV2PreparedUiValue(InValue); }
    static FGV2PreparedUiValue MakeInteger(int64 InValue) { return FGV2PreparedUiValue(InValue); }
    static FGV2PreparedUiValue MakeNumber(double InValue) { return FGV2PreparedUiValue(InValue); }
    static FGV2PreparedUiValue MakeString(FString InValue) { return FGV2PreparedUiValue(MoveTemp(InValue)); }
    static FGV2PreparedUiValue MakeKey(FString InKey) { return FGV2PreparedUiValue(FGV2PreparedUiKey{ MoveTemp(InKey) }); }
    static FGV2PreparedUiValue MakeText(FGV2TextViewModel InText) { return FGV2PreparedUiValue(MoveTemp(InText)); }
    static FGV2PreparedUiValue MakeStableId(FString InId, FString InTargetKind = TEXT("")) { return FGV2PreparedUiValue(FGV2PreparedUiStableId{ MoveTemp(InId), MoveTemp(InTargetKind) }); }
    static FGV2PreparedUiValue MakeBinding(FGV2UiBindingHandle InBinding) { return FGV2PreparedUiValue(MoveTemp(InBinding)); }
    static FGV2PreparedUiValue MakeObject(TSharedRef<const FGV2PreparedUiObject> InObject) { return FGV2PreparedUiValue(MoveTemp(InObject)); }
    static FGV2PreparedUiValue MakeArray(TSharedRef<const FGV2PreparedUiArray> InArray) { return FGV2PreparedUiValue(MoveTemp(InArray)); }

    EGV2PreparedUiValueKind GetKind() const;

    bool IsNull() const { return Storage.IsType<FEmptyVariantState>(); }
    bool IsBoolean() const { return Storage.IsType<bool>(); }
    bool IsInteger() const { return Storage.IsType<int64>(); }
    bool IsNumber() const { return Storage.IsType<double>(); }
    bool IsString() const { return Storage.IsType<FString>(); }
    bool IsKey() const { return Storage.IsType<FGV2PreparedUiKey>(); }
    bool IsText() const { return Storage.IsType<FGV2TextViewModel>(); }
    bool IsStableId() const { return Storage.IsType<FGV2PreparedUiStableId>(); }
    bool IsBinding() const { return Storage.IsType<FGV2UiBindingHandle>(); }
    bool IsObject() const { return Storage.IsType<TSharedRef<const FGV2PreparedUiObject>>(); }
    bool IsArray() const { return Storage.IsType<TSharedRef<const FGV2PreparedUiArray>>(); }

    bool AsBoolean() const { check(IsBoolean()); return Storage.Get<bool>(); }
    int64 AsInteger() const { check(IsInteger()); return Storage.Get<int64>(); }
    double AsNumber() const { check(IsNumber()); return Storage.Get<double>(); }
    const FString& AsString() const { check(IsString()); return Storage.Get<FString>(); }
    const FString& AsKey() const { check(IsKey()); return Storage.Get<FGV2PreparedUiKey>().Value; }
    const FGV2PreparedUiKey& AsKeyStruct() const { check(IsKey()); return Storage.Get<FGV2PreparedUiKey>(); }
    const FGV2TextViewModel& AsText() const { check(IsText()); return Storage.Get<FGV2TextViewModel>(); }
    const FGV2PreparedUiStableId& AsStableId() const { check(IsStableId()); return Storage.Get<FGV2PreparedUiStableId>(); }
    const FGV2UiBindingHandle& AsBinding() const { check(IsBinding()); return Storage.Get<FGV2UiBindingHandle>(); }
    // Defined out-of-line below FGV2PreparedUiObject/FGV2PreparedUiArray: dereferencing
    // the shared container requires it to be a complete type at the point of use.
    const FGV2PreparedUiObject& AsObject() const;
    const FGV2PreparedUiArray& AsArray() const;

    TSharedRef<const FGV2PreparedUiObject> AsObjectRef() const { check(IsObject()); return Storage.Get<TSharedRef<const FGV2PreparedUiObject>>(); }
    TSharedRef<const FGV2PreparedUiArray> AsRefArray() const { check(IsArray()); return Storage.Get<TSharedRef<const FGV2PreparedUiArray>>(); }

    bool operator==(const FGV2PreparedUiValue& Other) const;
    bool operator!=(const FGV2PreparedUiValue& Other) const { return !(*this == Other); }

    FString ToDebugString(const FString& PropertyPath = TEXT("")) const;

private:
    explicit FGV2PreparedUiValue(bool InVal) : Storage(TInPlaceType<bool>(), InVal) {}
    explicit FGV2PreparedUiValue(int64 InVal) : Storage(TInPlaceType<int64>(), InVal) {}
    explicit FGV2PreparedUiValue(double InVal) : Storage(TInPlaceType<double>(), InVal) {}
    explicit FGV2PreparedUiValue(FString InVal) : Storage(TInPlaceType<FString>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(FGV2PreparedUiKey InVal) : Storage(TInPlaceType<FGV2PreparedUiKey>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(FGV2TextViewModel InVal) : Storage(TInPlaceType<FGV2TextViewModel>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(FGV2PreparedUiStableId InVal) : Storage(TInPlaceType<FGV2PreparedUiStableId>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(FGV2UiBindingHandle InVal) : Storage(TInPlaceType<FGV2UiBindingHandle>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(TSharedRef<const FGV2PreparedUiObject> InVal) : Storage(TInPlaceType<TSharedRef<const FGV2PreparedUiObject>>(), MoveTemp(InVal)) {}
    explicit FGV2PreparedUiValue(TSharedRef<const FGV2PreparedUiArray> InVal) : Storage(TInPlaceType<TSharedRef<const FGV2PreparedUiArray>>(), MoveTemp(InVal)) {}

    FVariantType Storage;
};

/**
 * Immutable ordered property collection.
 * Property iteration order is strictly canonical (lexicographical by property name)
 * and independent of parser key insertion order.
 */
class GV2_API FGV2PreparedUiObject final
{
public:
    FGV2PreparedUiObject() = default;

    static TSharedRef<const FGV2PreparedUiObject> Create(TArray<TPair<FString, FGV2PreparedUiValue>> InFields);
    static TSharedRef<const FGV2PreparedUiObject> Create(const TMap<FString, FGV2PreparedUiValue>& InFields);

    const FGV2PreparedUiValue* FindField(const FString& InName) const;
    int32 Num() const { return Fields.Num(); }
    bool IsEmpty() const { return Fields.IsEmpty(); }
    const TArray<TPair<FString, FGV2PreparedUiValue>>& GetFields() const { return Fields; }

    const TPair<FString, FGV2PreparedUiValue>* begin() const { return Fields.GetData(); }
    const TPair<FString, FGV2PreparedUiValue>* end() const { return Fields.GetData() + Fields.Num(); }

    bool operator==(const FGV2PreparedUiObject& Other) const;
    bool operator!=(const FGV2PreparedUiObject& Other) const { return !(*this == Other); }

    FString ToDebugString(const FString& PropertyPath = TEXT("")) const;

private:
    explicit FGV2PreparedUiObject(TArray<TPair<FString, FGV2PreparedUiValue>> InFields);

    TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
};

/**
 * Immutable ordered values array.
 */
class GV2_API FGV2PreparedUiArray final
{
public:
    FGV2PreparedUiArray() = default;

    static TSharedRef<const FGV2PreparedUiArray> Create(TArray<FGV2PreparedUiValue> InElements);

    int32 Num() const { return Elements.Num(); }
    bool IsEmpty() const { return Elements.IsEmpty(); }
    const FGV2PreparedUiValue& operator[](int32 Index) const { return Elements[Index]; }
    const TArray<FGV2PreparedUiValue>& GetElements() const { return Elements; }

    const FGV2PreparedUiValue* begin() const { return Elements.GetData(); }
    const FGV2PreparedUiValue* end() const { return Elements.GetData() + Elements.Num(); }

    bool operator==(const FGV2PreparedUiArray& Other) const;
    bool operator!=(const FGV2PreparedUiArray& Other) const { return !(*this == Other); }

    FString ToDebugString(const FString& PropertyPath = TEXT("")) const;

private:
    explicit FGV2PreparedUiArray(TArray<FGV2PreparedUiValue> InElements);

    TArray<FGV2PreparedUiValue> Elements;
};
