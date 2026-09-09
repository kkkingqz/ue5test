#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2LoadingIndicatorWidgetBase.generated.h"

class UCircularThrobber;

UCLASS(Blueprintable)
class GV2_API UGV2LoadingIndicatorWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2PreparedLoadingIndicatorStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedLoadingIndicatorStyle(const GV2PresentationApply::FPreparedLoadingIndicatorStyle& Style) override
    {
        ApplyLoadingIndicatorStyleValues(Style.Brush, Style.Period, Style.Radius, Style.Pieces);
    }

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyLoadingIndicatorStyleValues(const FSlateBrush& Brush, float Period, float Radius, int32 Pieces);

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCircularThrobber> LoadingIndicator;
};
