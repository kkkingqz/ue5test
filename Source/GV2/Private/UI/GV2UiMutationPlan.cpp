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

            if (TargetWidget == nullptr && Cast<IGV2UiPropertyHost>(HostWidget))
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
                // Absent optional property in candidate -> Reset consumer
                TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    Cap.SupportedKind, Cap.TargetType, Cap.TargetKind);

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
                // Reused instance must reset property no longer in schema
                TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    Cap.SupportedKind, Cap.TargetType, Cap.TargetKind);

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
