#include "UI/GV2UiCapability.h"
#include "Blueprint/UserWidget.h"

// --- FGV2UiPropertyCapability ---

bool FGV2UiPropertyCapability::operator==(const FGV2UiPropertyCapability& Other) const
{
    return PropertyName == Other.PropertyName
        && SupportedKind == Other.SupportedKind
        && TargetType == Other.TargetType
        && TargetName == Other.TargetName
        && TargetKind == Other.TargetKind
        && IntMin == Other.IntMin
        && IntMax == Other.IntMax
        && NumberMin == Other.NumberMin
        && NumberMax == Other.NumberMax
        && bRequiresKeyedIdentity == Other.bRequiresKeyedIdentity
        && KeyPropertyName == Other.KeyPropertyName
        && EntryWidgetClass == Other.EntryWidgetClass
        && ChildCapabilityName == Other.ChildCapabilityName;
}

// --- FGV2UiCapabilityBuilder ---

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddText(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Text;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddImage(const FString& Name, const FName& TargetName, const FString& TargetKind)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::StableId;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.TargetKind = TargetKind;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddBoolean(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Boolean;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddInteger(const FString& Name, const FName& TargetName, TOptional<int64> Min, TOptional<int64> Max)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Integer;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.IntMin = Min;
    Cap.IntMax = Max;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddNumber(const FString& Name, const FName& TargetName, TOptional<double> Min, TOptional<double> Max)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Number;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.NumberMin = Min;
    Cap.NumberMax = Max;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddString(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::String;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKey(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Key;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddBinding(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Binding;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKeyedCollection(
    const FString& Name,
    const FName& TargetName,
    FGV2UiPropertyCapability ItemCapability,
    const FString& KeyField,
    TSubclassOf<UUserWidget> EntryWidgetClass)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
    Cap.TargetType = EGV2UiCapabilityTargetType::CollectionHost;
    Cap.TargetName = TargetName;
    Cap.bRequiresKeyedIdentity = true;
    Cap.KeyPropertyName = KeyField;
    Cap.EntryWidgetClass = EntryWidgetClass;
    Cap.ItemCapability = MakeShared<FGV2UiPropertyCapability>(MoveTemp(ItemCapability));
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKeyedCollection(
    const FString& Name,
    const FName& TargetName,
    FGV2UiCapabilityTree ItemCapabilityTree,
    const FString& KeyField,
    TSubclassOf<UUserWidget> EntryWidgetClass)
{
    FGV2UiPropertyCapability ItemCap;
    ItemCap.SupportedKind = EGV2PreparedUiValueKind::Object;
    ItemCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    ItemCap.ChildTree = MakeShared<FGV2UiCapabilityTree>(MoveTemp(ItemCapabilityTree));
    ItemCap.EntryWidgetClass = EntryWidgetClass;
    return AddKeyedCollection(Name, TargetName, MoveTemp(ItemCap), KeyField, EntryWidgetClass);
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddNestedScreenCollection(
    const FString& Name,
    const FName& TargetName,
    const FString& KeyField)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
    Cap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;
    Cap.TargetName = TargetName;
    Cap.bRequiresKeyedIdentity = true;
    Cap.KeyPropertyName = KeyField;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddCustom(
    const FString& Name,
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FName& TargetName)
{
    checkf(Kind != EGV2PreparedUiValueKind::Object && Kind != EGV2PreparedUiValueKind::Null,
        TEXT("Cannot declare capability with inapplicable kind (Null or Object)"));

    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = Kind;
    Cap.TargetType = TargetType;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::SetChildCapabilityName(const FString& Name, const FString& ChildCapabilityName)
{
    if (FGV2UiPropertyCapability* Cap = Tree.Properties.Find(Name))
    {
        Cap->ChildCapabilityName = ChildCapabilityName;
    }
    return *this;
}
