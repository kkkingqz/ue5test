#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GV2ScreenFieldHost.generated.h"

UINTERFACE(MinimalAPI)
class UGV2ScreenFieldHost : public UInterface
{
    GENERATED_BODY()
};

/**
 * UPP-27: identifies a widget as the top-level target for one named Screen Field
 * ("top_bar", "scene", "commands", ...), replacing the retired
 * IGV2DynamicScreenElement::GetScreenFieldDescriptor scan. A widget implementing
 * this must also implement IGV2UiPropertyHost -- UGV2ScreenWidgetBase finds it by
 * scanning its WidgetTree for this interface, matches GetScreenFieldId() against
 * the field id the Lua presenter published, and runs PrepareUiHostProperties /
 * CommitUiHostProperties against it directly. GetScreenFieldId() returning
 * NAME_None means "not configured" -- such a widget is never matched to a field
 * and BuildApplyPlan-equivalent bijection checking (every configured host has a
 * value, every value has a host) reports it as a contract error, exactly like an
 * unset FieldId on the old descriptor did.
 */
class GV2_API IGV2ScreenFieldHost
{
    GENERATED_BODY()

public:
    virtual FName GetScreenFieldId() const = 0;
};
