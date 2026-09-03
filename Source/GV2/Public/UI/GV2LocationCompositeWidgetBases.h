#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2LocationCompositeWidgetBases.generated.h"

class UGV2TextWidgetBase;
class UGV2PortraitWidgetBase;
class UGV2ProgressBarWidgetBase;
class UGV2ImageWidgetBase;
class UGV2ButtonWidgetBase;
class UWrapBox;
class UPanelWidget;

UCLASS(Blueprintable)
class GV2_API UGV2LocationPlayerStatusWidgetBase : public UCommonUserWidget, public IGV2UiPropertyHost, public IGV2ScreenFieldHost
{
    GENERATED_BODY()
public:
    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost
    // DUC-01: field_id is this host's identity within its enclosing screen --
    // the same shared HostIdentity every IGV2UiPropertyHost carries, not a
    // separate per-class property. See FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetItemRepeater() const { if (ItemRepeater) return ItemRepeater.Get(); return const_cast<UGV2LocationPlayerStatusWidgetBase*>(this)->ResolveItemRepeater(); }
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetEffectRepeater() const { if (EffectRepeater) return EffectRepeater.Get(); return const_cast<UGV2LocationPlayerStatusWidgetBase*>(this)->ResolveEffectRepeater(); }
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetMeterRepeater() const { if (MeterRepeater) return MeterRepeater.Get(); return const_cast<UGV2LocationPlayerStatusWidgetBase*>(this)->ResolveMeterRepeater(); }
    bool HasUsableMeterRepeaterHost() const;
    bool HasUsableItemRepeaterHost() const;
    bool HasUsableEffectRepeaterHost() const;
    UGV2ListViewWidgetBase* ResolveItemRepeater();
    UGV2ListViewWidgetBase* ResolveEffectRepeater();
    UGV2ListViewWidgetBase* ResolveMeterRepeater();

    // DCA-03: class element of a collection is set explicitly on the declaring asset
    // (Designer), not derived at runtime. A plain accessor, not a resolver -- there is
    // no fallback chain, so an unset class simply reads back as null.
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ImageWidgetBase> GetIconWidgetClass() const { return IconWidgetClass; }
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ProgressBarWidgetBase> GetMeterWidgetClass() const { return MeterWidgetClass; }

protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UGV2TextWidgetBase> PlayerNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2PortraitWidgetBase> Portrait;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> MeterRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> MeterContainer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> ItemRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> EffectRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> ItemIcons;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> EffectIcons;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ImageWidgetBase> IconWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ProgressBarWidgetBase> MeterWidgetClass;

private:
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;


    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalItemRepeater;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalEffectRepeater;
    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalMeterRepeater;
};

UCLASS(Blueprintable)
class GV2_API UGV2LocationSceneWidgetBase : public UCommonUserWidget, public IGV2UiPropertyHost, public IGV2ScreenFieldHost
{
    GENERATED_BODY()
public:
    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost
    // DUC-01: field_id is this host's identity within its enclosing screen --
    // the same shared HostIdentity every IGV2UiPropertyHost carries, not a
    // separate per-class property. See FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetCharacterRepeater() const { if (CharacterRepeater) return CharacterRepeater.Get(); return const_cast<UGV2LocationSceneWidgetBase*>(this)->ResolveCharacterRepeater(); }
    bool HasUsableCharacterRepeaterHost() const;
    UGV2ListViewWidgetBase* ResolveCharacterRepeater();

    // DCA-03: see GetIconWidgetClass -- plain accessor, no fallback chain.
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ImageWidgetBase> GetCharacterWidgetClass() const { return CharacterWidgetClass; }

protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2TextWidgetBase> SceneContextText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> Background;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ImageWidgetBase> BackgroundTile;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> CharacterRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UPanelWidget> CharacterContainer;
    UPROPERTY(EditDefaultsOnly, Category="GV2|UI") TSubclassOf<UGV2ImageWidgetBase> CharacterWidgetClass;

private:
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;


    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalCharacterRepeater;
};

/** LocationScreen's command field is a ButtonList with a textsystem schema. */
UCLASS(Blueprintable)
class GV2_API UGV2LocationCommandPanelWidgetBase : public UCommonUserWidget, public IGV2UiPropertyHost, public IGV2ScreenFieldHost
{
    GENERATED_BODY()
public:
    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost
    // DUC-01: field_id is this host's identity within its enclosing screen --
    // the same shared HostIdentity every IGV2UiPropertyHost carries, not a
    // separate per-class property. See FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    bool HasUsableRepeaterHost() const;
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    UGV2ListViewWidgetBase* GetRepeater() const { if (ButtonRepeater) return ButtonRepeater.Get(); return const_cast<UGV2LocationCommandPanelWidgetBase*>(this)->ResolveRepeater(); }
    UGV2ListViewWidgetBase* ResolveRepeater();

    // DCA-03: see GetIconWidgetClass -- plain accessor, no fallback chain.
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> GetButtonWidgetClass() const { return ButtonWidgetClass; }

protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UGV2ListViewWidgetBase> ButtonRepeater;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> ButtonContainer;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GV2|UI") TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;

private:
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;


    UPROPERTY(Transient) TObjectPtr<UGV2ListViewWidgetBase> InternalRepeater;
};

