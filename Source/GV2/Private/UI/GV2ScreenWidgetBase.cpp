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
    FString& OutError)
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

        FGV2UiHostMutationPlan MutationPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Host.HostWidget,
            Builder.Build(),
            *Value.PreparedValue,
            *Value.CompiledSchema,
            Value.SchemaId,
            FString(),
            PropertyHost->GetPropertyHostState().GetLastCommittedProperties(),
            MutationPlan,
            Diagnostics);
        if (!bPrepared)
        {
            OutError = FString::Printf(
                TEXT("screen field '%s' failed to prepare: %s"),
                *Host.FieldId.ToString(),
                Diagnostics.Num() > 0 ? *Diagnostics[0].ToString() : TEXT("unknown error"));
            return false;
        }

        ConsumedFieldIds.Add(Host.FieldId);
        OutPlans.Add({Host.HostWidget, MoveTemp(MutationPlan), Value.PreparedValue});
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

bool UGV2ScreenWidgetBase::PrepareScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    FGV2ScreenMutationPlan& OutPlan,
    FString& OutError) const
{
    return PrepareScreenFieldPlans(*this, ScreenFields, OutPlan.FieldPlans, OutError);
}

bool UGV2ScreenWidgetBase::CommitScreenFields(const FGV2ScreenMutationPlan& Plan)
{
    for (const FGV2ScreenFieldPlan& FieldPlan : Plan.FieldPlans)
    {
        FString FailedPath, CommitError;
        if (!CommitUiHostProperties(FieldPlan.HostWidget, FieldPlan.MutationPlan, FailedPath, CommitError))
        {
            // Every plan above already prepared cleanly; CommitUiHostProperties is
            // documented infallible against a plan it prepared itself. Reaching this
            // is therefore either injected test failure or a genuine engine-level
            // fault, not a predictable content error -- there is nothing to roll back
            // to (no compensating capture exists any more), so this is logged as the
            // implementation-limit case it is, not silently absorbed.
            UE_LOG(
                LogGV2ScreenWidget,
                Error,
                TEXT("ApplyScreenFields commit failed on '%s': %s"),
                *FailedPath,
                *CommitError);
            return false;
        }
        if (IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(FieldPlan.HostWidget.Get()))
        {
            PropertyHost->GetPropertyHostState().SetLastCommittedProperties(*FieldPlan.CommittedValue);
        }
    }

    OnScreenFieldsApplied();
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
