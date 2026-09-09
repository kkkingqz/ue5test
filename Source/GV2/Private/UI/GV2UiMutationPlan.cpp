#include "UI/GV2UiMutationPlan.h"
#include "Blueprint/UserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2ScreenFieldHost.h"

namespace
{
// GBH-08: DeclaredComposite<->child compatibility uses its own, pre-existing
// `core:diagnostic.ui_consumer.*` diagnostic namespace (distinct from schema<->Widget
// compatibility's `core:diagnostic.ui_capability.*`); this only names which of those
// existing codes corresponds to which IsUiCapabilitySubset mismatch reason, it does not
// introduce a new namespace.
FString MapSubsetMismatchToConsumerDiagnosticCode(EGV2UiCapabilitySubsetMismatch Mismatch)
{
    switch (Mismatch)
    {
    case EGV2UiCapabilitySubsetMismatch::IntRangeMismatch:
    case EGV2UiCapabilitySubsetMismatch::NumberRangeMismatch:
        return TEXT("core:diagnostic.ui_consumer.range_unsupported");
    case EGV2UiCapabilitySubsetMismatch::KeyedIdentityMismatch:
        return TEXT("core:diagnostic.ui_consumer.collection_identity_mismatch");
    case EGV2UiCapabilitySubsetMismatch::ItemMismatch:
        return TEXT("core:diagnostic.ui_consumer.item_capability_mismatch");
    case EGV2UiCapabilitySubsetMismatch::KeyPropertyMismatch:
    case EGV2UiCapabilitySubsetMismatch::EntryWidgetClassMismatch:
        return TEXT("core:diagnostic.ui_consumer.collection_identity_mismatch");
    case EGV2UiCapabilitySubsetMismatch::KindMismatch:
    case EGV2UiCapabilitySubsetMismatch::TargetKindMismatch:
    case EGV2UiCapabilitySubsetMismatch::None:
    default:
        return TEXT("core:diagnostic.ui_consumer.target_kind_mismatch");
    }
}

// GBF-04 (ADR-0041): undoes the first CommittedCount mutations of a forward plan by
// replaying the corresponding mutations of its validated RollbackPlan, in reverse order.
// ValidateUiRollbackPlan has established that RollbackMutations[i] targets the same
// property/widget as ForwardMutations[i], so restoring [0..CommittedCount-1] is simply
// re-running Commit/Reset with the prior prepared value, not a bespoke undo path.
//
// PAH-01: best-effort -- every mutation in range is still attempted even after one fails,
// since skipping the rest would leave strictly more of the host on its rejected new value,
// not less. The result names the FIRST failure; RollbackFailureInjector (test-only) lets a
// test force a specific replayed mutation to fail without needing a genuinely broken widget.
[[nodiscard]] FGV2UiRollbackResult RollbackCommittedMutations(
    const FGV2UiHostMutationPlan& RollbackPlan,
    int32 CommittedCount,
    const TFunction<bool(const FString& PropertyPath)>& RollbackFailureInjector = nullptr)
{
    FGV2UiRollbackResult Result = FGV2UiRollbackResult::Restored();
    const TArray<FGV2UiPropertyMutation>& RollbackMutations = RollbackPlan.GetMutations();
    const int32 RollbackCount = FMath::Min(CommittedCount, RollbackMutations.Num());
    for (int32 Index = RollbackCount - 1; Index >= 0; --Index)
    {
        const FGV2UiPropertyMutation& Mutation = RollbackMutations[Index];
        if (!Mutation.Consumer.IsValid() || !Mutation.TargetWidget.IsValid())
        {
            continue;
        }
        if (Mutation.bIsReset)
        {
            Mutation.Consumer->Reset(Mutation.TargetWidget.Get());
        }
        else
        {
            FString RollbackError;
            const bool bInjectedFailure = RollbackFailureInjector && RollbackFailureInjector(Mutation.PropertyPath);
            const bool bRestored = !bInjectedFailure && Mutation.Consumer->Commit(Mutation.TargetWidget.Get(), RollbackError);
            if (!bRestored)
            {
                if (bInjectedFailure)
                {
                    RollbackError = TEXT("core:diagnostic.ui_mutation.rollback_failed_injected: Injected restoration failure");
                }
                UE_LOG(LogTemp, Error,
                    TEXT("GBH-10: rollback commit failed on '%s': %s -- invariant violation, physical state may not match previous revision"),
                    *Mutation.PropertyPath, *RollbackError);
                if (Result.bRestored)
                {
                    Result = FGV2UiRollbackResult::RestorationFailed(Mutation.PropertyPath, RollbackError);
                }
            }
        }
    }
    return Result;
}

// PAH-01: the one place CommitUiHostProperties consumes RollbackCommittedMutations'
// [[nodiscard]] result -- overwrites OutError with the typed rollback-failure code plus
// both failures (forward and restoration) when restoration itself fails, so this
// function's own OutError never silently reverts to only the forward-failure message.
void ReportSelfHealResult(const FGV2UiRollbackResult& SelfHealResult, FString& OutError)
{
    if (!SelfHealResult.bRestored)
    {
        OutError = FString::Printf(
            TEXT("%s: forward failure (%s) AND restoration failed on '%s': %s"),
            GGV2UiRollbackFailedDiagnosticCode,
            *OutError,
            *SelfHealResult.FailedPropertyPath,
            *SelfHealResult.Diagnostic);
    }
}
}

