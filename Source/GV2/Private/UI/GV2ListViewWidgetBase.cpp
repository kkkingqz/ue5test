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
    // PSC-10B: this implementation applies nothing from the Theme. The fetch that used
    // to stand here read the configured Theme only to null-check it and threw the value
    // away -- a value obtained and discarded, the same family as ResourceIcon /
    // ApplyOptionalXxx / OnBindingInvoked / bFatal. It also inflated this task's scope,
    // because the class has no central style payload at all.
    return true;
}

void UGV2ListViewWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    // PCC-10: this class is a generic repeater primitive -- every real usage (see the
    // ResolveXxxRepeater() family in GV2LocationCompositeWidgetBases.cpp) drives its entries
    // directly through the ReconcileEntries/ReconcilePreparedEntries C++ templates, never
    // through the schema-driven Prepare/Commit pipeline this capability tree describes. It
    // has no single fixed entry class to declare honestly (each call site parameterizes its
    // own widget/model types), so declaring an "items" collection here would be exactly the
    // "declared but never consumed" shape this plan exists to remove. The owning composite
    // (a UGV2DeclaredCompositeWidgetBase's own CollectionHost entry, e.g. TopBar/Scene/
    // PlayerStatus/CommandPanel) is the one that declares real, verifiable capabilities
    // for what it repeats.
    (void)OutBuilder;
}
