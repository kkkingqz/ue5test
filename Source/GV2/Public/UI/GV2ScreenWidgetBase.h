#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "GV2ScreenWidgetBase.generated.h"

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ScreenWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Screen")
    bool ApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    bool CanApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields) const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    TArray<FGV2ScreenFieldDescriptor> GetScreenFieldContract() const;

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Screen", meta = (DisplayName = "On Screen Fields Applied"))
    void OnScreenFieldsApplied();
};
