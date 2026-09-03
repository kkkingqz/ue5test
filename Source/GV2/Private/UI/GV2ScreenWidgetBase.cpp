#include "UI/GV2ScreenWidgetBase.h"

#include "Bridge/GV2StableIdUE.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
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
    TObjectPtr<UUserWidget> HostWidget;
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
    const TArray<FString>* ActiveCompositionChain)
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
        const FGV2UiPropertyHostState& HostState = PropertyHost->GetPropertyHostState();
		FGV2UiPropertyHostState::FCommittedSnapshot PreviousCommittedSnapshot = HostState.GetCommittedSnapshot();
        const FGV2PreparedUiObject PreviousCommittedValue = HostState.GetLastCommittedProperties();

        FGV2UiHostMutationPlan MutationPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Host.HostWidget,
            CapabilityTree,
            *Value.PreparedValue,
            *Value.CompiledSchema,
            Value.SchemaId,
            FString(),
            PreviousCommittedValue,
            MutationPlan,
            Diagnostics,
            ActiveCompositionChain);
        if (!bPrepared)
        {
            OutError = FString::Printf(
                TEXT("screen field '%s' failed to prepare: %s"),
                *Host.FieldId.ToString(),
                Diagnostics.Num() > 0 ? *Diagnostics[0].ToString() : TEXT("unknown error"));
            return false;
        }

        // GBF-04 (ADR-0041): an inverse is prepared from the complete committed
        // snapshot. On the first revision there is no old schema, so the candidate
        // schema correctly produces an all-Reset inverse. A non-empty old value without
        // its old schema is never guessed from the candidate schema: that would reset
        // properties the old schema owned but the candidate no longer owns.
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
                Host.HostWidget,
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
                ActiveCompositionChain))
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

void RollbackFieldPlans(TArrayView<const FGV2ScreenFieldPlan> FieldPlans)
{
    for (int32 Index = FieldPlans.Num() - 1; Index >= 0; --Index)
    {
        const FGV2ScreenFieldPlan& FieldPlan = FieldPlans[Index];
        FString RollbackFailedPath, RollbackError;
        if (!CommitUiHostProperties(FieldPlan.HostWidget, FieldPlan.RollbackPlan, RollbackFailedPath, RollbackError))
        {
            UE_LOG(LogGV2ScreenWidget, Error,
                TEXT("GBH-10: rollback failed restoring host '%s' property '%s': %s -- invariant violation, physical state may not match previous revision"),
                *GetNameSafe(FieldPlan.HostWidget.Get()), *RollbackFailedPath, *RollbackError);
        }
        else if (IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(FieldPlan.HostWidget.Get()))
        {
            PropertyHost->GetPropertyHostState().RestoreCommittedSnapshot(FieldPlan.PreviousCommittedSnapshot);
        }
    }
}

bool UGV2ScreenWidgetBase::PrepareScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    FGV2ScreenMutationPlan& OutPlan,
    FString& OutError,
    const TArray<FString>* ActiveCompositionChain) const
{
    return PrepareScreenFieldPlans(*this, ScreenFields, OutPlan.FieldPlans, OutError, ActiveCompositionChain);
}

// GBF-07: rollback_boundary=ScreenFields
bool UGV2ScreenWidgetBase::CommitScreenFields(
    const FGV2ScreenMutationPlan& Plan,
    TFunction<bool(const FString& PropertyPath)> FailureInjector)
{
    int32 CommittedHostCount = 0;
    for (const FGV2ScreenFieldPlan& FieldPlan : Plan.FieldPlans)
    {
        FString FailedPath, CommitError;
        if (!CommitUiHostProperties(FieldPlan.HostWidget, FieldPlan.MutationPlan, FailedPath, CommitError, FailureInjector, &FieldPlan.RollbackPlan))
        {
            // GBH-10 (ADR-0041): every plan above already prepared cleanly, so reaching
            // this is either injected test failure or a genuine engine-level fault, not
            // a predictable content error. CommitUiHostProperties already self-healed
            // *this* host back to its own previous value via FieldPlan.RollbackPlan;
            // hosts committed earlier in this same call still sit on their NEW value and
            // must be restored too, since this whole screen's revision is not published.
            RollbackFieldPlans(TArrayView<const FGV2ScreenFieldPlan>(Plan.FieldPlans.GetData(), CommittedHostCount));
            UE_LOG(
                LogGV2ScreenWidget,
                Error,
                TEXT("ApplyScreenFields commit failed on '%s': %s"),
                *FailedPath,
                *CommitError);
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
            PropertyHost->GetPropertyHostState().SetLastCommittedSnapshot(
                *FieldPlan.CommittedValue,
                FieldPlan.CommittedSchema,
                FieldPlan.CommittedSchemaId);
        }
    }

    return true;
}

bool UGV2ScreenWidgetBase::ApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields)
{
    FGV2ScreenMutationPlan Plan;
    FString Error;
    if (!PrepareScreenFields(ScreenFields, Plan, Error))
    {
        UE_LOG(LogGV2ScreenWidget, Error, TEXT("ApplyScreenFields rejected: %s"), *Error);
        return false;
    }
    return CommitScreenFields(Plan);
}

bool UGV2ScreenWidgetBase::CanApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields) const
{
    FGV2ScreenMutationPlan Plan;
    FString Error;
    return PrepareScreenFields(ScreenFields, Plan, Error);
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
