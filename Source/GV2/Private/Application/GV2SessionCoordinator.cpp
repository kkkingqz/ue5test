#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

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

// PAH-04: pre_ready_discovery -- only called from StartSession(), before this
// session's Status.bIsReady is ever set true.
// PSC-02 (ADR-0043 D1/D5): ResolvedPackageSet is the caller's single already-resolved
// package set. When given, this function reads PackageId/Root straight from it -- no
// second discovery of the package set, not even a per-root re-parse of package.json5
// (the old code called DiscoverPackageFromDirectory again here, after the caller had
// already discovered the same roots to build RuntimePackageRoots). nullptr falls back to
// this function's own discovery, for callers -- mostly tests -- with no resolved set of
// their own.
bool LoadPortableRuntimeSources(
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources,
    GV2RuntimeCore::FRuntimeFault& OutFault,
    const GV2ContentHostSupport::FResolvedPackageSet* ResolvedPackageSet,
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

    if (ResolvedPackageSet != nullptr)
    {
        OutSchemaPackageRoots.Reserve(static_cast<int32>(ResolvedPackageSet->OrderedSources.size()));
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedPackageSet->OrderedSources)
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

    // Fallback discovery for a caller with no resolved package set of its own (mostly
    // tests) -- unchanged from before PSC-02, just no longer the production path.
    const FString GameDataDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = SessionCoordinatorToUtf8(GameDataDirectory);
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    std::vector<std::filesystem::path> OrderedRoots;

    bool bUseSampleOverride = false;
#if WITH_DEV_AUTOMATION_TESTS
    bUseSampleOverride = FGV2SessionCoordinator::bTestForceIncludeSamplePackage;
#endif

    if (bUseSampleOverride)
    {
        // CBM-03 override: GameData/sample and GameData/rh both bind the shared
        // "textsystem:action.location.travel" action, so they cannot load
        // together. Tests that opt in via bTestForceIncludeSamplePackage want
        // the sample demo/debug-start screen, not rh's gameplay content, so
        // this substitutes rh for sample instead of following mods.lock.json5.
        // core is listed too (PAH-04A) -- omitting it here only ever happened to
        // be safe for the Lua-sources loop below, which always skips core (its
        // scripts already loaded from Scripts/ above); mirrors
        // GV2RuntimeSubsystem.cpp's own equivalent override list.
        OrderedRoots = {
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("core")))),
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("textsystem")))),
            std::filesystem::path(SessionCoordinatorToUtf8(FPaths::Combine(GameDataDirectory, TEXT("sample")))),
        };
    }
    else if (!GV2ContentHostSupport::DiscoverPackagesFromContainer(
            std::filesystem::path(GameDataDirUtf8),
            Diags,
            &OrderedRoots))
    {
        return true;
    }

    OutSchemaPackageRoots.Reserve(static_cast<int32>(OrderedRoots.size()));
    for (const auto& Root : OrderedRoots)
    {
        std::vector<GV2ContentCore::FDiagnostic> PkgDiags;
        auto Descriptor = GV2ContentHostSupport::DiscoverPackageFromDirectory(Root, PkgDiags);
        if (!Descriptor)
        {
            continue;
        }

        OutSchemaPackageRoots.Add(FGV2SchemaPackageRoot{
            UTF8_TO_TCHAR(Descriptor->GetPackageId().c_str()),
            UTF8_TO_TCHAR(Root.string().c_str())});

        if (Descriptor->GetPackageId() == "core")
        {
            continue;
        }
        auto PkgSources = GV2ContentHostSupport::DiscoverPackageScripts(Root, Descriptor->GetPackageId());
        for (auto& Src : PkgSources)
        {
            GV2RuntimeCore::FRuntimeSource& Source = OutSources.emplace_back();
            Source.Name = std::move(Src.Name);
            Source.Text = std::move(Src.Text);
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

bool FGV2SessionCoordinator::StartSession(
    const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
    const int64 InRepositoryVersion,
    const GV2ContentHostSupport::FResolvedPackageSet* ResolvedPackageSet)
{
    check(IsInGameThread());

    // PSC-05 (BootstrapAndSessionLifecycle.md "Целевое правило"): captured before anything
    // is touched. A failure below, before the commit-to-replace boundary, either preserves
    // this exact session (nothing mutated yet) or -- if there was nothing valid running --
    // transitions to Failed so the attempt is never silently indistinguishable from "no
    // session was ever started".
    const bool bHadPriorReadySession = Status.bIsReady;

    if (!InPinnedRepository.IsValid())
    {
        FailReplacementAttempt(
            {"RepositoryNotReady", "No published GameDataRepository to pin."}, bHadPriorReadySession);
        return false;
    }

    GV2RuntimeCore::FRuntimeFault Fault;
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    TArray<FGV2SchemaPackageRoot> SchemaPackageRoots;
    if (!LoadPortableRuntimeSources(RuntimeSources, Fault, ResolvedPackageSet, SchemaPackageRoots))
    {
        FailReplacementAttempt(Fault, bHadPriorReadySession);
        return false;
    }

    // PAH-04A (ADR-0042, INV-P1): discovery happens here, synchronously, before this
    // session can reach Ready -- the exact SchemaPackageRoots LoadPortableRuntimeSources
    // just resolved this session's Lua sources from, not a second independent lookup.
    TArray<FString> ClosurePackageIds;
    ClosurePackageIds.Reserve(SchemaPackageRoots.Num());
    for (const FGV2SchemaPackageRoot& SchemaRoot : SchemaPackageRoots)
    {
        ClosurePackageIds.Add(SchemaRoot.PackageId);
    }

    // PSC-04/05 (ADR-0043 D1): resolves Screen Registry/Image Catalog/Theme/GameShell/
    // eagerly compiled schemas from this exact ResolvedPackageSet -- still entirely before
    // touching whatever session is currently active, so a content-builder failure here
    // (UiSchemaNotReady/ScreenRegistryNotReady/ImageCatalogNotReady/ThemeNotReady) has not
    // yet committed to replacing anything. PSC-10C retired the image catalog session
    // global entirely; RebuildSchemaCacheForSession below is the last remaining one.
    TUniquePtr<FGV2SessionContentSnapshot> Candidate = MakeUnique<FGV2SessionContentSnapshot>();
    GV2RuntimeCore::FRuntimeFault CandidateFault;
    if (!FGV2SessionContentCandidate::Build(
            InPinnedRepository,
            ResolvedPackageSet,
            SchemaPackageRoots,
            RuntimeSources,
            *Candidate,
            CandidateFault))
    {
        FailReplacementAttempt(CandidateFault, bHadPriorReadySession);
        return false;
    }

    // ---- Past this point, StartSession commits to replacing whatever was active. ----
    // Every failure from here on legitimately ends this attempt with the prior session
    // already gone (its VM is about to be stopped below) -- FailRuntime, not
    // FailReplacementAttempt, is correct for all of them.
    BindingRegistry.EndSession();
    IngressQueue.Reset();
    GV2RuntimeCore::FRuntimeFault StopFault;
    if (!RuntimeSession.Stop(&StopFault))
    {
        FailRuntime(StopFault);
        return false;
    }
    GV2ScreenFieldMaterializer::ReleaseSchemaCacheForSession();

    ++Status.SessionGeneration;
    Status.ApplicationState = EGV2ApplicationState::Bootstrapping;
    Status.SessionState = EGV2SessionState::Creating;
    Status.bIsReady = false;
    Status.RepositoryVersion = InRepositoryVersion;
    NextInputSequence = 1;
    UiRevision = 0;
    PinnedRepository = InPinnedRepository;
    BindingRegistry.BeginSession(Status.SessionGeneration);

    GV2ScreenFieldMaterializer::RebuildSchemaCacheForSession(MoveTemp(SchemaPackageRoots));

    // PSC-10C: no second image catalog is built here any more. The candidate above already
    // built this session's catalog from the same closure package ids and pinned it in the
    // snapshot, failing with the same ImageCatalogNotReady fault on the same inputs; a
    // process-global copy alongside it was a second content authority for one session.

    // PSC-04: RuntimeSession consumes the snapshot's own Lua source set -- it was moved
    // into the candidate above, not read a second time from a separately-held local copy.
    if (!RuntimeSession.Start(Status.SessionGeneration, InPinnedRepository, Candidate->GetLuaSources(), Fault))
    {
        FailRuntime(Fault);
        return false;
    }
    FGV2SessionContentCandidate::FinalizeScriptIdentity(*Candidate, RuntimeSession.GetScriptSetHash());
    // PSC-06: the initial document's own Prepare step (below) needs this candidate's
    // resolved Screen Registry/Image Catalog -- GetContentSnapshotForPrepare() exposes it
    // internally from this point on, while GetContentSnapshot() stays null until Ready.
    InProgressCandidate = Candidate.Get();

    std::optional<GV2RuntimeCore::FUiDocument> PendingDoc;
    if (!RuntimeSession.TakePendingDocument(PendingDoc, Fault))
    {
        FailRuntime(Fault);
        return false;
    }
    if (!PendingDoc.has_value())
    {
        FailRuntime({"InitialPresentationMissing", "Session start did not publish an initial UI document."});
        return false;
    }

    FGV2UiDocumentViewModel DocModel;
    FGV2PreparedBindingSet PreparedBindings;
    if (!PrepareDocumentRequest(*PendingDoc, DocModel, PreparedBindings))
    {
        FailRuntime({"InitialPresentationInvalid", "Initial UI document failed binding preparation."});
        return false;
    }

    const bool bApplied = DocumentSink && DocumentSink(DocModel);
    if (!bApplied)
    {
        FailRuntime({"InitialPresentationApplyFailed", "Initial UI document could not be applied."});
        return false;
    }
    if (!BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
    {
        FailRuntime({"InitialPresentationCommitFailed", "Initial UI binding candidate could not be committed."});
        return false;
    }

    // PSC-05 (ADR-0043 D1): ContentSnapshot becomes observable atomically with the exact
    // moment this session becomes Ready -- never before, and never on an attempt that
    // fails at any later step. InProgressCandidate's raw pointer is now dangling-but-unused:
    // GetContentSnapshotForPrepare() checks ContentSnapshot first, so it's never read again
    // once this line runs; still cleared for clarity.
    ContentSnapshot = MoveTemp(Candidate);
    InProgressCandidate = nullptr;
    UiRevision = DocModel.Revision;
    Status.ApplicationState = EGV2ApplicationState::MenuActive;
    Status.SessionState = EGV2SessionState::Ready;
    Status.bIsReady = true;

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
    GV2ScreenFieldMaterializer::ReleaseSchemaCacheForSession();
    ContentSnapshot.Reset();
    InProgressCandidate = nullptr;
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
    FGV2PreparedBindingSet& OutBindings)
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
        if (!GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, InstDefs))
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

    // PSC-10A (ADR-0043 D1): same TOptional<FGV2PresentationPrepareContext> idiom
    // UGV2RuntimeSubsystem::HandleDocumentRequested already uses -- GetContentSnapshotForPrepare()
    // returns the in-progress candidate while StartSession() is still preparing/committing
    // the initial document (before Ready), or the published snapshot for every later
    // document update, so this covers both of PrepareDocumentRequest's own call sites.
    const FGV2SessionContentSnapshot* SnapshotForPrepare = GetContentSnapshotForPrepare();
    const TOptional<FGV2PresentationPrepareContext> PrepareContext =
        SnapshotForPrepare != nullptr
            ? TOptional<FGV2PresentationPrepareContext>(FGV2PresentationPrepareContext(*SnapshotForPrepare))
            : TOptional<FGV2PresentationPrepareContext>();
    const FGV2PresentationPrepareContext* PrepareContextPtr = PrepareContext.IsSet() ? &PrepareContext.GetValue() : nullptr;

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
        if (!GV2ScreenFieldMaterializer::BuildFields(Request, InstHandles, OutInstModel.Fields, PrepareContextPtr))
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

        if (PendingDoc)
        {
            FGV2UiDocumentViewModel DocModel;
            FGV2PreparedBindingSet PreparedBindings;
            if (PrepareDocumentRequest(*PendingDoc, DocModel, PreparedBindings))
            {
                const bool bApplied = DocumentSink && DocumentSink(DocModel);
                if (bApplied && BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
                {
                    UiRevision = DocModel.Revision;
                }
            }
        }
        if (InteractionSink)
        {
            InteractionSink(Item);
        }
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
        Status.ApplicationState = EGV2ApplicationState::Failed;
        Status.SessionState = EGV2SessionState::Failed;
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
    GV2ScreenFieldMaterializer::ReleaseSchemaCacheForSession();
    ContentSnapshot.Reset();
    InProgressCandidate = nullptr;
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
