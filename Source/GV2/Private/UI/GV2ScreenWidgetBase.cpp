#include "UI/GV2ScreenWidgetBase.h"

#include "Bridge/GV2StableIdUE.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2ScreenWidget, Log, All);

namespace
{
bool IsCanonicalFieldId(const FName FieldId)
{
    const FString Value = FieldId.ToString();
    return GV2StableIdUE::IsValidSegment(Value);
}

struct FGV2ScreenHostRecord
{
    TWeakObjectPtr<UUserWidget> HostWidget;
    FName FieldId;
};

// UPP-27 replacement for the retired IGV2DynamicScreenElement tree scan: finds
// every widget implementing IGV2ScreenFieldHost, keyed by its configured (non-
// NAME_None) GetScreenFieldId(). A host with NAME_None is unconfigured and is
// silently skipped, exactly like the old descriptor's !IsConfigured() case.
bool CollectScreenFieldHosts(
    const UGV2ScreenWidgetBase& Screen,
    TArray<FGV2ScreenHostRecord>& OutHosts,
    FString& OutError)
{
    OutHosts.Reset();
    if (Screen.WidgetTree == nullptr)
    {
        OutError = TEXT("screen has no WidgetTree");
        return false;
    }

    TSet<FName> SeenFieldIds;
    Screen.WidgetTree->ForEachWidget([&OutHosts, &SeenFieldIds, &OutError](UWidget* Widget)
    {
        if (!OutError.IsEmpty() || Widget == nullptr)
        {
            return;
        }
        IGV2ScreenFieldHost* Host = Cast<IGV2ScreenFieldHost>(Widget);
        if (Host == nullptr)
        {
            return;
        }
        const FName FieldId = Host->GetScreenFieldId();
        if (FieldId.IsNone())
        {
            return;
        }
        if (!IsCanonicalFieldId(FieldId))
        {
            OutError = FString::Printf(
                TEXT("screen field host '%s' has non-canonical field_id '%s'"),
                *Widget->GetName(),
                *FieldId.ToString());
            return;
        }
        if (SeenFieldIds.Contains(FieldId))
        {
            OutError = FString::Printf(TEXT("duplicate screen field host '%s'"), *FieldId.ToString());
            return;
        }
        UUserWidget* HostAsUserWidget = Cast<UUserWidget>(Widget);
        if (HostAsUserWidget == nullptr)
        {
            OutError = FString::Printf(TEXT("screen field host '%s' is not a UUserWidget"), *FieldId.ToString());
            return;
        }

        SeenFieldIds.Add(FieldId);
        OutHosts.Add({HostAsUserWidget, FieldId});
    });

    OutHosts.Sort([](const FGV2ScreenHostRecord& Left, const FGV2ScreenHostRecord& Right)
    {
        return Left.FieldId.LexicalLess(Right.FieldId);
    });
    return OutError.IsEmpty();
}

// The whole of UPP-27 / UPP-28: prepares every configured screen field host's mutation
// plan up front. A field host with a value that fails PrepareUiHostProperties
// -- including a deep child inside a keyed collection, since that consumer's
// own Prepare recurses fully before returning -- fails *here*, before any
// widget anywhere on the screen has been touched. There is nothing left to
// compensate for by the time Commit runs, so there is no captured "previous
// value" to roll back to.
bool PrepareScreenFieldPlans(
    const UGV2ScreenWidgetBase& Screen,
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    TArray<FGV2ScreenFieldPlan>& OutPlans,
    FString& OutError,
    const TArray<FString>* ActiveCompositionChain,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    TArray<FGV2ScreenHostRecord> Hosts;
    if (!CollectScreenFieldHosts(Screen, Hosts, OutError))
    {
        return false;
    }

    TMap<FName, const FGV2ScreenFieldValue*> ValuesById;
    for (const FGV2ScreenFieldValue& Value : ScreenFields)
    {
        if (!IsCanonicalFieldId(Value.FieldId))
        {
            OutError = FString::Printf(TEXT("payload has non-canonical field_id '%s'"), *Value.FieldId.ToString());
            return false;
        }
        if (ValuesById.Contains(Value.FieldId))
        {
            OutError = FString::Printf(TEXT("payload contains duplicate field '%s'"), *Value.FieldId.ToString());
            return false;
        }
        ValuesById.Add(Value.FieldId, &Value);
    }

    TSet<FName> ConsumedFieldIds;
    OutPlans.Reset();
    OutPlans.Reserve(Hosts.Num());
    for (const FGV2ScreenHostRecord& Host : Hosts)
    {
        const FGV2ScreenFieldValue* const* Found = ValuesById.Find(Host.FieldId);
        if (Found == nullptr)
        {
            OutError = FString::Printf(TEXT("screen field host '%s' has no value in the payload"), *Host.FieldId.ToString());
            return false;
        }
        const FGV2ScreenFieldValue& Value = **Found;
        if (!Value.PreparedValue.IsValid() || !Value.CompiledSchema)
        {
            OutError = FString::Printf(TEXT("screen field '%s' has no materialized value (BuildFields did not run)"), *Host.FieldId.ToString());
            return false;
        }

        IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(Host.HostWidget.Get());
        if (PropertyHost == nullptr)
        {
            OutError = FString::Printf(TEXT("screen field host '%s' does not implement IGV2UiPropertyHost"), *Host.FieldId.ToString());
            return false;
        }

        FGV2UiCapabilityBuilder Builder;
        PropertyHost->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree CapabilityTree = Builder.Build();
        const FGV2UiHostSemanticState& HostState =
            GetUiHostSemanticState(PropertyHost->GetPropertyHostState());
		FGV2UiHostCommittedSnapshot PreviousCommittedSnapshot = HostState.GetCommittedSnapshot();
        const FGV2PreparedUiObject PreviousCommittedValue = HostState.GetLastCommittedProperties();

        FGV2UiHostMutationPlan MutationPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Host.HostWidget.Get(),
            CapabilityTree,
            *Value.PreparedValue,
            *Value.CompiledSchema,
            Value.SchemaId,
            FString(),
            PreviousCommittedValue,
            MutationPlan,
            Diagnostics,
            ActiveCompositionChain,
            PrepareContext);
        if (!bPrepared)
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_mutation.prepare_failed: screen field '%s' cannot prepare: %s"),
                *Host.FieldId.ToString(),
                Diagnostics.Num() > 0 ? *Diagnostics[0].ToString() : TEXT("unknown error"));
            return false;
        }

        // GBH-10 (ADR-0041): build the mandatory inverse plan off-tree, alongside
        // MutationPlan, before any host commits.
        const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec>& PreviousCommittedSchema = HostState.GetLastCommittedSchema();
        const FString& PreviousCommittedSchemaId = HostState.GetLastCommittedSchemaId();
        const bool bHasCommittedSnapshot = PreviousCommittedSchema && !PreviousCommittedSchemaId.IsEmpty();
        if (!PreviousCommittedValue.IsEmpty() && !bHasCommittedSnapshot)
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_rollback.missing_committed_schema: screen field '%s' has a previous value but no committed schema snapshot"),
                *Host.FieldId.ToString());
            return false;
        }

        const GV2ContentCore::FCompiledUiFieldSpec& RollbackSchema = bHasCommittedSnapshot
            ? *PreviousCommittedSchema
            : *Value.CompiledSchema;
        const FString& RollbackSchemaId = bHasCommittedSnapshot
            ? PreviousCommittedSchemaId
            : Value.SchemaId;
        FGV2UiHostMutationPlan RollbackPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> RollbackDiagnostics;
        if (!PrepareUiHostRollbackPlan(
                Host.HostWidget.Get(),
                CapabilityTree,
                MutationPlan,
                PreviousCommittedValue,
                RollbackSchema,
                RollbackSchemaId,
                *Value.CompiledSchema,
                Value.SchemaId,
                FString(),
                RollbackPlan,
                RollbackDiagnostics,
                ActiveCompositionChain,
                PrepareContext))
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_rollback.prepare_failed: screen field '%s' cannot prepare inverse: %s"),
                *Host.FieldId.ToString(),
                RollbackDiagnostics.Num() > 0 ? *RollbackDiagnostics[0].ToString() : TEXT("unknown error"));
            return false;
        }

        FString RollbackPlanError;
        if (!ValidateUiRollbackPlan(MutationPlan, RollbackPlan, RollbackPlanError))
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_rollback.prepare_failed: screen field '%s' has no valid inverse: %s"),
                *Host.FieldId.ToString(), *RollbackPlanError);
            return false;
        }

        ConsumedFieldIds.Add(Host.FieldId);
        OutPlans.Add({Host.HostWidget, MoveTemp(MutationPlan), Value.PreparedValue, Value.CompiledSchema, Value.SchemaId, MoveTemp(PreviousCommittedSnapshot), MoveTemp(RollbackPlan)});
    }

    for (const TPair<FName, const FGV2ScreenFieldValue*>& Pair : ValuesById)
    {
        if (!ConsumedFieldIds.Contains(Pair.Key))
        {
            OutError = FString::Printf(TEXT("payload contains unknown field '%s'"), *Pair.Key.ToString());
            return false;
        }
    }
    return true;
}
}

