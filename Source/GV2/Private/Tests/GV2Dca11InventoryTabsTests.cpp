#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2PackageClosure.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "Tests/GV2PresentationTestFixtures.h"

// DCA-11: proves the deepest chain the plan promises -- screen -> tabs ->
// nested screen -> declared block -> keyed item collection -- entirely from
// Content/GameData, and that a Prepare-time failure at the very bottom (an
// item inside a collection inside a block inside a tab) leaves no trace on
// any ancestor, exactly like DUC-10's third-level test already proved one
// level shallower. Also demonstrates DUC-11's pre-existing composition-cycle
// guard against this new content, which no new C++ was needed to obtain.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2Dca11InventoryTabsFixtureTest,
    "GV2.Runtime.UI.Dca11InventoryTabs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2Dca11InventoryTabsFixtureTest::RunTest(const FString& Parameters)
{
    const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
    UGV2ScreenRegistry* Registry = RegistrySettings != nullptr && !RegistrySettings->RegistryAsset.IsNull()
        ? RegistrySettings->RegistryAsset.LoadSynchronous()
        : nullptr;
    TestNotNull(TEXT("DCA-11: configured Screen Registry is available"), Registry);
    if (Registry == nullptr)
    {
        return false;
    }

    // PSC-02 (ADR-0043 D1/D5): one resolved package set feeds the registry build,
    // the repository build, and the session below -- not three independent discoveries.
    const std::vector<std::filesystem::path> FixturePackageRoots = {
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/textsystem")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/rh")))),
    };
    std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(FixturePackageRoots, ResolveDiagnostics);
    TestTrue(TEXT("DCA-11: fixture package set resolves"), ResolvedSet.has_value());
    if (!ResolvedSet.has_value())
    {
        return false;
    }

    FString RegistryBuildError;
    FGV2ResolvedScreenRegistry ResolvedRegistry;
    const bool bRegistryBuilt = Registry != nullptr
        && Registry->CompileResolvedRegistry(GV2PackageClosure::FromResolvedPackageSet(*ResolvedSet), ResolvedRegistry, RegistryBuildError);
    TestTrue(
        *FString::Printf(TEXT("DCA-11: Screen Registry builds [Error: %s]"), *RegistryBuildError),
        bRegistryBuilt);

    const TPair<const TCHAR*, FGV2ScreenPlacement> ExpectedScreens[] = {
        {TEXT("textsystem:screen.dca11_inventory_fixture"), FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent)},
        {TEXT("textsystem:screen.dca11_inventory_weapons"), FGV2ScreenPlacement::Embedded()},
        {TEXT("textsystem:screen.dca11_inventory_consumables"), FGV2ScreenPlacement::Embedded()},
    };
    for (const auto& [ScreenId, Placement] : ExpectedScreens)
    {
        FGV2ResolvedScreenDescriptor Descriptor;
        FGV2ScreenResolutionRejection Rejection;
        const bool bResolved = ResolvedRegistry.Resolve(ScreenId, Placement, Descriptor, Rejection);
        if (!TestTrue(
                *FString::Printf(TEXT("DCA-11: '%s' is registered [Error: %s]"), ScreenId, *Rejection.Message),
                bResolved))
        {
            return false;
        }
        TestNotNull(
            *FString::Printf(TEXT("DCA-11: '%s' resolves to a trusted Screen Template class"), ScreenId),
            Descriptor.WidgetClass);
    }

    const GV2ContentCore::FBuildResult RepositoryBuild = BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
    TestTrue(TEXT("DCA-11: fixture package repository builds"), RepositoryBuild.IsSuccess());
    if (!RepositoryBuild.IsSuccess())
    {
        return false;
    }

    FGV2SessionCoordinator Coordinator;
    FGV2UiDocumentViewModel CapturedDocument;
    int32 DocumentCount = 0;
    Coordinator.SetDocumentSink([&CapturedDocument, &DocumentCount](const FGV2UiDocumentViewModel& Document, const FGV2PresentationPrepareContext&)
    {
        CapturedDocument = Document;
        ++DocumentCount;
        return true;
    });
    TestTrue(
        TEXT("DCA-11: fixture session starts"),
        Coordinator.StartSession(RepositoryBuild.GetCandidate().GetReadHandle(), 1, *ResolvedSet));
    if (!Coordinator.GetStatus().bIsReady)
    {
        return false;
    }
    const FGV2SessionContentSnapshot* SessionSnapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("DCA-11: session publishes presentation snapshot"), SessionSnapshot);
    if (SessionSnapshot == nullptr)
    {
        return false;
    }
    const FGV2PresentationPrepareContext PrepareContext(*SessionSnapshot);

    auto DispatchFixtureCommand = [&Coordinator, &CapturedDocument, &DocumentCount, this](
        const FString& CommandId,
        FGV2UiDocumentViewModel& OutDocument) -> bool
    {
        FGV2UiBindingDefinition Definition;
        const FString ElementId = CommandId.RightChop(CommandId.Find(TEXT("dca11_")) + 6);
        Definition.ElementId = ElementId;
        Definition.NodeKeyPath = { TEXT("dca11"), Definition.ElementId };
        Definition.CommandId = CommandId;

        TArray<FGV2UiBindingHandle> CommandHandles;
        if (!TestTrue(
                *FString::Printf(TEXT("DCA-11: binding publishes for '%s'"), *CommandId),
                Coordinator.PublishUiBindings(
                    CapturedDocument.UiInstanceId,
                    Coordinator.GetUiRevision() + 1,
                    {Definition},
                    CommandHandles))
            || CommandHandles.Num() != 1)
        {
            AddError(FString::Printf(TEXT("DCA-11: command '%s' did not receive exactly one binding"), *CommandId));
            return false;
        }

        const int32 DocumentsBeforeCommand = DocumentCount;
        if (!TestEqual(
                *FString::Printf(TEXT("DCA-11: Lua command '%s' is accepted"), *CommandId),
                Coordinator.SubmitUiInteraction(CommandHandles[0], {}),
                EGV2SubmitUiInteractionResult::Accepted)
            || !TestEqual(
                *FString::Printf(TEXT("DCA-11: command '%s' publishes one document"), *CommandId),
                DocumentCount,
                DocumentsBeforeCommand + 1))
        {
            return false;
        }

        OutDocument = CapturedDocument;
        return true;
    };

    FGV2UiDocumentViewModel ShowDocument;
    if (!DispatchFixtureCommand(TEXT("textsystem:command.debug.dca11_show"), ShowDocument))
    {
        return false;
    }
    TestTrue(TEXT("DCA-11: Lua document has a route"), ShowDocument.bHasRoute);
    if (ShowDocument.bHasRoute)
    {
        TestEqual(
            TEXT("DCA-11: Lua document targets the inventory root template"),
            ShowDocument.Route.ScreenId,
            FString(TEXT("textsystem:screen.dca11_inventory_fixture")));
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UWorld* const TestWorld = WorldContext.GetWorld();
    TestNotNull(TEXT("DCA-11: standalone world is available"), TestWorld);
    if (TestWorld == nullptr)
    {
        return false;
    }

    auto ScreenFactory = [&ResolvedRegistry, TestWorld](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
    {
        FGV2ResolvedScreenDescriptor Descriptor;
        FGV2ScreenResolutionRejection Rejection;
        if (!ResolvedRegistry.Resolve(ScreenId, FGV2ScreenPlacement::TopLevel(Layer), Descriptor, Rejection))
        {
            return nullptr;
        }
        return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, Descriptor.WidgetClass);
    };

    FGV2LayeredUiReconciler Reconciler;
    FString ReconcileError;
    TestTrue(
        *FString::Printf(TEXT("DCA-11: baseline document reconciles [Error: %s]"), *ReconcileError),
        Reconciler.Reconcile(
            nullptr, ShowDocument, ScreenFactory, ReconcileError, PrepareContext, nullptr));

    UGV2ScreenWidgetBase* const RootScreen = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("dca11_inventory"));
    TestNotNull(TEXT("DCA-11: root screen is published after successful reconcile"), RootScreen);
    UGV2DeclaredCompositeWidgetBase* const InventoryTabs = RootScreen != nullptr
        ? Cast<UGV2DeclaredCompositeWidgetBase>(RootScreen->GetWidgetFromName(TEXT("InventoryTabs"))) : nullptr;
    TestNotNull(TEXT("DCA-11: root screen exposes the generic InventoryTabs host"), InventoryTabs);
    UGV2TabContainerWidgetBase* const Tabs = InventoryTabs != nullptr
        ? Cast<UGV2TabContainerWidgetBase>(InventoryTabs->GetWidgetFromName(TEXT("Tabs"))) : nullptr;
    TestNotNull(TEXT("DCA-11: InventoryTabs reaches the reusable tab container"), Tabs);

    UGV2ScreenWidgetBase* const WeaponsScreen = Tabs != nullptr
        ? Cast<UGV2ScreenWidgetBase>(Tabs->GetScreenWidgetForTab(TEXT("weapons"))) : nullptr;
    TestNotNull(TEXT("DCA-11: weapons tab resolves its registered nested Screen Template"), WeaponsScreen);
    UGV2ScreenWidgetBase* const ConsumablesScreen = Tabs != nullptr
        ? Cast<UGV2ScreenWidgetBase>(Tabs->GetScreenWidgetForTab(TEXT("consumables"))) : nullptr;
    TestNotNull(TEXT("DCA-11: consumables tab resolves its registered nested Screen Template"), ConsumablesScreen);

    UGV2DeclaredCompositeWidgetBase* const WeaponsBlock = WeaponsScreen != nullptr
        ? Cast<UGV2DeclaredCompositeWidgetBase>(WeaponsScreen->GetWidgetFromName(TEXT("CategoryBlock"))) : nullptr;
    TestNotNull(TEXT("DCA-11: fourth-level declared block exists under the weapons tab"), WeaponsBlock);
    UGV2ListViewWidgetBase* const WeaponsRepeater = WeaponsBlock != nullptr
        ? Cast<UGV2ListViewWidgetBase>(WeaponsBlock->GetWidgetFromName(TEXT("ItemRepeater"))) : nullptr;
    TestNotNull(TEXT("DCA-11: fourth-level block exposes its keyed item collection host"), WeaponsRepeater);

    // GetAppliedResourceId() bookkeeping is only populated when Commit targets the
    // image host widget itself; a keyed-collection item's own "resource_id"
    // capability targets its inner "Image" child directly (UGV2IconWidgetBase's
    // own DescribeUiCapabilities), which Commit applies via
    // FGV2ImagePresentation::ResolveAndApply straight onto that UImage -- correct
    // rendering, but that bookkeeping field is simply never reached on this path.
    // The brush's resolved resource object is the real, render-level proof instead.
    UObject* BaselineSwordResourceObject = nullptr;
    if (WeaponsRepeater != nullptr)
    {
        TestEqual(TEXT("DCA-11: baseline weapons collection has two items"), WeaponsRepeater->GetActiveWidgetsMap().Num(), 2);
        UGV2IconWidgetBase* const SwordIcon = Cast<UGV2IconWidgetBase>(WeaponsRepeater->GetActiveWidgetsMap().FindRef(TEXT("sword")));
        TestNotNull(TEXT("DCA-11: fifth-level collection item icon widget exists for 'sword'"), SwordIcon);
        if (SwordIcon != nullptr && SwordIcon->GetImageWidget() != nullptr)
        {
            BaselineSwordResourceObject = SwordIcon->GetImageWidget()->GetBrush().GetResourceObject();
            TestNotNull(
                TEXT("DCA-11: Lua resource id reaches the fifth-level collection item icon's brush"),
                BaselineSwordResourceObject);
        }
    }

    // Deepest-level Prepare failure: one weapons item carries an unresolvable
    // resource_id. The whole document must be rejected atomically at Prepare,
    // and every ancestor (tab container, nested screen, root screen, and the
    // untouched sword icon) must retain its exact prior state -- the same
    // ADR-0041 guarantee DUC-10 already proved one level shallower.
    FGV2UiDocumentViewModel ItemFailureDocument;
    if (DispatchFixtureCommand(TEXT("textsystem:command.debug.dca11_item_prepare_failure"), ItemFailureDocument))
    {
        FGV2LayeredUiReconciler::FPreparedReconciliationPlan FailurePlan;
        TestFalse(
            TEXT("DCA-11: an unresolvable resource_id on a collection item fails Prepare entirely"),
            Reconciler.PrepareReconcile(
                nullptr, ItemFailureDocument, ScreenFactory, FailurePlan, ReconcileError, PrepareContext));
        TestEqual(
            TEXT("DCA-11: Prepare failure retains the published root screen instance"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("dca11_inventory")),
            RootScreen);
        TestEqual(
            TEXT("DCA-11: Prepare failure retains the same weapons nested screen instance"),
            Tabs != nullptr ? Cast<UGV2ScreenWidgetBase>(Tabs->GetScreenWidgetForTab(TEXT("weapons"))) : nullptr,
            WeaponsScreen);
        TestEqual(
            TEXT("DCA-11: Prepare failure retains the same consumables nested screen instance"),
            Tabs != nullptr ? Cast<UGV2ScreenWidgetBase>(Tabs->GetScreenWidgetForTab(TEXT("consumables"))) : nullptr,
            ConsumablesScreen);
        if (WeaponsRepeater != nullptr)
        {
            TestEqual(
                TEXT("DCA-11: Prepare failure leaves the weapons collection item count unchanged"),
                WeaponsRepeater->GetActiveWidgetsMap().Num(),
                2);
            UGV2IconWidgetBase* const SwordIcon = Cast<UGV2IconWidgetBase>(WeaponsRepeater->GetActiveWidgetsMap().FindRef(TEXT("sword")));
            if (SwordIcon != nullptr && SwordIcon->GetImageWidget() != nullptr)
            {
                TestEqual(
                    TEXT("DCA-11: Prepare failure leaves the untouched item's applied resource unmodified"),
                    SwordIcon->GetImageWidget()->GetBrush().GetResourceObject(),
                    BaselineSwordResourceObject);
            }
        }
    }

    // Composition-cycle guard (DUC-11, already generic): the weapons tab's
    // screen_id is set to the root template's own id. No new C++ exists for
    // this -- it is the pre-existing cycle guard applied to new content.
    FGV2UiDocumentViewModel CycleDocument;
    if (DispatchFixtureCommand(TEXT("textsystem:command.debug.dca11_cycle"), CycleDocument))
    {
        FGV2LayeredUiReconciler::FPreparedReconciliationPlan CyclePlan;
        TestFalse(
            TEXT("DCA-11: a tab screen_id equal to its own root screen id is rejected before Ready"),
            Reconciler.PrepareReconcile(
                nullptr, CycleDocument, ScreenFactory, CyclePlan, ReconcileError, PrepareContext));
        TestTrue(
            TEXT("DCA-11: cycle rejection reports DUC-11's diagnostic code"),
            ReconcileError.Contains(TEXT("core:diagnostic.ui_composition.cycle_detected")));
        TestEqual(
            TEXT("DCA-11: cycle rejection retains the published root screen instance"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("dca11_inventory")),
            RootScreen);
    }

    DispatchFixtureCommand(TEXT("textsystem:command.debug.dca11_clear"), CapturedDocument);

    return true;
}

#endif
