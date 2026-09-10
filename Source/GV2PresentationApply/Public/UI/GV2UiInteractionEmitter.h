#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"

class GV2PRESENTATIONAPPLY_API FGV2UiInteractionEmitter
{
public:
    static EGV2SubmitUiInteractionResult Submit(
        const UObject* WorldContextObject,
        FGV2UiBindingHandle BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues);
};
