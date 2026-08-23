#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "GV2LocationCompositeWidgetBases.generated.h"

class UGV2TextWidgetBase;
class UGV2PortraitWidgetBase;
class UGV2ProgressBarWidgetBase;
class UGV2ImageWidgetBase;
class UGV2ButtonWidgetBase;
class UWrapBox;
class UPanelWidget;

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
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> DayText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> LocationText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> PrimaryResourceText;
    UPROPERTY(meta=(BindWidgetOptional, DeprecatedProperty, DeprecationMessage="Deprecated: ResourceIcon is unused because PrimaryResource contains formatted text.")) TObjectPtr<UWidget> ResourceIcon;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2TextWidgetBase> DayLocationSeparator;
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
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetItemRepeater() const { return ItemRepeater ? ItemRepeater : InternalItemRepeater; }
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetEffectRepeater() const { return EffectRepeater ? EffectRepeater : InternalEffectRepeater; }
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetMeterRepeater() const { return MeterRepeater ? MeterRepeater : InternalMeterRepeater; }
    bool HasUsableMeterRepeaterHost() const;
    bool HasUsableItemRepeaterHost() const;
    bool HasUsableEffectRepeaterHost() const;
protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> PlayerNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2PortraitWidgetBase> Portrait;
    UPROPERTY(meta=(BindWidgetOptional, DeprecatedProperty, DeprecationMessage="Deprecated: Use MeterRepeater / MeterContainer with repeated Meters.")) TObjectPtr<UGV2ProgressBarWidgetBase> StaminaMeter;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> MeterRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> MeterContainer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> ItemRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> EffectRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> ItemIcons;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> EffectIcons;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ImageWidgetBase> IconWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ProgressBarWidgetBase> MeterWidgetClass;
    virtual TSubclassOf<UGV2ImageWidgetBase> ResolveIconWidgetClass() const;
    virtual TSubclassOf<UGV2ProgressBarWidgetBase> ResolveMeterWidgetClass() const;
private:
    FGV2LocationPlayerStatusViewModel Applied;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalItemRepeater;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalEffectRepeater;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalMeterRepeater;
    UGV2ListViewWidgetBase* ResolveItemRepeater();
    UGV2ListViewWidgetBase* ResolveEffectRepeater();
    UGV2ListViewWidgetBase* ResolveMeterRepeater();
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
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetCharacterRepeater() const { return CharacterRepeater ? CharacterRepeater : InternalCharacterRepeater; }
    bool HasUsableCharacterRepeaterHost() const;
protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2TextWidgetBase> SceneContextText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> Background;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> BackgroundTile;
    UPROPERTY(meta=(BindWidgetOptional, DeprecatedProperty, DeprecationMessage="Deprecated: Use CharacterRepeater / CharacterContainer with repeated Characters.")) TObjectPtr<UGV2ImageWidgetBase> Character;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> CharacterRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> CharacterContainer;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ImageWidgetBase> CharacterWidgetClass;
    virtual TSubclassOf<UGV2ImageWidgetBase> ResolveCharacterWidgetClass() const;
private:
    FGV2LocationSceneViewModel Applied;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalCharacterRepeater;
    UGV2ListViewWidgetBase* ResolveCharacterRepeater();
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
    bool HasUsableRepeaterHost() const;
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetRepeater() const { return ButtonRepeater ? ButtonRepeater : InternalRepeater; }
    UPROPERTY(BlueprintAssignable, Category = "GV2|UI") FGV2LocationCommandBindingInvoked OnBindingInvoked;
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutValue) const override;
    virtual bool ResetScreenField_Implementation() override;
protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> ButtonRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> ButtonContainer;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GV2|UI") TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;
    virtual TSubclassOf<UGV2ButtonWidgetBase> ResolveButtonWidgetClass() const;
private:
    UFUNCTION() void HandleButtonBindingInvoked(FGV2UiBindingHandle BindingHandle, EGV2SubmitUiInteractionResult Result);
    UGV2ListViewWidgetBase* ResolveRepeater();
    UPROPERTY(Transient) TArray<FGV2ButtonViewModel> AppliedButtonModels;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalRepeater;
};

