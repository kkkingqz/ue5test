#include "UI/GV2UiHostSemanticState.h"

FGV2UiHostSemanticState& GetUiHostSemanticState(FGV2UiPropertyHostState& State)
{
    if (!State.GetSemanticExtension().IsValid())
    {
        State.SetSemanticExtension(MakeShared<FGV2UiHostSemanticState>());
    }
    return *StaticCastSharedPtr<FGV2UiHostSemanticState>(State.GetSemanticExtension());
}

const FGV2UiHostSemanticState& GetUiHostSemanticState(const FGV2UiPropertyHostState& State)
{
    return GetUiHostSemanticState(const_cast<FGV2UiPropertyHostState&>(State));
}

// DUC-03: the one place the claimed set is written down. See the declaration for why a list
// is acceptable here and what keeps it equal to the actual set.
bool IsHostClaimedKeyCapability(const FName PropertyName)
{
    static const FName SelectedKey(TEXT("selected_key"));
    static const FName DefaultTabKey(TEXT("default_tab_key"));
    return PropertyName == SelectedKey || PropertyName == DefaultTabKey;
}
