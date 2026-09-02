#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PropertyConsumers.h"

class UWidget;
class UUserWidget;

/**
 * Single prepared mutation step targeting a property and its widget control.
 */
struct GV2_API FGV2UiPropertyMutation
{
    FString PropertyName;
    FString PropertyPath;
    EGV2PreparedUiValueKind Kind = EGV2PreparedUiValueKind::Null;
    TSharedPtr<IGV2PropertyConsumer> Consumer;
    TWeakObjectPtr<UWidget> TargetWidget;
    bool bIsReset = false;
    FGV2PreparedUiValue PreparedValue;
};

/**
 * Immutable/pre-calculated host mutation plan produced during Prepare.
 * Consumed strictly by Commit to execute infallible mutations.
 */
class GV2_API FGV2UiHostMutationPlan
{
public:
    void AddMutation(FGV2UiPropertyMutation InMutation)
    {
        Mutations.Add(MoveTemp(InMutation));
    }

    const TArray<FGV2UiPropertyMutation>& GetMutations() const { return Mutations; }
    int32 Num() const { return Mutations.Num(); }
    bool IsEmpty() const { return Mutations.IsEmpty(); }
    void Reset() { Mutations.Reset(); }

private:
    TArray<FGV2UiPropertyMutation> Mutations;
};

/**
 * The authoritative set of presentation value kinds that can produce a direct widget
 * mutation and therefore require an inverse entry in every rollback plan. Tests iterate
 * this set; a new applicable kind cannot silently bypass the rollback-pair gate.
 */
GV2_API TConstArrayView<EGV2PreparedUiValueKind> GetUiMutationKindsRequiringInverse();

/**
 * Verifies the mandatory one-to-one correspondence between a forward plan and its
 * inverse. The forward plan itself enumerates every mutation that must be undoable.
 */
GV2_API bool ValidateUiRollbackPlan(
    const FGV2UiHostMutationPlan& ForwardPlan,
    const FGV2UiHostMutationPlan& RollbackPlan,
    FString& OutError);

/** Builds one inverse entry for every entry in ForwardPlan from previous and candidate schemas. */
GV2_API bool PrepareUiHostRollbackPlan(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2UiHostMutationPlan& ForwardPlan,
    const FGV2PreparedUiObject& PreviousCommittedProperties,
    const GV2ContentCore::FCompiledUiFieldSpec& PreviousSchema,
    const FString& PreviousSchemaId,
    const GV2ContentCore::FCompiledUiFieldSpec& CandidateSchema,
    const FString& CandidateSchemaId,
    const FString& PropertyPathPrefix,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics,
    const TArray<FString>* ActiveCompositionChain = nullptr);

/**
 * Prepares all property mutations for a host widget off-tree without mutating physical widget state.
 * Validates capabilities, target presence, styles, formats, assets, and builds mutation plan.
 */
// DUC-11: ActiveCompositionChain (default nullptr) is forwarded unchanged to the
// NestedScreen (tab container) consumer -- see FGV2TabContainerTabsPropertyConsumer's
// composition-cycle guard and UGV2ScreenWidgetBase::PrepareScreenFields' own doc
// comment for the full picture. Every other consumer ignores it.
GV2_API bool PrepareUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2PreparedUiObject& Candidate,
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    const FGV2PreparedUiObject& LastCommittedProperties,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics,
    const TArray<FString>* ActiveCompositionChain = nullptr);

/**
 * Commits a prepared mutation plan to the physical widget hierarchy.
 * Supports failure injection to verify atomicity and error diagnostics.
 *
 * GBF-04 (ADR-0041): RollbackPlan, when given, is a validated plan prepared against the
 * host's previously committed value and schema. If Commit fails partway through Plan,
 * every mutation already applied is undone by replaying the corresponding RollbackPlan
 * mutation, in reverse order, through the same consumer Commit/Reset path -- before this
 * function returns false. Callers that are themselves already inside a self-healing
 * scope (rolling back a mutation built purely for that purpose) pass nullptr to avoid
 * recursing into a rollback of a rollback.
 */
GV2_API bool CommitUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiHostMutationPlan& Plan,
    FString& OutFailedPropertyPath,
    FString& OutError,
    TFunction<bool(const FString& PropertyPath)> FailureInjector = nullptr,
    const FGV2UiHostMutationPlan* RollbackPlan = nullptr);
