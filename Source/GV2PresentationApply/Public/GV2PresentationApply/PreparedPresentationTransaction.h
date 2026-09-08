#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UImage;

namespace GV2PresentationApply
{
// PSC-09A (ADR-0043 D3): a single already-resolved image mutation. Brush is the FINAL
// Slate brush -- Prepare (upper GV2) already validated scale-policy compatibility and
// baked Tiling/DrawAs for the target's scale policy, so Apply performs no decision, only
// widget mutation. No authority type is reachable from this struct's own module: this
// header's module (GV2PresentationApply.Build.cs) denies GV2, GV2ContentCore,
// GV2ContentHostSupport, GV2RuntimeCore, DeveloperSettings, AssetRegistry and ImageCore
// outright, so there is nothing here to dereference back into a resolver even if a field
// were added carelessly later.
struct GV2PRESENTATIONAPPLY_API FPreparedImageResourceOperation
{
    TWeakObjectPtr<UImage> TargetWidget;
    FSlateBrush Brush;
};

// PSC-09A (ADR-0043 D3): immutable once built -- an upper GV2 preparer appends
// operations, then hands the finished transaction to Apply() below; nothing mutates it
// afterward, and it carries no PrepareContext, snapshot, or resolver of any kind.
// PSC-09B closes the gap between this and Payload.md's own Done bullet: only image
// resource operations exist here yet -- every other IGV2PropertyConsumer kind still
// performs its own Commit() directly (see GV2PropertyConsumers.h's own PSC-09A doc
// comment on BuildPreparedOperation for the current, explicitly incomplete kind list).
class GV2PRESENTATIONAPPLY_API FGV2PreparedPresentationTransaction
{
public:
    void AddImageResourceOperation(FPreparedImageResourceOperation Operation)
    {
        ImageResourceOperations.Add(MoveTemp(Operation));
    }

    const TArray<FPreparedImageResourceOperation>& GetImageResourceOperations() const
    {
        return ImageResourceOperations;
    }

    bool IsEmpty() const
    {
        return ImageResourceOperations.IsEmpty();
    }

private:
    TArray<FPreparedImageResourceOperation> ImageResourceOperations;
};

// PSC-09A (ADR-0043 D2): the only public entry point physical Apply exposes -- there is
// no second path into this module's mutation logic. Performs every operation in
// Transaction and nothing else: no lookup, no resolution, no fallback to a value this
// transaction didn't already carry.
GV2PRESENTATIONAPPLY_API bool Apply(const FGV2PreparedPresentationTransaction& Transaction, FString& OutError);
}
