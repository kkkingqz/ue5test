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
