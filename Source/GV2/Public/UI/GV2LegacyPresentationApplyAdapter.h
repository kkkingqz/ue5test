#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"

// PSC-09B (ADR-0043 D2, Payload.md M3): temporary GV2-side completion of Apply for
// operations whose real target needs a GV2-owned widget/interface type --
// GV2PresentationApply::Apply cannot Cast to UGV2DropdownSelectWidgetBase,
// UGV2InputFieldWidgetBase, UGV2TabContainerWidgetBase, IGV2UiPropertyHost or
// IGV2UiBindingTarget by construction (its own Build.cs denies any dependency on GV2).
// This adapter performs NO semantic lookup/resolution of its own -- every value it
// applies was already resolved by Prepare and is read unchanged off the same
// transaction GV2PresentationApply::Apply() also reads; it only supplies the Cast this
// module is denied. It exists because Payload.md's own M3 intro explicitly allows
// physical Apply to still be invoked through a temporary GV2-side adapter during this
// milestone -- PSC-11 is where the widget-owning UCLASSes themselves physically move
// into GV2PresentationApply, at which point this adapter's remaining work moves with
// them and this file is deleted, not migrated.
namespace GV2LegacyPresentationApplyAdapter
{
GV2_API bool Apply(const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction, FString& OutError);
}
