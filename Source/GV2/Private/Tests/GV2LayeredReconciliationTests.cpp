#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "CommonRichTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

#include "Bridge/GV2BridgeTypes.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PresentationAuthorityProbe.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenAnchorHost.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiHostSemanticState.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ContentCore/UiSchema.h"
#include "GV2PresentationApply/PreparedKeyedCollection.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"

#include "Tests/GV2ForgeryTestWidgets.h"
#include "Tests/GV2PresentationTestFixtures.h"

namespace
{
UGV2ScreenRegistry* LoadConfiguredRegistryForTest()
{
    const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
    return Settings != nullptr && !Settings->RegistryAsset.IsNull()
        ? Settings->RegistryAsset.LoadSynchronous()
        : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiLayeredBasicLifecycleTest,
    "GV2.UI.LayeredReconciliation.BasicLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiLayeredBasicLifecycleTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    // GBF-06: enumerate every asset package actually present under Content, then inspect
    // every screen Blueprint generated class. A new .uasset cannot evade this audit by
    // being absent from a hand-maintained list of known screens.
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter AllGameAssetsFilter;
    AllGameAssetsFilter.PackagePaths.Add(TEXT("/Game"));
    AllGameAssetsFilter.bRecursivePaths = true;
    TArray<FAssetData> AllGameAssets;
    AssetRegistryModule.Get().GetAssets(AllGameAssetsFilter, AllGameAssets);
    TSet<FName> RegistryPackageNames;
    for (const FAssetData& Asset : AllGameAssets)
    {
        RegistryPackageNames.Add(Asset.PackageName);
    }
    TArray<FString> ContentAssetFiles;
    IFileManager::Get().FindFilesRecursive(ContentAssetFiles, *FPaths::ProjectContentDir(), TEXT("*.uasset"), true, false);
    TestTrue(TEXT("GBF-06: project contains assets to audit"), ContentAssetFiles.Num() > 0);
    for (const FString& AssetFilename : ContentAssetFiles)
    {
        FString PackageName;
        const bool bHasPackageName = FPackageName::TryConvertFilenameToLongPackageName(AssetFilename, PackageName);
        TestTrue(*FString::Printf(TEXT("GBF-06: Asset Registry enumerates Content asset '%s'"), *AssetFilename), bHasPackageName);
        if (bHasPackageName)
        {
            TestTrue(*FString::Printf(TEXT("GBF-06: Asset Registry has package '%s'"), *PackageName),
                RegistryPackageNames.Contains(FName(*PackageName)));
        }
    }
    TArray<FString> CallbackImplementers;
    TArray<FString> TabModelCallbackImplementers;
    TArray<FString> TabSelectionCallbackImplementers;
    TArray<FString> CentralStyleBlueprintOverrides;
    for (const FAssetData& Asset : AllGameAssets)
    {
        if (Asset.AssetClassPath.GetAssetName() != TEXT("WidgetBlueprint"))
        {
            continue;
        }
        const FString GeneratedClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString());
        UClass* const GeneratedClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        TestNotNull(*FString::Printf(TEXT("GBF-06: Widget Blueprint generated class loads '%s'"), *Asset.PackageName.ToString()), GeneratedClass);
        if (GeneratedClass == nullptr)
        {
            continue;
        }
        const UFunction* CallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnScreenFieldsApplied"));
        if (GeneratedClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass())
            && CallbackFunction != nullptr
            && CallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            CallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* TabModelCallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnTabModelApplied"));
        if (GeneratedClass->IsChildOf(UGV2TabContainerWidgetBase::StaticClass())
            && TabModelCallbackFunction != nullptr
            && TabModelCallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            TabModelCallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* TabSelectionCallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnTabSelectionUpdated"));
        if (GeneratedClass->IsChildOf(UGV2TabContainerWidgetBase::StaticClass())
            && TabSelectionCallbackFunction != nullptr
            && TabSelectionCallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            TabSelectionCallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* CentralStyleFunction = GeneratedClass->FindFunctionByName(TEXT("ApplyCentralStyle"));
        if (GeneratedClass->ImplementsInterface(UGV2UiStyleConsumer::StaticClass())
            && CentralStyleFunction != nullptr
            && CentralStyleFunction->GetOuterUClass() == GeneratedClass)
        {
            CentralStyleBlueprintOverrides.Add(Asset.PackageName.ToString());
        }
    }
    TestEqual(TEXT("GBF-06: no screen Blueprint implements the obsolete callback"), CallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: screen base exposes no callback inside document Commit"),
        UGV2ScreenWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnScreenFieldsApplied")));
    TestEqual(TEXT("GBF-06: no tab Blueprint implements the obsolete model callback"), TabModelCallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: tab base exposes no model callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnTabModelApplied")));
    TestEqual(TEXT("GBF-06: no tab Blueprint implements the obsolete selection callback"), TabSelectionCallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: tab base exposes no selection callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnTabSelectionUpdated")));
    TestNull(TEXT("GBF-06: tab base exposes no multicast callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindPropertyByName(TEXT("OnTabChanged")));
    TestEqual(TEXT("GBF-06: no Widget Blueprint overrides the central style hook used by Commit"), CentralStyleBlueprintOverrides.Num(), 0);

    // 1. UIF-17: Game Shell Layers Validation & Order
    TestTrue(TEXT("background is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("background")));
    TestTrue(TEXT("location_content is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("location_content")));
    TestTrue(TEXT("character_presentation is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("character_presentation")));
    TestTrue(TEXT("core_interface is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("core_interface")));
    TestTrue(TEXT("overlay_stack is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("overlay_stack")));
    TestTrue(TEXT("modal_stack is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("modal_stack")));
    TestFalse(TEXT("arbitrary_layer is invalid"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("arbitrary_layer")));

    const TArray<FName>& ApprovedLayers = UGV2GameShellWidgetBase::GetApprovedLayers();
    TestEqual(TEXT("Exactly 6 approved layers"), ApprovedLayers.Num(), 6);
    TestEqual(TEXT("Layer 0 is background"), ApprovedLayers[0], UGV2GameShellWidgetBase::LayerBackground);
    TestEqual(TEXT("Layer 1 is location_content"), ApprovedLayers[1], UGV2GameShellWidgetBase::LayerLocationContent);
    TestEqual(TEXT("Layer 2 is character_presentation"), ApprovedLayers[2], UGV2GameShellWidgetBase::LayerCharacterPresentation);
    TestEqual(TEXT("Layer 3 is core_interface"), ApprovedLayers[3], UGV2GameShellWidgetBase::LayerCoreInterface);
    TestEqual(TEXT("Layer 4 is overlay_stack"), ApprovedLayers[4], UGV2GameShellWidgetBase::LayerOverlayStack);
    TestEqual(TEXT("Layer 5 is modal_stack"), ApprovedLayers[5], UGV2GameShellWidgetBase::LayerModalStack);

    // 2. UIF-17: Screen Registry Validation
    UGV2ScreenRegistry* Registry = NewObject<UGV2ScreenRegistry>();
    FGV2ResolvedScreenRegistry EmptyResolvedRegistry;
    FString ValidationError;
    TestFalse(TEXT("Empty registry fails to build"), Registry->CompileResolvedRegistry({}, EmptyResolvedRegistry, ValidationError));

    // 3. UIF-19, UIF-20, UIF-21: Multi-layer Reconciliation, Reuse, Replacement, Modal Blocking, Atomicity
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr,
            TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(
            TestWorld,
            GameShellClass);
        TestNotNull(TEXT("Game shell instantiated"), Shell);
        GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
        if (Shell != nullptr)
        {
            const TPair<FName, FName> LayersToVerify[] = {
                {UGV2GameShellWidgetBase::LayerBackground, TEXT("BackgroundHost")},
                {UGV2GameShellWidgetBase::LayerLocationContent, TEXT("LocationContentHost")},
                {UGV2GameShellWidgetBase::LayerCharacterPresentation, TEXT("CharacterPresentationHost")},
                {UGV2GameShellWidgetBase::LayerCoreInterface, TEXT("CoreInterfaceHost")},
                {UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("OverlayStackHost")},
                {UGV2GameShellWidgetBase::LayerModalStack, TEXT("ModalStackHost")},
            };
            for (const TPair<FName, FName>& LayerAndHost : LayersToVerify)
            {
                const FName LayerToVerify = LayerAndHost.Key;
                UPanelWidget* AuthoredHost = Cast<UPanelWidget>(Shell->GetWidgetFromName(LayerAndHost.Value));
                TestNotNull(
                    *FString::Printf(TEXT("Game shell authors a visible host for layer '%s'"), *LayerToVerify.ToString()),
                    AuthoredHost);
                if (AuthoredHost != nullptr)
                {
                    TestTrue(
                        *FString::Printf(TEXT("Layer host '%s' belongs to the rendered Widget Tree"), *LayerToVerify.ToString()),
                        AuthoredHost == Shell->WidgetTree->RootWidget || AuthoredHost->GetParent() != nullptr);
                }
                UUserWidget* ProbeWidget = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
                const bool bAttached = Shell->AttachScreenToLayer(LayerToVerify, ProbeWidget);
                TestTrue(
                    *FString::Printf(TEXT("Game shell binds a host panel for layer '%s'"), *LayerToVerify.ToString()),
                    bAttached);
                if (AuthoredHost != nullptr)
                {
                    TestEqual(
                        *FString::Printf(TEXT("Layer '%s' attaches to its authored host"), *LayerToVerify.ToString()),
                        ProbeWidget->GetParent(),
                        AuthoredHost);
                }
                const bool bDetached = Shell->DetachScreen(ProbeWidget);
                TestTrue(*FString::Printf(TEXT("Probe widget for layer '%s' detaches cleanly"), *LayerToVerify.ToString()), bDetached);
            }
        }

        FGV2LayeredUiReconciler Reconciler;

        TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> ScreenClasses;
        ScreenClasses.Add(TEXT("core:screen.main"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.alt"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.modal_confirm"), UGV2ScreenWidgetBase::StaticClass());

        int32 FactoryInstantiations = 0;
        auto MockFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
        {
            TSubclassOf<UGV2ScreenWidgetBase>* FoundClass = ScreenClasses.Find(ScreenId);
            if (FoundClass == nullptr || *FoundClass == nullptr)
            {
                return nullptr;
            }
            ++FactoryInstantiations;
            return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, *FoundClass);
        };

        // Step A: Initial Document with Route
        FGV2UiDocumentViewModel Doc1;
        Doc1.UiInstanceId = TEXT("ui@1:1");
        Doc1.Revision = 1;
        Doc1.bHasRoute = true;
        Doc1.Route.Layer = TEXT("location_content");
        Doc1.Route.InstanceKey = TEXT("main");
        Doc1.Route.ScreenId = TEXT("core:screen.main");

        FString ReconcileError;
        TestTrue(TEXT("Reconcile initial Doc1 succeeds"), Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError, *PrepareContext));
        TestEqual(TEXT("Factory instantiated 1 screen widget"), FactoryInstantiations, 1);

        UGV2ScreenWidgetBase* RouteWidget1 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestNotNull(TEXT("Route widget exists in location_content layer"), RouteWidget1);

        // Step B: Update Doc2 with same ScreenId -> Must REUSE widget instance (0 new instantiations)
        FGV2UiDocumentViewModel Doc2;
        Doc2.UiInstanceId = TEXT("ui@1:1");
        Doc2.Revision = 2;
        Doc2.bHasRoute = true;
        Doc2.Route.Layer = TEXT("location_content");
        Doc2.Route.InstanceKey = TEXT("main");
        Doc2.Route.ScreenId = TEXT("core:screen.main");

        TestTrue(TEXT("Reconcile Doc2 succeeds"), Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError, *PrepareContext));
        TestEqual(TEXT("Widget reused without new instantiation"), FactoryInstantiations, 1);
        UGV2ScreenWidgetBase* RouteWidget2 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestEqual(TEXT("Widget instance pointer is preserved across revisions"), RouteWidget2, RouteWidget1);

        // Step C: Doc3 with changed ScreenId -> Replaces widget
        FGV2UiDocumentViewModel Doc3;
        Doc3.UiInstanceId = TEXT("ui@1:1");
        Doc3.Revision = 3;
        Doc3.bHasRoute = true;
        Doc3.Route.Layer = TEXT("location_content");
        Doc3.Route.InstanceKey = TEXT("main");
        Doc3.Route.ScreenId = TEXT("core:screen.alt");

        TestTrue(TEXT("Reconcile Doc3 succeeds"), Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError, *PrepareContext));
        TestEqual(TEXT("Factory called to instantiate new screen class"), FactoryInstantiations, 2);
        UGV2ScreenWidgetBase* RouteWidget3 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestNotNull(TEXT("New route widget exists"), RouteWidget3);
        TestNotEqual(TEXT("Old route widget replaced"), RouteWidget3, RouteWidget1);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiLayeredModalInteractivityAndPrepareAtomicityTest,
    "GV2.UI.LayeredReconciliation.ModalInteractivityAndPrepareAtomicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiLayeredModalInteractivityAndPrepareAtomicityTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }

    UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    if (GameShellClass == nullptr)
    {
        GameShellClass = UGV2GameShellWidgetBase::StaticClass();
    }
    UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(
        TestWorld,
        GameShellClass);
    TestNotNull(TEXT("Game shell instantiated"), Shell);
    GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);

    FGV2LayeredUiReconciler Reconciler;

    TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> ScreenClasses;
    ScreenClasses.Add(TEXT("core:screen.main"), UGV2ScreenWidgetBase::StaticClass());
    ScreenClasses.Add(TEXT("core:screen.alt"), UGV2ScreenWidgetBase::StaticClass());
    ScreenClasses.Add(TEXT("core:screen.modal_confirm"), UGV2ScreenWidgetBase::StaticClass());

    int32 FactoryInstantiations = 0;
    auto MockFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
    {
        TSubclassOf<UGV2ScreenWidgetBase>* FoundClass = ScreenClasses.Find(ScreenId);
        if (FoundClass == nullptr || *FoundClass == nullptr)
        {
            return nullptr;
        }
        ++FactoryInstantiations;
        return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, *FoundClass);
    };

    FString ReconcileError;

    // Baseline route screen (Doc3)
    FGV2UiDocumentViewModel Doc3;
    Doc3.UiInstanceId = TEXT("ui@1:1");
    Doc3.Revision = 3;
    Doc3.bHasRoute = true;
    Doc3.Route.Layer = TEXT("location_content");
    Doc3.Route.InstanceKey = TEXT("main");
    Doc3.Route.ScreenId = TEXT("core:screen.alt");
    TestTrue(TEXT("Reconcile baseline Doc3 succeeds"), Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError, *PrepareContext));
    UGV2ScreenWidgetBase* RouteWidget3 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
    TestNotNull(TEXT("Baseline route widget exists"), RouteWidget3);

    // Step D: Doc4 adds a modal -> Lower layers blocked, top modal interactive (UIF-20)
    FGV2UiDocumentViewModel Doc4 = Doc3;
        Doc4.Revision = 4;
        FGV2ScreenInstanceViewModel ModalInst;
        ModalInst.Layer = TEXT("modal_stack");
        ModalInst.InstanceKey = TEXT("confirm_dialog");
        ModalInst.ScreenId = TEXT("core:screen.modal_confirm");
        Doc4.Modals.Add(ModalInst);

        TestTrue(TEXT("Reconcile Doc4 with modal succeeds"), Reconciler.Reconcile(Shell, Doc4, MockFactory, ReconcileError, *PrepareContext));
        TestEqual(TEXT("Factory instantiated modal widget"), FactoryInstantiations, 2);
        if (Shell != nullptr)
        {
            TestFalse(TEXT("Location content layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Background layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("background")));
            TestFalse(TEXT("Core interface layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("core_interface")));
            TestTrue(TEXT("Modal stack layer is interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
        }

        // Step E: Doc5 closes modal -> Lower layers unblocked
        FGV2UiDocumentViewModel Doc5 = Doc3;
        Doc5.Revision = 5;
        Doc5.Modals.Empty();

        TestTrue(TEXT("Reconcile Doc5 (modal closed) succeeds"), Reconciler.Reconcile(Shell, Doc5, MockFactory, ReconcileError, *PrepareContext));
        if (Shell != nullptr)
        {
            TestTrue(TEXT("Location content layer is unblocked"), Shell->IsLayerInteractive(TEXT("location_content")));
        }
        TestNull(TEXT("Modal widget detached and removed from active list"), Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("confirm_dialog")));

        // Step F: Atomicity - Candidate with invalid ScreenId rejected without modifying active set (UIF-21)
        FGV2UiDocumentViewModel BadDoc;
        BadDoc.UiInstanceId = TEXT("ui@1:1");
        BadDoc.Revision = 6;
        BadDoc.bHasRoute = true;
        BadDoc.Route.Layer = TEXT("location_content");
        BadDoc.Route.InstanceKey = TEXT("main");
        BadDoc.Route.ScreenId = TEXT("invalid:screen.does_not_exist");

        TestFalse(TEXT("Reconcile BadDoc fails"), Reconciler.Reconcile(Shell, BadDoc, MockFactory, ReconcileError, *PrepareContext));
        TestEqual(
            TEXT("Previous active screen remains intact after rejected candidate"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")),
            RouteWidget3);

        // Step G: UPP-28 Multi-layer failure injection across layers
        // REV3-07 closure: FGV2LayeredUiReconciler.Reconcile is document-level atomic — MultiDoc2 below
        // injects a failing field in one layer (modal_stack) and Step G's assertions confirm ALL layers
        // (route, overlay, modal) are left completely untouched, not just the layer that failed.
        // Set up active state with 3 layers: Route in location_content, Overlay in overlay_stack, Modal in modal_stack
        FGV2UiDocumentViewModel MultiDoc1;
        MultiDoc1.UiInstanceId = TEXT("ui@1:1");
        MultiDoc1.Revision = 7;
        MultiDoc1.bHasRoute = true;
        MultiDoc1.Route.Layer = TEXT("location_content");
        MultiDoc1.Route.InstanceKey = TEXT("main");
        MultiDoc1.Route.ScreenId = TEXT("core:screen.main");

        FGV2ScreenInstanceViewModel OverlayInst1;
        OverlayInst1.Layer = TEXT("overlay_stack");
        OverlayInst1.InstanceKey = TEXT("hud");
        OverlayInst1.ScreenId = TEXT("core:screen.alt");
        MultiDoc1.Overlays.Add(OverlayInst1);

        FGV2ScreenInstanceViewModel ModalInst1;
        ModalInst1.Layer = TEXT("modal_stack");
        ModalInst1.InstanceKey = TEXT("dialog1");
        ModalInst1.ScreenId = TEXT("core:screen.modal_confirm");
        MultiDoc1.Modals.Add(ModalInst1);

        const bool bMulti1Success = Reconciler.Reconcile(Shell, MultiDoc1, MockFactory, ReconcileError, *PrepareContext);
        TestTrue(*FString::Printf(TEXT("Reconcile MultiDoc1 succeeds [Error: %s]"), *ReconcileError), bMulti1Success);
        UGV2ScreenWidgetBase* RouteWidgetMulti = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        UGV2ScreenWidgetBase* OverlayWidgetMulti = Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud"));
        UGV2ScreenWidgetBase* ModalWidgetMulti = Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1"));
        TestNotNull(TEXT("RouteWidgetMulti active"), RouteWidgetMulti);
        TestNotNull(TEXT("OverlayWidgetMulti active"), OverlayWidgetMulti);
        TestNotNull(TEXT("ModalWidgetMulti active"), ModalWidgetMulti);

        if (Shell != nullptr)
        {
            TestTrue(TEXT("location_content contains RouteWidgetMulti"), Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidgetMulti));
            TestTrue(TEXT("overlay_stack contains OverlayWidgetMulti"), Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(OverlayWidgetMulti));
            TestTrue(TEXT("modal_stack contains ModalWidgetMulti"), Shell->GetScreensInLayer(TEXT("modal_stack")).Contains(ModalWidgetMulti));
            TestTrue(TEXT("Modal stack is interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
            TestFalse(TEXT("Location content is blocked by modal"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Overlay stack is blocked by modal"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
        }

        // Now prepare MultiDoc2: Replaces route, adds new overlay, but injects failing field in modal
        FGV2UiDocumentViewModel MultiDoc2;
        MultiDoc2.UiInstanceId = TEXT("ui@1:1");
        MultiDoc2.Revision = 8;
        MultiDoc2.bHasRoute = true;
        MultiDoc2.Route.Layer = TEXT("location_content");
        MultiDoc2.Route.InstanceKey = TEXT("main");
        MultiDoc2.Route.ScreenId = TEXT("core:screen.alt"); // Replaces route

        FGV2ScreenInstanceViewModel OverlayInst2;
        OverlayInst2.Layer = TEXT("overlay_stack");
        OverlayInst2.InstanceKey = TEXT("minimap");
        OverlayInst2.ScreenId = TEXT("core:screen.main"); // New overlay
        MultiDoc2.Overlays.Add(OverlayInst2);

        FGV2ScreenInstanceViewModel FailingModalInst;
        FailingModalInst.Layer = TEXT("modal_stack");
        FailingModalInst.InstanceKey = TEXT("dialog1");
        FailingModalInst.ScreenId = TEXT("core:screen.modal_confirm");
        FGV2ScreenFieldValue BadField;
        BadField.FieldId = TEXT("invalid_non_canonical_field_name_with_UPPERCASE"); // fails IsCanonicalFieldId in PrepareScreenFields
        FailingModalInst.Fields.Add(BadField);
        MultiDoc2.Modals.Add(FailingModalInst);

        const bool bMulti2Success = Reconciler.Reconcile(Shell, MultiDoc2, MockFactory, ReconcileError, *PrepareContext);
        TestFalse(TEXT("Reconcile MultiDoc2 fails due to failing modal field"), bMulti2Success);
        TestFalse(TEXT("ReconcileError is populated"), ReconcileError.IsEmpty());

        // Verify ALL layers remain completely untouched in both Reconciler and Shell!
        TestEqual(
            TEXT("Route widget in location_content untouched"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")),
            RouteWidgetMulti);
        TestEqual(
            TEXT("Overlay widget in overlay_stack untouched"),
            Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud")),
            OverlayWidgetMulti);
        TestEqual(
            TEXT("Modal widget in modal_stack untouched"),
            Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1")),
            ModalWidgetMulti);
        TestNull(
            TEXT("Minimap overlay was never added to active set"),
            Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("minimap")));

        if (Shell != nullptr)
        {
            TestTrue(TEXT("Old Route widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidgetMulti));
            TestTrue(TEXT("Old Overlay widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(OverlayWidgetMulti));
            TestTrue(TEXT("Old Modal widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("modal_stack")).Contains(ModalWidgetMulti));
            TestFalse(TEXT("Location content layer is still blocked"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Overlay stack layer is still blocked"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
            TestTrue(TEXT("Modal stack layer is still interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
        }

        // Step H: Clean recovery - Remove modals and reuse Route widget instance
        FGV2UiDocumentViewModel MultiDoc3;
        MultiDoc3.UiInstanceId = TEXT("ui@1:1");
        MultiDoc3.Revision = 9;
        MultiDoc3.bHasRoute = true;
        MultiDoc3.Route.Layer = TEXT("location_content");
        MultiDoc3.Route.InstanceKey = TEXT("main");
        MultiDoc3.Route.ScreenId = TEXT("core:screen.main"); // Reuses RouteWidgetMulti

        const bool bMulti3Success = Reconciler.Reconcile(Shell, MultiDoc3, MockFactory, ReconcileError, *PrepareContext);
        TestTrue(*FString::Printf(TEXT("Reconcile MultiDoc3 succeeds [Error: %s]"), *ReconcileError), bMulti3Success);
        UGV2ScreenWidgetBase* RouteWidgetReused = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestEqual(TEXT("Route widget instance was reused preserving UI-local state"), RouteWidgetReused, RouteWidgetMulti);
        TestNull(TEXT("Overlay widget was detached"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud")));
        TestNull(TEXT("Modal widget was detached"), Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1")));

        // Step I: PCC-06 -- Commit-phase failure injection into a single screen. Before
        // PCC-06, CommitReconcile discarded CommitScreenFields' result and kept iterating
        // (the exact swallowed-failure shape this task closes); this proves the traversal
        // now stops on first failure, Reconcile reports it, and the previous revision's
        // observable widget state is left untouched rather than partially overwritten.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* FaultScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            FaultScreen->WidgetTree = NewObject<UWidgetTree>(FaultScreen);
            UGV2DeclaredCompositeWidgetBase* TopBar = FaultScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            FaultScreen->WidgetTree->RootWidget = TopBar;
            // DescribeUiCapabilities resolves "day" via GetWidgetFromName("DayText"), a lookup
            // in TopBar's *own* WidgetTree (DUC-08: TopBar is now the generic declared
            // composite, so DayText must be nested inside it, not a sibling in the screen's
            // tree, exactly like a real WBP instance's Designer tree).
            TopBar->WidgetTree = NewObject<UWidgetTree>(TopBar);
            UGV2TextWidgetBase* FaultDayTextWidget = TopBar->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBar->WidgetTree->RootWidget = FaultDayTextWidget;
            TestNotNull(TEXT("PCC-06: DayText child constructs"), FaultDayTextWidget);
            // DUC-01: identity is the shared HostIdentity (public via IGV2UiPropertyHost),
            // not a per-class reflected property -- no reflection needed to set it.
            TopBar->SetHostIdentity(FName(TEXT("top_bar")));
            TopBar->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            auto MakeDayFieldValue = [](const FString& DayText) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ "day", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });

                FGV2TextViewModel DayModel;
                DayModel.Text = FText::FromString(DayText);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeText(DayModel));

                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("top_bar");
                FieldValue.SchemaId = TEXT("test:schema.pcc06_fault_top_bar.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeFaultDoc = [&](const FString& DayText) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 20;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("fault_slot");
                Doc.Route.ScreenId = TEXT("core:screen.pcc06_fault_target");
                Doc.Route.Fields.Add(MakeDayFieldValue(DayText));
                return Doc;
            };

            TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> FaultScreenClasses;
            FaultScreenClasses.Add(TEXT("core:screen.pcc06_fault_target"), UGV2ScreenWidgetBase::StaticClass());
            auto FaultFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return FaultScreen;
            };

            IGV2UiPropertyHost* TopBarHost = Cast<IGV2UiPropertyHost>(TopBar);
            TestNotNull(TEXT("PCC-06: TopBar is an IGV2UiPropertyHost"), TopBarHost);

            FString FaultReconcileError;
            const bool bBaseline = Reconciler.Reconcile(Shell, MakeFaultDoc(TEXT("Monday")), FaultFactory, FaultReconcileError, *PrepareContext);
            TestTrue(*FString::Printf(TEXT("PCC-06: baseline reconcile of fault screen succeeds [Error: %s]"), *FaultReconcileError), bBaseline);
            if (TopBarHost != nullptr)
            {
                const FGV2PreparedUiValue* BaselineDay =
                    GetUiHostSemanticState(TopBarHost->GetPropertyHostState()).GetLastCommittedProperties().FindField(TEXT("day"));
                TestNotNull(TEXT("PCC-06: baseline commit recorded a 'day' property"), BaselineDay);
                if (BaselineDay != nullptr)
                {
                    TestEqual(TEXT("PCC-06: baseline committed day value is Monday"), BaselineDay->AsText().Text.ToString(), TEXT("Monday"));
                }
            }

            auto FailInjector = [](const FString& ScreenId, const FString& /*PropertyPath*/) -> bool
            {
                return ScreenId == TEXT("core:screen.pcc06_fault_target");
            };
            const bool bFaultResult = Reconciler.Reconcile(Shell, MakeFaultDoc(TEXT("Tuesday")), FaultFactory, FaultReconcileError, *PrepareContext, FailInjector);

            TestFalse(TEXT("PCC-06: Reconcile fails when injected Commit failure occurs"), bFaultResult);
            TestTrue(TEXT("PCC-06: ReconcileError names the commit-phase diagnostic"),
                FaultReconcileError.Contains(TEXT("core:diagnostic.ui_reconcile.commit_failed")));
            TestTrue(TEXT("PCC-06: ReconcileError names the failed screen_id"),
                FaultReconcileError.Contains(TEXT("core:screen.pcc06_fault_target")));
            if (TopBarHost != nullptr)
            {
                const FGV2PreparedUiValue* DayAfterFault =
                    GetUiHostSemanticState(TopBarHost->GetPropertyHostState()).GetLastCommittedProperties().FindField(TEXT("day"));
                TestNotNull(TEXT("PCC-06: 'day' property still tracked after failed commit"), DayAfterFault);
                if (DayAfterFault != nullptr)
                {
                    TestEqual(TEXT("PCC-06: committed day value is still Monday, not Tuesday -- injected Commit never ran SetLastCommittedProperties"),
                        DayAfterFault->AsText().Text.ToString(), TEXT("Monday"));
                }
            }
            TestEqual(TEXT("PCC-06: reused widget instance is still the active screen for the slot"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("fault_slot")), FaultScreen);
        }

        if (Shell != nullptr)
        {
            TestTrue(TEXT("Location content layer is unblocked after modal closure"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestTrue(TEXT("Overlay stack layer is unblocked"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
            TestTrue(TEXT("Background layer is unblocked"), Shell->IsLayerInteractive(TEXT("background")));
            TestTrue(TEXT("Core interface layer is unblocked"), Shell->IsLayerInteractive(TEXT("core_interface")));
        }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiLayeredMultiLayerCommitFailureTest,
    "GV2.UI.LayeredReconciliation.MultiLayerCommitFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiLayeredMultiLayerCommitFailureTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }

    UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    if (GameShellClass == nullptr)
    {
        GameShellClass = UGV2GameShellWidgetBase::StaticClass();
    }
    UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(
        TestWorld,
        GameShellClass);
    TestNotNull(TEXT("Game shell instantiated"), Shell);
    GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);

    FGV2LayeredUiReconciler Reconciler;

    // Step J: PCC-07 -- multi-layer Commit-phase failure injection. UPP-28's own DoD
    // ("failed Prepare leaves the active set and bindings untouched") only covered the
    // Prepare phase; before PCC-07's reorder in CommitReconcile (commit every screen
        // BEFORE touching Shell attach/detach or ActiveScreens), an injected Commit failure
        // on one screen of a multi-layer document could leave *other*, unrelated layers'
        // screens already replaced in the Shell tree while ActiveScreens rolled back whole
        // -- a real state/bookkeeping divergence, not just a wrong count. This proves that
        // after a Commit failure on layer A, layer B's PREVIOUS widget instance is still
        // both the reconciler's ActiveScreens entry AND still attached in the Shell tree,
        // not merely present in some count.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* TopBarScreenV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            TopBarScreenV1->WidgetTree = NewObject<UWidgetTree>(TopBarScreenV1);
            UGV2DeclaredCompositeWidgetBase* TopBarV1 = TopBarScreenV1->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            TopBarScreenV1->WidgetTree->RootWidget = TopBarV1;
            // DayText must be nested inside TopBarV1's *own* WidgetTree (DUC-08).
            TopBarV1->WidgetTree = NewObject<UWidgetTree>(TopBarV1);
            UGV2TextWidgetBase* TopBarV1DayText = TopBarV1->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBarV1->WidgetTree->RootWidget = TopBarV1DayText;
            TestNotNull(TEXT("PCC-07: layer A v1 DayText child constructs"), TopBarV1DayText);
            TopBarV1->SetHostIdentity(FName(TEXT("top_bar")));
            TopBarV1->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* TopBarScreenV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            TopBarScreenV2->WidgetTree = NewObject<UWidgetTree>(TopBarScreenV2);
            UGV2DeclaredCompositeWidgetBase* TopBarV2 = TopBarScreenV2->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            TopBarScreenV2->WidgetTree->RootWidget = TopBarV2;
            // DayText must be nested inside TopBarV2's *own* WidgetTree (DUC-08).
            TopBarV2->WidgetTree = NewObject<UWidgetTree>(TopBarV2);
            UGV2TextWidgetBase* TopBarV2DayText = TopBarV2->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBarV2->WidgetTree->RootWidget = TopBarV2DayText;
            TestNotNull(TEXT("PCC-07: layer A v2 DayText child constructs"), TopBarV2DayText);
            TopBarV2->SetHostIdentity(FName(TEXT("top_bar")));
            TopBarV2->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* PlainScreenV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            UGV2ScreenWidgetBase* PlainScreenV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            auto MakeTopBarFieldValue = [](const FString& DayText) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ "day", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel DayModel;
                DayModel.Text = FText::FromString(DayText);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeText(DayModel));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("top_bar");
                FieldValue.SchemaId = TEXT("test:schema.pcc07_multi_layer_top_bar.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeMultiLayerDoc = [&](const FString& ScreenIdSuffix, const FString& DayText) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 30;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("pcc07_a");
                Doc.Route.ScreenId = FString::Printf(TEXT("core:screen.pcc07_a_%s"), *ScreenIdSuffix);
                Doc.Route.Fields.Add(MakeTopBarFieldValue(DayText));

                FGV2ScreenInstanceViewModel OverlayInst;
                OverlayInst.Layer = TEXT("overlay_stack");
                OverlayInst.InstanceKey = TEXT("pcc07_b");
                OverlayInst.ScreenId = FString::Printf(TEXT("core:screen.pcc07_b_%s"), *ScreenIdSuffix);
                Doc.Overlays.Add(OverlayInst);
                return Doc;
            };

            TMap<FString, UGV2ScreenWidgetBase*> MultiLayerScreensByScreenId;
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_a_v1"), TopBarScreenV1);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_a_v2"), TopBarScreenV2);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_b_v1"), PlainScreenV1);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_b_v2"), PlainScreenV2);
            auto MultiLayerFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                UGV2ScreenWidgetBase** Found = MultiLayerScreensByScreenId.Find(ScreenId);
                return Found != nullptr ? *Found : nullptr;
            };

            FString MultiLayerError;
            const bool bMultiBaseline = Reconciler.Reconcile(Shell, MakeMultiLayerDoc(TEXT("v1"), TEXT("Monday")), MultiLayerFactory, MultiLayerError, *PrepareContext);
            TestTrue(*FString::Printf(TEXT("PCC-07: baseline multi-layer reconcile succeeds [Error: %s]"), *MultiLayerError), bMultiBaseline);
            TestEqual(TEXT("PCC-07: layer A baseline widget is TopBarScreenV1"), Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("pcc07_a")), TopBarScreenV1);
            TestEqual(TEXT("PCC-07: layer B baseline widget is PlainScreenV1"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("pcc07_b")), PlainScreenV1);

            auto MultiLayerFailInjector = [](const FString& ScreenId, const FString& /*PropertyPath*/) -> bool
            {
                return ScreenId == TEXT("core:screen.pcc07_a_v2");
            };
            const bool bMultiFault = Reconciler.Reconcile(Shell, MakeMultiLayerDoc(TEXT("v2"), TEXT("Tuesday")), MultiLayerFactory, MultiLayerError, *PrepareContext, MultiLayerFailInjector);

            TestFalse(TEXT("PCC-07: Reconcile fails when layer A's Commit is injected to fail"), bMultiFault);
            TestTrue(TEXT("PCC-07: error names layer A's screen_id"), MultiLayerError.Contains(TEXT("core:screen.pcc07_a_v2")));

            TestEqual(TEXT("PCC-07: layer A (the failing layer) is still its PREVIOUS widget instance"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("pcc07_a")), TopBarScreenV1);
            TestEqual(TEXT("PCC-07: layer B (an unrelated, otherwise-successful layer) is STILL its previous widget instance, not silently advanced"),
                Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("pcc07_b")), PlainScreenV1);

            if (Shell != nullptr)
            {
                TestTrue(TEXT("PCC-07: Shell still shows layer A's v1 widget attached (state, not count)"),
                    Shell->GetScreensInLayer(TEXT("location_content")).Contains(TopBarScreenV1));
                TestFalse(TEXT("PCC-07: Shell never attached layer A's v2 widget"),
                    Shell->GetScreensInLayer(TEXT("location_content")).Contains(TopBarScreenV2));
                TestTrue(TEXT("PCC-07: Shell still shows layer B's v1 widget attached, untouched by layer A's failure"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(PlainScreenV1));
                TestFalse(TEXT("PCC-07: Shell never attached layer B's v2 widget either -- the whole document aborted, not just layer A"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(PlainScreenV2));
            }

            // Clean up this scenario's route/overlay so later shared-Shell assertions in
            // this test function see the state they expect (no PCC-07-specific residue).
            FGV2UiDocumentViewModel CleanupDoc;
            CleanupDoc.UiInstanceId = TEXT("ui@1:1");
            CleanupDoc.Revision = 31;
            CleanupDoc.bHasRoute = false;
            FString CleanupError;
            TestTrue(*FString::Printf(TEXT("PCC-07: cleanup reconcile succeeds [Error: %s]"), *CleanupError),
                Reconciler.Reconcile(Shell, CleanupDoc, MultiLayerFactory, CleanupError, *PrepareContext));
        }

        // GBH-01: a document naming a layer with no authored Shell host is rejected
        // wholesale in Prepare, before the first live mutation -- not discovered
        // partway through Commit after an earlier layer already attached. Uses its
        // own partial-host Shell (the shared `Shell` above authors all six layers),
        // built via reflection since BackgroundHost/etc are protected BindWidgetOptional
        // fields with no public setter -- the same FindFProperty pattern
        // PrepareUiHostProperties already uses to read a target widget by name.
        {
            UGV2GameShellWidgetBase* PartialShell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, UGV2GameShellWidgetBase::StaticClass());
            TestNotNull(TEXT("GBH-01: partial-host shell instantiated"), PartialShell);
            GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedPartialShell(PartialShell);
            if (PartialShell != nullptr)
            {

                UVerticalBox* LocationHostPanel = NewObject<UVerticalBox>(PartialShell);
                if (FObjectPropertyBase* HostProp = FindFProperty<FObjectPropertyBase>(PartialShell->GetClass(), TEXT("LocationContentHost")))
                {
                    HostProp->SetObjectPropertyValue_InContainer(PartialShell, LocationHostPanel);
                }
                TestTrue(TEXT("GBH-01: location_content host is authored on the fixture"), PartialShell->HasHostForLayer(TEXT("location_content")));
                TestFalse(TEXT("GBH-01: character_presentation host is NOT authored on the fixture"), PartialShell->HasHostForLayer(TEXT("character_presentation")));

                FGV2LayeredUiReconciler GbhReconciler;
                TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> GbhScreenClasses;
                GbhScreenClasses.Add(TEXT("core:screen.gbh01_a"), UGV2ScreenWidgetBase::StaticClass());
                GbhScreenClasses.Add(TEXT("core:screen.gbh01_b"), UGV2ScreenWidgetBase::StaticClass());
                auto GbhFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
                {
                    TSubclassOf<UGV2ScreenWidgetBase>* FoundClass = GbhScreenClasses.Find(ScreenId);
                    return FoundClass != nullptr && *FoundClass != nullptr
                        ? CreateWidget<UGV2ScreenWidgetBase>(TestWorld, *FoundClass)
                        : nullptr;
                };

                // Route (location_content) has a host and would attach first, successfully.
                // The overlay (character_presentation) has none and would attach second,
                // and fail -- exactly the "first succeeds, second doesn't" shape GBH-01
                // must catch in Prepare rather than leave the first physically attached.
                FGV2UiDocumentViewModel GbhDoc;
                GbhDoc.UiInstanceId = TEXT("ui@gbh01:1");
                GbhDoc.Revision = 1;
                GbhDoc.bHasRoute = true;
                GbhDoc.Route.Layer = TEXT("location_content");
                GbhDoc.Route.InstanceKey = TEXT("gbh01_a");
                GbhDoc.Route.ScreenId = TEXT("core:screen.gbh01_a");
                FGV2ScreenInstanceViewModel GbhOverlay;
                GbhOverlay.Layer = TEXT("character_presentation");
                GbhOverlay.InstanceKey = TEXT("gbh01_b");
                GbhOverlay.ScreenId = TEXT("core:screen.gbh01_b");
                GbhDoc.Overlays.Add(GbhOverlay);

                FGV2LayeredUiReconciler::FPreparedReconciliationPlan GbhPlan;
                FString GbhError;
                const bool bGbhPrepared = GbhReconciler.PrepareReconcile(PartialShell, GbhDoc, GbhFactory, GbhPlan, GbhError, *PrepareContext);
                TestFalse(*FString::Printf(TEXT("GBH-01: missing layer host is rejected in Prepare [Error: %s]"), *GbhError), bGbhPrepared);
                TestTrue(*FString::Printf(TEXT("GBH-01: rejection names the missing-host diagnostic [Error: %s]"), *GbhError),
                    GbhError.Contains(TEXT("core:diagnostic.ui_reconcile.missing_layer_host")));
                TestTrue(*FString::Printf(TEXT("GBH-01: rejection names the failing layer [Error: %s]"), *GbhError),
                    GbhError.Contains(TEXT("character_presentation")));

                TestEqual(TEXT("GBH-01: Shell tree unchanged -- location_content host still has no children"),
                    LocationHostPanel->GetChildrenCount(), 0);
                TestEqual(TEXT("GBH-01: Reconciler's ActiveScreens untouched"), GbhReconciler.GetActiveScreens().Num(), 0);

                // Full Reconcile() (Prepare + Commit) also fails wholesale and never
                // reaches Commit -- the plan it would have committed is simply discarded.
                FString GbhReconcileError;
                const bool bGbhReconciled = GbhReconciler.Reconcile(PartialShell, GbhDoc, GbhFactory, GbhReconcileError, *PrepareContext);
                TestFalse(TEXT("GBH-01: full Reconcile() also rejects the document wholesale"), bGbhReconciled);

                // Positive control: the same partial-host Shell accepts a document that
                // only targets the layer it does have a host for -- the new check does
                // not over-reject.
                FGV2UiDocumentViewModel GbhPositiveDoc;
                GbhPositiveDoc.UiInstanceId = TEXT("ui@gbh01:2");
                GbhPositiveDoc.Revision = 1;
                GbhPositiveDoc.bHasRoute = true;
                GbhPositiveDoc.Route = GbhDoc.Route;
                FString GbhPositiveError;
                TestTrue(*FString::Printf(TEXT("GBH-01: a document naming only the authored layer still succeeds [Error: %s]"), *GbhPositiveError),
                    GbhReconciler.Reconcile(PartialShell, GbhPositiveDoc, GbhFactory, GbhPositiveError, *PrepareContext));
                TestEqual(TEXT("GBH-01: positive control actually attached to the authored host"),
                    LocationHostPanel->GetChildrenCount(), 1);
            }
        }

        // GBF-01 (rewritten for PAH-06B): a real engine-level per-layer reconcile failure
        // must abort CommitReconcile and restore every layer's Shell tree exactly, not
        // just the failing one. USizeBox is a real single-child UPanelWidget: asking a
        // single-child host to hold 2 simultaneous screens makes the swap's second
        // AddChild fail by construction (UPanelWidget::AddChild's own single-child gate --
        // see PAH-06A's finding that this is the one deterministic, engine-native trigger,
        // not a mock). This deliberately reaches production CommitReconcile instead of
        // calling ReconcilePrepared as a helper-level unit test, and deliberately also
        // replaces location_content's route in the SAME document, so the failure is on a
        // layer processed AFTER location_content (GetApprovedLayers order) -- proving the
        // cross-layer rollback PAH-06B adds: location_content must be restored to v1 too,
        // not just overlay_stack left alone.
        {
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.layer_reconcile_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2GameShellWidgetBase* AttachFailureShell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, UGV2GameShellWidgetBase::StaticClass());
            TestNotNull(TEXT("GBF-01: attach-failure Shell instantiated"), AttachFailureShell);
            GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedAttachFailureShell(AttachFailureShell);
            if (AttachFailureShell != nullptr)
            {

                UOverlay* LocationHostPanel = NewObject<UOverlay>(AttachFailureShell);
                USizeBox* SingleChildOverlayHost = NewObject<USizeBox>(AttachFailureShell);
                auto SetShellHost = [](UGV2GameShellWidgetBase* TargetShell, const FName PropertyName, UPanelWidget* Host)
                {
                    if (FObjectPropertyBase* HostProperty = FindFProperty<FObjectPropertyBase>(TargetShell->GetClass(), PropertyName))
                    {
                        HostProperty->SetObjectPropertyValue_InContainer(TargetShell, Host);
                    }
                };
                SetShellHost(AttachFailureShell, TEXT("LocationContentHost"), LocationHostPanel);
                SetShellHost(AttachFailureShell, TEXT("OverlayStackHost"), SingleChildOverlayHost);

                UGV2ScreenWidgetBase* RouteV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* RouteV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* AcceptedOverlay = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* RejectedOverlay = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                TestNotNull(TEXT("GBF-01: prior route constructs"), RouteV1);
                TestNotNull(TEXT("GBF-01: replacement route constructs"), RouteV2);
                TestNotNull(TEXT("GBF-01: accepted overlay constructs"), AcceptedOverlay);
                TestNotNull(TEXT("GBF-01: rejected overlay constructs"), RejectedOverlay);

                TMap<FString, UGV2ScreenWidgetBase*> ScreensById;
                ScreensById.Add(TEXT("core:screen.gbf01_route_v1"), RouteV1);
                ScreensById.Add(TEXT("core:screen.gbf01_route_v2"), RouteV2);
                ScreensById.Add(TEXT("core:screen.gbf01_overlay_accepted"), AcceptedOverlay);
                ScreensById.Add(TEXT("core:screen.gbf01_overlay_rejected"), RejectedOverlay);
                auto AttachFailureFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
                {
                    UGV2ScreenWidgetBase** Found = ScreensById.Find(ScreenId);
                    return Found != nullptr ? *Found : nullptr;
                };

                FGV2LayeredUiReconciler AttachFailureReconciler;
                FString AttachFailureError;

                FGV2UiDocumentViewModel BaselineDoc;
                BaselineDoc.UiInstanceId = TEXT("ui@gbf01:1");
                BaselineDoc.Revision = 1;
                BaselineDoc.bHasRoute = true;
                BaselineDoc.Route.Layer = TEXT("location_content");
                BaselineDoc.Route.InstanceKey = TEXT("gbf01_route");
                BaselineDoc.Route.ScreenId = TEXT("core:screen.gbf01_route_v1");

                const bool bBaselineCommitted = AttachFailureReconciler.Reconcile(AttachFailureShell, BaselineDoc, AttachFailureFactory, AttachFailureError, *PrepareContext);
                TestTrue(*FString::Printf(TEXT("GBF-01: baseline route commits [Error: %s]"), *AttachFailureError), bBaselineCommitted);
                TestEqual(TEXT("GBF-01: baseline active route is v1"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("location_content"), TEXT("gbf01_route")), RouteV1);
                TestTrue(TEXT("GBF-01: baseline route is physically attached"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV1));

                const FGV2LayeredUiReconciler::FScreenSlotKey RouteKey{TEXT("location_content"), TEXT("gbf01_route")};
                const FGV2LayeredUiReconciler::FActiveScreenEntry* BaselineRouteEntry = AttachFailureReconciler.GetActiveScreens().Find(RouteKey);
                TestNotNull(TEXT("GBF-01: baseline ActiveScreens stores route metadata"), BaselineRouteEntry);
                if (BaselineRouteEntry != nullptr)
                {
                    TestEqual(TEXT("GBF-01: baseline metadata stores v1 screen id"), BaselineRouteEntry->ScreenId, TEXT("core:screen.gbf01_route_v1"));
                }

                const int32 ActiveCountBeforeFailure = AttachFailureReconciler.GetActiveScreens().Num();
                FGV2UiDocumentViewModel CandidateDoc;
                CandidateDoc.UiInstanceId = TEXT("ui@gbf01:1");
                CandidateDoc.Revision = 2;
                CandidateDoc.bHasRoute = true;
                CandidateDoc.Route.Layer = TEXT("location_content");
                CandidateDoc.Route.InstanceKey = TEXT("gbf01_route");
                CandidateDoc.Route.ScreenId = TEXT("core:screen.gbf01_route_v2");
                FGV2ScreenInstanceViewModel AcceptedOverlayInst;
                AcceptedOverlayInst.Layer = TEXT("overlay_stack");
                AcceptedOverlayInst.InstanceKey = TEXT("gbf01_overlay_accepted");
                AcceptedOverlayInst.ScreenId = TEXT("core:screen.gbf01_overlay_accepted");
                CandidateDoc.Overlays.Add(AcceptedOverlayInst);
                FGV2ScreenInstanceViewModel RejectedOverlayInst;
                RejectedOverlayInst.Layer = TEXT("overlay_stack");
                RejectedOverlayInst.InstanceKey = TEXT("gbf01_overlay_rejected");
                RejectedOverlayInst.ScreenId = TEXT("core:screen.gbf01_overlay_rejected");
                CandidateDoc.Overlays.Add(RejectedOverlayInst);

                const bool bRejectedCommit = AttachFailureReconciler.Reconcile(AttachFailureShell, CandidateDoc, AttachFailureFactory, AttachFailureError, *PrepareContext);

                TestFalse(TEXT("GBF-01: single-child overlay host with 2 desired screens rejects document Commit"), bRejectedCommit);
                TestTrue(*FString::Printf(TEXT("GBF-01: failure reports the layer-reconcile diagnostic [Error: %s]"), *AttachFailureError),
                    AttachFailureError.Contains(TEXT("core:diagnostic.ui_reconcile.layer_reconcile_failed")));
                TestTrue(TEXT("GBF-01: failure names the failing layer"),
                    AttachFailureError.Contains(TEXT("overlay_stack")));
                TestEqual(TEXT("GBF-01: ActiveScreens count stays on the previous revision"),
                    AttachFailureReconciler.GetActiveScreens().Num(), ActiveCountBeforeFailure);
                TestEqual(TEXT("GBF-01: prior route stays active after recovery"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("location_content"), TEXT("gbf01_route")), RouteV1);
                TestNull(TEXT("GBF-01: accepted overlay is absent from ActiveScreens (whole document rejected)"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbf01_overlay_accepted")));
                TestNull(TEXT("GBF-01: rejected overlay is absent from ActiveScreens"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbf01_overlay_rejected")));
                const FGV2LayeredUiReconciler::FActiveScreenEntry* RouteAfterFailure = AttachFailureReconciler.GetActiveScreens().Find(RouteKey);
                TestNotNull(TEXT("GBF-01: prior route metadata remains present after recovery"), RouteAfterFailure);
                if (RouteAfterFailure != nullptr)
                {
                    TestEqual(TEXT("GBF-01: prior route metadata remains on v1"), RouteAfterFailure->ScreenId, TEXT("core:screen.gbf01_route_v1"));
                }
                TestTrue(TEXT("GBF-01: prior route is reattached to the Shell tree (cross-layer rollback)"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV1));
                UOverlaySlot* const RestoredRouteSlot = Cast<UOverlaySlot>(RouteV1->Slot);
                TestNotNull(TEXT("PSC-AF-02: cross-layer rollback recreates the route OverlaySlot"), RestoredRouteSlot);
                if (RestoredRouteSlot != nullptr)
                {
                    TestEqual(
                        TEXT("PSC-AF-02: rollback reapplies horizontal Fill to the fresh route slot"),
                        RestoredRouteSlot->GetHorizontalAlignment(),
                        HAlign_Fill);
                    TestEqual(
                        TEXT("PSC-AF-02: rollback reapplies vertical Fill to the fresh route slot"),
                        RestoredRouteSlot->GetVerticalAlignment(),
                        VAlign_Fill);
                }
                TestFalse(TEXT("GBF-01: replacement route is removed by recovery"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV2));
                TestEqual(TEXT("GBF-01: overlay_stack host is restored to its exact prior (empty) state"),
                    SingleChildOverlayHost->GetChildrenCount(), 0);
                TestFalse(TEXT("GBF-01: accepted overlay is physically absent from the Shell tree"),
                    AttachFailureShell->GetScreensInLayer(TEXT("overlay_stack")).Contains(AcceptedOverlay));
                TestFalse(TEXT("GBF-01: rejected overlay is physically absent from the Shell tree"),
                    AttachFailureShell->GetScreensInLayer(TEXT("overlay_stack")).Contains(RejectedOverlay));
            }
        }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiLayeredReusedScreenCommitRollbackTest,
    "GV2.UI.LayeredReconciliation.ReusedScreenCommitRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiLayeredReusedScreenCommitRollbackTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }

    UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    if (GameShellClass == nullptr)
    {
        GameShellClass = UGV2GameShellWidgetBase::StaticClass();
    }
    UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(
        TestWorld,
        GameShellClass);
    TestNotNull(TEXT("Game shell instantiated"), Shell);
    GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);

    FGV2LayeredUiReconciler Reconciler;

    // Step K: GBH-11 (REM-02, ADR-0041) -- the danger point PCC-07 (Step J above)
        // never reached. PCC-07 injects failure on a screen that is being REPLACED by a
        // brand-new widget instance (V1 -> V2): the target widget is off-tree until the
        // whole document commits, so a mid-Commit failure there was always safe -- there
        // is nothing live to leave half-mutated. This step targets the actually dangerous
        // case: the SAME reused live screen widget across two fields (field_a, field_b),
        // where field_a's Commit succeeds -- physically mutating a widget that is already
        // the active, on-screen previous revision -- before field_b's Commit is injected
        // to fail. Before GBH-10 this left field_a's widget showing the new revision's
        // text while ActiveScreens/LastCommittedProperties stayed on revision 1 (REM-02's
        // literal "logically old, physically part-new" shape). Also closes the one
        // GBH-10 boundary that had no dedicated fault-injection test yet: several field
        // hosts of one reused Screen (CommitScreenFields' own multi-host loop).
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 2);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 2);

            UGV2ScreenWidgetBase* ReusedScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            ReusedScreen->WidgetTree = NewObject<UWidgetTree>(ReusedScreen);
            UVerticalBox* ReusedScreenRoot = ReusedScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            ReusedScreen->WidgetTree->RootWidget = ReusedScreenRoot;

            UGV2DeclaredCompositeWidgetBase* FieldA = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldA"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldA);
            FieldA->WidgetTree = NewObject<UWidgetTree>(FieldA);
            UGV2TextWidgetBase* TextA = FieldA->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextA"));
            FieldA->WidgetTree->RootWidget = TextA;
            FieldA->SetHostIdentity(FName(TEXT("field_a")));
            FieldA->DeclaredCapabilities.Add({ FName(TEXT("value_a")), FName(TEXT("TextA")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2DeclaredCompositeWidgetBase* FieldB = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldB"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldB);
            FieldB->WidgetTree = NewObject<UWidgetTree>(FieldB);
            UGV2TextWidgetBase* TextB = FieldB->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextB"));
            FieldB->WidgetTree->RootWidget = TextB;
            FieldB->SetHostIdentity(FName(TEXT("field_b")));
            FieldB->DeclaredCapabilities.Add({ FName(TEXT("value_b")), FName(TEXT("TextB")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* SiblingFailureScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            SiblingFailureScreen->WidgetTree = NewObject<UWidgetTree>(SiblingFailureScreen);
            UGV2DeclaredCompositeWidgetBase* SiblingFailureField = SiblingFailureScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("SiblingFailureField"));
            SiblingFailureScreen->WidgetTree->RootWidget = SiblingFailureField;
            SiblingFailureField->WidgetTree = NewObject<UWidgetTree>(SiblingFailureField);
            UGV2TextWidgetBase* SiblingFailureText = SiblingFailureField->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("SiblingFailureText"));
            SiblingFailureField->WidgetTree->RootWidget = SiblingFailureText;
            SiblingFailureField->SetHostIdentity(FName(TEXT("sibling_field")));
            SiblingFailureField->DeclaredCapabilities.Add({ FName(TEXT("value_s")), FName(TEXT("SiblingFailureText")), EGV2DeclaredUiCapabilityKind::Text });

            auto MakeReusedFieldValue = [](const FName& FieldId, const FString& PropName, const FString& Text) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ TCHAR_TO_UTF8(*PropName), false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel Model;
                Model.Text = FText::FromString(Text);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(PropName, FGV2PreparedUiValue::MakeText(Model));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = FieldId;
                FieldValue.SchemaId = TEXT("test:schema.gbh11_reused_field.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeReusedDoc = [&](const FString& TextA_Value, const FString& TextB_Value) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 40;
                Doc.bHasRoute = false;
                FGV2ScreenInstanceViewModel OverlayInst;
                OverlayInst.Layer = TEXT("overlay_stack");
                OverlayInst.InstanceKey = TEXT("gbh11_reused");
                OverlayInst.ScreenId = TEXT("core:screen.gbh11_reused");
                OverlayInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("field_a")), TEXT("value_a"), TextA_Value));
                OverlayInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("field_b")), TEXT("value_b"), TextB_Value));
                Doc.Overlays.Add(OverlayInst);
                FGV2ScreenInstanceViewModel SiblingInst;
                SiblingInst.Layer = TEXT("overlay_stack");
                SiblingInst.InstanceKey = TEXT("gbf05_sibling_failure");
                SiblingInst.ScreenId = TEXT("core:screen.gbf05_sibling_failure");
                SiblingInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("sibling_field")), TEXT("value_s"), TEXT("Sibling")));
                Doc.Overlays.Add(SiblingInst);
                return Doc;
            };

            auto ReusedFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                if (ScreenId == TEXT("core:screen.gbh11_reused"))
                {
                    return ReusedScreen;
                }
                return ScreenId == TEXT("core:screen.gbf05_sibling_failure") ? SiblingFailureScreen : nullptr;
            };

            FString ReusedError;
            TestTrue(*FString::Printf(TEXT("GBH-11: baseline reconcile of the reused screen succeeds [Error: %s]"), *ReusedError),
                Reconciler.Reconcile(Shell, MakeReusedDoc(TEXT("OldA"), TEXT("OldB")), ReusedFactory, ReusedError, *PrepareContext));
            TestEqual(TEXT("GBH-11: baseline widget is ReusedScreen"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbh11_reused")), ReusedScreen);
            TestEqual(TEXT("GBH-11: baseline TextA reads OldA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("GBH-11: baseline TextB reads OldB"), TextB->GetTextContent().ToString(), TEXT("OldB"));

            const int32 ActiveScreensCountBeforeFault = Reconciler.GetActiveScreens().Num();

            auto ReusedFailInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.gbh11_reused") && PropertyPath == TEXT("value_b");
            };
            const bool bReusedFault = Reconciler.Reconcile(Shell, MakeReusedDoc(TEXT("NewA"), TEXT("NewB")), ReusedFactory, ReusedError, *PrepareContext, ReusedFailInjector);
            TestFalse(TEXT("GBH-11: Reconcile fails when the reused screen's field_b Commit is injected"), bReusedFault);

            TestEqual(TEXT("GBH-11: same reused widget is STILL the active screen (never replaced)"),
                Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbh11_reused")), ReusedScreen);
            TestEqual(TEXT("GBH-11: ActiveScreens count is unchanged by the failed reconcile"),
                Reconciler.GetActiveScreens().Num(), ActiveScreensCountBeforeFault);

            // The actual danger point: field_a's Commit succeeded (it runs before field_b
            // in mutation order) and physically wrote "NewA" to TextA before field_b's
            // injected failure aborted the screen. Without GBH-10's rollback, this
            // assertion is exactly the one that would fail -- TextA would read "NewA".
            TestEqual(TEXT("GBH-11: TextA restored to OldA, NOT left on the uncommitted NewA (REM-02's core claim)"),
                TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("GBH-11: TextB still reads OldB (its own Commit was never reached)"),
                TextB->GetTextContent().ToString(), TEXT("OldB"));

            const FGV2PreparedUiObject& FieldALastCommitted =
                GetUiHostSemanticState(FieldA->GetPropertyHostState()).GetLastCommittedProperties();
            const FGV2PreparedUiValue* FieldALastValueA = FieldALastCommitted.FindField(TEXT("value_a"));
            TestTrue(TEXT("GBH-11: FieldA's LastCommittedProperties has value_a"), FieldALastValueA != nullptr);
            if (FieldALastValueA != nullptr)
            {
                TestEqual(TEXT("GBH-11: FieldA's LastCommittedProperties still names revision 1's OldA, not advanced past the failed commit"),
                    FieldALastValueA->AsText().Text.ToString(), TEXT("OldA"));
            }

            if (Shell != nullptr)
            {
                TestTrue(TEXT("GBH-11: Shell still shows the same reused widget attached (reuse never touches attach)"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(ReusedScreen));
            }

            // GBF-05: this is the document-level production path. The reused screen
            // commits both fields successfully, then the later sibling screen rejects
            // its commit. CommitReconcile must call RollbackFieldPlans for the already
            // committed screen and restore its accounting as well as its widgets.
            const auto OuterScreenFailureInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.gbf05_sibling_failure") && PropertyPath == TEXT("value_s");
            };
            const bool bOuterScreenFault = Reconciler.Reconcile(
                Shell, MakeReusedDoc(TEXT("OuterA"), TEXT("OuterB")), ReusedFactory, ReusedError, *PrepareContext, OuterScreenFailureInjector);
            TestFalse(TEXT("GBF-05: later sibling screen fault rejects the document transaction"), bOuterScreenFault);
            TestEqual(TEXT("GBF-05: document rollback physically restores the earlier reused screen"),
                TextA->GetTextContent().ToString(), TEXT("OldA"));
            const FGV2PreparedUiValue* FieldAAfterOuterScreenFault =
                GetUiHostSemanticState(FieldA->GetPropertyHostState()).GetLastCommittedProperties().FindField(TEXT("value_a"));
            TestNotNull(TEXT("GBF-05: document rollback restores earlier screen accounting"), FieldAAfterOuterScreenFault);
            if (FieldAAfterOuterScreenFault != nullptr)
            {
                TestEqual(TEXT("GBF-05: document rollback accounting matches restored widget"),
                    FieldAAfterOuterScreenFault->AsText().Text.ToString(), TEXT("OldA"));
            }
            TestEqual(TEXT("GBF-05: document rollback restores earlier screen schema id"),
                GetUiHostSemanticState(FieldA->GetPropertyHostState()).GetLastCommittedSchemaId(), TEXT("test:schema.gbh11_reused_field.v1"));

            // Clean up this scenario's overlay so later shared-Shell assertions in this
            // test function see the state they expect (no GBH-11-specific residue).
            FGV2UiDocumentViewModel ReusedCleanupDoc;
            ReusedCleanupDoc.UiInstanceId = TEXT("ui@1:1");
            ReusedCleanupDoc.Revision = 41;
            ReusedCleanupDoc.bHasRoute = false;
            FString ReusedCleanupError;
            TestTrue(*FString::Printf(TEXT("GBH-11: cleanup reconcile succeeds [Error: %s]"), *ReusedCleanupError),
                Reconciler.Reconcile(Shell, ReusedCleanupDoc, ReusedFactory, ReusedCleanupError, *PrepareContext));
        }

        // GBF-04 (GBH-R2, ADR-0041): the inverse of a reused field host is defined by
        // its committed schema, not the candidate schema.  Revision A owns two numeric
        // properties; revision B owns neither.  Injecting a fault after the first reset
        // makes the old implementation replay a second Reset under schema B, leaving the
        // earlier value at zero instead of restoring revision A.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* SchemaSwitchScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            SchemaSwitchScreen->WidgetTree = NewObject<UWidgetTree>(SchemaSwitchScreen);
            UVerticalBox* SchemaSwitchRoot = SchemaSwitchScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            SchemaSwitchScreen->WidgetTree->RootWidget = SchemaSwitchRoot;

            UGV2DeclaredCompositeWidgetBase* SchemaSwitchField = SchemaSwitchScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("SchemaSwitchField"));
            SchemaSwitchRoot->AddChildToVerticalBox(SchemaSwitchField);
            SchemaSwitchField->WidgetTree = NewObject<UWidgetTree>(SchemaSwitchField);
            UVerticalBox* SchemaSwitchFieldRoot = SchemaSwitchField->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FieldRoot"));
            SchemaSwitchField->WidgetTree->RootWidget = SchemaSwitchFieldRoot;
            UGV2ProgressBarWidgetBase* FirstMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("FirstMeter"));
            UGV2ProgressBarWidgetBase* SecondMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("SecondMeter"));
            UGV2ProgressBarWidgetBase* ThirdMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("ThirdMeter"));
            SchemaSwitchFieldRoot->AddChildToVerticalBox(FirstMeter);
            SchemaSwitchFieldRoot->AddChildToVerticalBox(SecondMeter);
            SchemaSwitchFieldRoot->AddChildToVerticalBox(ThirdMeter);
            SchemaSwitchField->SetHostIdentity(FName(TEXT("schema_switch")));
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("first")), FName(TEXT("FirstMeter")), EGV2DeclaredUiCapabilityKind::Number });
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("second")), FName(TEXT("SecondMeter")), EGV2DeclaredUiCapabilityKind::Number });
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("third")), FName(TEXT("ThirdMeter")), EGV2DeclaredUiCapabilityKind::Number });

            auto MakeSchemaSwitchValue = [](const bool bOwnsMeters, const bool bAddsThird = false) -> FGV2ScreenFieldValue
            {
                auto Schema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                if (bOwnsMeters)
                {
                    auto MakeNumberSpec = []() -> GV2ContentCore::FCompiledUiFieldSpecPtr
                    {
                        auto NumberSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                        NumberSpec->Kind = GV2ContentCore::EUiFieldKind::Scalar;
                        GV2ContentCore::FScalarFieldSpec Scalar;
                        Scalar.Kind = GV2ContentCore::EScalarFieldKind::Number;
                        Scalar.MinimumNumber = 0.0;
                        Scalar.MaximumNumber = 1.0;
                        NumberSpec->Scalar = Scalar;
                        return NumberSpec;
                    };
                    Schema->Fields.push_back({ "first", false, MakeNumberSpec() });
                    Schema->Fields.push_back({ "second", false, MakeNumberSpec() });
                    Fields.Emplace(TEXT("first"), FGV2PreparedUiValue::MakeNumber(0.25));
                    Fields.Emplace(TEXT("second"), FGV2PreparedUiValue::MakeNumber(0.75));
                    if (bAddsThird)
                    {
                        Schema->Fields.push_back({ "third", false, MakeNumberSpec() });
                        Fields.Emplace(TEXT("third"), FGV2PreparedUiValue::MakeNumber(0.5));
                    }
                }

                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("schema_switch");
                FieldValue.SchemaId = bOwnsMeters
                    ? TEXT("test:schema.gbf04_meter_pair.v1")
                    : TEXT("test:schema.gbf04_empty.v2");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = Schema;
                return FieldValue;
            };

            TestTrue(TEXT("GBF-04: baseline schema A commits"),
                SchemaSwitchScreen->ApplyScreenFields({ MakeSchemaSwitchValue(true) }, *PrepareContext));
            TestEqual(TEXT("GBF-04: baseline first meter is materialized"), FirstMeter->GetProgress(), 0.25f);
            TestEqual(TEXT("GBF-04: baseline second meter is materialized"), SecondMeter->GetProgress(), 0.75f);

            FGV2ScreenMutationPlan SchemaSwitchPlan;
            FString SchemaSwitchPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-04: candidate schema B prepares [Error: %s]"), *SchemaSwitchPrepareError),
                SchemaSwitchScreen->PrepareScreenFields(
                    { MakeSchemaSwitchValue(false) },
                    SchemaSwitchPlan,
                    SchemaSwitchPrepareError,
                    nullptr,
                    PrepareContext));
            TestEqual(TEXT("GBF-04: candidate has one field plan"), SchemaSwitchPlan.FieldPlans.Num(), 1);
            if (SchemaSwitchPlan.FieldPlans.Num() == 1)
            {
                const TArray<FGV2UiPropertyMutation>& ForwardMutations = SchemaSwitchPlan.FieldPlans[0].MutationPlan.GetMutations();
                TestEqual(TEXT("GBF-04: schema removal prepares a reset for each formerly-owned property"), ForwardMutations.Num(), 2);
                const FString LastMutationPath = ForwardMutations.Num() > 0 ? ForwardMutations.Last().PropertyPath : FString();
                TestFalse(TEXT("GBF-04: candidate has a final mutation to inject"), LastMutationPath.IsEmpty());
                if (!LastMutationPath.IsEmpty())
                {
                    FString SchemaSwitchCommitError;
                    TestFalse(TEXT("GBF-04: commit fault rejects candidate schema B"),
                        SchemaSwitchScreen->CommitScreenFields(SchemaSwitchPlan, SchemaSwitchCommitError, [&LastMutationPath](const FString& Path)
                        {
                            return Path == LastMutationPath;
                        }));
                }
            }

            TestEqual(TEXT("GBF-04: first meter returns to schema A value after failed schema switch"), FirstMeter->GetProgress(), 0.25f);
            TestEqual(TEXT("GBF-04: second meter returns to schema A value after failed schema switch"), SecondMeter->GetProgress(), 0.75f);

            // Schema expansion needs the other half of the inverse rule: `third` was
            // absent from schema A, so its inverse is Reset rather than an old value.
            FGV2ScreenMutationPlan ExpansionPlan;
            FString ExpansionPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-04: schema expansion prepares [Error: %s]"), *ExpansionPrepareError),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(true, true) }, ExpansionPlan, ExpansionPrepareError));
            TestEqual(TEXT("GBF-04: schema expansion has one inverse per forward mutation"),
                ExpansionPlan.FieldPlans[0].MutationPlan.Num(), ExpansionPlan.FieldPlans[0].RollbackPlan.Num());

            // GBF-05: a higher transaction can reject this already successful screen
            // after its Commit advanced the host snapshot. Rollback must restore both
            // the widgets and the snapshot which the *next* Prepare observes.
            FString ExpansionCommitError;
            TestTrue(TEXT("GBF-05: expanded revision commits before outer failure"),
                SchemaSwitchScreen->CommitScreenFields(ExpansionPlan, ExpansionCommitError));
            TestEqual(TEXT("GBF-05: expanded third meter is physically applied"), ThirdMeter->GetProgress(), 0.5f);
            const FGV2UiRollbackResult OuterRollbackResult = RollbackFieldPlans(ExpansionPlan.FieldPlans);
            TestTrue(TEXT("GBF-05: outer rollback of a cleanly-prepared plan restores successfully"), OuterRollbackResult.bRestored);
            TestEqual(TEXT("GBF-05: rollback physically resets candidate-only meter"), ThirdMeter->GetProgress(), 0.0f);
            const FGV2UiHostSemanticState& StateAfterOuterRollback =
                GetUiHostSemanticState(SchemaSwitchField->GetPropertyHostState());
            TestEqual(TEXT("GBF-05: outer rollback restores prior schema id"),
                StateAfterOuterRollback.GetLastCommittedSchemaId(), TEXT("test:schema.gbf04_meter_pair.v1"));
            TestNull(TEXT("GBF-05: outer rollback removes candidate-only property from committed state"),
                StateAfterOuterRollback.GetLastCommittedProperties().FindField(TEXT("third")));
            FGV2ScreenMutationPlan NextPreparePlan;
            FString NextPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-05: next Prepare reads restored revision [Error: %s]"), *NextPrepareError),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(true) }, NextPreparePlan, NextPrepareError));
            TestEqual(TEXT("GBF-05: next Prepare returns the one declared field plan"), NextPreparePlan.FieldPlans.Num(), 1);
            if (NextPreparePlan.FieldPlans.Num() == 1)
            {
                const TArray<FGV2UiPropertyMutation>& NextMutations = NextPreparePlan.FieldPlans[0].MutationPlan.GetMutations();
                TestEqual(TEXT("GBF-05: next Prepare has only the prior schema's two properties"), NextMutations.Num(), 2);
                const bool bNextPrepareResetsCancelledThird = NextMutations.ContainsByPredicate([](const FGV2UiPropertyMutation& Mutation)
                {
                    return Mutation.PropertyName == TEXT("third");
                });
                TestFalse(TEXT("GBF-05: next Prepare does not see the cancelled third property"), bNextPrepareResetsCancelledThird);
            }

            // A value-only legacy snapshot is deliberately not accepted as an inverse
            // source: guessing schema B here would recreate the original defect.
            GetUiHostSemanticState(SchemaSwitchField->GetPropertyHostState()).SetLastCommittedProperties(
                GetUiHostSemanticState(SchemaSwitchField->GetPropertyHostState()).GetLastCommittedProperties());
            FGV2ScreenMutationPlan MissingSchemaPlan;
            FString MissingSchemaError;
            TestFalse(TEXT("GBF-04: previous value without committed schema rejects Prepare"),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(false) }, MissingSchemaPlan, MissingSchemaError));
            TestTrue(TEXT("GBF-04: missing committed schema reports typed rollback diagnostic"),
                MissingSchemaError.Contains(TEXT("core:diagnostic.ui_rollback.missing_committed_schema")));
        }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ModalStackKeyedCollectionOrderingContract,
    "GV2.Runtime.UI.ModalStackKeyedCollectionOrderingContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06A (ADR-0042, INV-P3 proof slice): modal_stack is the one Game Shell layer
// converted from the per-widget AttachScreenToLayer loop to
// FGV2KeyedCollection::ReconcilePrepared (Source/GV2/Public/UI/GV2KeyedCollection.h). The
// old loop could not reorder two already-attached (reused) widgets at all --
// AttachScreenToLayer no-ops once a widget's parent already equals the host -- so this
// proves reuse + create + remove + reorder in a single reconcile call, reading the Host
// panel's ACTUAL physical child order (Shell::GetScreensInLayer, which walks
// Host->GetChildAt(i)), not just the reconciler's ActiveScreens bookkeeping or a returned
// bool. The remaining five layers are untouched -- this is scoped to modal_stack alone.
bool FGV2ModalStackKeyedCollectionOrderingContract::RunTest(const FString& Parameters)
{
    // PSC-10B: reconciliation requires the session snapshot -- central style is prepared
    // from it, and no widget resolves a Theme of its own any more.
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr,
            TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-06A: Game shell instantiated"), Shell);
        GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
        if (Shell != nullptr)
        {

            FGV2LayeredUiReconciler Reconciler;
            auto MockFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            };
            auto MakeModalInstance = [](FName InstanceKey) -> FGV2ScreenInstanceViewModel
            {
                FGV2ScreenInstanceViewModel Inst;
                Inst.Layer = UGV2GameShellWidgetBase::LayerModalStack;
                Inst.InstanceKey = InstanceKey;
                Inst.ScreenId = TEXT("core:screen.modal_probe");
                return Inst;
            };

            FString ReconcileError;

            FGV2UiDocumentViewModel Doc1;
            Doc1.UiInstanceId = TEXT("ui@pah06a");
            Doc1.Revision = 1;
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_a")));
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_b")));
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_c")));
            TestTrue(*FString::Printf(TEXT("PAH-06A: reconcile [A,B,C] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError, *PrepareContext));

            UGV2ScreenWidgetBase* WidgetA = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_a"));
            UGV2ScreenWidgetBase* WidgetB = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_b"));
            UGV2ScreenWidgetBase* WidgetC = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_c"));
            TestNotNull(TEXT("PAH-06A: A created"), WidgetA);
            TestNotNull(TEXT("PAH-06A: B created"), WidgetB);
            TestNotNull(TEXT("PAH-06A: C created"), WidgetC);

            const TArray<UUserWidget*> InitialOrder = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerModalStack);
            TestEqual(TEXT("PAH-06A: [A,B,C] has 3 physical children"), InitialOrder.Num(), 3);
            if (InitialOrder.Num() == 3)
            {
                TestEqual(TEXT("PAH-06A: physical child 0 is A"), InitialOrder[0], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06A: physical child 1 is B"), InitialOrder[1], Cast<UUserWidget>(WidgetB));
                TestEqual(TEXT("PAH-06A: physical child 2 is C"), InitialOrder[2], Cast<UUserWidget>(WidgetC));
            }

            // [A, B, C] -> [C, A, D]: A and C reused (same widget identity), B removed, D
            // newly created -- physical order must become exactly C, A, D.
            FGV2UiDocumentViewModel Doc2;
            Doc2.UiInstanceId = TEXT("ui@pah06a");
            Doc2.Revision = 2;
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_c")));
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_a")));
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_d")));
            TestTrue(*FString::Printf(TEXT("PAH-06A: reconcile [C,A,D] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError, *PrepareContext));

            TestEqual(TEXT("PAH-06A: A is the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_a")), WidgetA);
            TestEqual(TEXT("PAH-06A: C is the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_c")), WidgetC);
            UGV2ScreenWidgetBase* WidgetD = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_d"));
            TestNotNull(TEXT("PAH-06A: D was created"), WidgetD);
            TestNull(TEXT("PAH-06A: B removed from active set"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_b")));

            const TArray<UUserWidget*> ReorderedOrder = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerModalStack);
            TestEqual(TEXT("PAH-06A: [C,A,D] has 3 physical children"), ReorderedOrder.Num(), 3);
            if (ReorderedOrder.Num() == 3)
            {
                TestEqual(TEXT("PAH-06A: physical child 0 is C (reused, moved)"), ReorderedOrder[0], Cast<UUserWidget>(WidgetC));
                TestEqual(TEXT("PAH-06A: physical child 1 is A (reused, moved)"), ReorderedOrder[1], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06A: physical child 2 is D (newly created)"), ReorderedOrder[2], Cast<UUserWidget>(WidgetD));
            }
            TestFalse(TEXT("PAH-06A: B is no longer a child of modal_stack"), ReorderedOrder.Contains(Cast<UUserWidget>(WidgetB)));
            TestNull(TEXT("PAH-06A: B has no parent after removal"), WidgetB->GetParent());
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2KeyedCollectionReconcilePreparedRestoresOnSwapFailure,
    "GV2.Runtime.UI.KeyedCollectionReconcilePreparedRestoresOnSwapFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06A (ADR-0042, INV-P3 proof slice): proves FGV2KeyedCollection::ReconcilePrepared's
// container-swap restore-on-failure path (GV2KeyedCollection.h, the AddChild-fails branch
// a few lines after "Commit to Container atomically") by reading the ACTUAL container
// children afterward, not the returned bool. UPanelWidget::AddChild's only failure
// conditions are a null widget and the single-child gate
// (!bCanHaveMultipleChildren && GetChildrenCount() > 0); AddChild is not virtual, so no
// C++ subclass can inject a failure at an arbitrary position within a genuinely
// multi-child container. UBorder (a UContentWidget, single-child by construction) is the
// one deterministic, engine-native trigger: reconciling 2 desired widgets into it makes
// the swap's second AddChild call fail by construction, not by a test double standing in
// for the engine.
//
// PAH-06A's own modal_stack integration (GV2LayeredUiReconciler.cpp) could not exercise
// this branch through the REAL WBP_GameShell asset, since its authored layer hosts are
// all genuinely multi-child panels (Overlay/CanvasPanel-family) -- so this test, at the
// primitive that actually owns the guarantee, was its only exercise at the time. PAH-06B
// later found a production-reachable case after all: a misconfigured Shell whose host for
// some layer IS single-child (GBF-01, "GV2.UI.LayeredReconciliationContract") -- this test
// remains the more direct, minimal one.
bool FGV2KeyedCollectionReconcilePreparedRestoresOnSwapFailure::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UBorder* FailingContainer = NewObject<UBorder>(TestWorld);
    UGV2ButtonWidgetBase* ExistingWidget = NewObject<UGV2ButtonWidgetBase>(TestWorld);
    TestNotNull(TEXT("PAH-06A: pre-existing widget attaches to the single-child container"),
        FailingContainer->AddChild(ExistingWidget));
    TestEqual(TEXT("PAH-06A: container starts with exactly 1 child"), FailingContainer->GetChildrenCount(), 1);

    TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> WidgetsByKey;
    WidgetsByKey.Add(TEXT("existing"), ExistingWidget);

    UGV2ButtonWidgetBase* NewWidget = NewObject<UGV2ButtonWidgetBase>(TestWorld);
    const TArray<FName> DesiredKeys = { TEXT("existing"), TEXT("new") };

    struct FGV2TestNoopPrepared
    {
    };

    TArray<UGV2ButtonWidgetBase*> OrderedOut;
    TArray<UGV2ButtonWidgetBase*> PreviousOrderedOut;
    const bool bReconciled = FGV2KeyedCollection::ReconcilePrepared<UGV2ButtonWidgetBase, FName, FGV2TestNoopPrepared>(
        FailingContainer,
        DesiredKeys,
        WidgetsByKey,
        [](const FName& Key) { return Key; },
        [&]() -> UGV2ButtonWidgetBase* { return NewWidget; },
        [](UGV2ButtonWidgetBase&, const FName&, FGV2TestNoopPrepared&) { return true; },
        [](UGV2ButtonWidgetBase&, const FGV2TestNoopPrepared&) {},
        OrderedOut,
        nullptr,
        &PreviousOrderedOut);

    TestFalse(TEXT("PAH-06A: reconciling a 2nd widget into a single-child container fails (engine-native AddChild rejection, not a mock)"), bReconciled);
    TestEqual(TEXT("PAH-06A: container is restored to exactly its prior 1 child"), FailingContainer->GetChildrenCount(), 1);
    if (FailingContainer->GetChildrenCount() == 1)
    {
        TestEqual(TEXT("PAH-06A: the restored child is the ORIGINAL widget, not a partially-applied one"),
            FailingContainer->GetChildAt(0), Cast<UWidget>(ExistingWidget));
    }
    TestEqual(TEXT("PAH-06A: the widget that failed to attach was never parented anywhere"),
        NewWidget->GetParent(), static_cast<UPanelWidget*>(nullptr));

    TestEqual(TEXT("PAH-06A: OutPreviousOrderedWidgets captured the container's exact prior order"), PreviousOrderedOut.Num(), 1);
    if (PreviousOrderedOut.Num() == 1)
    {
        TestEqual(TEXT("PAH-06A: OutPreviousOrderedWidgets[0] is the original widget"), PreviousOrderedOut[0], ExistingWidget);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2NonModalLayerReorderAndReplaceContract,
    "GV2.Runtime.UI.NonModalLayerReorderAndReplaceContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06B (ADR-0042, INV-P3): PAH-06A proved the shared primitive fits modal_stack alone;
// this proves the generalization to the other five layers actually landed, using
// overlay_stack (two simultaneous instances are a legitimate, real use of that layer,
// unlike the single-instance-by-convention layers). Before PAH-06B, AttachScreenToLayer's
// per-widget loop could not reorder two already-attached reused widgets at all (no-ops
// once a widget's parent already equals the host), and replacing one of two screens could
// leave the untouched sibling in the wrong physical position (the replacement widget is
// freshly attached and appended, not inserted where the old one was). Both read the ACTUAL
// panel child order (Shell::GetScreensInLayer), not a returned bool.
bool FGV2NonModalLayerReorderAndReplaceContract::RunTest(const FString& Parameters)
{
    // PSC-10B: reconciliation requires the session snapshot -- central style is prepared
    // from it, and no widget resolves a Theme of its own any more.
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr,
            TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-06B: Game shell instantiated"), Shell);
        GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
        if (Shell != nullptr)
        {

            FGV2LayeredUiReconciler Reconciler;
            auto MockFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            };
            auto MakeOverlayInstance = [](FName InstanceKey, const FString& ScreenId) -> FGV2ScreenInstanceViewModel
            {
                FGV2ScreenInstanceViewModel Inst;
                Inst.Layer = UGV2GameShellWidgetBase::LayerOverlayStack;
                Inst.InstanceKey = InstanceKey;
                Inst.ScreenId = ScreenId;
                return Inst;
            };

            FString ReconcileError;

            // [A, B] -> [B, A]: both reused, only order changes.
            FGV2UiDocumentViewModel Doc1;
            Doc1.UiInstanceId = TEXT("ui@pah06b");
            Doc1.Revision = 1;
            Doc1.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            Doc1.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile [A,B] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError, *PrepareContext));

            UGV2ScreenWidgetBase* WidgetA = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a"));
            UGV2ScreenWidgetBase* WidgetB = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b"));
            TestNotNull(TEXT("PAH-06B: A created"), WidgetA);
            TestNotNull(TEXT("PAH-06B: B created"), WidgetB);

            TArray<UUserWidget*> Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: [A,B] has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: [A,B] child 0 is A"), Order[0], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06B: [A,B] child 1 is B"), Order[1], Cast<UUserWidget>(WidgetB));
            }

            FGV2UiDocumentViewModel Doc2;
            Doc2.UiInstanceId = TEXT("ui@pah06b");
            Doc2.Revision = 2;
            Doc2.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe")));
            Doc2.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile [B,A] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError, *PrepareContext));

            TestEqual(TEXT("PAH-06B: A is the same reused instance after reorder"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a")), WidgetA);
            TestEqual(TEXT("PAH-06B: B is the same reused instance after reorder"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b")), WidgetB);

            Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: [B,A] has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: [B,A] child 0 is B (reused, moved)"), Order[0], Cast<UUserWidget>(WidgetB));
                TestEqual(TEXT("PAH-06B: [B,A] child 1 is A (reused, moved)"), Order[1], Cast<UUserWidget>(WidgetA));
            }

            // Replace B (in the 2nd slot) with a new widget B2 -- A's position must not move.
            FGV2UiDocumentViewModel Doc3;
            Doc3.UiInstanceId = TEXT("ui@pah06b");
            Doc3.Revision = 3;
            Doc3.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe_v2")));
            Doc3.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile replace-B succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError, *PrepareContext));

            UGV2ScreenWidgetBase* WidgetB2 = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b"));
            TestNotNull(TEXT("PAH-06B: B2 was created"), WidgetB2);
            TestNotEqual(TEXT("PAH-06B: B2 is a different widget instance than B"), WidgetB2, WidgetB);
            TestEqual(TEXT("PAH-06B: A is still the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a")), WidgetA);

            Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: replace-B has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: replaced B2 keeps B's slot (position 0)"), Order[0], Cast<UUserWidget>(WidgetB2));
                TestEqual(TEXT("PAH-06B: untouched A keeps its position (position 1)"), Order[1], Cast<UUserWidget>(WidgetA));
            }
            TestFalse(TEXT("PAH-06B: old B is no longer a child of overlay_stack"), Order.Contains(Cast<UUserWidget>(WidgetB)));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HostLocalLayerParticipantContract,
    "GV2.Runtime.UI.HostLocalLayerParticipantContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PEP-06 (ADR-0042): the panel's content stops being exactly the document and becomes
// document ∪ a registry of non-document participants, in two tiers -- document below,
// host-local above. This proves the mechanism with a synthetic host-local participant (its
// own Done requirement: a registry given zero elements by this task would be a dead
// mechanism, not a placeholder for PEP-06B's real popover). Fail-closed is unchanged: a
// child named by neither enumerator is still dropped, not preserved.
bool FGV2HostLocalLayerParticipantContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }
    UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    if (GameShellClass == nullptr)
    {
        GameShellClass = UGV2GameShellWidgetBase::StaticClass();
    }
    UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
    TestNotNull(TEXT("PEP-06: Game shell instantiated"), Shell);
    GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
    if (Shell == nullptr)
    {
        return false;
    }

    FGV2LayeredUiReconciler Reconciler;
    auto MockFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
    {
        return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    };

    // 1. One document instance in overlay_stack, reconciled normally.
    FGV2UiDocumentViewModel Doc;
    Doc.UiInstanceId = TEXT("ui@pep06");
    Doc.Revision = 1;
    FGV2ScreenInstanceViewModel DocInstance;
    DocInstance.Layer = UGV2GameShellWidgetBase::LayerOverlayStack;
    DocInstance.InstanceKey = TEXT("doc_overlay");
    DocInstance.ScreenId = TEXT("core:screen.overlay_probe");
    Doc.Overlays.Add(DocInstance);
    FString ReconcileError;
    TestTrue(*FString::Printf(TEXT("PEP-06: initial document reconcile succeeds [Error: %s]"), *ReconcileError),
        Reconciler.Reconcile(Shell, Doc, MockFactory, ReconcileError, *PrepareContext));
    UGV2ScreenWidgetBase* DocWidget = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("doc_overlay"));
    TestNotNull(TEXT("PEP-06: document widget created"), DocWidget);

    // 2. Attach a synthetic host-local participant -- this task's own producer for the
    // registry, per the plan-index rule that an introduced set must have one.
    UGV2ScreenWidgetBase* HostLocalWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("PEP-06: synthetic host-local widget created"), HostLocalWidget);
    FName HostLocalKey;
    FString AttachError;
    TestTrue(*FString::Printf(TEXT("PEP-06: AttachHostLocalScreen succeeds [Error: %s]"), *AttachError),
        Reconciler.AttachHostLocalScreen(Shell, UGV2GameShellWidgetBase::LayerOverlayStack, HostLocalWidget, HostLocalKey, AttachError));
    TestTrue(TEXT("PEP-06: issued key uses the reserved host_local: prefix"),
        FGV2LayeredUiReconciler::IsHostLocalInstanceKey(HostLocalKey));

    TArray<UUserWidget*> Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
    TestEqual(TEXT("PEP-06: overlay_stack has both tiers after attach"), Order.Num(), 2);
    if (Order.Num() == 2)
    {
        TestEqual(TEXT("PEP-06: document tier stays below"), Order[0], Cast<UUserWidget>(DocWidget));
        TestEqual(TEXT("PEP-06: host-local tier sits above"), Order[1], Cast<UUserWidget>(HostLocalWidget));
    }

    // 3. An ORDINARY document commit (a different revision, same overlay) must not drop the
    // host-local participant -- proving CommitReconcile's own step 2 unions both
    // enumerators, not just the standalone Attach/Detach calls.
    FGV2UiDocumentViewModel Doc2;
    Doc2.UiInstanceId = TEXT("ui@pep06");
    Doc2.Revision = 2;
    Doc2.Overlays.Add(DocInstance);
    TestTrue(*FString::Printf(TEXT("PEP-06: second document reconcile succeeds [Error: %s]"), *ReconcileError),
        Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError, *PrepareContext));

    Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
    TestEqual(TEXT("PEP-06: host-local participant survives an ordinary document commit"), Order.Num(), 2);
    if (Order.Num() == 2)
    {
        TestEqual(TEXT("PEP-06: document tier still below after the second commit"), Order[0], Cast<UUserWidget>(DocWidget));
        TestEqual(TEXT("PEP-06: host-local tier still above after the second commit"), Order[1], Cast<UUserWidget>(HostLocalWidget));
    }

    // 4. Fail-closed is unchanged: a child named by NEITHER enumerator (spliced directly
    // into the panel, bypassing both AttachHostLocalScreen and the document) is dropped by
    // the very next reconcile, not preserved. This is the mutation the plan's own Done
    // criterion names: "ребёнок, не названный ни документом, ни реестром, отвергается".
    UPanelWidget* OverlayHost = Shell->GetHostForLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
    TestNotNull(TEXT("PEP-06: overlay_stack host resolves"), OverlayHost);
    UGV2ScreenWidgetBase* UnnamedWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    if (OverlayHost != nullptr && UnnamedWidget != nullptr)
    {
        OverlayHost->AddChild(UnnamedWidget);
        TestEqual(TEXT("PEP-06: unnamed child briefly present before the next reconcile"),
            Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack).Num(), 3);

        FGV2UiDocumentViewModel Doc3;
        Doc3.UiInstanceId = TEXT("ui@pep06");
        Doc3.Revision = 3;
        Doc3.Overlays.Add(DocInstance);
        TestTrue(*FString::Printf(TEXT("PEP-06: third document reconcile succeeds [Error: %s]"), *ReconcileError),
            Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError, *PrepareContext));

        Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
        TestEqual(TEXT("PEP-06: unnamed child is dropped, not preserved"), Order.Num(), 2);
        TestFalse(TEXT("PEP-06: unnamed child specifically is gone"), Order.Contains(Cast<UUserWidget>(UnnamedWidget)));
        TestTrue(TEXT("PEP-06: both real tiers remain"),
            Order.Contains(Cast<UUserWidget>(DocWidget)) && Order.Contains(Cast<UUserWidget>(HostLocalWidget)));
    }

    // 5. DetachHostLocalScreen removes exactly the host-local participant, leaving the
    // document tier untouched.
    FString DetachError;
    TestTrue(*FString::Printf(TEXT("PEP-06: DetachHostLocalScreen succeeds [Error: %s]"), *DetachError),
        Reconciler.DetachHostLocalScreen(Shell, UGV2GameShellWidgetBase::LayerOverlayStack, HostLocalKey, DetachError));
    Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
    TestEqual(TEXT("PEP-06: only the document tier remains after detach"), Order.Num(), 1);
    if (Order.Num() == 1)
    {
        TestEqual(TEXT("PEP-06: remaining child is the document widget"), Order[0], Cast<UUserWidget>(DocWidget));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverOverlayAnchorAndLifecycleContract,
    "GV2.UI.LayeredReconciliation.HoverOverlayAnchorAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PEP-06B: the hover popover's OWN Done criteria, exercised against the mechanisms PEP-06B
// built on top of PEP-06's host-local registry -- IGV2ScreenAnchorHost positioning, the
// generic viewport-refresh recursion reaching a host-local participant, non-blocking of
// lower layers via SelfHitTestInvisible, and session-replacement not surviving a Reset().
bool FGV2HoverOverlayAnchorAndLifecycleContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }
    UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    if (GameShellClass == nullptr)
    {
        GameShellClass = UGV2GameShellWidgetBase::StaticClass();
    }
    UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
    TestNotNull(TEXT("PEP-06B: game shell instantiated"), Shell);
    GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
    if (Shell == nullptr)
    {
        return false;
    }

    // An anchor-authored hover screen: a UCanvasPanel root with exactly one child (a
    // RichText widget, doubling as the viewport-scale probe below) -- the shape
    // UGV2ScreenWidgetBase::SetAnchoredContentPosition requires to be anything but a no-op.
    UGV2ScreenWidgetBase* HoverScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("PEP-06B: hover screen instantiates"), HoverScreen);
    if (HoverScreen == nullptr)
    {
        return false;
    }
    HoverScreen->WidgetTree = NewObject<UWidgetTree>(HoverScreen);
    UCanvasPanel* RootCanvas = HoverScreen->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    HoverScreen->WidgetTree->RootWidget = RootCanvas;
    UGV2RichTextBoundTestWidget* ContentText = HoverScreen->WidgetTree->ConstructWidget<UGV2RichTextBoundTestWidget>(
        UGV2RichTextBoundTestWidget::StaticClass(), TEXT("ContentText"));
    ContentText->BuildBoundSubWidgets();
    UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(ContentText));
    TestNotNull(TEXT("PEP-06B: hover content slot is a canvas slot"), ContentSlot);

    UGV2UiTheme* Theme = GV2PresentationTestFixtures::LoadConfiguredThemeForTest();
    TestNotNull(TEXT("PEP-06B: theme is available"), Theme);
    if (Theme != nullptr)
    {
        GV2PresentationApply::FPreparedRichTextStyle Style =
            GV2PresentationTestFixtures::MakePreparedRichTextStyleForTest(*Theme);
        // The shared fixture's default token carries UnscaledFontSize=0 (unscaled by
        // design, for callers that don't care) -- ScalePreparedTokenStyleAtHeight only
        // scales a token whose UnscaledFontSize is positive, so this probe needs its own
        // nonzero size to actually exercise the height-dependent branch below.
        Style.DefaultToken.UnscaledFontSize = 32.0f;
        ContentText->ApplyRichTextStyleValues(Style);
    }

    // 1. Attach as a host-local overlay_stack participant (PEP-06's own mechanism).
    FName InstanceKey;
    FString AttachError;
    FGV2LayeredUiReconciler Reconciler;
    TestTrue(*FString::Printf(TEXT("PEP-06B: AttachHostLocalScreen succeeds [Error: %s]"), *AttachError),
        Reconciler.AttachHostLocalScreen(Shell, UGV2GameShellWidgetBase::LayerOverlayStack, HoverScreen, InstanceKey, AttachError));

    // 2. Non-blocking of lower layers: AttachHostLocalScreen must have made the participant
    // itself self-hit-test-invisible -- the Fill/Fill root slot (uniform for every
    // participant, ApplyScreenSlotLayout) covers the whole layer, so this flag is the only
    // thing standing between "fills the layer" and "blocks everything under it".
    TestEqual(TEXT("PEP-06B: hover screen is self-hit-test-invisible (does not block lower layers)"),
        HoverScreen->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

    // PEP-AF-03: the stated property is that a lower layer stays REACHABLE while the
    // full-layer hover participant is open. Proven by a real Slate hit test over real
    // arranged geometry -- LocateWindowUnderMouse walks the actual widget tree and honours
    // each widget's actual visibility -- asserting the resulting path reaches the lower
    // control. Reading ESlateVisibility off the participant (the assertion above) only
    // states an intent about one widget; it cannot see that the participant's own root
    // panel, a separate UWidget defaulting to Visible, would swallow the hit.
    //
    // Deliberately NOT FSlateApplication::RoutePointerDownEvent/RoutePointerUpEvent on this
    // path: SVirtualWindow is not registered with FSlateApplication, so application-level
    // click routing (capture, press/release pairing that SButton::OnClicked needs) has no
    // supported meaning here, and the suite has no precedent for it -- every other
    // SVirtualWindow use in these tests is geometry-only. Hit-test reachability is the
    // property this task owns; click delivery is Slate's own, already-tested behaviour.
    const TSharedRef<SButton> LowerLayerButton = SNew(SButton);
    const TSharedRef<SOverlay> LayerStack = SNew(SOverlay)
        + SOverlay::Slot()[LowerLayerButton]
        + SOverlay::Slot()[HoverScreen->TakeWidget()];
    const FVector2D InteractionViewportSize(800.0f, 600.0f);
    const TSharedRef<SVirtualWindow> InteractionWindow = SNew(SVirtualWindow).Size(InteractionViewportSize);
    InteractionWindow->SetContent(LayerStack);
    GV2PresentationTestFixtures::GV2SimulateResponsiveFrame(InteractionWindow, InteractionViewportSize);

    const FVector2D ProbePosition(700.0f, 500.0f);
    const TArray<TSharedRef<SWindow>> InteractionWindows{InteractionWindow};
    const FWidgetPath HitPath = FSlateApplication::Get().LocateWindowUnderMouse(
        ProbePosition,
        InteractionWindows,
        false);
    TestTrue(TEXT("PEP-06B: a real Slate hit test under the open hover window yields a path"),
        HitPath.IsValid());
    TestTrue(
        TEXT("PEP-06B Done: the hit path reaches the lower-layer control through the open hover window"),
        HitPath.ContainsWidget(&LowerLayerButton.Get()));

    // 3. Anchor positioning: no cursor anywhere in this test -- a direct call, read back
    // from the real canvas slot, called twice with different points to prove the position
    // reproduces (is a pure function of the input) rather than sticking from the first call.
    IGV2ScreenAnchorHost* AnchorHost = Cast<IGV2ScreenAnchorHost>(HoverScreen);
    TestNotNull(TEXT("PEP-06B: hover screen implements IGV2ScreenAnchorHost"), AnchorHost);
    if (AnchorHost != nullptr)
    {
        AnchorHost->SetAnchoredContentPosition(FVector2D(10.0f, 20.0f));
        TestEqual(TEXT("PEP-06B: first anchor position applies to the real canvas slot"),
            ContentSlot->GetPosition(), FVector2D(10.0f, 20.0f));

        AnchorHost->SetAnchoredContentPosition(FVector2D(123.0f, 45.0f));
        TestEqual(TEXT("PEP-06B: a second, different anchor position reproduces (not stuck on the first)"),
            ContentSlot->GetPosition(), FVector2D(123.0f, 45.0f));
    }

    // 4. Viewport scaling: RefreshViewportPresentation's own generic walk (PEP-06, extended
    // for HostLocalScreens) must reach the hover screen's nested RichText the same way it
    // reaches an ordinary document screen -- proven at two heights other than the
    // reference, by reading back the actually-applied font size, not by re-deriving it.
    if (Theme != nullptr)
    {
        const float ReferenceHeight = Theme->ReferenceViewportHeight;
        const float HeightA = ReferenceHeight * 0.5f;
        const float HeightB = ReferenceHeight * 1.5f;
        TestNotEqual(TEXT("PEP-06B: the two probe heights actually differ from the reference"), HeightA, HeightB);

        FString RefreshErrorA;
        TestTrue(*FString::Printf(TEXT("PEP-06B: viewport refresh at height A succeeds [Error: %s]"), *RefreshErrorA),
            Reconciler.RefreshViewportPresentation(HeightA, RefreshErrorA));
        const float FontSizeA = ContentText->GetRichTextBlock()->GetCurrentDefaultTextStyle().Font.Size;

        FString RefreshErrorB;
        TestTrue(*FString::Printf(TEXT("PEP-06B: viewport refresh at height B succeeds [Error: %s]"), *RefreshErrorB),
            Reconciler.RefreshViewportPresentation(HeightB, RefreshErrorB));
        const float FontSizeB = ContentText->GetRichTextBlock()->GetCurrentDefaultTextStyle().Font.Size;

        TestNotEqual(TEXT("PEP-06B: the hover screen's own text actually rescaled between the two heights"),
            FontSizeA, FontSizeB);
    }

    // 5. Session replacement: Reset() is what a session boundary calls (UGV2RuntimeSubsystem::
    // TeardownActiveProjection, alongside discarding the Shell itself) -- its own contract is
    // clearing the HostLocalScreens registry, not clearing an about-to-be-discarded panel in
    // place. Proven by the registry actually forgetting the key: a Detach against it after
    // Reset() must fail, because Reset() left nothing there to detach.
    Reconciler.Reset();
    FString StaleDetachError;
    TestFalse(TEXT("PEP-06B: the previous session's hover overlay key is gone from the registry after Reset()"),
        Reconciler.DetachHostLocalScreen(Shell, UGV2GameShellWidgetBase::LayerOverlayStack, InstanceKey, StaleDetachError));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenAnchorHostViewportClampContract,
    "GV2.UI.LayeredReconciliation.ScreenAnchorHostViewportClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PEP-06C: the Slate tooltip window this content used to live in (before PEP-06B) kept it
// inside the viewport for free; moving it into an ordinary screen lost that silently. This
// proves UGV2ScreenWidgetBase::SetAnchoredContentPosition's own clamp restores it, by
// requesting a position beyond each of the four edges in turn against REAL post-paint
// geometry (SVirtualWindow, the same idiom GV2RichTextSpanHoverDetectorTests uses) and
// reading back the actually-applied canvas position, not re-deriving the expected clamp.
bool FGV2ScreenAnchorHostViewportClampContract::RunTest(const FString& Parameters)
{
    UGV2ScreenWidgetBase* HoverScreen = NewObject<UGV2ScreenWidgetBase>();
    TestNotNull(TEXT("PEP-06C: hover screen instantiates"), HoverScreen);
    if (HoverScreen == nullptr)
    {
        return false;
    }
    HoverScreen->WidgetTree = NewObject<UWidgetTree>(HoverScreen);
    UCanvasPanel* RootCanvas = HoverScreen->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    HoverScreen->WidgetTree->RootWidget = RootCanvas;
    USizeBox* Content = HoverScreen->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Content"));
    Content->SetWidthOverride(100.0f);
    Content->SetHeightOverride(60.0f);
    UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(Content));
    TestNotNull(TEXT("PEP-06C: hover content slot is a canvas slot"), ContentSlot);
    if (ContentSlot != nullptr)
    {
        // A default UCanvasPanelSlot's own Size (160x30) governs layout regardless of the
        // SizeBox's WidthOverride/HeightOverride unless the slot is told to size itself to
        // its content -- without this, ContentSize below would read the slot default, not
        // the 100x60 this test actually wants to clamp against.
        ContentSlot->SetAutoSize(true);
    }

    const FVector2D ViewportSize(800.0f, 600.0f);
    TSharedPtr<SWidget> SlateWidget = HoverScreen->TakeWidget();
    TestTrue(TEXT("PEP-06C: hover screen produces a valid Slate widget"), SlateWidget.IsValid());
    if (!SlateWidget.IsValid())
    {
        return false;
    }
    TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(ViewportSize);
    VirtualWindow->SetContent(SlateWidget.ToSharedRef());
    GV2PresentationTestFixtures::GV2SimulateResponsiveFrame(VirtualWindow, ViewportSize);

    IGV2ScreenAnchorHost* AnchorHost = Cast<IGV2ScreenAnchorHost>(HoverScreen);
    TestNotNull(TEXT("PEP-06C: hover screen implements IGV2ScreenAnchorHost"), AnchorHost);
    if (AnchorHost == nullptr)
    {
        return false;
    }

    const FVector2D MaxPosition = ViewportSize - FVector2D(100.0f, 60.0f);

    // Left/top edge: a point far off the top-left must clamp to (0, 0), not sit negative.
    AnchorHost->SetAnchoredContentPosition(FVector2D(-500.0f, -500.0f));
    TestEqual(TEXT("PEP-06C: a top-left-of-viewport anchor clamps to the origin"),
        ContentSlot->GetPosition(), FVector2D(0.0f, 0.0f));

    // Right/bottom edge: a point far past the bottom-right must clamp so the content's own
    // far edge lands exactly on the viewport's own far edge, not run off it.
    AnchorHost->SetAnchoredContentPosition(FVector2D(5000.0f, 5000.0f));
    TestEqual(TEXT("PEP-06C: a bottom-right-of-viewport anchor clamps so the content stays fully visible"),
        ContentSlot->GetPosition(), MaxPosition);

    // Left edge only (Y well inside bounds): only X clamps.
    AnchorHost->SetAnchoredContentPosition(FVector2D(-10.0f, 200.0f));
    TestEqual(TEXT("PEP-06C: a left-of-viewport anchor clamps only its X"),
        ContentSlot->GetPosition(), FVector2D(0.0f, 200.0f));

    // Top edge only (X well inside bounds): only Y clamps.
    AnchorHost->SetAnchoredContentPosition(FVector2D(200.0f, -10.0f));
    TestEqual(TEXT("PEP-06C: a top-of-viewport anchor clamps only its Y"),
        ContentSlot->GetPosition(), FVector2D(200.0f, 0.0f));

    // A point already fully inside bounds passes through unclamped -- the clamp is a
    // boundary correction, not a repositioning of every anchor.
    AnchorHost->SetAnchoredContentPosition(FVector2D(300.0f, 250.0f));
    TestEqual(TEXT("PEP-06C: a fully-in-bounds anchor is not altered"),
        ContentSlot->GetPosition(), FVector2D(300.0f, 250.0f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverWindowSelectableFromDataContract,
    "GV2.UI.LayeredReconciliation.HoverWindowSelectableFromData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PEP-06C: the project owner's own requirement -- a new hover window appearance needs no
// C++ -- proven by measuring two REAL WBP fixtures (WBP_HoverFixtureAlpha/Beta, both plain
// UGV2ScreenWidgetBase Blueprints authored entirely in Designer: a different frame color and
// a different fixed content size each) and reading back their actually-rendered sizes, not
// asserting the claim from the fact that both merely compile.
bool FGV2HoverWindowSelectableFromDataContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        return false;
    }

    UClass* AlphaClass = LoadClass<UGV2ScreenWidgetBase>(
        nullptr, TEXT("/Game/UI/Widgets/WBP_HoverFixtureAlpha.WBP_HoverFixtureAlpha_C"));
    UClass* BetaClass = LoadClass<UGV2ScreenWidgetBase>(
        nullptr, TEXT("/Game/UI/Widgets/WBP_HoverFixtureBeta.WBP_HoverFixtureBeta_C"));
    TestNotNull(TEXT("PEP-06C: hover fixture Alpha's generated class loads"), AlphaClass);
    TestNotNull(TEXT("PEP-06C: hover fixture Beta's generated class loads"), BetaClass);
    if (AlphaClass == nullptr || BetaClass == nullptr)
    {
        return false;
    }

    auto MeasureFixture = [TestWorld](UClass* Class) -> FVector2D
    {
        UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, Class);
        UWidget* Content = Screen != nullptr ? Screen->GetWidgetFromName(TEXT("ContentSize")) : nullptr;
        if (Screen == nullptr || Content == nullptr)
        {
            return FVector2D::ZeroVector;
        }
        TSharedPtr<SWidget> SlateWidget = Screen->TakeWidget();
        if (!SlateWidget.IsValid())
        {
            return FVector2D::ZeroVector;
        }
        const FVector2D ViewportSize(800.0f, 600.0f);
        TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(ViewportSize);
        VirtualWindow->SetContent(SlateWidget.ToSharedRef());
        GV2PresentationTestFixtures::GV2SimulateResponsiveFrame(VirtualWindow, ViewportSize);
        return Content->GetCachedGeometry().GetLocalSize();
    };

    const FVector2D AlphaSize = MeasureFixture(AlphaClass);
    const FVector2D BetaSize = MeasureFixture(BetaClass);

    TestEqual(TEXT("PEP-06C: hover fixture Alpha measures its own authored dimensions"),
        AlphaSize, FVector2D(120.0f, 80.0f));
    TestEqual(TEXT("PEP-06C: hover fixture Beta measures its own authored dimensions"),
        BetaSize, FVector2D(220.0f, 140.0f));
    TestNotEqual(TEXT("PEP-06C: two differently screen_id-named windows measure different sizes -- the appearance is selected from data, not fixed in C++"),
        AlphaSize, BetaSize);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PresentationAuthorityPhaseContract,
    "GV2.Runtime.UI.PresentationAuthorityPhaseContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-08 (ADR-0042, INV-P5): observational half of the two-part gate. The structural
// half (Tools/Testing/validate_presentation_authority_phase.py) classifies every
// authority access by phase and walks call chains out of every application-phase root;
// it reads source. This one observes the property directly: each authority bumps a
// counter (GV2PresentationAuthorityProbe.h), and the test brackets the two public phase
// functions and compares deltas. A source scan can be defeated by indirection
// (Commit() -> helper() -> service() -> lookup); a counter cannot.
//
// The Prepare > 0 half is not decoration. Without it the Commit == 0 assertion is
// satisfied just as well by a fixture that resolves nothing at all, which is a check
// that passes because it checks nothing -- the exact shape that made an earlier
// milestone's "every schema-required property arrived" assertion vacuous for a schema
// whose fields are all optional.
//
// Measurement point is CommitReconcile, never the enclosing Reconcile: since PAH-07,
// Reconcile answers a failed compensating rollback by replaying PrepareReconcile against
// the last committed document. That nested preparation is legitimate, and the third
// scenario below measures it rather than asserting it away -- a caveat that is proven is
// a caveat; one that is only written down is an excuse waiting to be used.
bool FGV2PresentationAuthorityPhaseContract::RunTest(const FString& Parameters)
{
    // PSC-10B: reconciliation requires the session snapshot -- central style is prepared
    // from it, and no widget resolves a Theme of its own any more.
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr, TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-08: Game shell instantiated"), Shell);
        GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);

        // The factory is registry-backed on purpose: GV2LayeredUiReconciler.h documents
        // FScreenFactory as "an implementation backed by UGV2ScreenRegistry::Resolve", so
        // this is the production shape, not an authority call invented for the test.
        UGV2ScreenRegistry* Registry = LoadConfiguredRegistryForTest();
        TestNotNull(TEXT("PAH-08: a configured Screen Registry is available"), Registry);
        FGV2ResolvedScreenRegistry ResolvedRegistry;
        FString CompileError;
        const bool bCompiled = Registry != nullptr
            && Registry->CompileResolvedRegistry(GV2PackageClosure::DiscoverFromGameData(), ResolvedRegistry, CompileError);
        TestTrue(TEXT("PAH-08: configured Screen Registry compiles"), bCompiled);

        UGV2ScreenWidgetBase* Fixture = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

        if (Shell != nullptr && Registry != nullptr && Fixture != nullptr)
        {

            auto Factory = [&](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
            {
                FGV2ResolvedScreenDescriptor Descriptor;
                FGV2ScreenResolutionRejection Rejection;
                // Result deliberately unused for widget selection: the fixture screen is
                // returned either way. What matters here is that the production factory
                // shape consults the authority, and that it does so during preparation.
                (void)ResolvedRegistry.Resolve(ScreenId, FGV2ScreenPlacement::TopLevel(Layer), Descriptor, Rejection);
                return Fixture;
            };

            FGV2UiDocumentViewModel Doc;
            Doc.UiInstanceId = TEXT("ui@1:1");
            Doc.Revision = 1;
            Doc.bHasRoute = true;
            Doc.Route.Layer = TEXT("location_content");
            Doc.Route.InstanceKey = TEXT("main");
            Doc.Route.ScreenId = TEXT("core:screen.pah08_probe");

            FGV2LayeredUiReconciler Reconciler;
            FGV2LayeredUiReconciler::FPreparedReconciliationPlan Plan;
            FString Error;

            const uint64 BeforePrepare = GV2PresentationAuthorityProbe::GetResolveCount();
            const bool bPrepared = Reconciler.PrepareReconcile(Shell, Doc, Factory, Plan, Error, *PrepareContext);
            const uint64 AfterPrepare = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: candidate prepares [Error: %s]"), *Error), bPrepared);
            TestTrue(
                *FString::Printf(
                    TEXT("PAH-08: preparation resolves at least one authority (delta %llu) -- without this the "
                         "commit assertion below would pass on a fixture that resolves nothing"),
                    static_cast<unsigned long long>(AfterPrepare - BeforePrepare)),
                AfterPrepare > BeforePrepare);

            const uint64 BeforeCommit = GV2PresentationAuthorityProbe::GetResolveCount();
            const bool bCommitted = Reconciler.CommitReconcile(Shell, Plan, Error);
            const uint64 AfterCommit = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: candidate commits [Error: %s]"), *Error), bCommitted);
            TestEqual(
                TEXT("PAH-08: application resolves no authority -- the prepared plan already carries what it needs"),
                static_cast<int64>(AfterCommit - BeforeCommit),
                static_cast<int64>(0));

            // Third scenario: the caveat, measured. A full Reconcile of a second revision
            // prepares once, so its delta is strictly positive -- which is exactly why the
            // invariant is asserted around CommitReconcile and not around Reconcile. If a
            // later change moved the bracket outward, this assertion is what shows the
            // measurement point is load-bearing rather than incidental.
            // A *different* screen id, not just a later revision: preparation consults the
            // factory only when a screen instance is new. Reusing the same screen resolves
            // nothing at all -- which is itself the invariant working, and which made the
            // first draft of this assertion fail. Recorded rather than quietly patched.
            FGV2UiDocumentViewModel NextDoc = Doc;
            NextDoc.Revision = 2;
            NextDoc.Route.ScreenId = TEXT("core:screen.pah08_probe_second");
            const uint64 BeforeWhole = GV2PresentationAuthorityProbe::GetResolveCount();
            FString WholeError;
            const bool bWhole = Reconciler.Reconcile(Shell, NextDoc, Factory, WholeError, *PrepareContext);
            const uint64 AfterWhole = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: whole reconcile succeeds [Error: %s]"), *WholeError), bWhole);
            TestTrue(
                TEXT("PAH-08: Reconcile as a whole DOES resolve (it contains preparation), so it is the wrong "
                     "bracket for the invariant -- CommitReconcile is"),
                AfterWhole > BeforeWhole);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PresentationCatastrophicRecoveryContract,
    "GV2.Runtime.UI.PresentationCatastrophicRecoveryContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-07 (ADR-0042, INV-P4): presentation has exactly one committed logical state; the
// physical UMG tree is its recoverable projection. Proves the two DIFFERENT observable
// outcomes of a Commit failure: (a) the compensating rollback (GBH-10, ADR-0041) succeeds
// -- FGV2LayeredUiReconciler::GetHealth() stays Nominal, the previous revision is intact
// in place, exactly like every pre-PAH-07 rollback test already covered; (b) the rollback
// itself ALSO fails -- GetHealth() becomes RecoveredFromCatastrophicFailure, and the
// physical tree (both the failing layer AND an untouched sibling layer, to prove the
// canonical part actually suffices INCLUDING composition and order there) is discarded
// and rebuilt from the last successfully committed document, not left in an unverified
// state. Both are forced via ScreenCommitFailureInjector/ScreenRollbackFailureInjector --
// the same production-path injectors PAH-01/GBH-10/GBF-05 already established, not a
// synthetic health flag flipped by hand.
bool FGV2PresentationCatastrophicRecoveryContract::RunTest(const FString& Parameters)
{
    // PSC-10B: reconciliation requires the session snapshot -- central style is prepared
    // from it, and no widget resolves a Theme of its own any more.
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr,
            TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-07: Game shell instantiated"), Shell);
        GV2PresentationTestFixtures::TScopedRootObject<UGV2GameShellWidgetBase> ScopedShell(Shell);
        if (Shell != nullptr)
        {

            // Route widget at location_content -- a plain, field-less screen. Reused by a
            // fixed-instance factory (not CreateWidget-per-call) so its C++ identity is
            // directly comparable before/after catastrophic recovery: recovery must
            // re-resolve "core:screen.route" through the SAME production ScreenFactory
            // path and land on this exact object again, proving the canonical composition
            // of an UNTOUCHED sibling layer survives, not just the layer that failed.
            UGV2ScreenWidgetBase* RouteWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            UGV2ScreenWidgetBase* ReplacementRouteWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            // A second overlay_stack instance alongside the reused/faulting one, so
            // recovery's physical order can be checked as [Reused, Extra], not just
            // composition.
            UGV2ScreenWidgetBase* ExtraOverlayWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            UGV2ScreenWidgetBase* ReusedScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            ReusedScreen->WidgetTree = NewObject<UWidgetTree>(ReusedScreen);
            UVerticalBox* ReusedScreenRoot = ReusedScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            ReusedScreen->WidgetTree->RootWidget = ReusedScreenRoot;

            // PSC-10B: a styled widget inside the recovered screen. Catastrophic recovery is
            // an ordinary fresh Prepare/Apply, so it must produce central style like any
            // other -- and since no widget pulls a Theme of its own any more, a recovery
            // that reconciled without the session snapshot would rebuild this tree
            // physically correct and entirely unstyled.
            UGV2SeparatorBoundTestWidget* RecoveredSeparator =
                ReusedScreen->WidgetTree->ConstructWidget<UGV2SeparatorBoundTestWidget>(
                    UGV2SeparatorBoundTestWidget::StaticClass(), TEXT("RecoveredSeparator"));
            RecoveredSeparator->BuildBoundSubWidgets();
            RecoveredSeparator->SetTestOrientation(Orient_Horizontal);
            ReusedScreenRoot->AddChildToVerticalBox(RecoveredSeparator);
            const float ThemeSeparatorThickness = PrepareContext->GetTheme().SeparatorThickness;
            constexpr float UnstyledSentinel = -41.5f;

            UGV2DeclaredCompositeWidgetBase* FieldA = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldA"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldA);
            FieldA->WidgetTree = NewObject<UWidgetTree>(FieldA);
            UGV2TextWidgetBase* TextA = FieldA->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextA"));
            FieldA->WidgetTree->RootWidget = TextA;
            FieldA->SetHostIdentity(FName(TEXT("field_a")));
            FieldA->DeclaredCapabilities.Add({ FName(TEXT("value_a")), FName(TEXT("TextA")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2DeclaredCompositeWidgetBase* FieldB = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldB"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldB);
            FieldB->WidgetTree = NewObject<UWidgetTree>(FieldB);
            UGV2TextWidgetBase* TextB = FieldB->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextB"));
            FieldB->WidgetTree->RootWidget = TextB;
            FieldB->SetHostIdentity(FName(TEXT("field_b")));
            FieldB->DeclaredCapabilities.Add({ FName(TEXT("value_b")), FName(TEXT("TextB")), EGV2DeclaredUiCapabilityKind::Text });

            TMap<FString, UGV2ScreenWidgetBase*> ScreensById;
            ScreensById.Add(TEXT("core:screen.pah07_route"), RouteWidget);
            ScreensById.Add(TEXT("core:screen.pah07_route_v2"), ReplacementRouteWidget);
            ScreensById.Add(TEXT("core:screen.pah07_extra"), ExtraOverlayWidget);
            ScreensById.Add(TEXT("core:screen.pah07_reused"), ReusedScreen);
            auto Factory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                UGV2ScreenWidgetBase** Found = ScreensById.Find(ScreenId);
                return Found != nullptr ? *Found : nullptr;
            };

            auto MakeFieldValue = [](const FName& FieldId, const FString& PropName, const FString& Text) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ TCHAR_TO_UTF8(*PropName), false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel Model;
                Model.Text = FText::FromString(Text);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(PropName, FGV2PreparedUiValue::MakeText(Model));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = FieldId;
                FieldValue.SchemaId = TEXT("test:schema.pah07_reused_field.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeDoc = [&](int64 Revision, const FString& RouteScreenId, const FString& TextA_Value, const FString& TextB_Value) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@pah07");
                Doc.Revision = Revision;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("main");
                Doc.Route.ScreenId = RouteScreenId;

                FGV2ScreenInstanceViewModel ReusedInst;
                ReusedInst.Layer = TEXT("overlay_stack");
                ReusedInst.InstanceKey = TEXT("reused");
                ReusedInst.ScreenId = TEXT("core:screen.pah07_reused");
                ReusedInst.Fields.Add(MakeFieldValue(FName(TEXT("field_a")), TEXT("value_a"), TextA_Value));
                ReusedInst.Fields.Add(MakeFieldValue(FName(TEXT("field_b")), TEXT("value_b"), TextB_Value));
                Doc.Overlays.Add(ReusedInst);

                FGV2ScreenInstanceViewModel ExtraInst;
                ExtraInst.Layer = TEXT("overlay_stack");
                ExtraInst.InstanceKey = TEXT("extra");
                ExtraInst.ScreenId = TEXT("core:screen.pah07_extra");
                Doc.Overlays.Add(ExtraInst);

                return Doc;
            };

            FGV2LayeredUiReconciler Reconciler;
            FString ReconcileError;

            // D1: baseline commit. This becomes LastCommittedDocument -- what catastrophic
            // recovery replays.
            TestTrue(*FString::Printf(TEXT("PAH-07: baseline D1 commits [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, MakeDoc(1, TEXT("core:screen.pah07_route"), TEXT("OldA"), TEXT("OldB")), Factory, ReconcileError, *PrepareContext));
            TestEqual(TEXT("PAH-07: baseline health is Nominal"), Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);
            TestEqual(TEXT("PAH-07: baseline route is RouteWidget"), Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestEqual(TEXT("PAH-07: baseline TextA reads OldA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("PAH-07: baseline TextB reads OldB"), TextB->GetTextContent().ToString(), TEXT("OldB"));
            TestNotEqual(TEXT("PSC-10B: the sentinel differs from the theme's own separator thickness"),
                ThemeSeparatorThickness, UnstyledSentinel);
            TestEqual(TEXT("PSC-10B: baseline commit styled the screen's separator from the snapshot Theme"),
                RecoveredSeparator->ReadAppliedThickness(), ThemeSeparatorThickness);
            {
                const TArray<UUserWidget*> BaselineOverlayOrder = Shell->GetScreensInLayer(TEXT("overlay_stack"));
                TestEqual(TEXT("PAH-07: baseline overlay_stack has 2 physical children"), BaselineOverlayOrder.Num(), 2);
                if (BaselineOverlayOrder.Num() == 2)
                {
                    TestEqual(TEXT("PAH-07: baseline overlay child 0 is ReusedScreen"), BaselineOverlayOrder[0], Cast<UUserWidget>(ReusedScreen));
                    TestEqual(TEXT("PAH-07: baseline overlay child 1 is ExtraOverlayWidget"), BaselineOverlayOrder[1], Cast<UUserWidget>(ExtraOverlayWidget));
                }
            }

            // D2 (ordinary path): field_b's Commit is injected to fail; its OWN self-heal
            // rollback is NOT injected, so it succeeds -- the pre-PAH-07 guarantee.
            // GetHealth() must stay Nominal: this Commit failure is NOT catastrophic.
            const auto OrdinaryCommitInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_b");
            };
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);
            const bool bOrdinaryFault = Reconciler.Reconcile(
                Shell, MakeDoc(2, TEXT("core:screen.pah07_route_v2"), TEXT("NewA"), TEXT("NewB")), Factory, ReconcileError, *PrepareContext, OrdinaryCommitInjector);
            TestFalse(TEXT("PAH-07: D2 (ordinary rollback) fails Reconcile"), bOrdinaryFault);
            TestFalse(TEXT("PAH-07: D2's OutError does NOT carry the rollback-failed marker"),
                ReconcileError.Contains(GGV2UiRollbackFailedDiagnosticCode));
            TestEqual(TEXT("PAH-07: health stays Nominal after an ordinary (successfully rolled back) Commit failure"),
                Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);
            TestEqual(TEXT("PAH-07: route is still RouteWidget (D2's route replacement never committed)"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestEqual(TEXT("PAH-07: TextA still restored to OldA after the ordinary rollback"), TextA->GetTextContent().ToString(), TEXT("OldA"));

            // D3 (catastrophic path): field_b's Commit is injected to fail AND field_a's
            // own self-heal rollback (value_a) is ALSO injected to fail -- the physical
            // tree's relationship to ActiveScreens is now undefined per ADR-0041, and
            // Reconcile must fall back to catastrophic recovery.
            const auto CatastrophicCommitInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_b");
            };
            const auto CatastrophicRollbackInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_a");
            };
            // Wipe the style so "still styled after recovery" cannot pass by the value
            // simply never having been touched.
            RecoveredSeparator->ApplySeparatorStyleValues(FSlateBrush(), UnstyledSentinel, /*bHorizontal=*/true);
            TestEqual(TEXT("PSC-10B: separator is on the sentinel immediately before the catastrophic round"),
                RecoveredSeparator->ReadAppliedThickness(), UnstyledSentinel);
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("GBH-10: rollback failed restoring host 'FieldA' property 'value_a'"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);
            const bool bCatastrophicFault = Reconciler.Reconcile(
                Shell, MakeDoc(3, TEXT("core:screen.pah07_route_v2"), TEXT("CatA"), TEXT("CatB")), Factory, ReconcileError, *PrepareContext,
                CatastrophicCommitInjector, CatastrophicRollbackInjector);
            TestFalse(TEXT("PAH-07: D3 (catastrophic) fails Reconcile"), bCatastrophicFault);
            TestTrue(*FString::Printf(TEXT("PAH-07: D3's OutError carries the rollback-failed marker [Error: %s]"), *ReconcileError),
                ReconcileError.Contains(GGV2UiRollbackFailedDiagnosticCode));
            TestEqual(TEXT("PAH-07: health becomes RecoveredFromCatastrophicFailure"),
                Reconciler.GetHealth(), EGV2PresentationHealth::RecoveredFromCatastrophicFailure);

            // Canonical state after recovery must match D1 (the last successfully
            // committed document), NOT D3 (the rejected candidate) -- read through the
            // SAME production accessors, not internal bookkeeping alone.
            TestEqual(TEXT("PAH-07: after recovery, route resolves back to D1's RouteWidget (same C++ identity via the fixed-instance factory)"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestTrue(TEXT("PAH-07: after recovery, RouteWidget is physically attached to location_content"),
                Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidget));
            TestFalse(TEXT("PAH-07: after recovery, the rejected D3 route replacement is NOT attached"),
                Shell->GetScreensInLayer(TEXT("location_content")).Contains(ReplacementRouteWidget));
            TestEqual(TEXT("PSC-10B: catastrophic recovery re-applied central style from the same snapshot"),
                RecoveredSeparator->ReadAppliedThickness(), ThemeSeparatorThickness);
            TestEqual(TEXT("PAH-07: after recovery, TextA reads D1's OldA, not D3's CatA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("PAH-07: after recovery, TextB reads D1's OldB, not D3's CatB"), TextB->GetTextContent().ToString(), TEXT("OldB"));
            {
                const TArray<UUserWidget*> RecoveredOverlayOrder = Shell->GetScreensInLayer(TEXT("overlay_stack"));
                TestEqual(TEXT("PAH-07: after recovery, overlay_stack has D1's 2 physical children"), RecoveredOverlayOrder.Num(), 2);
                if (RecoveredOverlayOrder.Num() == 2)
                {
                    TestEqual(TEXT("PAH-07: after recovery, overlay child 0 is ReusedScreen (D1's order preserved)"), RecoveredOverlayOrder[0], Cast<UUserWidget>(ReusedScreen));
                    TestEqual(TEXT("PAH-07: after recovery, overlay child 1 is ExtraOverlayWidget (D1's order preserved)"), RecoveredOverlayOrder[1], Cast<UUserWidget>(ExtraOverlayWidget));
                }
            }

            // A later, ordinary successful apply clears the recovery marker -- Health
            // reports the CURRENT state, not a permanent scar from a past incident.
            TestTrue(*FString::Printf(TEXT("PAH-07: D4 (ordinary, post-recovery) commits [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, MakeDoc(4, TEXT("core:screen.pah07_route"), TEXT("FinalA"), TEXT("FinalB")), Factory, ReconcileError, *PrepareContext));
            TestEqual(TEXT("PAH-07: health returns to Nominal after the next successful commit"),
                Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);
        }
    }

    return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
