#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ButtonListWidgetBase.generated.h"

class UGV2ButtonWidgetBase;
class UVerticalBox;

UCLASS(Blueprintable)
class GV2_API UGV2ButtonListWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
    , public IGV2PreparedKeyTarget
    , public IGV2PreparedKeyedCollectionTarget
    , public IGV2PreparedItemPaddingStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedItemPaddingStyle(const GV2PresentationApply::FPreparedItemPaddingStyle& Style) override
    {
        ApplyItemPaddingStyleValue(Style.Padding);
    }

    // PSC-11: value sinks for the prepared keyed-collection operation.
    virtual UPanelWidget* GetPreparedCollectionPanel() const override;
    virtual void OnPreparedCollectionSettled(
        const TArray<GV2PresentationApply::FPreparedKeyedCollectionEntry>& Entries) override;

    // PSC-11: value sink for the prepared `key` operation. The generic identity write is the
    // same one every property host already performs; a host that routes a NAMED key
    // capability overrides this and falls back to it.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override
    {
        GetPropertyHostState().SetKey(Value);
        return true;
    }

    UVerticalBox* GetButtonContainer() const { return ButtonContainer; }
    void SetButtonContainer(UVerticalBox* InContainer) { ButtonContainer = InContainer; }

    UGV2ButtonWidgetBase* GetButton(FName ButtonKey) const;

    // DCA-03: class element of the collection is set explicitly on the declaring asset
    // (Designer), not derived at runtime. A plain accessor, not a resolver -- there is
    // no fallback chain, so an unset class simply reads back as null.
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> GetButtonWidgetClass() const { return ButtonWidgetClass; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyItemPaddingStyleValue(const FMargin& Padding);

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> ButtonContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;

private:
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
