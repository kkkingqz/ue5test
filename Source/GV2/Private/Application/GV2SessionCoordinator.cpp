#include "Application/GV2SessionCoordinator.h"

#include "Templates/UnrealTemplate.h"

namespace
{
std::string ToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}

GV2RuntimeCore::FValue ToPortableValue(const FGV2UiControlValue& Value)
{
    switch (Value.Type)
    {
    case EGV2UiControlValueType::Null:
        return GV2RuntimeCore::FValue();
    case EGV2UiControlValueType::Boolean:
        return GV2RuntimeCore::FValue(Value.BooleanValue);
    case EGV2UiControlValueType::Integer:
        return GV2RuntimeCore::FValue(static_cast<std::int64_t>(Value.IntegerValue));
    case EGV2UiControlValueType::Number:
        return GV2RuntimeCore::FValue(Value.NumberValue);
    case EGV2UiControlValueType::String:
        return GV2RuntimeCore::FValue(ToUtf8(Value.StringValue));
    default:
        checkNoEntry();
        return GV2RuntimeCore::FValue();
    }
}

GV2RuntimeCore::FSemanticInput ToPortableInput(const FGV2UiIngressItem& Item)
{
    GV2RuntimeCore::FSemanticInput Input;
    Input.SessionGeneration = Item.Binding.SessionGeneration;
    Input.UiInstanceId = ToUtf8(Item.Binding.UiInstanceId);
    Input.Revision = Item.Binding.Revision;
    Input.Sequence = Item.Sequence;
    Input.ElementId = ToUtf8(Item.Binding.ElementId);
    Input.CommandId = ToUtf8(Item.Binding.CommandId);
    Input.NodeKeyPath.reserve(Item.Binding.NodeKeyPath.Num());
    for (const FString& Segment : Item.Binding.NodeKeyPath)
    {
        Input.NodeKeyPath.push_back(ToUtf8(Segment));
    }
    for (const FGV2UiControlValue& Value : Item.Binding.BoundArgs)
    {
        Input.Args.emplace(ToUtf8(Value.Name.ToString()), ToPortableValue(Value));
    }
    for (const FGV2UiControlValue& Value : Item.InputValues)
    {
        Input.Args.emplace(ToUtf8(Value.Name.ToString()), ToPortableValue(Value));
    }
    return Input;
}
}

FGV2SessionCoordinator::FGV2SessionCoordinator(const int32 InIngressCapacity)
    : IngressQueue(InIngressCapacity)
{
}

