#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GV2TextPipelineHost.generated.h"

UINTERFACE(MinimalAPI)
class UGV2TextPipelineHost : public UInterface
{
    GENERATED_BODY()
};

/**
 * DCA-17 (LayoutInvariant): marks a native UUserWidget base as approved to directly place a raw
 * UTextBlock/URichTextBlock in its own WidgetTree, because its C++ implementation is responsible
 * for routing that primitive's content through GV2TextPipeline (ApplyText/ResolveRunTextStyle or
 * equivalent) rather than leaving it as Designer-set literal text. Replaces the hand-written
 * 9-class IsChildOf chain in FGV2UiKitCentralThemeContract's "Text-bearing WBP must use a Text
 * Pipeline native base" check (GV2RuntimeSubsystemTests.cpp) -- a WBP asset failing that check
 * now means its native class doesn't implement this interface, not that someone forgot to add it
 * to a list. A widget that only composes a child WBP (e.g. WBP_DropdownSelect placing a WBP_Text
 * instance) does not need this interface: the check only fires on primitives placed directly in
 * the asset's own WidgetTree, and delegates to the child's own contract.
 */
class GV2_API IGV2TextPipelineHost
{
    GENERATED_BODY()
};