const TCHAR* const GGV2UiRollbackFailedDiagnosticCode = TEXT("core:diagnostic.ui_rollback.restoration_failed");

TConstArrayView<EGV2PreparedUiValueKind> GetUiMutationKindsRequiringInverse()
{
    // The property-consumer factory is the owner of which value kinds can physically
    // mutate a widget. Count makes this an enum traversal, not a second hand-written list.
    static const TArray<EGV2PreparedUiValueKind> Kinds = []
    {
        TArray<EGV2PreparedUiValueKind> Result;
        for (uint8 Index = 0; Index < static_cast<uint8>(EGV2PreparedUiValueKind::Count); ++Index)
        {
            const EGV2PreparedUiValueKind Kind = static_cast<EGV2PreparedUiValueKind>(Index);
            if (FGV2PropertyConsumerFactory::GetKindHandlingStatus(Kind) == EGV2PropertyConsumerKindStatus::Supported)
            {
                Result.Add(Kind);
            }
        }
        return Result;
    }();
    return MakeArrayView(Kinds);
}

bool ValidateUiRollbackPlan(
    const FGV2UiHostMutationPlan& ForwardPlan,
    const FGV2UiHostMutationPlan& RollbackPlan,
    FString& OutError)
{
    const TArray<FGV2UiPropertyMutation>& ForwardMutations = ForwardPlan.GetMutations();
    const TArray<FGV2UiPropertyMutation>& RollbackMutations = RollbackPlan.GetMutations();
    if (ForwardMutations.Num() != RollbackMutations.Num())
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_rollback.plan_mismatch: forward plan has %d mutations but inverse has %d"),
            ForwardMutations.Num(), RollbackMutations.Num());
        return false;
    }

    const TConstArrayView<EGV2PreparedUiValueKind> InverseKinds = GetUiMutationKindsRequiringInverse();
    for (int32 Index = 0; Index < ForwardMutations.Num(); ++Index)
    {
        const FGV2UiPropertyMutation& Forward = ForwardMutations[Index];
        const FGV2UiPropertyMutation& Inverse = RollbackMutations[Index];
        if (!InverseKinds.Contains(Forward.Kind))
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_rollback.unsupported_mutation_kind: forward property '%s' has no inverse-required kind"),
                *Forward.PropertyPath);
            return false;
        }
        if (Forward.PropertyName != Inverse.PropertyName ||
            Forward.PropertyPath != Inverse.PropertyPath ||
            Forward.Kind != Inverse.Kind ||
            Forward.TargetWidget.Get() != Inverse.TargetWidget.Get())
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_rollback.plan_mismatch: inverse mutation %d does not match forward property '%s'"),
                Index, *Forward.PropertyPath);
            return false;
        }
    }

    return true;
}

