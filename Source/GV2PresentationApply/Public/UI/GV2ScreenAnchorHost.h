#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "UObject/Interface.h"
#include "GV2ScreenAnchorHost.generated.h"

UINTERFACE(MinimalAPI)
class UGV2ScreenAnchorHost : public UInterface
{
    GENERATED_BODY()
};

/**
 * PEP-06B: identifies a widget as the positionable-content root for a screen that is a
 * host-local overlay_stack participant (the hover popover today; any future anchored
 * participant reuses the same mechanism, matching FGV2LayeredUiReconciler::
 * AttachHostLocalScreen's own already-generic shape). A layer participant's OWN root slot
 * is always Fill/Fill (UGV2GameShellWidgetBase::ApplyScreenSlotLayout, uniform across every
 * participant) -- the layer has no concept of position at all. A widget implementing this
 * interface owns a root UCanvasPanel sized to the whole screen and places its actual
 * visible content inside itself at a caller-given point, leaving the rest of its own
 * screen-filling area SelfHitTestInvisible so lower layers stay reachable.
 *
 * UGV2ScreenWidgetBase implements this interface directly on itself (not on some nested
 * sub-widget discovered via a tree walk): whoever attaches a host-local participant that
 * needs anchoring simply casts the resolved screen widget itself --
 * Cast<IGV2ScreenAnchorHost>(Span->Hover.ScreenWidget) -- and calls
 * SetAnchoredContentPosition once, before or after FGV2LayeredUiReconciler::
 * AttachHostLocalScreen -- this interface performs no attach/detach of its own, only the
 * physical placement of already-prepared content.
 */
class GV2PRESENTATIONAPPLY_API IGV2ScreenAnchorHost
{
    GENERATED_BODY()

public:
    // LocalPosition is in this host's OWN root canvas local space -- the caller is
    // responsible for converting an absolute/desktop-space anchor rect (e.g.
    // FGV2RichTextSpanAnchor::Rect) via this widget's GetTickSpaceGeometry().
    // AbsoluteToLocal(...) before calling. A host with no live geometry yet (never
    // painted) cannot be positioned correctly; the caller is expected to have already
    // attached this widget to a live panel before calling.
    virtual void SetAnchoredContentPosition(const FVector2D& LocalPosition) = 0;

    // PEP-08: the actual visible content's own on-screen rect, in the same absolute
    // (desktop) space FGV2RichTextSpanAnchor::Rect uses -- NOT this host's own root, which
    // is always Fill/Fill over the whole layer and would make "is the cursor over the
    // popover" trivially true anywhere on screen. Lets a caller (the hover detector) treat
    // "cursor moved onto the open popover itself" the same way it treats "cursor is still
    // over the source span" -- both keep a hover alive, neither is a special case. An empty
    // rect (FSlateRect()) means no content is currently placed (e.g. not yet painted).
    virtual FSlateRect GetAnchoredContentScreenRect() const = 0;
};
