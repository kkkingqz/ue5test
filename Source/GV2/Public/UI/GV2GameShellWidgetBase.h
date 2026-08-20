#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2GameShellWidgetBase.generated.h"

class UPanelWidget;
class UNamedSlot;

/**
 * UGV2GameShellWidgetBase (UIF-17, ADR-0035)
 * Top-level Game Shell container managing 6 standardized visual layers:
 * - background (Z: 0)
 * - location_content (Z: 10)
 * - character_presentation (Z: 20)
 * - core_interface (Z: 30)
 * - overlay_stack (Z: 40)
 * - modal_stack (Z: 50)
 */
UCLASS(Blueprintable)
class GV2_API UGV2GameShellWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    static const FName LayerBackground;
    static const FName LayerLocationContent;
    static const FName LayerCharacterPresentation;
    static const FName LayerCoreInterface;
    static const FName LayerOverlayStack;
    static const FName LayerModalStack;

    static bool IsValidLayerName(FName Layer);
    static const TArray<FName>& GetApprovedLayers();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|GameShell")
    bool AttachScreenToLayer(FName Layer, UUserWidget* ScreenWidget);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|GameShell")
    bool DetachScreen(UUserWidget* ScreenWidget);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|GameShell")
    void SetLayerInteractive(FName Layer, bool bInteractive);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|GameShell")
    bool IsLayerInteractive(FName Layer) const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|GameShell")
    TArray<UUserWidget*> GetScreensInLayer(FName Layer) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|GameShell")
    void ClearAllLayers();

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> BackgroundHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> LocationContentHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> CharacterPresentationHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> CoreInterfaceHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> OverlayStackHost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> ModalStackHost;

    UPanelWidget* FindHostForLayer(FName Layer) const;

private:
    UPROPERTY(Transient)
    TMap<FName, bool> LayerInteractivity;
};
