#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2RuntimeCore/GV2HostServices.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UnrealTemplate.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2TextPipeline.h"

namespace
{
std::string SessionCoordinatorToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}

bool IsValidSaveSlotId(const FString& SlotId)
{
    if (SlotId.IsEmpty())
    {
        return false;
    }
    const TCHAR First = SlotId[0];
    if (First < TEXT('a') || First > TEXT('z'))
    {
        return false;
    }
    for (int32 i = 1; i < SlotId.Len(); ++i)
    {
        const TCHAR C = SlotId[i];
        const bool bLower = (C >= TEXT('a') && C <= TEXT('z'));
        const bool bDigit = (C >= TEXT('0') && C <= TEXT('9'));
        if (!bLower && !bDigit && C != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

// PAH-04: pre_ready_discovery callers=FGV2SessionCoordinator::ExecuteSessionStart
// Called from FGV2SessionCoordinator::ExecuteSessionStart during candidate session start/replacement.
// Under ADR-0044, candidate preparation occurs before commit-to-replace while a prior Ready session
// may remain active; reading Lua sources and schema roots is scoped to the candidate session.
// PSC-02 (ADR-0043 D1/D5): ResolvedPackageSet is the caller's single already-resolved
// package set. This function reads PackageId/Root straight from it -- no
// second discovery of the package set, not even a per-root re-parse of package.json5
// (the old code called DiscoverPackageFromDirectory again here, after the caller had
// already discovered the same roots to build RuntimePackageRoots).
bool LoadPortableRuntimeSources(
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources,
    GV2RuntimeCore::FRuntimeFault& OutFault,
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet,
    TArray<FGV2SchemaPackageRoot>& OutSchemaPackageRoots)
{
    OutSources.clear();
    OutSchemaPackageRoots.Reset();
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
        Source.Name = "@core/" + SessionCoordinatorToUtf8(RelativePath);
        Source.Text.assign(
            reinterpret_cast<const char*>(Bytes.GetData() + Offset),
            static_cast<std::size_t>(Bytes.Num() - Offset));
    }

    OutSchemaPackageRoots.Reserve(static_cast<int32>(ResolvedPackageSet.OrderedSources.size()));
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedPackageSet.OrderedSources)
    {
        // PAH-04A: schemas reuse this exact resolved root/package_id pairing --
        // the same set this session's Lua sources load from, not a second
        // independent discovery pass. Unlike Lua sources, core's schemas (unlike
        // its scripts, which come from Scripts/ above) live under GameData/core/
        // like any other package's, so core is not skipped here.
        OutSchemaPackageRoots.Add(FGV2SchemaPackageRoot{
            UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
            UTF8_TO_TCHAR(Source.Root.string().c_str())});

        if (Source.Descriptor.GetPackageId() == "core")
        {
            continue;
        }
        auto PkgSources = GV2ContentHostSupport::DiscoverPackageScripts(Source.Root, Source.Descriptor.GetPackageId());
        for (auto& Src : PkgSources)
        {
            GV2RuntimeCore::FRuntimeSource& RuntimeSrc = OutSources.emplace_back();
            RuntimeSrc.Name = std::move(Src.Name);
            RuntimeSrc.Text = std::move(Src.Text);
        }
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

#if WITH_DEV_AUTOMATION_TESTS
bool FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false;
#endif

void FGV2SessionCoordinator::SetInteractionSink(FInteractionSink InSink)
{
    InteractionSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearInteractionSink()
{
    InteractionSink = nullptr;
}

void FGV2SessionCoordinator::SetDocumentSink(FDocumentSink InSink)
{
    DocumentSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearDocumentSink()
{
    DocumentSink = nullptr;
}

void FGV2SessionCoordinator::SetProjectionTeardownSink(FProjectionTeardownSink InSink)
{
    ProjectionTeardownSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearProjectionTeardownSink()
{
    ProjectionTeardownSink = nullptr;
}

void FGV2SessionCoordinator::SetProjectionPublishSink(FProjectionPublishSink InSink)
{
    ProjectionPublishSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearProjectionPublishSink()
{
    ProjectionPublishSink = nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
// PAH-04: pre_ready_discovery callers=none
// Test-only overload that resolves its fixture set before delegating to the production
// StartSession overload; it has no production callers and is absent from non-automation builds.
bool FGV2SessionCoordinator::StartSession(
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion)
{
    const FString GameDataDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentHostSupport::FResolvedPackageSet> Resolved;
    if (bTestForceIncludeSamplePackage)
    {
        const std::vector<std::filesystem::path> Roots = {
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("core")))),
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("textsystem")))),
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("sample")))),
        };
        Resolved = GV2ContentHostSupport::ResolvePackageSetFromDirectories(Roots, Diagnostics);
    }
    else
    {
        Resolved = GV2ContentHostSupport::ResolvePackageSetFromContainer(
            std::filesystem::path(SessionCoordinatorToUtf8(GameDataDirectory)), Diagnostics);
    }
    return Resolved.has_value() && StartSession(InPinnedRepository, InRepositoryVersion, *Resolved);
}
#endif

