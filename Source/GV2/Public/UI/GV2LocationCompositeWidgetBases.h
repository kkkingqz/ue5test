#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "GV2LocationCompositeWidgetBases.generated.h"

class UGV2TextWidgetBase;
class UGV2PortraitWidgetBase;
class UGV2ProgressBarWidgetBase;
class UGV2ImageWidgetBase;
class UGV2ButtonWidgetBase;
class UWrapBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2LocationCommandBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Blueprintable)
class GV2_API UGV2LocationTopBarWidgetBase : public UCommonUserWidget, public IGV2DynamicScreenElement
{
    GENERATED_BODY()
public:
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> DayText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> LocationText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> PrimaryResourceText;
private: FGV2LocationTopBarViewModel Applied;
};

UCLASS(Blueprintable)
class GV2_API UGV2LocationPlayerStatusWidgetBase : public UCommonUserWidget, public IGV2DynamicScreenElement
{
    GENERATED_BODY()
public:
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> PlayerNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2PortraitWidgetBase> Portrait;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ProgressBarWidgetBase> StaminaMeter;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> ItemIcons;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> EffectIcons;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ImageWidgetBase> IconWidgetClass;
private: FGV2LocationPlayerStatusViewModel Applied;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UGV2ImageWidgetBase>> ItemWidgetsByKey;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UGV2ImageWidgetBase>> EffectWidgetsByKey;
};

UCLASS(Blueprintable)
class GV2_API UGV2LocationSceneWidgetBase : public UCommonUserWidget, public IGV2DynamicScreenElement
{
    GENERATED_BODY()
public:
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2TextWidgetBase> SceneContextText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> Background;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> BackgroundTile;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> Character;
private: FGV2LocationSceneViewModel Applied;
};

/** LocationScreen's command field is a ButtonList with a textsystem schema. */
UCLASS(Blueprintable)
class GV2_API UGV2LocationCommandPanelWidgetBase : public UCommonUserWidget, public IGV2DynamicScreenElement
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels);
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels) const;
    UPROPERTY(BlueprintAssignable, Category = "GV2|UI") FGV2LocationCommandBindingInvoked OnBindingInvoked;
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutValue) const override;
    virtual bool ResetScreenField_Implementation() override;
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWrapBox> ButtonContainer;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GV2|UI") TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;
private:
    UFUNCTION() void HandleButtonBindingInvoked(FGV2UiBindingHandle BindingHandle, EGV2SubmitUiInteractionResult Result);
    TSubclassOf<UGV2ButtonWidgetBase> ResolveButtonWidgetClass() const;
    UPROPERTY(Transient) TArray<FGV2ButtonViewModel> AppliedButtonModels;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> ButtonsByKey;
};
