#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GV2UiStyleConsumer.generated.h"

UINTERFACE(BlueprintType)
class GV2_API UGV2UiStyleConsumer : public UInterface
{
    GENERATED_BODY()
};

class GV2_API IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GV2|UI|Style")
    bool ApplyCentralStyle();
};