bool FGV2SessionCoordinator::StartSession(
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion,
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet)
{
    FSessionStartDescriptor Descriptor;
    Descriptor.Mode = ESessionStartMode::NewGame;
    Descriptor.RepositoryVersion = FString::Printf(TEXT("%lld"), InRepositoryVersion);
    Descriptor.RepositoryContentHash = InPinnedRepository.IsValid()
        ? UTF8_TO_TCHAR(InPinnedRepository.GetContentHash().c_str())
        : TEXT("");
    Descriptor.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    const uint64 OpId = RequestSession(Descriptor, InPinnedRepository, InRepositoryVersion, ResolvedPackageSet);
    const TOptional<FGV2SessionOperationResult> Outcome = GetSessionOperationOutcome(OpId);
    return Outcome.IsSet() && Outcome->IsCompleted();
}

uint64 FGV2SessionCoordinator::RequestSession(
    const FSessionStartDescriptor& Descriptor,
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion,
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet)
{
    check(IsInGameThread());

    bool bJoined = false;
    uint64 JoinedOpId = 0;
    const uint64 OpId = TransitionPolicy.EnqueueRequest(Descriptor, bJoined, JoinedOpId);
    if (bJoined)
    {
        return JoinedOpId;
    }

    PendingStartContext = FPendingStartContext{InPinnedRepository, InRepositoryVersion, ResolvedPackageSet};

    if (bProcessingTransition || bPumpingIngress || bExecutingRuntime)
    {
        return OpId;
    }

    ProcessNextTransition();
    return OpId;
}

uint64 FGV2SessionCoordinator::RequestLoad(const FString& SlotId, const ESaveSlotRevision Revision)
{
    check(IsInGameThread());
    const GV2ContentCore::FRepositoryReadHandle* RepoToUse = nullptr;
    int64 RepoVerToUse = 0;
    const GV2ContentHostSupport::FResolvedPackageSet* PkgToUse = nullptr;

    if (ActivePackageSet.IsSet() && PinnedRepository.IsValid())
    {
        RepoToUse = &PinnedRepository;
        RepoVerToUse = Status.RepositoryVersion;
        PkgToUse = &*ActivePackageSet;
    }
    else if (PendingStartContext.IsSet())
    {
        RepoToUse = &PendingStartContext->PinnedRepository;
        RepoVerToUse = PendingStartContext->RepositoryVersion;
        PkgToUse = &PendingStartContext->ResolvedPackageSet;
    }

    if (!RepoToUse || !PkgToUse)
    {
        const uint64 OpId = TransitionPolicy.AllocateOperationId();
        TransitionPolicy.RecordFailure(OpId, FGV2OperationFault{FGV2SessionFaultCodes::RepositoryNotReady, TEXT("Repository or package set is not ready for RequestLoad")});
        return OpId;
    }

    FSessionStartDescriptor Descriptor;
    Descriptor.Mode = ESessionStartMode::LoadSave;
    Descriptor.SaveSlotId = SlotId;
    Descriptor.SaveSlotRevision = Revision == ESaveSlotRevision::Previous ? TEXT("Previous") : TEXT("Current");
    Descriptor.RepositoryVersion = FString::Printf(TEXT("%lld"), RepoVerToUse);
    Descriptor.RepositoryContentHash = UTF8_TO_TCHAR(RepoToUse->GetContentHash().c_str());
    Descriptor.Reason = TEXT("RequestLoad");

    return RequestSession(Descriptor, *RepoToUse, RepoVerToUse, *PkgToUse);
}

void FGV2SessionCoordinator::SetSaveSlotStorage(GV2RuntimeCore::ISaveSlotStorage* InStorage)
{
    SaveSlotStorage = InStorage;
    RuntimeSession.SetSaveSlotStorage(SaveSlotStorage);
}

uint64 FGV2SessionCoordinator::RequestSave(const FString& SlotId)
{
    check(IsInGameThread());
    const uint64 OpId = TransitionPolicy.AllocateOperationId();

    if (!Status.bIsReady)
    {
        TransitionPolicy.RecordFailure(OpId, FGV2OperationFault{FGV2SessionFaultCodes::SessionNotReady, TEXT("Session is not ready for RequestSave")});
        return OpId;
    }

    if (!IsValidSaveSlotId(SlotId))
    {
        TransitionPolicy.RecordFailure(OpId, FGV2OperationFault{FGV2SessionFaultCodes::InvalidSaveSlotId, FString::Printf(TEXT("Invalid save slot ID: %s"), *SlotId)});
        return OpId;
    }

    if (bPumpingIngress || bExecutingRuntime || bProcessingTransition)
    {
        PendingSaveRequests.Add(FPendingSaveRequest{OpId, SlotId});
        return OpId;
    }

    ExecuteSaveOperation(OpId, SlotId);
    return OpId;
}

void FGV2SessionCoordinator::ExecuteSaveOperation(const uint64 OpId, const FString& SlotId)
{
    if (!Status.bIsReady)
    {
        TransitionPolicy.RecordFailure(OpId, FGV2OperationFault{FGV2SessionFaultCodes::SessionNotReady, TEXT("Session is not ready for save operation")});
        return;
    }

    GV2RuntimeCore::FRuntimeFault Fault;
    const bool bSuccess = RuntimeSession.SaveToSlot(SessionCoordinatorToUtf8(SlotId), Fault);
    if (bSuccess)
    {
        TransitionPolicy.RecordOutcome(OpId, ESessionNonFailureOutcome::Completed);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("SaveToSlot failed: code=%s message=%s"),
            UTF8_TO_TCHAR(Fault.Code.c_str()),
            UTF8_TO_TCHAR(Fault.Message.c_str()));
        TransitionPolicy.RecordFailure(OpId, Fault);
    }
}

