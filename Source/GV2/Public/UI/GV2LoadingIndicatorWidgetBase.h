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
    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyLoadingIndicatorStyleValues(const FSlateBrush& Brush, float Period, float Radius, int32 Pieces);

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCircularThrobber> LoadingIndicator;
};
