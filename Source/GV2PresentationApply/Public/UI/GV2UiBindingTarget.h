#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "UObject/Interface.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "GV2UiBindingTarget.generated.h"

UINTERFACE(MinimalAPI)
class UGV2UiBindingTarget : public UGV2PreparedBindingTarget
{
    GENERATED_BODY()
};

/**
 * Implemented by any widget that receives an opaque FGV2UiBindingHandle from the
 * universal UI property pipeline's binding consumer (UPP-09/FGV2BindingPropertyConsumer).
 * The widget never receives a raw command ID or arguments, only this handle, which it
 * later submits as-is via FGV2UiInteractionEmitter::Submit.
 */
class GV2PRESENTATIONAPPLY_API IGV2UiBindingTarget : public IGV2PreparedBindingTarget
{
    GENERATED_BODY()

public:
    // PSC-11: every binding target IS a prepared-binding target; see IGV2UiPropertyHost.
    virtual void ApplyPreparedBinding(const FString& SerializedHandle) override
    {
        SetBindingHandle(FGV2UiBindingHandle::FromSerialized(SerializedHandle));
    }

public:
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) = 0;
    virtual FGV2UiBindingHandle GetBindingHandle() const = 0;
};