void FGV2SessionCoordinator::DrainPendingSaveRequests()
{
    while (!PendingSaveRequests.IsEmpty())
    {
        const FPendingSaveRequest Req = PendingSaveRequests[0];
        PendingSaveRequests.RemoveAt(0);
        ExecuteSaveOperation(Req.OperationId, Req.SlotId);
    }
}

ESessionCancellationResult FGV2SessionCoordinator::CancelSessionRequest(const uint64 OperationId)
{
    check(IsInGameThread());

    for (int32 Index = 0; Index < PendingSaveRequests.Num(); ++Index)
    {
        if (PendingSaveRequests[Index].OperationId == OperationId)
        {
            PendingSaveRequests.RemoveAt(Index);
            TransitionPolicy.RecordOutcome(OperationId, ESessionNonFailureOutcome::Cancelled);
            return ESessionCancellationResult::Accepted;
        }
    }

    const ESessionCancellationResult Result = TransitionPolicy.CancelRequest(OperationId);
    if (Result == ESessionCancellationResult::Accepted)
    {
        if (PendingStartContext.IsSet() && TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->OperationId != OperationId)
        {
            PendingStartContext.Reset();
        }
    }
    return Result;
}

TOptional<FGV2SessionOperationResult> FGV2SessionCoordinator::GetSessionOperationOutcome(const uint64 OperationId) const
{
    return TransitionPolicy.GetOutcome(OperationId);
}

void FGV2SessionCoordinator::ProcessNextTransition()
{
    if (bProcessingTransition)
    {
        return;
    }

    TGuardValue<bool> ProcessingGuard(bProcessingTransition, true);

    while (TransitionPolicy.HasPendingOperation())
    {
        TOptional<FSessionOperationRecord> NextOp = TransitionPolicy.DequeuePendingOperation();
        if (!NextOp.IsSet())
        {
            break;
        }

        switch (NextOp->Kind)
        {
        case ESessionTransitionKind::Menu:
        case ESessionTransitionKind::NewGame:
        case ESessionTransitionKind::LoadSave:
            if (PendingStartContext.IsSet())
            {
                FPendingStartContext Context = MoveTemp(*PendingStartContext);
                PendingStartContext.Reset();
                ExecuteSessionStart(*NextOp, Context.PinnedRepository, Context.RepositoryVersion, Context.ResolvedPackageSet);
            }
            else
            {
                TransitionPolicy.RecordFailure(NextOp->OperationId, FGV2OperationFault{FGV2SessionFaultCodes::NoPendingStartContext, TEXT("No pending start context found for session start")});
            }
            break;

        case ESessionTransitionKind::Shutdown:
            PendingStartContext.Reset();
            ExecuteShutdown(*NextOp, EGV2SessionState::Destroyed);
            break;
        }
    }
}

bool FGV2SessionCoordinator::ExecuteSessionStart(
    const FSessionOperationRecord& Op,
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion,
    const GV2ContentHostSupport::FResolvedPackageSet& InResolvedPackageSet)
{
    check(IsInGameThread());
    CurrentTransitionKind = Op.Kind;
    const bool bHadPriorReadySession = Status.bIsReady;

    FString DescriptorError;
    if (!Op.Descriptor.IsValid(&DescriptorError))
    {
        FailReplacementAttempt(
            {"InvalidSessionDescriptor", TCHAR_TO_UTF8(*DescriptorError)}, bHadPriorReadySession);
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::InvalidSessionDescriptor, DescriptorError});
        return false;
    }

    if (!InPinnedRepository.IsValid())
    {
        FailReplacementAttempt(
            {"RepositoryNotReady", "No published GameDataRepository to pin."}, bHadPriorReadySession);
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::RepositoryNotReady, TEXT("No published GameDataRepository to pin.")});
        return false;
    }

    GV2RuntimeCore::FRuntimeFault Fault;
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    TArray<FGV2SchemaPackageRoot> SchemaPackageRoots;
    if (!LoadPortableRuntimeSources(RuntimeSources, Fault, InResolvedPackageSet, SchemaPackageRoots))
    {
        FailReplacementAttempt(Fault, bHadPriorReadySession);
        TransitionPolicy.RecordFailure(Op.OperationId, Fault);
        return false;
    }

    TArray<FString> ClosurePackageIds;
    ClosurePackageIds.Reserve(SchemaPackageRoots.Num());
    for (const FGV2SchemaPackageRoot& SchemaRoot : SchemaPackageRoots)
    {
        ClosurePackageIds.Add(SchemaRoot.PackageId);
    }

    TUniquePtr<FGV2SessionContentSnapshot> Candidate = MakeUnique<FGV2SessionContentSnapshot>();
    GV2RuntimeCore::FRuntimeFault CandidateFault;
    if (!FGV2SessionContentCandidate::Build(
            InPinnedRepository,
            InResolvedPackageSet,
            SchemaPackageRoots,
            RuntimeSources,
            *Candidate,
            CandidateFault))
    {
        FailReplacementAttempt(CandidateFault, bHadPriorReadySession);
        TransitionPolicy.RecordFailure(Op.OperationId, CandidateFault);
        return false;
    }

    // Cancellation checkpoint 1: Before BeginReplace (Session A remains completely intact)
    if (Op.bCancellationRequested || (TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->bCancellationRequested))
    {
        FailReplacementAttempt({"OperationCancelled", "Session start cancelled before BeginReplace."}, bHadPriorReadySession);
        TransitionPolicy.RecordOutcome(Op.OperationId, ESessionNonFailureOutcome::Cancelled);
        return false;
    }

    // CFC-10: For LoadSave, capture save bytes from storage and perform active VM preflight before BeginReplace
    std::string CapturedSaveBytes;
    if (Op.Descriptor.Mode == ESessionStartMode::LoadSave)
    {
        if (SaveSlotStorage == nullptr)
        {
            FailReplacementAttempt(
                {"SaveSlotStorageUnavailable", "Save storage is not configured."},
                bHadPriorReadySession);
            TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::SaveSlotStorageUnavailable, TEXT("Save storage is not configured.")});
            return false;
        }

        GV2RuntimeCore::ESaveSlotRevision StorageRevision = GV2RuntimeCore::ESaveSlotRevision::Current;
        if (Op.Descriptor.SaveSlotRevision.Equals(TEXT("Previous"), ESearchCase::IgnoreCase))
        {
            StorageRevision = GV2RuntimeCore::ESaveSlotRevision::Previous;
        }

        const GV2RuntimeCore::FSaveSlotReadResult ReadResult = SaveSlotStorage->ReadSlot(
            SessionCoordinatorToUtf8(Op.Descriptor.SaveSlotId), StorageRevision);
        if (ReadResult.Result == GV2RuntimeCore::ESaveSlotResult::NotFound)
        {
            FailReplacementAttempt(
                {"SaveSlotNotFound", "Requested save slot was not found."},
                bHadPriorReadySession);
            TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::SaveSlotNotFound, TEXT("Requested save slot was not found.")});
            return false;
        }
        if (ReadResult.Result != GV2RuntimeCore::ESaveSlotResult::Ok)
        {
            FailReplacementAttempt(
                {"SaveSlotUnreadable", "Requested save slot is unreadable."},
                bHadPriorReadySession);
            TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::SaveSlotUnreadable, TEXT("Requested save slot is unreadable.")});
            return false;
        }

        CapturedSaveBytes = ReadResult.Bytes;
