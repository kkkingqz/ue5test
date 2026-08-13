#pragma once

#include "Bridge/GV2BridgeTypes.h"

class GV2_API FGV2UiInteractionEmitter
{
public:
    static EGV2SubmitUiInteractionResult Submit(
        const UObject* WorldContextObject,
        FGV2UiBindingHandle BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues);
};
