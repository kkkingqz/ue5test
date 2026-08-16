#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2ScreenFieldAdapterRegistry.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UnrealTemplate.h"
#include "UI/GV2TextPipeline.h"

namespace
{
std::string SessionCoordinatorToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}

bool LoadPortableRuntimeSources(
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources,
    GV2RuntimeCore::FRuntimeFault& OutFault)
{
    OutSources.clear();
    FString ScriptsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDirectory);
    const FString ScriptsPrefix = ScriptsDirectory + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(
        SourceFiles,
        *ScriptsDirectory,
        TEXT("*.lua"),
        true,
        false,
        false);
    SourceFiles.Sort();
    if (SourceFiles.IsEmpty())
    {
        OutFault = {"LuaRuntimeSourceMissing", "Scripts directory contains no Lua sources."};
        return false;
    }

    OutSources.reserve(SourceFiles.Num());
    for (const FString& FullPath : SourceFiles)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *FullPath))
        {
            OutFault = {
                "LuaRuntimeSourceMissing",
                SessionCoordinatorToUtf8(FString::Printf(TEXT("Lua source could not be read: %s"), *FullPath))};
            return false;
        }

        int32 Offset = 0;
        if (Bytes.Num() >= 3 && Bytes[0] == 0xef && Bytes[1] == 0xbb && Bytes[2] == 0xbf)
        {
            Offset = 3;
        }
        if (Bytes.Num() <= Offset)
        {
            OutFault = {
                "LuaRuntimeSourceInvalid",
                SessionCoordinatorToUtf8(FString::Printf(TEXT("Lua source is empty: %s"), *FullPath))};
            return false;
        }
        FString NormalizedFullPath = FullPath;
        FPaths::NormalizeFilename(NormalizedFullPath);
        if (!NormalizedFullPath.StartsWith(ScriptsPrefix, ESearchCase::CaseSensitive))
        {
            OutFault = {"LuaRuntimeSourceInvalid", "Lua source is outside the Scripts directory."};
            return false;
        }
        const FString RelativePath = NormalizedFullPath.RightChop(ScriptsPrefix.Len());
        GV2RuntimeCore::FRuntimeSource& Source = OutSources.emplace_back();
        Source.Name = "@Scripts/" + SessionCoordinatorToUtf8(RelativePath);
        Source.Text.assign(
            reinterpret_cast<const char*>(Bytes.GetData() + Offset),
            static_cast<std::size_t>(Bytes.Num() - Offset));
    }
    return true;
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
        return GV2RuntimeCore::FValue(SessionCoordinatorToUtf8(Value.StringValue));
    default:
        checkNoEntry();
        return GV2RuntimeCore::FValue();
    }
}

