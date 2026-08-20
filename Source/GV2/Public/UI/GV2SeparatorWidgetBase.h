#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2SeparatorWidgetBase.generated.h"

class UImage;
class USizeBox;

UCLASS(Blueprintable)
class GV2_API UGV2SeparatorWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    TEnumAsByte<EOrientation> Orientation = Orient_Horizontal;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<USizeBox> SeparatorSizeBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> SeparatorImage;
};
