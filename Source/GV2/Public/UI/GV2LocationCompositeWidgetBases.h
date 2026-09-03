#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2LocationCompositeWidgetBases.generated.h"

class UGV2ButtonWidgetBase;
class UWrapBox;

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

