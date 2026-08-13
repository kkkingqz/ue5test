#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "UObject/Interface.h"
#include "GV2DynamicScreenElement.generated.h"

UINTERFACE(BlueprintType)
class GV2_API UGV2DynamicScreenElement : public UInterface
{
    GENERATED_BODY()
};

class GV2_API IGV2DynamicScreenElement
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GV2|UI|Screen Field")
    FGV2ScreenFieldDescriptor GetScreenFieldDescriptor() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GV2|UI|Screen Field")
    bool CanApplyScreenField(const FGV2ScreenFieldValue& FieldValue) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GV2|UI|Screen Field")
    bool ApplyScreenField(const FGV2ScreenFieldValue& FieldValue);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GV2|UI|Screen Field")
    bool CaptureScreenField(FGV2ScreenFieldValue& OutFieldValue) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GV2|UI|Screen Field")
    bool ResetScreenField();
};