#if WITH_DEV_AUTOMATION_TESTS
        if (TestOnSaveBytesCaptured)
        {
            TestOnSaveBytesCaptured();
        }
#endif

        if (RuntimeSession.IsStarted())
        {
            GV2RuntimeCore::FRuntimeFault PreflightFault;
            if (!RuntimeSession.PreflightSaveBytes(CapturedSaveBytes, PreflightFault))
            {
                FailReplacementAttempt(PreflightFault, bHadPriorReadySession);
                TransitionPolicy.RecordFailure(Op.OperationId, PreflightFault);
                return false;
            }
        }
    }

    if (InRepositoryVersion <= 0 || !InPinnedRepository.IsValid())
    {
        FailReplacementAttempt({"RepositoryVersionChanged", "Repository handle is invalid or changed before BeginReplace."}, bHadPriorReadySession);
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::RepositoryVersionChanged, TEXT("Repository handle is invalid or changed before BeginReplace.")});
        return false;
    }

    FSessionReplacementToken Token(MoveTemp(Candidate));

    GV2RuntimeCore::FRuntimeFault ReplaceFault;
    if (!BeginReplace(Token, InPinnedRepository, InRepositoryVersion, ReplaceFault, Op.Kind))
    {
        TransitionPolicy.RecordFailure(Op.OperationId, ReplaceFault);
        return false;
    }

    if (TransitionPolicy.GetActiveOperation().IsSet())
    {
        TransitionPolicy.GetActiveOperation()->bCommitted = true;
    }

    GV2RuntimeCore::FSessionStartInputs StartInputs;
    StartInputs.SessionGeneration = Status.SessionGeneration;
    StartInputs.SeedHex = Op.Descriptor.Mode == ESessionStartMode::LoadSave ? "" : TCHAR_TO_UTF8(*Op.Descriptor.SeedHex);
    StartInputs.Mode = Op.Descriptor.Mode == ESessionStartMode::Menu ? "Menu" : (Op.Descriptor.Mode == ESessionStartMode::LoadSave ? "LoadSave" : "NewGame");
    StartInputs.RepositoryVersion = TCHAR_TO_UTF8(*Op.Descriptor.RepositoryVersion);
    StartInputs.RepositoryContentHash = TCHAR_TO_UTF8(*Op.Descriptor.RepositoryContentHash);

    const std::string* LoadContainerBytesPtr = Op.Descriptor.Mode == ESessionStartMode::LoadSave ? &CapturedSaveBytes : nullptr;

    // Discrete phase execution using StartSessionPhases
    bool bPhasesOk = RuntimeSession.StartSessionPhases(
        StartInputs,
        InPinnedRepository,
        Token.GetCandidate().GetLuaSources(),
        LoadContainerBytesPtr,
        [this, Op](GV2RuntimeCore::ERuntimeLifecyclePhase Phase, const GV2RuntimeCore::FRuntimePhaseResult& Result) -> bool
        {
            if (TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->bCancellationRequested)
            {
                return false;
            }

            switch (Phase)
            {
            case GV2RuntimeCore::ERuntimeLifecyclePhase::Registering:
                TryTransitionSessionState(Status, EGV2SessionState::Registering, Op.Kind);
                break;
            case GV2RuntimeCore::ERuntimeLifecyclePhase::BuildingState:
                TryTransitionSessionState(Status, EGV2SessionState::BuildingState, Op.Kind);
                break;
            case GV2RuntimeCore::ERuntimeLifecyclePhase::RestoringInstances:
                TryTransitionSessionState(Status, EGV2SessionState::RestoringInstances, Op.Kind);
                break;
            case GV2RuntimeCore::ERuntimeLifecyclePhase::Starting:
                TryTransitionSessionState(Status, EGV2SessionState::Starting, Op.Kind);
                break;
            }

            if (TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->bCancellationRequested)
            {
                return false;
            }

            return true;
        },
        ReplaceFault);

    if (!bPhasesOk)
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "phase_failure");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=%s message=%s"),
            UTF8_TO_TCHAR(ReplaceFault.Code.c_str()),
            UTF8_TO_TCHAR(ReplaceFault.Message.c_str()));
        const bool bWasCancelled = (ReplaceFault.Code == "OperationCancelled")
            || (TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->bCancellationRequested);
        if (bWasCancelled)
        {
            TransitionPolicy.RecordOutcome(Op.OperationId, ESessionNonFailureOutcome::Cancelled);
        }
        else
        {
            TransitionPolicy.RecordFailure(Op.OperationId, ReplaceFault);
        }
        return false;
    }

    FGV2SessionContentCandidate::FinalizeScriptIdentity(Token.GetCandidate(), RuntimeSession.GetScriptSetHash());
    Token.TransitionTo(EReplacementStage::Preparing);

    TryTransitionSessionState(Status, EGV2SessionState::PreparingPresentation, Op.Kind);

    std::optional<GV2RuntimeCore::FUiDocument> PendingDoc;
    GV2RuntimeCore::FRuntimeFault DocFault;
    if (!RuntimeSession.TakePendingDocument(PendingDoc, DocFault))
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "doc_fault");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=%s message=%s"),
            UTF8_TO_TCHAR(DocFault.Code.c_str()),
            UTF8_TO_TCHAR(DocFault.Message.c_str()));
        TransitionPolicy.RecordFailure(Op.OperationId, DocFault);
        return false;
    }
    if (!PendingDoc.has_value())
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "doc_missing");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=InitialPresentationMissing message=Session start did not publish an initial UI document."));
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::InitialPresentationMissing, TEXT("Session start did not publish an initial UI document.")});
        return false;
    }

    const FGV2PresentationPrepareContext PrepareContext(Token.GetCandidate());
    FGV2UiDocumentViewModel DocModel;
    FGV2PreparedBindingSet PreparedBindings;
    if (!PrepareDocumentRequest(*PendingDoc, DocModel, PreparedBindings, PrepareContext))
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "prepare_failed");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=InitialPresentationInvalid message=Initial UI document failed binding preparation."));
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::InitialPresentationInvalid, TEXT("Initial UI document failed binding preparation.")});
        return false;
    }

    const bool bApplied = DocumentSink && DocumentSink(DocModel, PrepareContext);
    if (!bApplied)
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "apply_failed");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=InitialPresentationApplyFailed message=Initial UI document could not be applied."));
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::InitialPresentationApplyFailed, TEXT("Initial UI document could not be applied.")});
        return false;
    }
    if (!BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, Op.Kind);
        RuntimeSession.Stop(nullptr, "commit_failed");
        TryTransitionSessionState(Status, EGV2SessionState::Failed, Op.Kind);
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime fault: code=InitialPresentationCommitFailed message=Initial UI binding candidate could not be committed."));
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::InitialPresentationCommitFailed, TEXT("Initial UI binding candidate could not be committed.")});
        return false;
    }

    const bool bReadyOk = PublishReady(MoveTemp(Token), DocModel.Revision, Op.Kind);
    if (bReadyOk)
    {
        ActivePackageSet = InResolvedPackageSet;
        TransitionPolicy.RecordOutcome(Op.OperationId, ESessionNonFailureOutcome::Completed);
    }
    else
    {
        TransitionPolicy.RecordFailure(Op.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::PublishReadyFailed, TEXT("Failed to publish Ready state for session.")});
    }
    return bReadyOk;
}

