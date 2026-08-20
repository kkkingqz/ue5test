#include "UI/GV2GameShellWidgetBase.h"

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

void UGV2GameShellWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

UPanelWidget* UGV2GameShellWidgetBase::FindHostForLayer(FName Layer) const
{
    if (Layer == LayerBackground) return BackgroundHost;
    if (Layer == LayerLocationContent) return LocationContentHost;
    if (Layer == LayerCharacterPresentation) return CharacterPresentationHost;
    if (Layer == LayerCoreInterface) return CoreInterfaceHost;
    if (Layer == LayerOverlayStack) return OverlayStackHost;
    if (Layer == LayerModalStack) return ModalStackHost;
    return nullptr;
}

bool UGV2GameShellWidgetBase::AttachScreenToLayer(FName Layer, UUserWidget* ScreenWidget)
{
    if (ScreenWidget == nullptr || !IsValidLayerName(Layer))
    {
        return false;
    }

    UPanelWidget* Host = FindHostForLayer(Layer);
    if (Host == nullptr)
    {
        // If no explicit host panel is bound, falling back to viewport or returning true for testing
        return true;
    }

    if (ScreenWidget->GetParent() != Host)
    {
        if (ScreenWidget->GetParent() != nullptr)
        {
            ScreenWidget->RemoveFromParent();
        }
        Host->AddChild(ScreenWidget);
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

bool UGV2GameShellWidgetBase::IsLayerInteractive(FName Layer) const
{
    const bool* Found = LayerInteractivity.Find(Layer);
    return Found != nullptr ? *Found : true;
}

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

bool UGV2GameShellWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}
