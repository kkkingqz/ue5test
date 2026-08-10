#include "Runtime/GV2RuntimeSubsystem.h"

#include "Application/GV2SessionCoordinator.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2Runtime, Log, All);

UGV2RuntimeSubsystem::UGV2RuntimeSubsystem(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

UGV2RuntimeSubsystem::~UGV2RuntimeSubsystem() = default;

void UGV2RuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Coordinator = MakePimpl<FGV2SessionCoordinator>();
    Coordinator->SetInteractionSink([this](const FGV2UiIngressItem& Item)
    {
        UE_LOG(
            LogGV2Runtime,
            Display,
            TEXT("Dispatched UI ingress: sequence=%lld binding=%s command=%s"),
            Item.Sequence,
            *Item.BindingHandle.ToString(),
            *Item.Binding.CommandId);

        OnTestInteractionAccepted.Broadcast(
            Item.Binding.CommandId,
            Item.BindingHandle,
            Item.Sequence);
    });
}

void UGV2RuntimeSubsystem::Deinitialize()
{
    if (Coordinator)
    {
        Coordinator->EndSession(EGV2SessionState::Destroyed);
        Coordinator->ClearInteractionSink();
        Coordinator.Reset();
    }

    Super::Deinitialize();
}

FGV2SessionStatus UGV2RuntimeSubsystem::GetSessionState() const
{
    return Coordinator ? Coordinator->GetStatus() : FGV2SessionStatus();
}

EGV2SubmitUiInteractionResult UGV2RuntimeSubsystem::SubmitUiInteraction(
    const FGV2UiBindingHandle BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    return Coordinator
        ? Coordinator->SubmitUiInteraction(BindingHandle, InputValues)
        : EGV2SubmitUiInteractionResult::RuntimeNotReady;
}

void UGV2RuntimeSubsystem::StartTestSession()
{
    check(IsInGameThread());
    check(Coordinator);

    if (!Coordinator->StartTestSession())
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to start GV2 test session"));
        return;
    }
    UE_LOG(
        LogGV2Runtime,
        Display,
        TEXT("Started test session generation %d"),
        Coordinator->GetStatus().SessionGeneration);
}

void UGV2RuntimeSubsystem::EndTestSession()
{
    check(IsInGameThread());
    check(Coordinator);
    Coordinator->EndSession(EGV2SessionState::Destroyed);
}

FGV2TestScreenViewModel UGV2RuntimeSubsystem::CreateTestScreenModel(
    const FText& DescriptionText,
    const TArray<FGV2TestButtonSpec>& Buttons)
{
    check(IsInGameThread());

    FGV2TestScreenViewModel Model;
    Model.DescriptionText = DescriptionText;
    if (!Coordinator || !Coordinator->GetStatus().bIsReady)
    {
        UE_LOG(LogGV2Runtime, Warning, TEXT("CreateTestScreenModel rejected: session is not Ready"));
        return Model;
    }

    TArray<const FGV2TestButtonSpec*> ValidButtons;
    TArray<FGV2UiBindingDefinition> Definitions;
    ValidButtons.Reserve(Buttons.Num());
    Definitions.Reserve(Buttons.Num());

    for (const FGV2TestButtonSpec& Button : Buttons)
    {
        if (Button.TestAction.IsEmpty())
        {
            UE_LOG(LogGV2Runtime, Warning, TEXT("Skipped test button with an empty action"));
            continue;
        }

        const int32 ButtonIndex = ValidButtons.Num();
        ValidButtons.Add(&Button);

        FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
        Definition.NodeKeyPath = {TEXT("route"), FString::Printf(TEXT("button_%d"), ButtonIndex)};
        Definition.CommandId = Button.TestAction;
    }

    TArray<FGV2UiBindingHandle> Handles;
    if (!Coordinator->PublishTestUiBindings(Definitions, Handles)
        || Handles.Num() != ValidButtons.Num())
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to publish test screen UI bindings"));
        return Model;
    }

    Model.Buttons.Reserve(ValidButtons.Num());
    for (int32 Index = 0; Index < ValidButtons.Num(); ++Index)
    {
        FGV2ButtonViewModel& ButtonModel = Model.Buttons.AddDefaulted_GetRef();
        ButtonModel.Text = ValidButtons[Index]->Text;
        ButtonModel.Binding = Handles[Index];
    }

    return Model;
}