bool PrepareUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2PreparedUiObject& Candidate,
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    const FGV2PreparedUiObject& LastCommittedProperties,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics,
    const TArray<FString>* ActiveCompositionChain,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    // 1. Validate Schema ⊆ Capabilities
    if (!CheckUiSchemaCapabilityCompatibility(Schema, Capabilities, SchemaId, PropertyPathPrefix, OutDiagnostics))
    {
        return false;
    }

    OutPlan.Reset();

    // 2. Iterate capabilities and prepare mutations
    for (const auto& CapEntry : Capabilities.Properties)
    {
        const FString& PropName = CapEntry.Key;
        const FGV2UiPropertyCapability& Cap = CapEntry.Value;
        const FString ChildPath = PropertyPathPrefix.IsEmpty()
            ? PropName
            : FString::Printf(TEXT("%s.%s"), *PropertyPathPrefix, *PropName);

        // Check if Schema owns this property
        bool bSchemaOwns = false;
        const GV2ContentCore::FCompiledUiFieldSpecPtr* MatchingFieldSpec = nullptr;
        if (Schema.Kind == GV2ContentCore::EUiFieldKind::Object || Schema.Kind == GV2ContentCore::EUiFieldKind::ScreenFields)
        {
            for (const auto& FieldEntry : Schema.Fields)
            {
                if (UTF8_TO_TCHAR(FieldEntry.Name.c_str()) == PropName)
                {
                    bSchemaOwns = true;
                    MatchingFieldSpec = &FieldEntry.Spec;
                    break;
                }
            }
        }

        UWidget* TargetWidget = nullptr;
        if (HostWidget)
        {
            if (Cap.TargetName != NAME_None)
            {
                TargetWidget = HostWidget->GetWidgetFromName(Cap.TargetName);
                if (TargetWidget == nullptr)
                {
                    if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(HostWidget->GetClass(), Cap.TargetName))
                    {
                        TargetWidget = Cast<UWidget>(Prop->GetObjectPropertyValue_InContainer(HostWidget));
                    }
                }
            }
            else
            {
                TargetWidget = Cast<UWidget>(HostWidget);
            }

            // A capability with NAME_None intentionally targets its owning property host
            // (for example the shared `key` capability). A named target is a declaration
            // about the instance WidgetTree; falling back to the host for a typo converts a
            // missing child into an unrelated target-type mismatch. DUC-05 needs that
            // declaration drift rejected deterministically during preflight.
            if (TargetWidget == nullptr
                && Cap.TargetName == NAME_None
                && Cast<IGV2UiPropertyHost>(HostWidget))
            {
                TargetWidget = Cast<UWidget>(HostWidget);
            }
        }

        if (bSchemaOwns)
        {
            const FGV2PreparedUiValue* PresentVal = Candidate.FindField(PropName);
            if (PresentVal)
            {
                // Property is present in Candidate
                if (HostWidget && !TargetWidget)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.missing_target");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("Target widget '%s' not found on host for property '%s'"),
                        *Cap.TargetName.ToString(), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                // DUC-07: a named target that is itself a property host declares its own
                // capability tree independently of this composite's declaration -- checking
                // the declared Kind against that second source catches drift (e.g. `Number`
                // declared against a child that only ever declares `Text`) that the schema
                // check above cannot, since the schema only ever sees this composite's side.
                // Scoped to RendererControl: a CollectionHost/NestedScreen/CustomControl
                // target (a repeater, a nested-screen slot) is a generic property host in
                // its own right and is not expected to self-declare a capability of the
                // same Kind it is addressed by from the outside.
                if (TargetWidget != nullptr && Cap.TargetName != NAME_None
                    && Cap.TargetType == EGV2UiCapabilityTargetType::RendererControl)
                {
                    if (const IGV2UiPropertyHost* ChildHost = Cast<IGV2UiPropertyHost>(TargetWidget))
                    {
                        FGV2UiCapabilityBuilder ChildBuilder;
                        ChildHost->DescribeUiCapabilities(ChildBuilder);
                        // GBH-08: Build() returns FGV2UiCapabilityTree by value -- must be
                        // kept alive in a named local for as long as ResolvedChildCap (a
                        // pointer INTO it) is read, including by the subset check below.
                        // Passing ChildBuilder.Build() directly as the call argument would
                        // dangle the moment this statement finished (temporary lifetime
                        // ends at the semicolon), which nothing detected before GBH-08
                        // since no caller dereferenced the resolved pointer afterward.
                        const FGV2UiCapabilityTree ChildCapabilityTree = ChildBuilder.Build();
                        const FGV2UiPropertyCapability* ResolvedChildCap = nullptr;
                        FString ResolveError;
                        bool bAmbiguous = false;
                        if (!ResolveDelegatedChildCapability(ChildCapabilityTree, Cap.SupportedKind, Cap.ChildCapabilityName, ResolvedChildCap, ResolveError, bAmbiguous, PropName))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = bAmbiguous
                                ? TEXT("core:diagnostic.ui_consumer.ambiguous_child_capability")
                                : TEXT("core:diagnostic.ui_consumer.target_kind_mismatch");
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *ResolveError);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }

                        // GBH-08: resolution above only disambiguates *which* child
                        // capability is meant; this is the same subset rule schema<->Widget
                        // compatibility uses (IsUiCapabilitySubset), now also comparing
                        // range/target_kind/keyed-identity here, not just kind.
                        EGV2UiCapabilitySubsetMismatch SubsetMismatch;
                        FString SubsetDetail;
                        if (!IsUiCapabilitySubset(Cap, *ResolvedChildCap, SubsetMismatch, SubsetDetail))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = MapSubsetMismatchToConsumerDiagnosticCode(SubsetMismatch);
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *SubsetDetail);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }
                    }
                }

                TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    Cap.SupportedKind, Cap.TargetType, Cap.TargetKind);
                if (!Consumer)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.unsupported_kind");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("No consumer available for kind on property '%s'"), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }
                // PSC-06 (ADR-0043 D1): no-op for most consumer kinds; the ones that
                // resolve session-scoped content override it (see IGV2PropertyConsumer's
                // own doc comment).
                Consumer->SetPrepareContext(PrepareContext);

                // Item schemas belong only to the generic keyed-collection consumer.
                // NestedScreen also has Array kind but is handled by the distinct tab
                // consumer, whose child fields carry their own Screen Field envelope.
                // Casting that consumer as FGV2KeyedCollectionPropertyConsumer corrupts
                // its state before Prepare can report a typed result.
                if (Cap.TargetType == EGV2UiCapabilityTargetType::CollectionHost)
                {
                    FGV2KeyedCollectionPropertyConsumer* const CollConsumer =
                        static_cast<FGV2KeyedCollectionPropertyConsumer*>(Consumer.Get());
                    if (MatchingFieldSpec != nullptr && *MatchingFieldSpec != nullptr && (*MatchingFieldSpec)->Items != nullptr)
                    {
                        FString FieldIdStr = PropName;
                        if (HostWidget != nullptr && HostWidget->GetClass()->ImplementsInterface(UGV2ScreenFieldHost::StaticClass()))
                        {
                            if (IGV2ScreenFieldHost* ScreenFieldHost = Cast<IGV2ScreenFieldHost>(HostWidget))
                            {
                                FieldIdStr = ScreenFieldHost->GetScreenFieldId().ToString();
                            }
                        }
                        CollConsumer->SetCompiledItemSpec(
                            (*MatchingFieldSpec)->Items,
                            SchemaId,
                            ChildPath,
                            FString(),
                            FieldIdStr);
                    }
                }
                else if (Cap.TargetType == EGV2UiCapabilityTargetType::NestedScreen)
                {
                    // DUC-11: hand the in-progress composition path down to the tab
                    // consumer so it can reject a screen_id reappearing on its own
                    // path (direct or indirect cycle) before creating/recursing into
                    // the nested screen. See FGV2ScreenMutationPlan/PrepareScreenFields'
                    // own doc comment for the full picture.
                    FGV2TabContainerTabsPropertyConsumer* TabConsumer =
                        static_cast<FGV2TabContainerTabsPropertyConsumer*>(Consumer.Get());
                    TabConsumer->SetActiveCompositionChain(ActiveCompositionChain);
                    // PSC-06 (ADR-0043 D1): same injection shape as ActiveCompositionChain
                    // above -- see PrepareUiHostProperties' own doc comment.
                    TabConsumer->SetPrepareContext(PrepareContext);
                }

                FString PrepError;
                if (!Consumer->Prepare(*PresentVal, Cap, TargetWidget, PrepError))
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_mutation.prepare_failed");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = PrepError;
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                FGV2UiPropertyMutation Mutation;
                Mutation.PropertyName = PropName;
                Mutation.PropertyPath = ChildPath;
                Mutation.Kind = Cap.SupportedKind;
                Mutation.Consumer = Consumer;
                Mutation.TargetWidget = TargetWidget;
                Mutation.bIsReset = false;
                Mutation.PreparedValue = *PresentVal;
                OutPlan.AddMutation(MoveTemp(Mutation));
            }
            else
            {
                // Absent optional property in candidate -> Reset consumer.
                // PCC-08: reset is held to the exact same invariants as apply (mirrors the
                // missing_target/unsupported_kind checks a few lines above) -- a reset the
                // widget cannot actually perform must reject Prepare with a typed
                // diagnostic, not silently add an unresolvable mutation that Commit later
                // no-ops without a trace.
                if (HostWidget && !TargetWidget)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.missing_target");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("Target widget '%s' not found on host for reset of property '%s'"),
                        *Cap.TargetName.ToString(), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                // DUC-07: same independent-source check as the apply branch above.
                if (TargetWidget != nullptr && Cap.TargetName != NAME_None
                    && Cap.TargetType == EGV2UiCapabilityTargetType::RendererControl)
                {
                    if (const IGV2UiPropertyHost* ChildHost = Cast<IGV2UiPropertyHost>(TargetWidget))
                    {
                        FGV2UiCapabilityBuilder ChildBuilder;
                        ChildHost->DescribeUiCapabilities(ChildBuilder);
                        // GBH-08: Build() returns FGV2UiCapabilityTree by value -- must be
                        // kept alive in a named local for as long as ResolvedChildCap (a
                        // pointer INTO it) is read, including by the subset check below.
                        // Passing ChildBuilder.Build() directly as the call argument would
                        // dangle the moment this statement finished (temporary lifetime
                        // ends at the semicolon), which nothing detected before GBH-08
                        // since no caller dereferenced the resolved pointer afterward.
                        const FGV2UiCapabilityTree ChildCapabilityTree = ChildBuilder.Build();
                        const FGV2UiPropertyCapability* ResolvedChildCap = nullptr;
                        FString ResolveError;
                        bool bAmbiguous = false;
                        if (!ResolveDelegatedChildCapability(ChildCapabilityTree, Cap.SupportedKind, Cap.ChildCapabilityName, ResolvedChildCap, ResolveError, bAmbiguous, PropName))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = bAmbiguous
                                ? TEXT("core:diagnostic.ui_consumer.ambiguous_child_capability")
                                : TEXT("core:diagnostic.ui_consumer.target_kind_mismatch");
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for reset of property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *ResolveError);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }

                        EGV2UiCapabilitySubsetMismatch SubsetMismatch;
                        FString SubsetDetail;
                        if (!IsUiCapabilitySubset(Cap, *ResolvedChildCap, SubsetMismatch, SubsetDetail))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = MapSubsetMismatchToConsumerDiagnosticCode(SubsetMismatch);
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for reset of property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *SubsetDetail);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }
                    }
                }

                TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    Cap.SupportedKind, Cap.TargetType, Cap.TargetKind);
                if (!Consumer)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.unsupported_kind");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("No consumer available for reset of property '%s'"), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                // DUC-03: a reset mutation's consumer never goes through Prepare(), so the
                // Key consumer needs PropertyName injected directly to route "selected_key"/
                // "default_tab_key" apart from the generic `key` on Reset.
                if (Cap.SupportedKind == EGV2PreparedUiValueKind::Key)
                {
                    static_cast<FGV2KeyPropertyConsumer*>(Consumer.Get())->SetPropertyNameForRouting(PropName);
                }

                FGV2UiPropertyMutation Mutation;
                Mutation.PropertyName = PropName;
                Mutation.PropertyPath = ChildPath;
                Mutation.Kind = Cap.SupportedKind;
                Mutation.Consumer = Consumer;
                Mutation.TargetWidget = TargetWidget;
                Mutation.bIsReset = true;
                Mutation.PreparedValue = FGV2PreparedUiValue::MakeNull();
                OutPlan.AddMutation(MoveTemp(Mutation));
            }
        }
        else
        {
            // Property not in schema: check if previously owned by this reused instance
            if (LastCommittedProperties.FindField(PropName) != nullptr)
            {
                // Reused instance must reset property no longer in schema. Same PCC-08
                // invariants as the branch above.
                if (HostWidget && !TargetWidget)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.missing_target");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("Target widget '%s' not found on host for reset of property '%s'"),
                        *Cap.TargetName.ToString(), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                // DUC-07: same independent-source check as the branches above.
                if (TargetWidget != nullptr && Cap.TargetName != NAME_None
                    && Cap.TargetType == EGV2UiCapabilityTargetType::RendererControl)
                {
                    if (const IGV2UiPropertyHost* ChildHost = Cast<IGV2UiPropertyHost>(TargetWidget))
                    {
                        FGV2UiCapabilityBuilder ChildBuilder;
                        ChildHost->DescribeUiCapabilities(ChildBuilder);
                        // GBH-08: Build() returns FGV2UiCapabilityTree by value -- must be
                        // kept alive in a named local for as long as ResolvedChildCap (a
                        // pointer INTO it) is read, including by the subset check below.
                        // Passing ChildBuilder.Build() directly as the call argument would
                        // dangle the moment this statement finished (temporary lifetime
                        // ends at the semicolon), which nothing detected before GBH-08
                        // since no caller dereferenced the resolved pointer afterward.
                        const FGV2UiCapabilityTree ChildCapabilityTree = ChildBuilder.Build();
                        const FGV2UiPropertyCapability* ResolvedChildCap = nullptr;
                        FString ResolveError;
                        bool bAmbiguous = false;
                        if (!ResolveDelegatedChildCapability(ChildCapabilityTree, Cap.SupportedKind, Cap.ChildCapabilityName, ResolvedChildCap, ResolveError, bAmbiguous, PropName))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = bAmbiguous
                                ? TEXT("core:diagnostic.ui_consumer.ambiguous_child_capability")
                                : TEXT("core:diagnostic.ui_consumer.target_kind_mismatch");
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for reset of property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *ResolveError);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }

                        EGV2UiCapabilitySubsetMismatch SubsetMismatch;
                        FString SubsetDetail;
                        if (!IsUiCapabilitySubset(Cap, *ResolvedChildCap, SubsetMismatch, SubsetDetail))
                        {
                            FGV2UiSchemaCompatibilityDiagnostic Diag;
                            Diag.Code = MapSubsetMismatchToConsumerDiagnosticCode(SubsetMismatch);
                            Diag.PropertyPath = ChildPath;
                            Diag.SchemaId = SchemaId;
                            Diag.Message = FString::Printf(
                                TEXT("Target widget '%s' for reset of property '%s': %s"),
                                *Cap.TargetName.ToString(), *PropName, *SubsetDetail);
                            OutDiagnostics.Add(MoveTemp(Diag));
                            return false;
                        }
                    }
                }

                TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    Cap.SupportedKind, Cap.TargetType, Cap.TargetKind);
                if (!Consumer)
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_consumer.unsupported_kind");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.Message = FString::Printf(TEXT("No consumer available for reset of property '%s'"), *PropName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    return false;
                }

                if (Cap.SupportedKind == EGV2PreparedUiValueKind::Key)
                {
                    static_cast<FGV2KeyPropertyConsumer*>(Consumer.Get())->SetPropertyNameForRouting(PropName);
                }

                FGV2UiPropertyMutation Mutation;
                Mutation.PropertyName = PropName;
                Mutation.PropertyPath = ChildPath;
                Mutation.Kind = Cap.SupportedKind;
                Mutation.Consumer = Consumer;
                Mutation.TargetWidget = TargetWidget;
                Mutation.bIsReset = true;
                Mutation.PreparedValue = FGV2PreparedUiValue::MakeNull();
                OutPlan.AddMutation(MoveTemp(Mutation));
            }
        }
    }

    return true;
}

