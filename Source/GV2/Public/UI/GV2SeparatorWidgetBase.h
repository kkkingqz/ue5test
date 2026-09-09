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
    // PSC-10B: read by the preparer to pick the axis; the widget itself no longer decides
    // anything about its style, only which of two prepared values the axis field carries.
    bool IsHorizontal() const { return Orientation == Orient_Horizontal; }

    // PSC-10B: the ONLY physical central-style write for this class. Pure value sink --
    // every input is a finished value, so it cannot reach a Theme, a settings object or
    // any other runtime authority no matter who calls it. Prepare resolves the values and
    // ships them inside FGV2PreparedPresentationTransaction; design-time preview passes
    // this widget's own serialized values. Both callers hit this one function.
    void ApplySeparatorStyleValues(const FSlateBrush& Brush, float Thickness, bool bHorizontal);

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