FGV2UiRollbackResult RollbackFieldPlans(
    TArrayView<const FGV2ScreenFieldPlan> FieldPlans,
    TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector)
{
    FGV2UiRollbackResult Result = FGV2UiRollbackResult::Restored();
    for (int32 Index = FieldPlans.Num() - 1; Index >= 0; --Index)
    {
        const FGV2ScreenFieldPlan& FieldPlan = FieldPlans[Index];
        if (!FieldPlan.HostWidget.IsValid())
        {
            if (Result.bRestored)
            {
                Result = FGV2UiRollbackResult::RestorationFailed(
                    TEXT("host"),
                    TEXT("core:diagnostic.ui_screen.host_invalidated: Screen field host widget is no longer valid during rollback"));
            }
            continue;
        }
        FString RollbackFailedPath, RollbackError;
        if (!CommitUiHostProperties(FieldPlan.HostWidget.Get(), FieldPlan.RollbackPlan, RollbackFailedPath, RollbackError, RollbackFailureInjector))
        {
            UE_LOG(LogGV2ScreenWidget, Error,
                TEXT("GBH-10: rollback failed restoring host '%s' property '%s': %s -- invariant violation, physical state may not match previous revision"),
                *GetNameSafe(FieldPlan.HostWidget.Get()), *RollbackFailedPath, *RollbackError);
            if (Result.bRestored)
            {
                Result = FGV2UiRollbackResult::RestorationFailed(RollbackFailedPath, RollbackError);
            }
        }
        else if (IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(FieldPlan.HostWidget.Get()))
        {
            GetUiHostSemanticState(PropertyHost->GetPropertyHostState()).RestoreCommittedSnapshot(
                FieldPlan.PreviousCommittedSnapshot);
        }
    }
    return Result;
}

