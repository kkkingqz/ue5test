#include "UI/GV2GameShellWidgetBase.h"

#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "UI/GV2UiTheme.h"

const FName UGV2GameShellWidgetBase::LayerBackground(TEXT("background"));
const FName UGV2GameShellWidgetBase::LayerLocationContent(TEXT("location_content"));
const FName UGV2GameShellWidgetBase::LayerCharacterPresentation(TEXT("character_presentation"));
const FName UGV2GameShellWidgetBase::LayerCoreInterface(TEXT("core_interface"));
const FName UGV2GameShellWidgetBase::LayerOverlayStack(TEXT("overlay_stack"));
const FName UGV2GameShellWidgetBase::LayerModalStack(TEXT("modal_stack"));

const TArray<FName>& UGV2GameShellWidgetBase::GetApprovedLayers()
{
    static const TArray<FName> Approved = {
        LayerBackground,
        LayerLocationContent,
        LayerCharacterPresentation,
        LayerCoreInterface,
        LayerOverlayStack,
        LayerModalStack,
    };
    return Approved;
}

bool UGV2GameShellWidgetBase::IsValidLayerName(FName Layer)
{
    return GetApprovedLayers().Contains(Layer);
}

UPanelWidget* UGV2GameShellWidgetBase::FindHostForLayer(FName Layer) const
{
    UPanelWidget* Host = nullptr;
    if (Layer == LayerBackground) Host = BackgroundHost;
    else if (Layer == LayerLocationContent) Host = LocationContentHost;
    else if (Layer == LayerCharacterPresentation) Host = CharacterPresentationHost;
    else if (Layer == LayerCoreInterface) Host = CoreInterfaceHost;
    else if (Layer == LayerOverlayStack) Host = OverlayStackHost;
    else if (Layer == LayerModalStack) Host = ModalStackHost;

    return Host;
}

bool UGV2GameShellWidgetBase::HasHostForLayer(FName Layer) const
{
    return FindHostForLayer(Layer) != nullptr;
}

void UGV2GameShellWidgetBase::ApplyScreenSlotLayout(UPanelSlot& Slot)
{
    if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(&Slot))
    {
        OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
        OverlaySlot->SetVerticalAlignment(VAlign_Fill);
    }
}

// GBF-07: rollback_boundary=ShellAttach
bool UGV2GameShellWidgetBase::AttachScreenToLayer(FName Layer, UUserWidget* ScreenWidget)
{
    if (ScreenWidget == nullptr || !IsValidLayerName(Layer))
    {
        return false;
    }

    UPanelWidget* Host = FindHostForLayer(Layer);
    if (Host == nullptr)
    {
        return false;
    }

    if (ScreenWidget->GetParent() != Host)
    {
        if (ScreenWidget->GetParent() != nullptr)
        {
            ScreenWidget->RemoveFromParent();
        }
        UPanelSlot* NewSlot = Host->AddChild(ScreenWidget);
        if (NewSlot == nullptr)
        {
            return false;
        }
        UGV2GameShellWidgetBase::ApplyScreenSlotLayout(*NewSlot);
    }
    else if (ScreenWidget->Slot != nullptr)
    {
        UGV2GameShellWidgetBase::ApplyScreenSlotLayout(*ScreenWidget->Slot);
    }
    return true;
}

bool UGV2GameShellWidgetBase::DetachScreen(UUserWidget* ScreenWidget)
{
    if (ScreenWidget == nullptr)
    {
        return false;
    }
    if (ScreenWidget->GetParent() != nullptr)
    {
        ScreenWidget->RemoveFromParent();
        return true;
    }
    return false;
}

void UGV2GameShellWidgetBase::SetLayerInteractive(FName Layer, bool bInteractive)
{
    LayerInteractivity.Add(Layer, bInteractive);
    UPanelWidget* Host = FindHostForLayer(Layer);
    if (Host != nullptr)
    {
        Host->SetIsEnabled(bInteractive);
        Host->SetVisibility(bInteractive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
    }
}

void UGV2GameShellWidgetBase::SetTopModalInteractive(const TArray<UUserWidget*>& OrderedModals)
{
    for (int32 Index = 0; Index < OrderedModals.Num(); ++Index)
    {
        if (OrderedModals[Index] != nullptr)
        {
            OrderedModals[Index]->SetIsEnabled(Index == OrderedModals.Num() - 1);
        }
    }
}

bool UGV2GameShellWidgetBase::IsLayerInteractive(FName Layer) const
{
    const bool* Found = LayerInteractivity.Find(Layer);
    return Found != nullptr ? *Found : true;
}

// PEP-06: reads the panel's raw children, so after FGV2LayeredUiReconciler started
// rebuilding a layer from document ∪ host-local registry, this returns both tiers
// together, document order first then host-local order -- it has no concept of tier and
// cannot filter by one. Audited callers (test-only; there are no production callers):
// GV2LayeredReconciliationTests.cpp, GV2RuntimeSubsystemTests.cpp assert Contains() against
// specific document-sourced widgets or order among a known document set -- none register a
// host-local participant, so all are tier-indifferent as written. A future test asserting
// order across both tiers should read HostLocalScreens/ActiveScreens directly rather than
// add a tier filter here.
TArray<UUserWidget*> UGV2GameShellWidgetBase::GetScreensInLayer(FName Layer) const
{
    TArray<UUserWidget*> Screens;
    UPanelWidget* Host = FindHostForLayer(Layer);
    if (Host != nullptr)
    {
        for (int32 i = 0; i < Host->GetChildrenCount(); ++i)
        {
            if (UUserWidget* ChildWidget = Cast<UUserWidget>(Host->GetChildAt(i)))
            {
                Screens.Add(ChildWidget);
            }
        }
    }
    return Screens;
}

void UGV2GameShellWidgetBase::ClearAllLayers()
{
    for (const FName& Layer : GetApprovedLayers())
    {
        UPanelWidget* Host = FindHostForLayer(Layer);
        if (Host != nullptr)
        {
            Host->ClearChildren();
        }
    }
    LayerInteractivity.Reset();
}