bool FGV2SessionCoordinator::BeginReplace(
    FSessionReplacementToken& Token,
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion,
    GV2RuntimeCore::FRuntimeFault& OutFault,
    const ESessionTransitionKind TransitionKind)
{
    check(Token.GetStage() == EReplacementStage::Preflight);
    Token.TransitionTo(EReplacementStage::Replacing);

    // ---- Past this point, StartSession commits to replacing whatever was active. ----
    BindingRegistry.EndSession();
    IngressQueue.Reset();
    ContentSnapshot.Reset();

    GV2RuntimeCore::FRuntimeFault StopFault;
    if (RuntimeSession.IsStarted() && !RuntimeSession.Stop(&StopFault))
    {
        Token.TransitionTo(EReplacementStage::Aborted);
        FailRuntime(StopFault);
        OutFault = StopFault;
        return false;
    }

    if (ProjectionTeardownSink)
    {
        ProjectionTeardownSink();
    }

    ++Status.SessionGeneration;
    TryTransitionApplicationState(Status, EGV2ApplicationState::Bootstrapping);
    TryTransitionSessionState(Status, EGV2SessionState::Creating, TransitionKind);
    Status.bIsReady = false;
    Status.RepositoryVersion = InRepositoryVersion;
    NextInputSequence = 1;
    UiRevision = 0;
    PinnedRepository = InPinnedRepository;
    BindingRegistry.BeginSession(Status.SessionGeneration);
    RuntimeSession.SetSaveSlotStorage(SaveSlotStorage);

    return true;
}

