#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2LoadingIndicatorWidgetBase.generated.h"

class UCircularThrobber;

UCLASS(Blueprintable)
class GV2_API UGV2LoadingIndicatorWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCircularThrobber> LoadingIndicator;
};