void FGV2SessionCoordinator::SetInteractionSink(FInteractionSink InSink)
{
    InteractionSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearInteractionSink()
{
    InteractionSink = nullptr;
}

bool FGV2SessionCoordinator::StartTestSession()
{
    check(IsInGameThread());

    BindingRegistry.EndSession();
    IngressQueue.Reset();
    RuntimeSession.Stop();

    ++Status.SessionGeneration;
    Status.ApplicationState = EGV2ApplicationState::Bootstrapping;
    Status.SessionState = EGV2SessionState::Creating;
    Status.bIsReady = false;
    NextInputSequence = 1;
    TestUiRevision = 0;
    BindingRegistry.BeginSession(Status.SessionGeneration);

    GV2RuntimeCore::FRuntimeFault Fault;
    if (!RuntimeSession.StartTestRuntime(Status.SessionGeneration, Fault))
    {
        FailRuntime(Fault);
        return false;
    }

    Status.ApplicationState = EGV2ApplicationState::MenuActive;
    Status.SessionState = EGV2SessionState::Ready;
    Status.bIsReady = true;
    return true;
}

void FGV2SessionCoordinator::EndSession(const EGV2SessionState FinalState)
{
    check(IsInGameThread());

    Status.bIsReady = false;
    BindingRegistry.EndSession();
    IngressQueue.Reset();
    RuntimeSession.Stop();
    Status.ApplicationState = EGV2ApplicationState::Uninitialized;
    Status.SessionState = FinalState;
    NextInputSequence = 1;
    TestUiRevision = 0;
}

const FGV2SessionStatus& FGV2SessionCoordinator::GetStatus() const
{
    return Status;
}

bool FGV2SessionCoordinator::PublishUiBindings(
    const FString& UiInstanceId,
    const int64 Revision,
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    check(IsInGameThread());

    if (!Status.bIsReady || Status.SessionState != EGV2SessionState::Ready)
    {
        OutHandles.Reset();
        return false;
    }

    return BindingRegistry.PublishBindings(UiInstanceId, Revision, Definitions, OutHandles);
}

bool FGV2SessionCoordinator::PublishTestUiBindings(
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = TestUiRevision + 1;
    if (!PublishUiBindings(UiInstanceId, CandidateRevision, Definitions, OutHandles))
    {
        return false;
    }

    TestUiRevision = CandidateRevision;
    return true;
}

EGV2SubmitUiInteractionResult FGV2SessionCoordinator::SubmitUiInteraction(
    const FGV2UiBindingHandle& BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    check(IsInGameThread());

    if (!Status.bIsReady || Status.SessionState != EGV2SessionState::Ready)
    {
        return EGV2SubmitUiInteractionResult::RuntimeNotReady;
    }

    FGV2UiBindingRecord Binding;
    switch (BindingRegistry.Resolve(BindingHandle, Binding))
    {
    case EGV2BindingResolveResult::Invalid:
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    case EGV2BindingResolveResult::Stale:
        return EGV2SubmitUiInteractionResult::StaleBindingHandle;
    case EGV2BindingResolveResult::Found:
        break;
    default:
        checkNoEntry();
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    if (!ValidateInputValues(Binding, InputValues))
    {
        return EGV2SubmitUiInteractionResult::InvalidInputValues;
    }

    FGV2UiIngressItem Item;
    Item.BindingHandle = BindingHandle;
    Item.Binding = MoveTemp(Binding);
    Item.InputValues = InputValues;
    Item.Sequence = NextInputSequence;

    if (!IngressQueue.TryEnqueue(MoveTemp(Item)))
    {
        return EGV2SubmitUiInteractionResult::IngressQueueFull;
    }

    ++NextInputSequence;
    PumpIngress();
    return EGV2SubmitUiInteractionResult::Accepted;
}

bool FGV2SessionCoordinator::IsExecutingRuntime() const
{
    return bExecutingRuntime;
}

bool FGV2SessionCoordinator::IsLuaVmStarted() const
{
    return RuntimeSession.IsStarted();
}

int32 FGV2SessionCoordinator::GetQueuedIngressCount() const
{
    return IngressQueue.Num();
}

bool FGV2SessionCoordinator::ValidateInputValues(
    const FGV2UiBindingRecord& Binding,
    const TArray<FGV2UiControlValue>& InputValues)
{
    TSet<FName> SeenNames;
    for (const FGV2UiControlValue& InputValue : InputValues)
    {
        const EGV2UiControlValueType* ExpectedType = Binding.InputFieldTypes.Find(InputValue.Name);
        if (InputValue.Name.IsNone()
            || SeenNames.Contains(InputValue.Name)
            || ExpectedType == nullptr
            || InputValue.Type != *ExpectedType
            || (InputValue.Type == EGV2UiControlValueType::Number && !FMath::IsFinite(InputValue.NumberValue)))
        {
            return false;
        }
        SeenNames.Add(InputValue.Name);
    }

    for (const FName RequiredField : Binding.RequiredInputFields)
    {
        if (!SeenNames.Contains(RequiredField))
        {
            return false;
        }
    }

    return true;
}

void FGV2SessionCoordinator::PumpIngress()
{
    check(IsInGameThread());

    if (bPumpingIngress)
    {
        return;
    }

    TGuardValue<bool> PumpGuard(bPumpingIngress, true);
    FGV2UiIngressItem Item;
    while (Status.bIsReady && IngressQueue.Dequeue(Item))
    {
        TGuardValue<bool> ExecutionGuard(bExecutingRuntime, true);
        GV2RuntimeCore::FRuntimeFault Fault;
        if (!RuntimeSession.DispatchSemanticInput(ToPortableInput(Item), Fault))
        {
            FailRuntime(Fault);
            return;
        }
        if (InteractionSink)
        {
            InteractionSink(Item);
        }
    }
}

void FGV2SessionCoordinator::FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault)
{
    Status.bIsReady = false;
    Status.ApplicationState = EGV2ApplicationState::Failed;
    Status.SessionState = EGV2SessionState::Failed;
    BindingRegistry.EndSession();
    IngressQueue.Reset();

    UE_LOG(
        LogTemp,
        Error,
        TEXT("GV2 Lua runtime fault: code=%s message=%s"),
        UTF8_TO_TCHAR(Fault.Code.c_str()),
        UTF8_TO_TCHAR(Fault.Message.c_str()));
}
