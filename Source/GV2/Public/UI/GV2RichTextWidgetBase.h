#pragma once

#include "CommonUserWidget.h"
#include "GV2RichTextWidgetBase.generated.h"

class UCommonRichTextBlock;

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2RichTextWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyRichTextContent(const FText& Content);

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonRichTextBlock> RichTextBlock;
};
