#include "UI/GV2ListViewWidgetBase.h"

#include "UI/GV2UiTheme.h"
#include "UI/GV2UiCapability.h"

void UGV2ListViewWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ListViewWidgetBase::SetOrientation(EOrientation InOrientation)
{
    Orientation = InOrientation;
}

void UGV2ListViewWidgetBase::SetContainerPanel(UPanelWidget* InContainerPanel)
{
    ContainerPanel = InContainerPanel;
}

void UGV2ListViewWidgetBase::ClearEntries()
{
    if (ContainerPanel != nullptr)
    {
        ContainerPanel->ClearChildren();
    }
    ActiveWidgetsByKey.Reset();
}

TArray<UWidget*> UGV2ListViewWidgetBase::GetOrderedEntries() const
{
    TArray<UWidget*> Result;
    if (ContainerPanel != nullptr)
    {
        const int32 ChildCount = ContainerPanel->GetChildrenCount();
        Result.Reserve(ChildCount);
        for (int32 Index = 0; Index < ChildCount; ++Index)
        {
            if (UWidget* Child = ContainerPanel->GetChildAt(Index))
            {
                Result.Add(Child);
            }
        }
    }
    return Result;
}

TArray<FName> UGV2ListViewWidgetBase::GetActiveKeys() const
{
    TArray<FName> Keys;
    ActiveWidgetsByKey.GetKeys(Keys);
    return Keys;
}

bool UGV2ListViewWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}

void UGV2ListViewWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    FGV2UiPropertyCapability ItemCap;
    ItemCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    OutBuilder.AddKeyedCollection(TEXT("items"), FName(TEXT("ContainerPanel")), ItemCap, TEXT("key"));
}
