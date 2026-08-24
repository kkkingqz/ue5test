#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2TextWidgetBase.generated.h"

class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2TextWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyText(const FGV2TextViewModel& Content);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FText GetTextContent() const;

    UCommonTextBlock* GetTextBlock() const;

    virtual bool ApplyCentralStyle_Implementation() override;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> TextBlock;

private:
    UPROPERTY(Transient)
    FGV2TextViewModel CurrentContent;

    FGV2UiPropertyHostState PropertyHostState;
};
