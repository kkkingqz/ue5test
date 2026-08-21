#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2ScreenFieldAdapterRegistry.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

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
    GV2RuntimeCore::FRuntimeFault& OutFault,
    const TArray<FString>& RuntimePackageRoots)
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
        Source.Name = "@core/" + SessionCoordinatorToUtf8(RelativePath);
        Source.Text.assign(
            reinterpret_cast<const char*>(Bytes.GetData() + Offset),
            static_cast<std::size_t>(Bytes.Num() - Offset));
    }

    const FString GameDataDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = SessionCoordinatorToUtf8(GameDataDirectory);
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    std::vector<std::filesystem::path> OrderedRoots;

    bool bUseSampleOverride = false;
#if WITH_DEV_AUTOMATION_TESTS
    bUseSampleOverride = FGV2SessionCoordinator::bTestForceIncludeSamplePackage;
#endif

    if (!RuntimePackageRoots.IsEmpty())
    {
        OrderedRoots.reserve(RuntimePackageRoots.Num());
        for (const FString& Root : RuntimePackageRoots)
        {
            OrderedRoots.emplace_back(SessionCoordinatorToUtf8(Root));
        }
    }
    else if (bUseSampleOverride)
    {
        // CBM-03 override: GameData/sample and GameData/rh both bind the shared
        // "textsystem:action.location.travel" action, so they cannot load
        // together. Tests that opt in via bTestForceIncludeSamplePackage want
        // the sample demo/debug-start screen, not rh's gameplay content, so
        // this substitutes rh for sample instead of following mods.lock.json5.
        OrderedRoots = {
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

    for (const auto& Root : OrderedRoots)
    {
        std::vector<GV2ContentCore::FDiagnostic> PkgDiags;
        auto Descriptor = GV2ContentHostSupport::DiscoverPackageFromDirectory(Root, PkgDiags);
        if (!Descriptor || Descriptor->GetPackageId() == "core")
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

void FGV2SessionCoordinator::SetScreenSink(FScreenSink InSink)
{
    ScreenSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearScreenSink()
{
    ScreenSink = nullptr;
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
    const TArray<FString>& RuntimePackageRoots)
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
    if (!LoadPortableRuntimeSources(RuntimeSources, Fault, RuntimePackageRoots)
        || !RuntimeSession.Start(Status.SessionGeneration, InPinnedRepository, RuntimeSources, Fault))
    {
        FailRuntime(Fault);
        return false;
    }

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

    bool bApplied = false;
    if (DocumentSink)
    {
        bApplied = DocumentSink(DocModel);
    }
    else if (ScreenSink && DocModel.bHasRoute)
    {
        FGV2ScreenViewModel ScreenModel;
        ScreenModel.ScreenId = DocModel.Route.ScreenId;
        ScreenModel.Fields = DocModel.Route.Fields;
        bApplied = ScreenSink(ScreenModel);
    }
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

    // UIF-26: If binding belongs to a tab container (path length >= 6), only the active tab is interactive
    if (Binding.NodeKeyPath.Num() >= 6)
    {
        const FString ContainerPath = FString::Printf(
            TEXT("%s/%s/%s"),
            *Binding.NodeKeyPath[0],
            *Binding.NodeKeyPath[1],
            *Binding.NodeKeyPath[2]);
        const FString* ActiveTab = ActiveTabsByContainerPath.Find(ContainerPath);
        if (ActiveTab != nullptr && *ActiveTab != Binding.NodeKeyPath[3])
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

    const FGV2ScreenFieldAdapterRegistry& FieldAdapters = FGV2ScreenFieldAdapterRegistry::Get();
    TArray<FGV2UiBindingDefinition> AllDefinitions;

    auto PrepareInstanceDefs = [&](const GV2RuntimeCore::FScreenInstance& Inst, int32& OutDefStartIndex, int32& OutDefCount) -> bool
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = Inst.ScreenId;
        Request.Fields = Inst.Fields;

        TArray<FGV2UiBindingDefinition> InstDefs;
        if (!FieldAdapters.PrepareBindingDefinitions(Request, InstDefs))
        {
            UE_LOG(LogTemp, Error, TEXT("GV2 initial document has unsupported fields for screen '%s'"), UTF8_TO_TCHAR(Inst.ScreenId.c_str()));
            return false;
        }

        for (FGV2UiBindingDefinition& Def : InstDefs)
        {
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
    // id (see PublishScreenBindings/PrepareScreenRequest below), but Lua's
    // presentation layer always publishes a fixed "ui@default" placeholder
    // (screen_requests.lua) since it has no way to know the session
    // generation. Trusting that placeholder here made PrepareBindings()
    // reject every document (wrong generation), so mint the canonical id the
    // same way the other binding call sites do instead of trusting Lua's.
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = OutModel.Revision > 0 ? OutModel.Revision : (UiRevision + 1);

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
        if (!FieldAdapters.BuildFields(Request, InstHandles, OutInstModel.Fields))
        {
            UE_LOG(LogTemp, Error, TEXT("GV2 initial document fields could not be built for screen '%s'"), *OutInstModel.ScreenId);
            return false;
        }

        // Initialize / validate active tab state for tab container fields (UIF-25)
        for (const FGV2ScreenFieldValue& FieldVal : OutInstModel.Fields)
        {
            if (FieldVal.TabContainerValue.IsValid())
            {
                const FString ContainerPath = FString::Printf(
                    TEXT("%s/%s/%s"),
                    *OutInstModel.Layer.ToString(),
                    *OutInstModel.InstanceKey.ToString(),
                    *FieldVal.FieldId.ToString());

                const FGV2TabContainerViewModel& TabModel = *FieldVal.TabContainerValue;
                const FString* ExistingActive = ActiveTabsByContainerPath.Find(ContainerPath);
                const bool bExistingStillValid = ExistingActive != nullptr && TabModel.Tabs.ContainsByPredicate([ExistingActive](const FGV2TabItemViewModel& Tab)
                {
                    return Tab.Key.ToString() == *ExistingActive;
                });

                if (!bExistingStillValid)
                {
                    FName InitialKey = !TabModel.DefaultTabKey.IsNone() ? TabModel.DefaultTabKey : (TabModel.Tabs.Num() > 0 ? TabModel.Tabs[0].Key : NAME_None);
                    if (!InitialKey.IsNone())
                    {
                        ActiveTabsByContainerPath.Add(ContainerPath, InitialKey.ToString());
                    }
                }
            }
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
                bool bApplied = false;
                if (DocumentSink)
                {
                    bApplied = DocumentSink(DocModel);
                }
                else if (ScreenSink && DocModel.bHasRoute)
                {
                    FGV2ScreenViewModel ScreenModel;
                    ScreenModel.ScreenId = DocModel.Route.ScreenId;
                    ScreenModel.Fields = DocModel.Route.Fields;
                    bApplied = ScreenSink(ScreenModel);
                }
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