bool UGV2ScreenWidgetBase::PrepareScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    FGV2ScreenMutationPlan& OutPlan,
    FString& OutError,
    const TArray<FString>* ActiveCompositionChain,
    const FGV2PresentationPrepareContext* PrepareContext) const
{
    return PrepareScreenFieldPlans(*this, ScreenFields, OutPlan.FieldPlans, OutError, ActiveCompositionChain, PrepareContext);
}

// GBF-07: rollback_boundary=ScreenFields
bool UGV2ScreenWidgetBase::CommitScreenFields(
    const FGV2ScreenMutationPlan& Plan,
    FString& OutError,
    TFunction<bool(const FString& PropertyPath)> FailureInjector,
    TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector)
{
    OutError.Reset();
    int32 CommittedHostCount = 0;
    for (const FGV2ScreenFieldPlan& FieldPlan : Plan.FieldPlans)
    {
        if (!FieldPlan.HostWidget.IsValid())
        {
            OutError = TEXT("core:diagnostic.ui_screen.host_invalidated: Screen field host widget is no longer valid");
            return false;
        }
        FString FailedPath, CommitError;
        if (!CommitUiHostProperties(FieldPlan.HostWidget.Get(), FieldPlan.MutationPlan, FailedPath, CommitError, FailureInjector, &FieldPlan.RollbackPlan, RollbackFailureInjector))
        {
            // GBH-10 (ADR-0041): every plan above already prepared cleanly, so reaching
            // this is either injected test failure or a genuine engine-level fault, not
            // a predictable content error. CommitUiHostProperties already self-healed
            // *this* host back to its own previous value via FieldPlan.RollbackPlan (and,
            // per PAH-01, already folded GGV2UiRollbackFailedDiagnosticCode into CommitError
            // if that self-heal itself failed). Hosts committed earlier in this same call
            // still sit on their NEW value and must be restored too, since this whole
            // screen's revision is not published.
            OutError = CommitError;
            const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(
                TArrayView<const FGV2ScreenFieldPlan>(Plan.FieldPlans.GetData(), CommittedHostCount),
                RollbackFailureInjector);
            if (!SiblingRollback.bRestored && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
            {
                OutError = FString::Printf(
                    TEXT("%s: forward failure on '%s' (%s) AND sibling restoration failed on '%s': %s"),
                    GGV2UiRollbackFailedDiagnosticCode, *FailedPath, *CommitError,
                    *SiblingRollback.FailedPropertyPath, *SiblingRollback.Diagnostic);
            }
            UE_LOG(
                LogGV2ScreenWidget,
                Error,
                TEXT("ApplyScreenFields commit failed on '%s': %s"),
                *FailedPath,
                *OutError);
            return false;
        }
        ++CommittedHostCount;
    }

    // Only reached once every host above committed cleanly -- advance the complete
    // committed snapshots in one pass, after the fact, so a mid-loop failure never sees
    // metadata for the new revision while rollback restores the old physical state.
    for (const FGV2ScreenFieldPlan& FieldPlan : Plan.FieldPlans)
    {
        if (IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(FieldPlan.HostWidget.Get()))
        {
            GetUiHostSemanticState(PropertyHost->GetPropertyHostState()).SetLastCommittedSnapshot(
                *FieldPlan.CommittedValue,
                FieldPlan.CommittedSchema,
                FieldPlan.CommittedSchemaId);
        }
    }

    return true;
}

bool UGV2ScreenWidgetBase::ApplyScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    const FGV2PresentationPrepareContext& PrepareContext)
{
    FGV2ScreenMutationPlan Plan;
    FString Error;
    if (!PrepareScreenFields(ScreenFields, Plan, Error, nullptr, &PrepareContext))
    {
        UE_LOG(LogGV2ScreenWidget, Error, TEXT("ApplyScreenFields rejected: %s"), *Error);
        return false;
    }
    FString CommitError;
    return CommitScreenFields(Plan, CommitError);
}