bool FGV2SessionCoordinator::PublishReady(
    FSessionReplacementToken&& Token,
    const int64 InUiRevision,
    const ESessionTransitionKind TransitionKind)
{
    check(Token.GetStage() == EReplacementStage::Preparing);
    ContentSnapshot = Token.TakeCandidate();
    Token.TransitionTo(EReplacementStage::Committed);

    UiRevision = InUiRevision;
    switch (TransitionKind)
    {
    case ESessionTransitionKind::Menu:
        TryTransitionApplicationState(Status, EGV2ApplicationState::MenuActive);
        break;
    case ESessionTransitionKind::NewGame:
    case ESessionTransitionKind::LoadSave:
        TryTransitionApplicationState(Status, EGV2ApplicationState::GameActive);
        break;
    case ESessionTransitionKind::Shutdown:
        TryTransitionApplicationState(Status, EGV2ApplicationState::Uninitialized);
        break;
    }

    TryTransitionSessionState(Status, EGV2SessionState::Ready, TransitionKind);
    Status.bIsReady = true;

    if (ProjectionPublishSink)
    {
        ProjectionPublishSink();
    }
    return true;
}

void FGV2SessionCoordinator::FailBootstrap(const FString& Code, const FString& Message)
{
    check(IsInGameThread());
    check(!Status.bIsReady);
    GV2RuntimeCore::FRuntimeFault Fault{
        TCHAR_TO_UTF8(*Code),
        TCHAR_TO_UTF8(*Message)};
    FailRuntime(Fault);
}

void FGV2SessionCoordinator::EndSession(const EGV2SessionState FinalState)
{
    check(IsInGameThread());
    if (!TransitionPolicy.GetActiveOperation().IsSet() || TransitionPolicy.GetActiveOperation()->Kind != ESessionTransitionKind::Shutdown)
    {
        bool bJoined = false;
        uint64 JoinedOpId = 0;
        TransitionPolicy.EnqueueShutdown(bJoined, JoinedOpId);
        const TOptional<FSessionOperationRecord> NextOp = TransitionPolicy.DequeuePendingOperation();
        if (NextOp.IsSet())
        {
            TransitionPolicy.SetActiveOperation(*NextOp);
        }
    }

    CurrentTransitionKind = ESessionTransitionKind::Shutdown;

    Status.bIsReady = false;
    BindingRegistry.EndSession();
    IngressQueue.Reset();

    GV2RuntimeCore::FRuntimeFault StopFault;
    if (RuntimeSession.IsStarted())
    {
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, ESessionTransitionKind::Shutdown);
        TryTransitionApplicationState(Status, EGV2ApplicationState::ShuttingDown);
        RuntimeSession.Stop(&StopFault, "shutdown");
    }

    PinnedRepository = GV2ContentCore::FRepositoryReadHandle();
    ContentSnapshot.Reset();
    ActivePackageSet.Reset();

    if (ProjectionTeardownSink)
    {
        ProjectionTeardownSink();
    }

    if (Status.ApplicationState != EGV2ApplicationState::Uninitialized)
    {
        TryTransitionApplicationState(Status, EGV2ApplicationState::Uninitialized);
    }
    TryTransitionSessionState(Status, FinalState, ESessionTransitionKind::Shutdown);
    Status.RepositoryVersion = 0;
    NextInputSequence = 1;
    UiRevision = 0;

    for (const FPendingSaveRequest& Req : PendingSaveRequests)
    {
        TransitionPolicy.RecordFailure(Req.OperationId, FGV2OperationFault{FGV2SessionFaultCodes::SessionShutdown, TEXT("Save request aborted due to session shutdown.")});
    }
    PendingSaveRequests.Empty();

    if (TransitionPolicy.GetActiveOperation().IsSet() && TransitionPolicy.GetActiveOperation()->Kind == ESessionTransitionKind::Shutdown)
    {
        TransitionPolicy.RecordOutcome(TransitionPolicy.GetActiveOperation()->OperationId, ESessionNonFailureOutcome::Completed);
        TransitionPolicy.ClearActiveOperation();
    }
}

void FGV2SessionCoordinator::ExecuteShutdown(const FSessionOperationRecord& Op, const EGV2SessionState FinalState)
{
    EndSession(FinalState);
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

    if (!BindingRegistry.PublishBindings(UiInstanceId, Revision, Definitions, OutHandles))
    {
        return false;
    }

    UiRevision = FMath::Max(UiRevision, Revision);
    return true;
}

