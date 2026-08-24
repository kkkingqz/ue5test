#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2UiBindingTarget.generated.h"

UINTERFACE(MinimalAPI)
class UGV2UiBindingTarget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Implemented by any widget that receives an opaque FGV2UiBindingHandle from the
 * universal UI property pipeline's binding consumer (UPP-09/FGV2BindingPropertyConsumer).
 * The widget never receives a raw command ID or arguments, only this handle, which it
 * later submits as-is via FGV2UiInteractionEmitter::Submit.
 */
class GV2_API IGV2UiBindingTarget
{
    GENERATED_BODY()

public:
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) = 0;
    virtual FGV2UiBindingHandle GetBindingHandle() const = 0;
};
