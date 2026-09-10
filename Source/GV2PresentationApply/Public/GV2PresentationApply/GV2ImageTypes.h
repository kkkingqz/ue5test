#pragma once

#include "CoreMinimal.h"

#include "GV2ImageTypes.generated.h"

UENUM(BlueprintType)
enum class EGV2PrimitiveScalePolicy : uint8
{
    Unset = 0,
    FreeStretch,
    Tile,
    NineSlice,
    PreserveAspect
};