GV2RuntimeCore::FSemanticInput ToPortableInput(const FGV2UiIngressItem& Item)
{
    GV2RuntimeCore::FSemanticInput Input;
    Input.SessionGeneration = Item.Binding.SessionGeneration;
    Input.UiInstanceId = SessionCoordinatorToUtf8(Item.Binding.UiInstanceId);
    Input.Revision = Item.Binding.Revision;
    Input.Sequence = Item.Sequence;
    Input.ElementId = SessionCoordinatorToUtf8(Item.Binding.ElementId);
    Input.CommandId = SessionCoordinatorToUtf8(Item.Binding.CommandId);
    Input.NodeKeyPath.reserve(Item.Binding.NodeKeyPath.Num());
    for (const FString& Segment : Item.Binding.NodeKeyPath)
    {
        Input.NodeKeyPath.push_back(SessionCoordinatorToUtf8(Segment));
    }
    for (const FGV2UiControlValue& Value : Item.Binding.BoundArgs)
    {
        Input.Args.emplace(SessionCoordinatorToUtf8(Value.Name.ToString()), ToPortableValue(Value));
    }
    for (const FGV2UiControlValue& Value : Item.InputValues)
    {
        Input.Args.emplace(SessionCoordinatorToUtf8(Value.Name.ToString()), ToPortableValue(Value));
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

void FGV2SessionCoordinator::SetScreenSink(FScreenSink InSink)
{
    ScreenSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearScreenSink()
{
    ScreenSink = nullptr;
}

bool FGV2SessionCoordinator::StartSession(
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion)
{
    check(IsInGameThread());

    BindingRegistry.EndSession();
    IngressQueue.Reset();
    
    GV2RuntimeCore::FRuntimeFault StopFault;
    if (!RuntimeSession.Stop(&StopFault))
    {
        FailRuntime(StopFault);
        return false;
    }
    PinnedRepository = GV2ContentCore::FRepositoryReadHandle();
    Status.RepositoryVersion = 0;

    if (!InPinnedRepository.IsValid())
    {
        GV2RuntimeCore::FRuntimeFault Fault{"RepositoryNotReady", "No published GameDataRepository to pin."};
        FailRuntime(Fault);
        return false;
    }

    ++Status.SessionGeneration;
    Status.ApplicationState = EGV2ApplicationState::Bootstrapping;
    Status.SessionState = EGV2SessionState::Creating;
    Status.bIsReady = false;
    Status.RepositoryVersion = InRepositoryVersion;
    NextInputSequence = 1;
    UiRevision = 0;
    PinnedRepository = InPinnedRepository;
    BindingRegistry.BeginSession(Status.SessionGeneration);

    GV2RuntimeCore::FRuntimeFault Fault;
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    if (!LoadPortableRuntimeSources(RuntimeSources, Fault)
        || !RuntimeSession.Start(Status.SessionGeneration, InPinnedRepository, RuntimeSources, Fault))
    {
        FailRuntime(Fault);
        return false;
    }

    Status.ApplicationState = EGV2ApplicationState::MenuActive;
    Status.SessionState = EGV2SessionState::Ready;
    Status.bIsReady = true;

    std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
    if (RuntimeSession.TakePendingScreen(PendingScreen, Fault) && PendingScreen.has_value())
    {
        if (ScreenSink)
        {
            FGV2ScreenViewModel Model;
            FGV2PreparedBindingSet PreparedBindings;
            if (PrepareScreenRequest(*PendingScreen, Model, PreparedBindings)
                && ScreenSink(Model)
                && BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
            {
                ++UiRevision;
            }
        }
    }

    return true;
}

void FGV2SessionCoordinator::FailBootstrap(const FString& Code, const FString& Message)
{
    check(IsInGameThread());
    GV2RuntimeCore::FRuntimeFault Fault{
        TCHAR_TO_UTF8(*Code),
        TCHAR_TO_UTF8(*Message)};
    FailRuntime(Fault);
}

void FGV2SessionCoordinator::EndSession(const EGV2SessionState FinalState)
{
    check(IsInGameThread());

    Status.bIsReady = false;
    BindingRegistry.EndSession();
    IngressQueue.Reset();

    GV2RuntimeCore::FRuntimeFault StopFault;
    if (!RuntimeSession.Stop(&StopFault))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime Stop failed in EndSession: code=%s message=%s"),
            UTF8_TO_TCHAR(StopFault.Code.c_str()),
            UTF8_TO_TCHAR(StopFault.Message.c_str()));
    }
    PinnedRepository = GV2ContentCore::FRepositoryReadHandle();
    Status.ApplicationState = EGV2ApplicationState::Uninitialized;
    Status.SessionState = FinalState;
    Status.RepositoryVersion = 0;
    NextInputSequence = 1;
    UiRevision = 0;
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

bool FGV2SessionCoordinator::PublishScreenBindings(
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = UiRevision + 1;
    if (!PublishUiBindings(UiInstanceId, CandidateRevision, Definitions, OutHandles))
    {
        return false;
    }

    UiRevision = CandidateRevision;
    return true;
}

bool FGV2SessionCoordinator::PrepareScreenRequest(
    const GV2RuntimeCore::FScreenRequest& Request,
    FGV2ScreenViewModel& OutModel,
    FGV2PreparedBindingSet& OutBindings)
{
    OutModel = {};
    OutBindings = {};

    const FGV2ScreenFieldAdapterRegistry& FieldAdapters =
        FGV2ScreenFieldAdapterRegistry::Get();
    TArray<FGV2UiBindingDefinition> Definitions;
    if (!FieldAdapters.PrepareBindingDefinitions(Request, Definitions))
    {
        return false;
    }

    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = UiRevision + 1;
    if (!BindingRegistry.PrepareBindings(
            UiInstanceId,
            CandidateRevision,
            Definitions,
            OutBindings)
        || OutBindings.Handles.Num() != Definitions.Num())
    {
        return false;
    }

    OutModel.ScreenId = UTF8_TO_TCHAR(Request.ScreenId.c_str());
    return FieldAdapters.BuildFields(Request, OutBindings.Handles, OutModel.Fields);
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
        std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
        {
            TGuardValue<bool> ExecutionGuard(bExecutingRuntime, true);
            GV2RuntimeCore::FRuntimeFault Fault;
            if (!RuntimeSession.DispatchSemanticInput(ToPortableInput(Item), Fault))
            {
                FailRuntime(Fault);
                return;
            }
            if (!RuntimeSession.TakePendingScreen(PendingScreen, Fault))
            {
                FailRuntime(Fault);
                return;
            }
        }

        if (PendingScreen && ScreenSink)
        {
            FGV2ScreenViewModel Model;
            FGV2PreparedBindingSet PreparedBindings;
            if (PrepareScreenRequest(*PendingScreen, Model, PreparedBindings)
                && ScreenSink(Model)
                && BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
            {
                ++UiRevision;
            }
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
    GV2RuntimeCore::FRuntimeFault StopFault;
    if (!RuntimeSession.Stop(&StopFault))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime Stop failed in FailRuntime: code=%s message=%s"),
            UTF8_TO_TCHAR(StopFault.Code.c_str()),
            UTF8_TO_TCHAR(StopFault.Message.c_str()));
    }
    PinnedRepository = GV2ContentCore::FRepositoryReadHandle();
    Status.RepositoryVersion = 0;
    NextInputSequence = 1;
    UiRevision = 0;

    UE_LOG(
        LogTemp,
        Error,
        TEXT("GV2 Lua runtime fault: code=%s message=%s"),
        UTF8_TO_TCHAR(Fault.Code.c_str()),
        UTF8_TO_TCHAR(Fault.Message.c_str()));
}
