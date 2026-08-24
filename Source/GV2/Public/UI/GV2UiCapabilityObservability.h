#pragma once

#include "CoreMinimal.h"
#include "UI/GV2UiCapability.h"

class UUserWidget;
class UWidget;

/**
 * Harness for ADR-0040 Decision 4's observability requirement: a declared capability
 * whose application to the target does not produce a distinguishable physical effect is
 * not a real capability -- it is the same "whitelist != consumption" lie that
 * `ConsumedKeys` used to tell, relocated into the capability tree (UPP-11).
 */
struct GV2_API FGV2UiObservabilityFailure
{
    FString PropertyName;
    FString Reason;
};

/**
 * Captures a physical, comparable snapshot of every property a standard UPP-09 consumer
 * can write on a renderer control target (enabled, text, percent, brush resource identity,
 * binding handle), regardless of which single sub-property a given capability kind is
 * supposed to affect. Used so "nothing changed" is detectable without the harness having
 * to know in advance which physical property a given kind maps to.
 */
GV2_API FString CaptureUiTargetState(const UWidget* TargetWidget);

/**
 * For every RendererControl capability the tree declares, synthesizes two distinguishable
 * FGV2PreparedUiValue (A != B), runs the real Prepare/Commit pipeline for each in sequence
 * on HostWidget, and requires Capture(Commit(A)) != Capture(Commit(B)).
 *
 * A capability for which no distinguishable pair can be synthesized, for which Prepare or
 * Commit itself fails, or whose captured state does not change between A and B, is
 * reported as a failure in OutFailures -- never silently skipped. Returns true only if
 * every RendererControl capability in the tree is observable.
 *
 * Scope: RendererControl leaf/scalar-ish kinds matching UPP-09's standard consumers
 * (Boolean/Integer/Number/String/Key/Text/StableId/Binding). CollectionHost, NestedScreen
 * and Object/Array composite capabilities are out of this harness's scope -- they are
 * proven by the composite migration tasks (UPP-20+), which have their own consumers.
 */
GV2_API bool RunUiCapabilityObservabilityHarness(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    TArray<FGV2UiObservabilityFailure>& OutFailures);
