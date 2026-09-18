#pragma once

#include "CoreMinimal.h"

// GBF-07: this is the expected side of the rollback-boundary inventory. The
// actual side is derived from Commit roots in production source by
// validate_ui_rollback_boundaries.py; neither side is allowed to drift alone.
enum class EGV2UiRollbackBoundary : uint8
{
    PropertyMutation,
    ScreenFields,
    Document,
    KeyedCollection,
    NestedScreenTabs,
    ShellAttach,
    // PEP-06/PEP-10: FGV2LayeredUiReconciler::CommitLayerParticipants -- the shared
    // ClearChildren-and-rebuild primitive Attach/DetachHostLocalScreen and CommitReconcile's
    // own per-layer step all funnel through. Its own failure is a structural rebuild
    // failure (RestoreStructure) -- there is no per-property mutation here to replay the
    // inverse of.
    HostLocalLayerParticipants,
    // PEP-05/PEP-10: FGV2RichTextSpansPropertyConsumer's own sibling-rollback-on-failure
    // Commit path for hover nested screens -- same shape as NestedScreenTabs -- discovered
    // and named late; this marker predates its own enum entry until PEP-10 first ran the
    // portable ctest suite this plan depends on.
    RichTextSpansHover,
    Count,
};

// The recovery mechanism required when the corresponding source-derived Commit
// root can reject a revision after live mutation has begun.
enum class EGV2UiRollbackRecovery : uint8
{
    ReplayInverse,
    RestoreStructure,
};

GV2_API EGV2UiRollbackRecovery GetUiRollbackBoundaryRecovery(EGV2UiRollbackBoundary Boundary);
