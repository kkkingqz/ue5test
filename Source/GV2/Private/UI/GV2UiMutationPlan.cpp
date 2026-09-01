#include "UI/GV2UiMutationPlan.h"
#include "Blueprint/UserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2ScreenFieldHost.h"

bool PrepareUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2PreparedUiObject& Candidate,
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    const FGV2PreparedUiObject& LastCommittedProperties,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics)
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
                if (TargetWidget == nullptr)
                {
                    if (UGV2LocationPlayerStatusWidgetBase* PlayerStatus = Cast<UGV2LocationPlayerStatusWidgetBase>(HostWidget))
                    {
                        if (Cap.TargetName == TEXT("MeterRepeater") || Cap.TargetName == TEXT("MeterContainer"))
                        {
                            TargetWidget = PlayerStatus->GetMeterRepeater();
                        }
                        else if (Cap.TargetName == TEXT("ItemRepeater") || Cap.TargetName == TEXT("ItemIcons"))
                        {
                            TargetWidget = PlayerStatus->GetItemRepeater();
                        }
                        else if (Cap.TargetName == TEXT("EffectRepeater") || Cap.TargetName == TEXT("EffectIcons"))
                        {
                            TargetWidget = PlayerStatus->GetEffectRepeater();
                        }
                    }
                    else if (UGV2LocationSceneWidgetBase* Scene = Cast<UGV2LocationSceneWidgetBase>(HostWidget))
                    {
                        if (Cap.TargetName == TEXT("CharacterRepeater") || Cap.TargetName == TEXT("CharacterContainer"))
                        {
                            TargetWidget = Scene->GetCharacterRepeater();
                        }
                    }
                    else if (UGV2LocationCommandPanelWidgetBase* CmdPanel = Cast<UGV2LocationCommandPanelWidgetBase>(HostWidget))
                    {
                        if (Cap.TargetName == TEXT("ButtonRepeater") || Cap.TargetName == TEXT("ButtonContainer"))
                        {
                            TargetWidget = CmdPanel->GetRepeater();
                        }
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

                if (FGV2KeyedCollectionPropertyConsumer* CollConsumer = static_cast<FGV2KeyedCollectionPropertyConsumer*>(Consumer.Get()))
                {
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

bool CommitUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiHostMutationPlan& Plan,
    FString& OutFailedPropertyPath,
    FString& OutError,
    TFunction<bool(const FString& PropertyPath)> FailureInjector)
{
    for (const auto& Mutation : Plan.GetMutations())
    {
        // Check failure injection
        if (FailureInjector && FailureInjector(Mutation.PropertyPath))
        {
            OutFailedPropertyPath = Mutation.PropertyPath;
            OutError = FString::Printf(TEXT("core:diagnostic.ui_mutation.commit_failed_injected: Injected failure on property '%s'"),
                *Mutation.PropertyPath);
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
                return false;
            }
        }
        else
        {
            if (Mutation.Consumer.IsValid() && Mutation.TargetWidget.IsValid())
            {
                FString CommitError;
                if (!Mutation.Consumer->Commit(Mutation.TargetWidget.Get(), CommitError))
                {
                    OutFailedPropertyPath = Mutation.PropertyPath;
                    OutError = CommitError;
                    return false;
                }
            }
        }
    }

    return true;
}
