#pragma once

#include "UI/GV2ImageWidgetBase.h"
#include "GV2IconWidgetBase.generated.h"

/**
 * UGV2IconWidgetBase (UIF-15, ADR-0035)
 * Square visual icon primitive with fixed 1:1 aspect ratio and PreserveAspect scaling policy.
 */
UCLASS(Abstract, Blueprintable)
class GV2_API UGV2IconWidgetBase : public UGV2ImageWidgetBase
{
    GENERATED_BODY()

public:
    UGV2IconWidgetBase();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Icon")
    bool ApplyIcon(const FString& ResourceId, FString& OutError);
};