bool PrepareUiHostRollbackPlan(
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
    const TArray<FString>* ActiveCompositionChain,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    FGV2UiHostMutationPlan PreviousValuePlan;
    if (!PrepareUiHostProperties(HostWidget, Capabilities, PreviousCommittedProperties, PreviousSchema,
            PreviousSchemaId, PropertyPathPrefix, PreviousCommittedProperties, PreviousValuePlan,
            OutDiagnostics, ActiveCompositionChain, PrepareContext))
    {
        return false;
    }

    // Candidate-only properties did not exist in the previous schema/value. Their inverse
    // is Reset, prepared through the ordinary pipeline against the candidate schema.
    const FGV2PreparedUiObject EmptyProperties;
    FGV2UiHostMutationPlan CandidateResetPlan;
    if (!PrepareUiHostProperties(HostWidget, Capabilities, EmptyProperties, CandidateSchema,
            CandidateSchemaId, PropertyPathPrefix, EmptyProperties, CandidateResetPlan,
            OutDiagnostics, ActiveCompositionChain, PrepareContext))
    {
        return false;
    }

    OutPlan.Reset();
    for (const FGV2UiPropertyMutation& Forward : ForwardPlan.GetMutations())
    {
        const auto FindMatching = [&Forward](const FGV2UiHostMutationPlan& Plan) -> const FGV2UiPropertyMutation*
        {
            for (const FGV2UiPropertyMutation& Candidate : Plan.GetMutations())
            {
                if (Candidate.PropertyName == Forward.PropertyName && Candidate.PropertyPath == Forward.PropertyPath &&
                    Candidate.Kind == Forward.Kind && Candidate.TargetWidget.Get() == Forward.TargetWidget.Get())
                {
                    return &Candidate;
                }
            }
            return nullptr;
        };

        if (const FGV2UiPropertyMutation* PreviousMutation = FindMatching(PreviousValuePlan))
        {
            OutPlan.AddMutation(*PreviousMutation);
        }
        else if (const FGV2UiPropertyMutation* ResetMutation = FindMatching(CandidateResetPlan))
        {
            OutPlan.AddMutation(*ResetMutation);
        }
        else
        {
            FGV2UiSchemaCompatibilityDiagnostic Diag;
            Diag.Code = TEXT("core:diagnostic.ui_rollback.inverse_missing");
            Diag.PropertyPath = Forward.PropertyPath;
            Diag.SchemaId = CandidateSchemaId;
            Diag.Message = TEXT("Neither committed schema restoration nor candidate-schema reset produced an inverse mutation");
            OutDiagnostics.Add(MoveTemp(Diag));
            return false;
        }
    }

    FString ValidationError;
    if (!ValidateUiRollbackPlan(ForwardPlan, OutPlan, ValidationError))
    {
        FGV2UiSchemaCompatibilityDiagnostic Diag;
        Diag.Code = TEXT("core:diagnostic.ui_rollback.plan_mismatch");
        Diag.SchemaId = CandidateSchemaId;
        Diag.Message = ValidationError;
        OutDiagnostics.Add(MoveTemp(Diag));
        return false;
    }
    return true;
}

