#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "GV2ScreenWidgetBase.generated.h"

UCLASS(Blueprintable)
class GV2_API UGV2ScreenWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // UPP-27: prepares every field's full mutation plan (recursing through
    // PrepareUiHostProperties, which already predicts a deep child's failure --
    // a keyed collection item, a nested capability -- before anything commits) and
    // only then commits every plan. No widget is mutated unless every field's
    // plan prepared cleanly; there is no compensating capture/rollback because
    // by the time Commit runs, nothing can fail.
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Screen")
    bool ApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields);

    // Public preflight: runs the exact same Prepare pass as ApplyScreenFields and
    // discards the resulting plan, so a deep child's failure (STATUS-004) is
    // predicted here precisely because it is predicted identically during a real
    // apply -- this calls the same code, not a shallower approximation of it.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    bool CanApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields) const;

    // Screen Field ids this screen's tree currently declares (via
    // IGV2ScreenFieldHost), for contract introspection/tests.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    TArray<FName> GetScreenFieldIds() const;

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Screen", meta = (DisplayName = "On Screen Fields Applied"))
    void OnScreenFieldsApplied();
};
