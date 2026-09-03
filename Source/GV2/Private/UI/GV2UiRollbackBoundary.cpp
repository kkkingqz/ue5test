#include "UI/GV2UiRollbackBoundary.h"

EGV2UiRollbackRecovery GetUiRollbackBoundaryRecovery(EGV2UiRollbackBoundary Boundary)
{
    // No default: EGV2UiRollbackBoundary is an expected-side inventory. The
    // GBF-07 CTest requires an explicit recovery case for every non-Count value.
    switch (Boundary)
    {
    case EGV2UiRollbackBoundary::PropertyMutation:
        return EGV2UiRollbackRecovery::ReplayInverse;
    case EGV2UiRollbackBoundary::ScreenFields:
        return EGV2UiRollbackRecovery::ReplayInverse;
    case EGV2UiRollbackBoundary::Document:
        return EGV2UiRollbackRecovery::ReplayInverse;
    case EGV2UiRollbackBoundary::KeyedCollection:
        return EGV2UiRollbackRecovery::ReplayInverse;
    case EGV2UiRollbackBoundary::NestedScreenTabs:
        return EGV2UiRollbackRecovery::ReplayInverse;
    case EGV2UiRollbackBoundary::ShellAttach:
        return EGV2UiRollbackRecovery::RestoreStructure;
    case EGV2UiRollbackBoundary::Count:
        checkNoEntry();
        return EGV2UiRollbackRecovery::ReplayInverse;
    }

    checkNoEntry();
    return EGV2UiRollbackRecovery::ReplayInverse;
}