bool FGV2SessionCoordinator::PublishScreenBindings(
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = UiRevision + 1;
    return PublishUiBindings(UiInstanceId, CandidateRevision, Definitions, OutHandles);
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

    // UIF-26: If binding belongs to a tab container (path length >= 6), only the active tab is interactive
    if (Binding.NodeKeyPath.Num() >= 6)
    {
        const FString ContainerPath = FString::Printf(
            TEXT("%s/%s/%s"),
            *Binding.NodeKeyPath[0],
            *Binding.NodeKeyPath[1],
            *Binding.NodeKeyPath[2]);
        const FString* ActiveTab = ActiveTabsByContainerPath.Find(ContainerPath);
        if (ActiveTab == nullptr || *ActiveTab != Binding.NodeKeyPath[3])
        {
            return EGV2SubmitUiInteractionResult::StaleBindingHandle;
        }
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

void FGV2SessionCoordinator::SetActiveTab(const FString& ContainerPath, const FString& TabKey)
{
    ActiveTabsByContainerPath.Add(ContainerPath, TabKey);
}

FString FGV2SessionCoordinator::GetActiveTab(const FString& ContainerPath) const
{
    const FString* Found = ActiveTabsByContainerPath.Find(ContainerPath);
    return Found != nullptr ? *Found : FString();
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

bool FGV2SessionCoordinator::PrepareDocumentRequest(
    const GV2RuntimeCore::FUiDocument& Document,
    FGV2UiDocumentViewModel& OutModel,
    FGV2PreparedBindingSet& OutBindings,
    const FGV2PresentationPrepareContext& PrepareContext)
{
    OutModel = {};
    OutBindings = {};

    OutModel.UiInstanceId = UTF8_TO_TCHAR(Document.UiInstanceId.c_str());
    OutModel.Revision = Document.Revision;

    TArray<FGV2UiBindingDefinition> AllDefinitions;

    auto PrepareInstanceDefs = [&](const GV2RuntimeCore::FScreenInstance& Inst, int32& OutDefStartIndex, int32& OutDefCount) -> bool
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = Inst.ScreenId;
        Request.Fields = Inst.Fields;

        TArray<FGV2UiBindingDefinition> InstDefs;
        if (!GV2ScreenFieldMaterializer::PrepareBindingDefinitions(PrepareContext, Request, InstDefs))
        {
            UE_LOG(LogTemp, Error, TEXT("GV2 initial document has unsupported fields for screen '%s'"), UTF8_TO_TCHAR(Inst.ScreenId.c_str()));
            return false;
        }

        for (FGV2UiBindingDefinition& Def : InstDefs)
        {
            if (Def.NodeKeyPath.Num() >= 2 && Def.NodeKeyPath[0] == TEXT("route") && Def.NodeKeyPath[1] == TEXT("main"))
            {
                Def.NodeKeyPath.RemoveAt(0, 2);
            }
            Def.NodeKeyPath.Insert(UTF8_TO_TCHAR(Inst.InstanceKey.c_str()), 0);
            Def.NodeKeyPath.Insert(UTF8_TO_TCHAR(Inst.Layer.c_str()), 0);
        }

        OutDefStartIndex = AllDefinitions.Num();
        OutDefCount = InstDefs.Num();
        AllDefinitions.Append(InstDefs);
        return true;
    };

    struct FInstDefRange
    {
        int32 Start = 0;
        int32 Count = 0;
    };

    FInstDefRange RouteRange;
    if (Document.Route.has_value())
    {
        if (!PrepareInstanceDefs(*Document.Route, RouteRange.Start, RouteRange.Count))
        {
            return false;
        }
    }

    TArray<FInstDefRange> OverlayRanges;
    OverlayRanges.SetNum(Document.Overlays.size());
    for (size_t i = 0; i < Document.Overlays.size(); ++i)
    {
        if (!PrepareInstanceDefs(Document.Overlays[i], OverlayRanges[i].Start, OverlayRanges[i].Count))
        {
            return false;
        }
    }

    TArray<FInstDefRange> ModalRanges;
    ModalRanges.SetNum(Document.Modals.size());
    for (size_t i = 0; i < Document.Modals.size(); ++i)
    {
        if (!PrepareInstanceDefs(Document.Modals[i], ModalRanges[i].Start, ModalRanges[i].Count))
        {
            return false;
        }
    }

    // The binding registry requires an "ui@<SessionGeneration>:..." instance
    // id (see PublishScreenBindings above), but Lua's
    // presentation layer always publishes a fixed "ui@default" placeholder
    // (screen_requests.lua) since it has no way to know the session
    // generation. Trusting that placeholder here made PrepareBindings()
    // reject every document (wrong generation), so mint the canonical id the
    // same way the other binding call sites do instead of trusting Lua's.
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = OutModel.Revision > UiRevision ? OutModel.Revision : (UiRevision + 1);
    OutModel.UiInstanceId = UiInstanceId;
    OutModel.Revision = CandidateRevision;

    if (!BindingRegistry.PrepareBindings(UiInstanceId, CandidateRevision, AllDefinitions, OutBindings)
        || OutBindings.Handles.Num() != AllDefinitions.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("GV2 initial document bindings are invalid: instance='%s' revision=%lld definitions=%d handles=%d"), *UiInstanceId, CandidateRevision, AllDefinitions.Num(), OutBindings.Handles.Num());
        return false;
    }

    auto BuildInstanceModel = [&](const GV2RuntimeCore::FScreenInstance& Inst, const FInstDefRange& Range, FGV2ScreenInstanceViewModel& OutInstModel) -> bool
    {
        OutInstModel.Layer = FName(UTF8_TO_TCHAR(Inst.Layer.c_str()));
        OutInstModel.InstanceKey = FName(UTF8_TO_TCHAR(Inst.InstanceKey.c_str()));
        OutInstModel.ScreenId = UTF8_TO_TCHAR(Inst.ScreenId.c_str());

        TArray<FGV2UiBindingHandle> InstHandles;
        InstHandles.Reserve(Range.Count);
        for (int32 i = 0; i < Range.Count; ++i)
        {
            InstHandles.Add(OutBindings.Handles[Range.Start + i]);
        }

        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = Inst.ScreenId;
        Request.Fields = Inst.Fields;
        if (!GV2ScreenFieldMaterializer::BuildFields(PrepareContext, Request, InstHandles, OutInstModel.Fields))
        {
            UE_LOG(LogTemp, Error, TEXT("GV2 initial document fields could not be built for screen '%s'"), *OutInstModel.ScreenId);
            return false;
        }

        return true;
    };

    if (Document.Route.has_value())
    {
        OutModel.bHasRoute = true;
        if (!BuildInstanceModel(*Document.Route, RouteRange, OutModel.Route))
        {
            return false;
        }
    }

    OutModel.Overlays.SetNum(Document.Overlays.size());
    for (size_t i = 0; i < Document.Overlays.size(); ++i)
    {
        if (!BuildInstanceModel(Document.Overlays[i], OverlayRanges[i], OutModel.Overlays[i]))
        {
            return false;
        }
    }

    OutModel.Modals.SetNum(Document.Modals.size());
    for (size_t i = 0; i < Document.Modals.size(); ++i)
    {
        if (!BuildInstanceModel(Document.Modals[i], ModalRanges[i], OutModel.Modals[i]))
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

    {
        TGuardValue<bool> PumpGuard(bPumpingIngress, true);
        FGV2UiIngressItem Item;
        while (Status.bIsReady && IngressQueue.Dequeue(Item))
        {
            std::optional<GV2RuntimeCore::FUiDocument> PendingDoc;
            {
                TGuardValue<bool> ExecutionGuard(bExecutingRuntime, true);
                GV2RuntimeCore::FRuntimeFault Fault;
                if (!RuntimeSession.DispatchSemanticInput(ToPortableInput(Item), Fault))
                {
                    FailRuntime(Fault);
                    return;
                }
                if (!RuntimeSession.TakePendingDocument(PendingDoc, Fault))
                {
                    FailRuntime(Fault);
                    return;
                }
            }

            std::vector<GV2RuntimeCore::FHostControlRequest> PendingControlRequests;
            GV2RuntimeCore::FRuntimeFault ControlFault;
            if (RuntimeSession.TakePendingControlRequests(PendingControlRequests, ControlFault))
            {
                for (const auto& Req : PendingControlRequests)
                {
                    if (Req.Kind == "save")
                    {
                        RequestSave(UTF8_TO_TCHAR(Req.SlotId.c_str()));
                    }
                    else if (Req.Kind == "load")
                    {
                        ESaveSlotRevision Rev = ESaveSlotRevision::Current;
                        if (Req.Revision == "previous")
                        {
                            Rev = ESaveSlotRevision::Previous;
                        }
                        RequestLoad(UTF8_TO_TCHAR(Req.SlotId.c_str()), Rev);
                    }
                }
            }

            if (PendingDoc)
            {
                if (ContentSnapshot.IsValid())
                {
                    const FGV2PresentationPrepareContext PrepareContext(*ContentSnapshot);
                    FGV2UiDocumentViewModel DocModel;
                    FGV2PreparedBindingSet PreparedBindings;
                    if (PrepareDocumentRequest(*PendingDoc, DocModel, PreparedBindings, PrepareContext))
                    {
                        const bool bApplied = DocumentSink && DocumentSink(DocModel, PrepareContext);
                        if (bApplied && BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
                        {
                            UiRevision = DocModel.Revision;
                        }
                    }
                }
            }
            if (InteractionSink)
            {
                InteractionSink(Item);
            }

            DrainPendingSaveRequests();
        }
    }

    if (TransitionPolicy.HasPendingOperation())
    {
        ProcessNextTransition();
    }
}

void FGV2SessionCoordinator::FailReplacementAttempt(
    const GV2RuntimeCore::FRuntimeFault& Fault,
    const bool bHadPriorReadySession)
{
    if (!bHadPriorReadySession)
    {
        // Nothing valid was running -- report the failed attempt itself, same terminal
        // state FailRuntime would report, but without touching RuntimeSession/
        // BindingRegistry/PinnedRepository/ContentSnapshot, none of which this attempt
        // ever mutated.
        Status.bIsReady = false;
        TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
        TryTransitionSessionState(Status, EGV2SessionState::Failed, CurrentTransitionKind);
    }
    // else: a Ready session was active when this attempt began and nothing about it has
    // been touched -- Status/PinnedRepository/BindingRegistry/RuntimeSession/
    // ContentSnapshot all still describe it exactly as they did before this call.

    UE_LOG(
        LogTemp,
        Error,
        TEXT("GV2 Lua runtime fault: code=%s message=%s"),
        UTF8_TO_TCHAR(Fault.Code.c_str()),
        UTF8_TO_TCHAR(Fault.Message.c_str()));
}

void FGV2SessionCoordinator::FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault)
{
    for (const FPendingSaveRequest& Req : PendingSaveRequests)
    {
        TransitionPolicy.RecordFailure(Req.OperationId, Fault);
    }
    PendingSaveRequests.Empty();

    Status.bIsReady = false;
    BindingRegistry.EndSession();
    IngressQueue.Reset();

    GV2RuntimeCore::FRuntimeFault StopFault;
    if (RuntimeSession.IsStarted())
    {
        TryTransitionSessionState(Status, EGV2SessionState::Stopping, CurrentTransitionKind);
        RuntimeSession.Stop(&StopFault, "runtime_fault");
    }

    TryTransitionApplicationState(Status, EGV2ApplicationState::Failed);
    TryTransitionSessionState(Status, EGV2SessionState::Failed, CurrentTransitionKind);

    if (!RuntimeSession.IsStarted() && !StopFault.Code.empty())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Lua runtime Stop failed in FailRuntime: code=%s message=%s"),
            UTF8_TO_TCHAR(StopFault.Code.c_str()),
            UTF8_TO_TCHAR(StopFault.Message.c_str()));
    }

    PinnedRepository = GV2ContentCore::FRepositoryReadHandle();
    ContentSnapshot.Reset();
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
