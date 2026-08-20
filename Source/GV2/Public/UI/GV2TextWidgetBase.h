#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2TextWidgetBase.generated.h"

class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2TextWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyText(const FGV2TextViewModel& Content);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FText GetTextContent() const;

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> TextBlock;

private:
    UPROPERTY(Transient)
    FGV2TextViewModel CurrentContent;
};
