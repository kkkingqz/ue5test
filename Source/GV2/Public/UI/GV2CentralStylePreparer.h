#pragma once

#include "CoreMinimal.h"

namespace GV2PresentationApply { class FGV2PreparedPresentationTransaction; }

class FGV2PresentationPrepareContext;
class UWidget;

// PSC-10B (ADR-0043 D3): central style is PUSHED, not pulled.
//
// Every styled widget used to pull its own style from UGV2UiThemeSettings::GetConfiguredTheme()
// inside NativePreConstruct -- a UMG lifecycle callback that fires on a CDO, in the asset
// editor, and long before any session exists. That is what made "the authority set is open"
// unprovable: any widget could reach global authority from a callback nobody audits, and the
// PAH-08 gate's hand-written authority list never named GetConfiguredTheme (PAH-R7).
//
// Here the direction is inverted. Whoever prepares a screen already holds the session's
// FGV2PresentationPrepareContext, walks the subtree once, and emits one prepared operation per
// styled widget into the SAME transaction that carries every other prepared operation. Apply
// receives finished brushes, colours and margins. No widget reads a theme; there is no theme
// to read from below the Prepare boundary.
namespace GV2CentralStylePreparer
{
// Walks Root and everything below it (nested UUserWidget trees included) and appends one
// central-style operation per widget whose class declares a style role. Idempotent per widget
// within a single call. Emits nothing when the snapshot carries no theme -- cold start and
// core-minimal recovery keep the widgets' serialized values rather than inventing a style.
[[nodiscard]] GV2_API bool PrepareForSubtree(
    UWidget* Root,
    const FGV2PresentationPrepareContext& PrepareContext,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError);
}