// GBF-07: rollback_boundary=PropertyMutation
bool CommitUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiHostMutationPlan& Plan,
    FString& OutFailedPropertyPath,
    FString& OutError,
    TFunction<bool(const FString& PropertyPath)> FailureInjector,
    const FGV2UiHostMutationPlan* RollbackPlan,
    TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector)
{
    if (RollbackPlan != nullptr && !ValidateUiRollbackPlan(Plan, *RollbackPlan, OutError))
    {
        OutFailedPropertyPath.Reset();
        return false;
    }

    int32 CommittedCount = 0;
    for (const auto& Mutation : Plan.GetMutations())
    {
        // Check failure injection
        if (FailureInjector && FailureInjector(Mutation.PropertyPath))
        {
            OutFailedPropertyPath = Mutation.PropertyPath;
            OutError = FString::Printf(TEXT("core:diagnostic.ui_mutation.commit_failed_injected: Injected failure on property '%s'"),
                *Mutation.PropertyPath);
            if (RollbackPlan != nullptr)
            {
                ReportSelfHealResult(
                    RollbackCommittedMutations(*RollbackPlan, CommittedCount, RollbackFailureInjector),
                    OutError);
            }
            return false;
        }

        if (Mutation.bIsReset)
        {
            if (Mutation.Consumer.IsValid() && Mutation.TargetWidget.IsValid())
            {
                Mutation.Consumer->Reset(Mutation.TargetWidget.Get());
            }
            else
            {
                // PCC-08: PrepareUiHostProperties now rejects a reset mutation whose
                // consumer/target can't be resolved (core:diagnostic.ui_consumer.
                // missing_target / unsupported_kind), so a plan reaching Commit is only
                // ever supposed to contain resolvable reset mutations. Reaching this is
                // therefore a genuine consumer/target invalidation between Prepare and
                // Commit (e.g. GC'd widget), not a predictable content error -- surfaced,
                // not silently absorbed, matching the non-reset Commit failure below.
                OutFailedPropertyPath = Mutation.PropertyPath;
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_mutation.reset_target_invalidated: consumer or target for '%s' became invalid between Prepare and Commit"),
                    *Mutation.PropertyPath);
                if (RollbackPlan != nullptr)
                {
                    ReportSelfHealResult(
                        RollbackCommittedMutations(*RollbackPlan, CommittedCount, RollbackFailureInjector),
                        OutError);
                }
                return false;
            }
        }
        else
        {
            if (Mutation.Consumer.IsValid() && Mutation.TargetWidget.IsValid())
            {
                FString CommitError;
                if (!Mutation.Consumer->CommitWithFailureInjector(
                        Mutation.TargetWidget.Get(),
                        CommitError,
                        FailureInjector,
                        Mutation.PropertyPath))
                {
                    OutFailedPropertyPath = Mutation.PropertyPath;
                    OutError = CommitError;
                    if (RollbackPlan != nullptr)
                    {
                        ReportSelfHealResult(
                            RollbackCommittedMutations(*RollbackPlan, CommittedCount, RollbackFailureInjector),
                            OutError);
                    }
                    return false;
                }
            }
        }
        ++CommittedCount;
    }

    return true;
}
