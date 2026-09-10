#include "UI/GV2UiPropertyHost.h"

// DUC-03: the one place the claimed set is written down. See the declaration for why a list
// is acceptable here and what keeps it equal to the actual set.
bool IsHostClaimedKeyCapability(const FName PropertyName)
{
    static const FName SelectedKey(TEXT("selected_key"));
    static const FName DefaultTabKey(TEXT("default_tab_key"));
    return PropertyName == SelectedKey || PropertyName == DefaultTabKey;
}
