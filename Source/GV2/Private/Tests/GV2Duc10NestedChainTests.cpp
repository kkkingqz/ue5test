#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2SessionCoordinator.h"
#include "Components/ProgressBar.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2Duc10NestedChainFixtureTest,
    "GV2.Runtime.UI.Duc10NestedChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2Duc10NestedChainFixtureTest::RunTest(const FString& Parameters)
{
    UGV2ScreenRegistry* Registry = UGV2ScreenRegistrySettings::GetConfiguredRegistry();
    TestNotNull(TEXT("DUC-10: configured Screen Registry is available"), Registry);
    if (Registry == nullptr)
    {
        return false;
    }
    FString RegistryBuildError;
    TestTrue(
        *FString::Printf(TEXT("DUC-10: Screen Registry builds [Error: %s]"), *RegistryBuildError),
        Registry->Build(RegistryBuildError));

    FGV2ResolvedScreenDescriptor ChainDescriptor;
    FGV2ScreenResolutionRejection ChainRejection;
    const bool bChainResolved = Registry->Resolve(
        TEXT("textsystem:screen.duc10_nested_chain"),
        FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent),
        ChainDescriptor,
        ChainRejection);
    TestTrue(
        *FString::Printf(TEXT("DUC-10: root screen of the data-driven nested-chain fixture is registered [Error: %s]"), *ChainRejection.Message),
        bChainResolved);

    FGV2ResolvedScreenDescriptor BlockDescriptor;
    FGV2ScreenResolutionRejection BlockRejection;
    const bool bBlockResolved = Registry->Resolve(
        TEXT("textsystem:screen.duc10_nested_block"),
        FGV2ScreenPlacement::Embedded(),
        BlockDescriptor,
        BlockRejection);
    TestTrue(
        *FString::Printf(TEXT("DUC-10: nested block screen of the data-driven fixture is registered [Error: %s]"), *BlockRejection.Message),
        bBlockResolved);

    if (bChainResolved)
    {
        TestNotNull(
            TEXT("DUC-10: root screen fixture resolves to a trusted Screen Template class"),
            ChainDescriptor.WidgetClass);
    }
    if (bBlockResolved)
    {
        TestNotNull(
            TEXT("DUC-10: block screen fixture resolves to a trusted Screen Template class"),
            BlockDescriptor.WidgetClass);
    }

    const TArray<FString> PackageRoots = {
        FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core")),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/textsystem")),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/rh")),
    };
    const GV2ContentCore::FBuildResult RepositoryBuild = BuildGV2RepositoryFromDirectories(PackageRoots);
    TestTrue(TEXT("DUC-10: fixture package repository builds"), RepositoryBuild.IsSuccess());
    if (!RepositoryBuild.IsSuccess())
    {
        return false;
    }

    FGV2SessionCoordinator Coordinator;
    FGV2UiDocumentViewModel CapturedDocument;
    int32 DocumentCount = 0;
    Coordinator.SetDocumentSink([&CapturedDocument, &DocumentCount](const FGV2UiDocumentViewModel& Document)
    {
        CapturedDocument = Document;
        ++DocumentCount;
        return true;
    });
    TestTrue(
        TEXT("DUC-10: fixture session starts"),
        Coordinator.StartSession(RepositoryBuild.GetCandidate().GetReadHandle(), 1, PackageRoots));
    if (!Coordinator.GetStatus().bIsReady)
    {
        return false;
    }

    FGV2UiBindingDefinition ShowDefinition;
    ShowDefinition.ElementId = TEXT("duc10_show");
    ShowDefinition.NodeKeyPath = { TEXT("duc10"), TEXT("show") };
    ShowDefinition.CommandId = TEXT("textsystem:command.debug.duc10_show");

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("DUC-10: show-fixture binding publishes"),
        Coordinator.PublishUiBindings(
            CapturedDocument.UiInstanceId,
            Coordinator.GetUiRevision() + 1,
            {ShowDefinition},
            Handles));
    if (Handles.Num() != 1)
    {
        AddError(TEXT("DUC-10: expected exactly one show-fixture binding handle"));
        return false;
    }

    TestEqual(
        TEXT("DUC-10: Lua publishes the nested-chain document"),
        Coordinator.SubmitUiInteraction(Handles[0], {}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(TEXT("DUC-10: Lua command publishes one additional document"), DocumentCount, 2);
    TestTrue(TEXT("DUC-10: Lua document has a route"), CapturedDocument.bHasRoute);
    if (CapturedDocument.bHasRoute)
    {
        TestEqual(
            TEXT("DUC-10: Lua document targets the data-driven root template"),
            CapturedDocument.Route.ScreenId,
            FString(TEXT("textsystem:screen.duc10_nested_chain")));
    }

    auto DispatchFixtureCommand = [&Coordinator, &CapturedDocument, &DocumentCount, this](
        const FString& CommandId,
        FGV2UiDocumentViewModel& OutDocument) -> bool
    {
        FGV2UiBindingDefinition Definition;
        const FString ElementId = CommandId.RightChop(CommandId.Find(TEXT("duc10_")) + 6);
        Definition.ElementId = ElementId;
        Definition.NodeKeyPath = { TEXT("duc10"), Definition.ElementId };
        Definition.CommandId = CommandId;

        TArray<FGV2UiBindingHandle> CommandHandles;
        if (!TestTrue(
                *FString::Printf(TEXT("DUC-10: binding publishes for '%s'"), *CommandId),
                Coordinator.PublishUiBindings(
                    CapturedDocument.UiInstanceId,
                    Coordinator.GetUiRevision() + 1,
                    {Definition},
                    CommandHandles))
            || CommandHandles.Num() != 1)
        {
            AddError(FString::Printf(TEXT("DUC-10: command '%s' did not receive exactly one binding"), *CommandId));
            return false;
        }

        const int32 DocumentsBeforeCommand = DocumentCount;
        if (!TestEqual(
                *FString::Printf(TEXT("DUC-10: Lua command '%s' is accepted"), *CommandId),
                Coordinator.SubmitUiInteraction(CommandHandles[0], {}),
                EGV2SubmitUiInteractionResult::Accepted)
            || !TestEqual(
                *FString::Printf(TEXT("DUC-10: command '%s' publishes one document"), *CommandId),
                DocumentCount,
                DocumentsBeforeCommand + 1))
        {
            return false;
        }

        OutDocument = CapturedDocument;
        return true;
    };

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    TestNotNull(TEXT("DUC-10: standalone GameInstance is created"), GameInstance);
    if (GameInstance == nullptr)
    {
        return false;
    }
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* const TestWorld = GameInstance->GetWorld();
    TestNotNull(TEXT("DUC-10: standalone world is available"), TestWorld);
    if (TestWorld == nullptr)
    {
        GameInstance->Shutdown();
        GameInstance->RemoveFromRoot();
        return false;
    }

    auto ScreenFactory = [Registry, TestWorld](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
    {
        FGV2ResolvedScreenDescriptor Descriptor;
        FGV2ScreenResolutionRejection Rejection;
        if (!Registry->Resolve(ScreenId, FGV2ScreenPlacement::TopLevel(Layer), Descriptor, Rejection))
        {
            return nullptr;
        }
        return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, Descriptor.WidgetClass);
    };

    FGV2LayeredUiReconciler Reconciler;
    FString ReconcileError;
    TestTrue(
        *FString::Printf(TEXT("DUC-10: Lua nested-chain document reconciles [Error: %s]"), *ReconcileError),
        Reconciler.Reconcile(nullptr, CapturedDocument, ScreenFactory, ReconcileError));

    UGV2ScreenWidgetBase* const RootScreen = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("duc10"));
    TestNotNull(TEXT("DUC-10: root screen is published after successful reconcile"), RootScreen);
    UGV2DeclaredCompositeWidgetBase* const TabsHost = RootScreen != nullptr
        ? Cast<UGV2DeclaredCompositeWidgetBase>(RootScreen->GetWidgetFromName(TEXT("TabsHost"))) : nullptr;
    UGV2TabContainerWidgetBase* const Tabs = TabsHost != nullptr
        ? Cast<UGV2TabContainerWidgetBase>(TabsHost->GetWidgetFromName(TEXT("Tabs"))) : nullptr;
    TestNotNull(TEXT("DUC-10: generic root host reaches reusable tab container"), Tabs);
    UGV2ScreenWidgetBase* const BlockScreen = Tabs != nullptr ? Tabs->GetScreenWidgetForTab(TEXT("info")) : nullptr;
    TestNotNull(TEXT("DUC-10: tab resolves the registered nested Screen Template"), BlockScreen);
    UGV2DeclaredCompositeWidgetBase* const DayBlock = BlockScreen != nullptr
        ? Cast<UGV2DeclaredCompositeWidgetBase>(BlockScreen->GetWidgetFromName(TEXT("DayBlock"))) : nullptr;
    UGV2TextWidgetBase* const DayText = DayBlock != nullptr
        ? Cast<UGV2TextWidgetBase>(DayBlock->GetWidgetFromName(TEXT("DayText"))) : nullptr;
    UGV2ProgressBarWidgetBase* const ValueBar = DayBlock != nullptr
        ? Cast<UGV2ProgressBarWidgetBase>(DayBlock->GetWidgetFromName(TEXT("ValueBar"))) : nullptr;
    TestNotNull(TEXT("DUC-10: third-level declared block exposes Lua text leaf"), DayText);
    TestNotNull(TEXT("DUC-10: third-level declared block exposes Lua number leaf"), ValueBar);

    FString BaselineDayText;
    float BaselineProgress = 0.0f;
    if (DayText != nullptr)
    {
        BaselineDayText = DayText->GetTextContent().ToString();
        TestTrue(TEXT("DUC-10: Lua day value reaches third-level text leaf"), BaselineDayText.Contains(TEXT("1")));
    }
    if (ValueBar != nullptr)
    {
        BaselineProgress = ValueBar->GetProgress();
        TestEqual(TEXT("DUC-10: Lua numeric value reaches third-level progress leaf"), BaselineProgress, 0.25f);
    }

    FGV2UiDocumentViewModel PrepareFailureDocument;
    if (DispatchFixtureCommand(TEXT("textsystem:command.debug.duc10_prepare_failure"), PrepareFailureDocument))
    {
        FGV2LayeredUiReconciler::FPreparedReconciliationPlan PrepareFailurePlan;
        TestFalse(
            TEXT("DUC-10: invalid third-level field fails entirely during Prepare"),
            Reconciler.PrepareReconcile(nullptr, PrepareFailureDocument, ScreenFactory, PrepareFailurePlan, ReconcileError));
        TestEqual(
            TEXT("DUC-10: Prepare failure retains the published root screen instance"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("duc10")),
            RootScreen);
        TestTrue(TEXT("DUC-10: Prepare failure retains the same tab container instance"),
            (TabsHost != nullptr ? TabsHost->GetWidgetFromName(TEXT("Tabs")) : nullptr) == Tabs);
        TestEqual(TEXT("DUC-10: Prepare failure retains the same nested block screen"),
            Tabs != nullptr ? Tabs->GetScreenWidgetForTab(TEXT("info")) : nullptr,
            BlockScreen);
        if (DayText != nullptr)
        {
            TestEqual(TEXT("DUC-10: Prepare failure retains committed third-level text"), DayText->GetTextContent().ToString(), BaselineDayText);
        }
        if (ValueBar != nullptr)
        {
            TestEqual(TEXT("DUC-10: Prepare failure retains committed third-level number"), ValueBar->GetProgress(), BaselineProgress);
        }
    }

    FGV2UiDocumentViewModel CommitFailureDocument;
    if (DispatchFixtureCommand(TEXT("textsystem:command.debug.duc10_update"), CommitFailureDocument))
    {
        FGV2LayeredUiReconciler::FPreparedReconciliationPlan CommitFailurePlan;
        TestTrue(
            *FString::Printf(TEXT("DUC-10: third-level update prepares [Error: %s]"), *ReconcileError),
            Reconciler.PrepareReconcile(nullptr, CommitFailureDocument, ScreenFactory, CommitFailurePlan, ReconcileError));

        AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed on 'day'"), EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed on 'tabs'"), EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);
        const bool bCommitFailed = Reconciler.CommitReconcile(
            nullptr,
            CommitFailurePlan,
            ReconcileError,
            [](const FString& ScreenId, const FString& PropertyPath)
            {
                return ScreenId == TEXT("textsystem:screen.duc10_nested_chain")
                    && PropertyPath == TEXT("tabs.info.day");
            });
        TestFalse(TEXT("DUC-10: injected third-level commit failure rejects the document"), bCommitFailed);
        TestTrue(TEXT("DUC-10: commit failure reports ADR-0040 reconciliation diagnostic"),
            ReconcileError.Contains(TEXT("core:diagnostic.ui_reconcile.commit_failed")));
        TestEqual(TEXT("DUC-10: commit failure retains prior active revision"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("duc10")),
            RootScreen);
        TestTrue(TEXT("DUC-10: commit failure retains root tab-container state"),
            (TabsHost != nullptr ? TabsHost->GetWidgetFromName(TEXT("Tabs")) : nullptr) == Tabs);
        TestEqual(TEXT("DUC-10: commit failure retains nested Screen Template state"),
            Tabs != nullptr ? Tabs->GetScreenWidgetForTab(TEXT("info")) : nullptr,
            BlockScreen);
        if (DayText != nullptr)
        {
            TestEqual(TEXT("DUC-10: commit failure leaves third-level text unmodified"), DayText->GetTextContent().ToString(), BaselineDayText);
        }
        if (ValueBar != nullptr)
        {
            TestEqual(TEXT("DUC-10: commit failure leaves third-level number unmodified"), ValueBar->GetProgress(), BaselineProgress);
        }
    }

    GameInstance->Shutdown();
    GameInstance->RemoveFromRoot();

    return true;
}

#endif