bool UGV2ScreenWidgetBase::CanApplyScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    const FGV2PresentationPrepareContext& PrepareContext) const
{
    FGV2ScreenMutationPlan Plan;
    FString Error;
    return PrepareScreenFields(ScreenFields, Plan, Error, nullptr, &PrepareContext);
}

void UGV2ScreenWidgetBase::SetAnchoredContentPosition(const FVector2D& LocalPosition)
{
    UCanvasPanel* RootCanvas = WidgetTree != nullptr ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
    if (RootCanvas == nullptr || RootCanvas->GetChildrenCount() != 1)
    {
        // PEP-06B: not an anchor-authored screen (no canvas root, or more/fewer than one
        // child) -- a documented no-op, not a failure a caller has to check for.
        return;
    }
    UWidget* Content = RootCanvas->GetChildAt(0);
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Content->Slot);
    if (Slot == nullptr)
    {
        return;
    }

    // PEP-06C: keep the content inside the viewport -- the Slate tooltip window this content
    // used to live in (before PEP-06B) did this for free; moving it into an ordinary screen
    // lost it silently, and this restores it as ours. This screen fills its whole layer
    // (ApplyScreenSlotLayout's uniform Fill/Fill), so its own cached size IS the viewport
    // size. Both sizes come from live post-paint geometry, matching NativeTick's own
    // self-healing re-apply every tick; a size of zero (not yet painted) leaves that axis
    // unclamped rather than pinning it to the corner.
    const FVector2D ScreenSize = GetCachedGeometry().GetLocalSize();
    const FVector2D ContentSize = Content->GetCachedGeometry().GetLocalSize();
    FVector2D ClampedPosition = LocalPosition;
    if (ScreenSize.X > 0.0f)
    {
        ClampedPosition.X = FMath::Clamp(ClampedPosition.X, 0.0f, FMath::Max(0.0f, ScreenSize.X - ContentSize.X));
    }
    if (ScreenSize.Y > 0.0f)
    {
        ClampedPosition.Y = FMath::Clamp(ClampedPosition.Y, 0.0f, FMath::Max(0.0f, ScreenSize.Y - ContentSize.Y));
    }
    Slot->SetPosition(ClampedPosition);
}

FSlateRect UGV2ScreenWidgetBase::GetAnchoredContentScreenRect() const
{
    const UCanvasPanel* RootCanvas = WidgetTree != nullptr ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
    if (RootCanvas == nullptr || RootCanvas->GetChildrenCount() != 1)
    {
        return FSlateRect();
    }
    const UWidget* Content = RootCanvas->GetChildAt(0);
    return Content != nullptr ? Content->GetTickSpaceGeometry().GetLayoutBoundingRect() : FSlateRect();
}

TArray<FName> UGV2ScreenWidgetBase::GetScreenFieldIds() const
{
    TArray<FGV2ScreenHostRecord> Hosts;
    FString Error;
    if (!CollectScreenFieldHosts(*this, Hosts, Error))
    {
        UE_LOG(LogGV2ScreenWidget, Warning, TEXT("GetScreenFieldIds failed: %s"), *Error);
        return {};
    }

    TArray<FName> Result;
    Result.Reserve(Hosts.Num());
    for (const FGV2ScreenHostRecord& Host : Hosts)
    {
        Result.Add(Host.FieldId);
    }
    return Result;
}
