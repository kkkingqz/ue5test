#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"

#include "Widgets/SVirtualWindow.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "Application/GV2ScreenFieldMaterializer.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"
#include "GV2PresentationTestFixtures.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2UiHostSemanticState.h"
#include "UI/GV2TextWidgetBase.h"
#include "CommonTextBlock.h"

// =========================================================================
// TSR-07 (ADR-0046, Plan TestSuiteRestructuring):
// Content Smoke Tests for TextSystem & RH widgets and assets.
// These tests verify that authored game blueprints load, configure, and render correctly.
// =========================================================================

namespace
{
using GV2PresentationTestFixtures::LoadConfiguredThemeForTest;
using GV2PresentationTestFixtures::MakeResolvedLiteralTextForTest;
using GV2PresentationTestFixtures::MakePreparedResolvedImageForTest;
using GV2PresentationTestFixtures::GV2SimulateResponsiveFrame;

bool GV2FitsInBounds(const FVector2D& Pos, const FVector2D& Size, const FVector2D& Bounds)
{
    return Pos.X >= -1.0f && Pos.Y >= -1.0f
        && (Pos.X + Size.X) <= (Bounds.X + 1.0f)
        && (Pos.Y + Size.Y) <= (Bounds.Y + 1.0f);
}
}

// CCF-06..12: Location Composite Correctness Smoke Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationCompositeContractTest,
    "GV2.Runtime.UI.LocationCompositeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationCompositeContractTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    // CCF-06: Capabilities declaration
    {
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneWidget = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        FGV2UiCapabilityBuilder SceneBuilder;
        SceneWidget->DescribeUiCapabilities(SceneBuilder);
        FGV2UiCapabilityTree SceneTree = SceneBuilder.Build();
        TestNotNull(TEXT("CCF-06: Scene capabilities declared"), SceneTree.FindProperty(TEXT("key")));
        TestNotNull(TEXT("CCF-06: Scene context_text declared"), SceneTree.FindProperty(TEXT("context_text")));
        TestNotNull(TEXT("CCF-06: Scene characters declared"), SceneTree.FindProperty(TEXT("characters")));

        UClass* CmdClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2DeclaredCompositeWidgetBase* CmdWidget = CmdClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        FGV2UiCapabilityBuilder CmdBuilder;
        CmdWidget->DescribeUiCapabilities(CmdBuilder);
        FGV2UiCapabilityTree CmdTree = CmdBuilder.Build();
        TestNotNull(TEXT("CCF-06: Command capabilities declared"), CmdTree.FindProperty(TEXT("key")));
        TestNotNull(TEXT("CCF-06: Command items declared"), CmdTree.FindProperty(TEXT("items")));
    }

    // CCF-07: Repeated elements go through Repeater only (0, 1, 2 cases)
    {
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);

        UGV2ListViewWidgetBase* CharRep = Cast<UGV2ListViewWidgetBase>(SceneView->GetWidgetFromName(TEXT("CharacterRepeater")));
        if (CharRep != nullptr)
        {
            struct FTestCharEntry { FName Key; FString ResourceId; };
            auto GetKey = [](const FTestCharEntry& E) { return E.Key; };
            auto CreateWidgetLambda = [TestWorld]() -> UGV2ImageWidgetBase*
            {
                return NewObject<UGV2ImageWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ImageWidgetBase& Widget, const FTestCharEntry& Entry)
            {
                Widget.SetKey(Entry.Key);
                return true;
            };

            // 0 characters
            TArray<FTestCharEntry> C0;
            TestTrue(TEXT("CCF-07: 0 characters applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C0, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 0 for empty characters"), CharRep->GetEntryCount(), 0);

            // 1 character
            TArray<FTestCharEntry> C1 = { { FName(TEXT("c_aria")), TEXT("textsystem:resource.ui.missing_portrait") } };
            TestTrue(TEXT("CCF-07: 1 character applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C1, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 1 for 1 character"), CharRep->GetEntryCount(), 1);
            TestNotNull(TEXT("CCF-07: CharA entry in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_aria"))));

            // 2 characters
            TArray<FTestCharEntry> C2 = { { FName(TEXT("c_aria")), TEXT("textsystem:resource.ui.missing_portrait") }, { FName(TEXT("c_merchant")), TEXT("textsystem:resource.ui.missing_portrait") } };
            TestTrue(TEXT("CCF-07: 2 characters applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C2, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 2 for 2 characters"), CharRep->GetEntryCount(), 2);
            TestNotNull(TEXT("CCF-07: CharA still in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_aria"))));
            TestNotNull(TEXT("CCF-07: CharB in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_merchant"))));
        }
    }

    // CCF-11: Key and host state semantics for SceneView and CommandPanel
    {
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        SceneView->SetKey(FName(TEXT("scene_test")));
        TestEqual(TEXT("CCF-11: SceneView Key getter/setter"), SceneView->GetKey(), FName(TEXT("scene_test")));

        UClass* CmdClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2DeclaredCompositeWidgetBase* CmdPanel = CmdClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        CmdPanel->SetKey(FName(TEXT("cmd_test")));
        TestEqual(TEXT("CCF-11: CommandPanel Key getter/setter"), CmdPanel->GetKey(), FName(TEXT("cmd_test")));
    }

    return true;
}

// UIH-09..UIH-12: Location Composite Semantics & Validation Smoke Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationCompositeSemanticsTest,
    "GV2.Runtime.UI.LocationCompositeSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationCompositeSemanticsTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld != nullptr)
    {
        // 1. SceneView validation & semantics
        {
            UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
            UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
            TestNotNull(TEXT("SceneView created"), SceneView);

            FGV2UiCapabilityBuilder Builder;
            SceneView->DescribeUiCapabilities(Builder);
            FGV2UiCapabilityTree Caps = Builder.Build();
            TestNotNull(TEXT("SceneView has background_tile_resource_id cap"), Caps.FindProperty(TEXT("background_tile_resource_id")));
            TestNotNull(TEXT("SceneView has background_resource_id cap"), Caps.FindProperty(TEXT("background_resource_id")));
            TestNotNull(TEXT("SceneView has context_text cap"), Caps.FindProperty(TEXT("context_text")));
            TestNotNull(TEXT("SceneView has characters cap"), Caps.FindProperty(TEXT("characters")));
            TestNotNull(TEXT("SceneView has key cap"), Caps.FindProperty(TEXT("key")));
        }

        // 4. CommandPanel validation & semantics
        {
            UClass* CommandClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
            UGV2DeclaredCompositeWidgetBase* CommandPanel = CommandClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CommandClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
            TestNotNull(TEXT("CommandPanel created"), CommandPanel);

            FGV2UiCapabilityBuilder Builder;
            CommandPanel->DescribeUiCapabilities(Builder);
            FGV2UiCapabilityTree Caps = Builder.Build();
            TestNotNull(TEXT("CommandPanel has items cap"), Caps.FindProperty(TEXT("items")));
            TestNotNull(TEXT("CommandPanel has key cap"), Caps.FindProperty(TEXT("key")));
        }
    }

    return true;
}

// UIH-13: Real Viewport / Layout Matrix Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenViewportMatrixTest,
    "GV2.Runtime.UI.LocationScreenViewportMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenViewportMatrixTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld != nullptr)
    {
        const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
        TestNotNull(TEXT("Configured theme is available for prepared text fixtures"), Theme);

        // 1. Load actual registered WBP_LocationScreen
        UClass* ScreenClass = LoadClass<UGV2ScreenWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
        TestNotNull(TEXT("WBP_LocationScreen is loadable"), ScreenClass);

        if (ScreenClass != nullptr)
        {
            UGV2ScreenWidgetBase* LocationScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, ScreenClass);
            TestNotNull(TEXT("WBP_LocationScreen instantiated"), LocationScreen);

            if (LocationScreen != nullptr)
            {
                TSharedPtr<SWidget> SlateWidget = LocationScreen->TakeWidget();
                TestTrue(TEXT("LocationScreen produces valid Slate widget"), SlateWidget.IsValid());

                if (SlateWidget.IsValid())
                {
                    TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(FVector2D(1920, 1080));
                    VirtualWindow->SetContent(SlateWidget.ToSharedRef());

                    // Find child composite widgets in tree
                    TArray<UWidget*> ChildWidgets;
                    LocationScreen->WidgetTree->GetAllWidgets(ChildWidgets);

                    UGV2DeclaredCompositeWidgetBase* TopBarWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* PlayerStatusWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* SceneWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;

                    for (UWidget* W : ChildWidgets)
                    {
                        // DUC-08/DCA-05/DCA-06/DCA-07: TopBar, Scene, PlayerStatus and
                        // CommandPanel are now the generic declared composite -- matched by
                        // HostIdentity, not a dedicated C++ class, since several other
                        // declared composites could also appear in this tree.
                        if (auto* TB = Cast<UGV2DeclaredCompositeWidgetBase>(W); TB != nullptr && TB->GetHostIdentity() == FName(TEXT("top_bar"))) TopBarWidget = TB;
                        else if (auto* PS = Cast<UGV2DeclaredCompositeWidgetBase>(W); PS != nullptr && PS->GetHostIdentity() == FName(TEXT("player_status"))) PlayerStatusWidget = PS;
                        else if (auto* SC = Cast<UGV2DeclaredCompositeWidgetBase>(W); SC != nullptr && SC->GetHostIdentity() == FName(TEXT("scene"))) SceneWidget = SC;
                        else if (auto* CP = Cast<UGV2DeclaredCompositeWidgetBase>(W); CP != nullptr && CP->GetHostIdentity() == FName(TEXT("commands"))) CommandWidget = CP;
                    }

                    TestNotNull(TEXT("TopBar child composite exists"), TopBarWidget);
                    TestNotNull(TEXT("PlayerStatus child composite exists"), PlayerStatusWidget);
                    TestNotNull(TEXT("Scene child composite exists"), SceneWidget);
                    TestNotNull(TEXT("CommandPanel child composite exists"), CommandWidget);

                    if (CommandWidget != nullptr)
                    {
                        if (UGV2ListViewWidgetBase* CmdRep = Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater"))))
                        {
                            struct FTestCmdEntry { FName Key; FText Text; };
                            TArray<FTestCmdEntry> TestButtons;
                            for (int32 Index = 1; Index <= 6; ++Index)
                            {
                                TestButtons.Add({ *FString::Printf(TEXT("cmd_%d"), Index), FText::FromString(*FString::Printf(TEXT("[LOCALE_TEST] Speak with Master Alchemist about Mysterious Elixir (#%d)"), Index)) });
                            }
                            UClass* CmdButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
                            CmdRep->ReconcileEntries<UGV2ButtonWidgetBase, FTestCmdEntry>(
                                TestButtons,
                                [](const FTestCmdEntry& E) { return E.Key; },
                                [TestWorld, CmdButtonClass]() -> UGV2ButtonWidgetBase*
                                {
                                    return CmdButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, CmdButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
                                },
                                [Theme](UGV2ButtonWidgetBase& Btn, const FTestCmdEntry& Entry)
                                {
                                    Btn.SetKey(Entry.Key);
                                    return Theme != nullptr
                                        && Btn.ApplyText(MakeResolvedLiteralTextForTest(
                                            *Theme,
                                            Entry.Text.ToString()));
                                });
                        }
                    }

                    // 3. Test layout geometry across 6 resolutions using real SVirtualWindow
                    struct FViewportResolution
                    {
                        const TCHAR* Name;
                        FVector2D Size;
                        bool bUltrawide;
                    };
                    const FViewportResolution TestResolutions[] = {
                        { TEXT("4K (3840x2160)"), FVector2D(3840, 2160), false },
                        { TEXT("QHD (2560x1440)"), FVector2D(2560, 1440), false },
                        { TEXT("FHD (1920x1080)"), FVector2D(1920, 1080), false },
                        { TEXT("HD (1280x720)"), FVector2D(1280, 720), false },
                        { TEXT("UW-QHD (3440x1440)"), FVector2D(3440, 1440), true },
                        { TEXT("UW-FHD (2560x1080)"), FVector2D(2560, 1080), true }
                    };


                    float SceneWidthFHD = 0.0f;
                    float SceneWidthUWFHD = 0.0f;

                    // DCA-13: prove the harness fix actually recalculates a dynamic
                    // WrapBox's wrap threshold -- not just that geometry queries return
                    // non-stale numbers for widgets that never depended on Tick() in the
                    // first place (every check below this already worked before DCA-13,
                    // since Paint/Arrange update GetTickSpaceGeometry() on their own).
                    // A standalone SWrapBox with UseAllottedSize=true, unrelated to any
                    // production composite, is measured at the narrowest and widest
                    // resolutions in the matrix: if the fix works, more fixed-width
                    // slots fit on the first row at 3840 than at 1280.
                    {
                        TSharedRef<SWrapBox> ProbeWrapBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
                        for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
                        {
                            ProbeWrapBox->AddSlot()
                            [
                                SNew(SBox).WidthOverride(300.0f).HeightOverride(40.0f)
                            ];
                        }
                        TSharedRef<SVirtualWindow> ProbeWindow = SNew(SVirtualWindow).Size(FVector2D(1280, 720));
                        ProbeWindow->SetContent(ProbeWrapBox);

                        auto MeasureFirstRowSlotCount = [&ProbeWindow, &ProbeWrapBox](const FVector2D& Size) -> int32
                        {
                            GV2SimulateResponsiveFrame(ProbeWindow, Size);
                            FArrangedChildren ArrangedChildren(EVisibility::All);
                            ProbeWrapBox->ArrangeChildren(ProbeWrapBox->GetTickSpaceGeometry(), ArrangedChildren, true);
                            int32 FirstRowCount = 0;
                            float FirstRowY = -1.0f;
                            for (int32 Index = 0; Index < ArrangedChildren.Num(); ++Index)
                            {
                                const float Y = ArrangedChildren[Index].Geometry.GetAbsolutePosition().Y;
                                if (FirstRowY < 0.0f)
                                {
                                    FirstRowY = Y;
                                }
                                if (!FMath::IsNearlyEqual(Y, FirstRowY, 1.0f))
                                {
                                    break;
                                }
                                ++FirstRowCount;
                            }
                            return FirstRowCount;
                        };

                        const int32 FirstRowAt1280 = MeasureFirstRowSlotCount(FVector2D(1280, 720));
                        const int32 FirstRowAt3840 = MeasureFirstRowSlotCount(FVector2D(3840, 2160));
                        TestTrue(
                            *FString::Printf(TEXT("DCA-13: dynamic WrapBox wrap threshold differs between 1280 (%d/row) and 3840 (%d/row)"), FirstRowAt1280, FirstRowAt3840),
                            FirstRowAt3840 > FirstRowAt1280);
                    }

                    // DCA-13: strict per-button geometry coverage is computed from the
                    // loop itself, not asserted by name -- a resolution that cannot be
                    // strictly checked is named and counted as excluded, and the total
                    // is compared against the matrix size below, so silently narrowing
                    // coverage back to one resolution shows up as a numeric mismatch
                    // instead of passing quietly.
                    int32 StrictButtonGeometryCoveredCount = 0;
                    TArray<FString> StrictButtonGeometryExclusionReasons;

                    // DCA-14: counted the same way -- two real GV2FitsInBounds-backed
                    // positive assertions per button (viewport containment, CommandPanel
                    // containment), summed from the loop itself and compared below
                    // against the expected total for however many resolutions actually
                    // reached strict coverage. Deleting a positive assertion (or all of
                    // them) reduces this count without touching the negative self-tests,
                    // so it surfaces as its own numeric mismatch instead of leaving the
                    // negative tests as the only, silently-insufficient signal.
                    int32 PositivePerButtonBoundsAssertionCount = 0;

                    for (const auto& Res : TestResolutions)
                    {
                        LocationScreen->InvalidateLayoutAndVolatility();
                        GV2SimulateResponsiveFrame(VirtualWindow, Res.Size);

                        const FVector2D WindowAllocated = VirtualWindow->GetTickSpaceGeometry().GetLocalSize();
                        TestEqual(
                            *FString::Printf(TEXT("CCF-16: [%s] Window allocated size matches target resolution"), Res.Name),
                            WindowAllocated,
                            Res.Size);

                        // 1. TopBar allocated geometry: height <= 25% of Res.Size.Y across ALL resolutions
                        if (TopBarWidget != nullptr && TopBarWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D TopBarAllocated = TopBarWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated height is positive: %f"), Res.Name, TopBarAllocated.Y),
                                TopBarAllocated.Y > 0.0f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated height <= 25%% of screen height (%f <= %f)"), Res.Name, TopBarAllocated.Y, Res.Size.Y * 0.25f),
                                TopBarAllocated.Y <= Res.Size.Y * 0.25f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated width fills screen width (%f == %f)"), Res.Name, TopBarAllocated.X, Res.Size.X),
                                FMath::IsNearlyEqual(TopBarAllocated.X, Res.Size.X, 1.0f));
                        }

                        // 2. PlayerStatus allocated geometry: width <= 60% of Res.Size.X across ALL resolutions
                        if (PlayerStatusWidget != nullptr && PlayerStatusWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D PlayerStatusAllocated = PlayerStatusWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] PlayerStatus allocated width is positive: %f"), Res.Name, PlayerStatusAllocated.X),
                                PlayerStatusAllocated.X > 0.0f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] PlayerStatus allocated width <= 60%% of screen width (%f <= %f)"), Res.Name, PlayerStatusAllocated.X, Res.Size.X * 0.6f),
                                PlayerStatusAllocated.X <= Res.Size.X * 0.6f);
                        }

                        // 3. SceneView allocated geometry: fills remaining body width
                        if (SceneWidget != nullptr && SceneWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D SceneAllocated = SceneWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] SceneView allocated area is positive (%f x %f)"), Res.Name, SceneAllocated.X, SceneAllocated.Y),
                                SceneAllocated.X > 0.0f && SceneAllocated.Y > 0.0f);

                            if (Res.Size.X == 1920.0f && Res.Size.Y == 1080.0f)
                            {
                                SceneWidthFHD = SceneAllocated.X;
                            }
                            else if (Res.Size.X == 2560.0f && Res.Size.Y == 1080.0f)
                            {
                                SceneWidthUWFHD = SceneAllocated.X;
                            }
                        }

                        // 4. CommandPanel allocated geometry: check button placement within bounds
                        if (CommandWidget != nullptr && CommandWidget->GetCachedWidget().IsValid())
                        {
                            const FGeometry CommandGeom = CommandWidget->GetCachedWidget()->GetTickSpaceGeometry();
                            const FVector2D CommandAllocated = CommandGeom.GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] CommandPanel allocated size is positive (%f x %f)"), Res.Name, CommandAllocated.X, CommandAllocated.Y),
                                CommandAllocated.X > 0.0f && CommandAllocated.Y > 0.0f);

                            if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater"))))
                            {
                                TestEqual(
                                    *FString::Printf(TEXT("CCF-17: [%s] All 6 command buttons instantiated in repeater"), Res.Name),
                                    Repeater->GetEntryCount(),
                                    6);

                                // DCA-13: strict per-button geometry, on every resolution in the
                                // matrix, not only 720p -- the harness fix above (GV2SimulateResponsiveFrame)
                                // is what makes this honest at every width, not only the one where the
                                // old single Paint pass happened to already be correct.
                                const TArray<UWidget*> Entries = Repeater->GetOrderedEntries();
                                if (TestEqual(*FString::Printf(TEXT("CCF-17: [%s] Repeater ordered entry widgets count matches 6"), Res.Name), Entries.Num(), 6))
                                {
                                    for (int32 BtnIndex = 0; BtnIndex < Entries.Num(); ++BtnIndex)
                                    {
                                        if (Entries[BtnIndex] != nullptr && Entries[BtnIndex]->GetCachedWidget().IsValid())
                                        {
                                            const FGeometry BtnGeom = Entries[BtnIndex]->GetCachedWidget()->GetTickSpaceGeometry();
                                            const FVector2D BtnLocalPos = VirtualWindow->GetTickSpaceGeometry().AbsoluteToLocal(BtnGeom.GetAbsolutePosition());
                                            const FVector2D BtnSize = BtnGeom.GetLocalSize();
                                            const FVector2D BtnInCommandPanel = CommandGeom.AbsoluteToLocal(BtnGeom.GetAbsolutePosition());

                                            TestTrue(
                                                *FString::Printf(TEXT("CCF-17: [%s] Button #%d allocated size is positive (%f x %f)"), Res.Name, BtnIndex + 1, BtnSize.X, BtnSize.Y),
                                                BtnSize.X > 0.0f && BtnSize.Y > 0.0f);

                                            // DCA-14: both positive checks below and both negative
                                            // self-tests further down call the exact same
                                            // GV2FitsInBounds -- there is no second, independently
                                            // maintained copy of "fits inside these bounds" anywhere
                                            // in this test.

                                            // 1. Viewport 2-axis containment
                                            const bool bFitsViewport = GV2FitsInBounds(BtnLocalPos, BtnSize, Res.Size);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [%s] Button #%d fits viewport bounds (pos=%s size=%s bounds=%s)"),
                                                    Res.Name, BtnIndex + 1, *BtnLocalPos.ToString(), *BtnSize.ToString(), *Res.Size.ToString()),
                                                bFitsViewport);
                                            ++PositivePerButtonBoundsAssertionCount;

                                            // 2. CommandPanel 2-axis containment
                                            const bool bFitsCommandPanel = GV2FitsInBounds(BtnInCommandPanel, BtnSize, CommandAllocated);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [%s] Button #%d fits CommandPanel bounds (pos=%s size=%s bounds=%s)"),
                                                    Res.Name, BtnIndex + 1, *BtnInCommandPanel.ToString(), *BtnSize.ToString(), *CommandAllocated.ToString()),
                                                bFitsCommandPanel);
                                            ++PositivePerButtonBoundsAssertionCount;

                                            // 3. Negative containment check: simulated oversized button detection
                                            TestFalse(
                                                *FString::Printf(TEXT("BAI-10: [%s] [Negative] Artificial horizontal overflow beyond panel width is rejected"), Res.Name),
                                                GV2FitsInBounds(BtnInCommandPanel, FVector2D(CommandAllocated.X + 50.0f, BtnSize.Y), CommandAllocated));
                                            TestFalse(
                                                *FString::Printf(TEXT("BAI-10: [%s] [Negative] Artificial vertical overflow beyond viewport height is rejected"), Res.Name),
                                                GV2FitsInBounds(BtnLocalPos, FVector2D(BtnSize.X, Res.Size.Y + 80.0f), Res.Size));
                                        }
                                    }
                                    ++StrictButtonGeometryCoveredCount;
                                }
                                else
                                {
                                    StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                        TEXT("[%s] ButtonRepeater entry count was not 6"), Res.Name));
                                }
                            }
                            else
                            {
                                StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                    TEXT("[%s] ButtonRepeater not found under CommandPanel"), Res.Name));
                            }
                        }
                        else
                        {
                            StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                TEXT("[%s] CommandPanel widget missing or not laid out"), Res.Name));
                        }

                        // Ultrawide check (CCF-18): 21:9 ratio verified
                        if (Res.bUltrawide)
                        {
                            TestTrue(
                                *FString::Printf(TEXT("CCF-18: [%s] Ultrawide aspect ratio is > 2.0"), Res.Name),
                                (Res.Size.X / Res.Size.Y) > 2.0f);
                        }
                    }

                    // DCA-13: the count above comes from the loop itself, not a hand-picked
                    // literal -- a regression that silently narrows strict coverage back
                    // down (e.g. reintroducing a single-resolution gate) shows up here as
                    // this count falling below UE_ARRAY_COUNT(TestResolutions), with each
                    // excluded resolution named and reasoned, not as a silent pass.
                    if (!TestEqual(
                        TEXT("DCA-13: strict per-button geometry check covers every resolution in the matrix"),
                        StrictButtonGeometryCoveredCount,
                        static_cast<int32>(UE_ARRAY_COUNT(TestResolutions))))
                    {
                        for (const FString& Reason : StrictButtonGeometryExclusionReasons)
                        {
                            AddError(FString::Printf(TEXT("DCA-13: resolution excluded from strict geometry coverage -- %s"), *Reason));
                        }
                    }

                    // DCA-14: two GV2FitsInBounds-backed positive assertions per button
                    // (viewport, CommandPanel), six buttons, once per resolution that
                    // reached strict coverage above -- deleting a positive assertion (or
                    // all of them) drops this count below the expected total, on its own,
                    // independently of whether the two negative self-tests still pass.
                    TestEqual(
                        TEXT("DCA-14: every strictly-covered resolution ran both per-button bounds assertions"),
                        PositivePerButtonBoundsAssertionCount,
                        StrictButtonGeometryCoveredCount * 6 * 2);

                    // CCF-18: Compare FHD (1920x1080) vs UW-FHD (2560x1080) allocated geometry
                    TestTrue(TEXT("CCF-18: FHD and UW-FHD scene widths measured"), SceneWidthFHD > 0.0f && SceneWidthUWFHD > 0.0f);
                    TestTrue(
                        *FString::Printf(TEXT("CCF-18: Ultrawide (21:9) SceneView allocated width (%f) is strictly greater than 16:9 width (%f)"), SceneWidthUWFHD, SceneWidthFHD),
                        SceneWidthUWFHD > SceneWidthFHD);
                    const float WidthDifference = SceneWidthUWFHD - SceneWidthFHD;
                    TestTrue(
                        *FString::Printf(TEXT("CCF-18: Extra ultrawide width allocated to SceneView (%f >= 600px)"), WidthDifference),
                        WidthDifference >= 600.0f);

                    // Negative test: Constrained / small viewport bounds layout elements and does not overflow
                    {
                        VirtualWindow->Resize(FVector2D(100.0f, 100.0f));
                        VirtualWindow->SlatePrepass(1.0f);
                        FSlateWindowElementList WindowElementListSmall(VirtualWindow);
                        VirtualWindow->PaintWindow(FPlatformTime::Seconds(), 0.016f, WindowElementListSmall, FWidgetStyle(), true);
                        if (TopBarWidget != nullptr && TopBarWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D TopBarSmall = TopBarWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(TEXT("CCF-16: [Negative] Constrained viewport bounds TopBar allocated size"), TopBarSmall.X <= 100.0f + 1.0f);
                        }
                    }
                }
            }
        }
    }

    return true;
}

// =========================================================================
// UIH-14: Rendering Conformance on Instantiated Widgets Test
// =========================================================================

// Diagnostic: LocationScene Image & Hierarchy Audit Smoke Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationSceneDiagnostic,
    "GV2.Runtime.Presentation.LocationSceneDiagnostic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationSceneDiagnostic::RunTest(const FString& Parameters)
{
    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString MarketResourceId = GameNamespace + TEXT(":resource.location.market");
    const FString HeroPortraitResourceId = GameNamespace + TEXT(":resource.portrait.hero");

    FString SceneCatalogError;
    UGV2ImageResourceCatalog* Catalog =
        GV2PresentationTestFixtures::BuildGameDataImageCatalog(SceneCatalogError);
    TestNotNull(*FString::Printf(TEXT("Image catalog builds [Error: %s]"), *SceneCatalogError), Catalog);
    if (Catalog != nullptr)
    {
        FGV2ResolvedImageResource MarketRes;
        FString Error;
        const bool bMarketResolved = Catalog->Resolve(MarketResourceId, MarketRes, Error);
        TestTrue(*FString::Printf(TEXT("Market resource resolved: %s"), *Error), bMarketResolved);
        if (bMarketResolved)
        {
            UObject* ResObj = MarketRes.Brush.GetResourceObject();
            TestNotNull(TEXT("Market brush resource object is valid"), ResObj);
            UTexture2D* Tex = Cast<UTexture2D>(ResObj);
            TestNotNull(TEXT("Market resource is UTexture2D"), Tex);
            if (Tex != nullptr)
            {
                AddInfo(FString::Printf(TEXT("Market Texture size: %dx%d, SRGB=%d, HasPlatformData=%d"),
                    Tex->GetSizeX(), Tex->GetSizeY(), Tex->SRGB, Tex->GetPlatformData() != nullptr));
            }
        }
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(
            nullptr,
            TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        TestNotNull(TEXT("SceneClass loaded"), SceneClass);
        if (SceneClass != nullptr)
        {
            UGV2DeclaredCompositeWidgetBase* SceneView = CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass);
            TestNotNull(TEXT("Scene created"), SceneView);
            if (SceneView != nullptr)
            {
                UGV2ImageWidgetBase* Bg = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("Background"))));
                UGV2ImageWidgetBase* BgTile = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("BackgroundTile"))));
                TestNotNull(TEXT("Background widget found"), Bg);
                TestNotNull(TEXT("BackgroundTile widget found"), BgTile);

                if (Bg != nullptr)
                {
                    FString Error;
                    FGV2ResolvedImageResource BgResolved;
                    if (Catalog->Resolve(MarketResourceId, BgResolved, Error))
                    {
                        Bg->ApplyResolvedImageResource(GV2PresentationTestFixtures::MakePreparedResolvedImageForTest(BgResolved), Error);
                    }
                    AddInfo(FString::Printf(TEXT("Background: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                        *Bg->GetAppliedResourceId(),
                        static_cast<int32>(Bg->GetVisibility()),
                        Bg->GetImageBrush().GetResourceObject() ? *Bg->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                }
                if (BgTile != nullptr)
                {
                    FString Error;
                    FGV2ResolvedImageResource TileResolved;
                    if (Catalog->Resolve(TEXT("core:resource.ui.old_paper_tile_256"), TileResolved, Error))
                    {
                        BgTile->ApplyResolvedImageResource(GV2PresentationTestFixtures::MakePreparedResolvedImageForTest(TileResolved), Error);
                    }
                    AddInfo(FString::Printf(TEXT("BackgroundTile: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                        *BgTile->GetAppliedResourceId(),
                        static_cast<int32>(BgTile->GetVisibility()),
                        BgTile->GetImageBrush().GetResourceObject() ? *BgTile->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                }
            }
        }
    }
    return true;
}

// Content Smoke Test for CommonUI text styles
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CommonUiStyleLoadSmokeTest,
    "GV2.ContentSmoke.CommonUiStyleLoads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CommonUiStyleLoadSmokeTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* StylePath : {
             TEXT("/Game/TextSystem/UI/Styles/BP_UIStyle_Text_Default.BP_UIStyle_Text_Default_C"),
             TEXT("/Game/TextSystem/UI/Styles/BP_UIStyle_ButtonLabel_Default.BP_UIStyle_ButtonLabel_Default_C")})
    {
        const UClass* StyleClass = LoadClass<UCommonTextStyle>(nullptr, StylePath);
        const UCommonTextStyle* Style = StyleClass != nullptr
            ? Cast<UCommonTextStyle>(StyleClass->GetDefaultObject())
            : nullptr;
        TestNotNull(TEXT("CommonUI text style is loadable"), Style);
        if (Style != nullptr)
        {
            FSlateFontInfo Font;
            Style->GetFont(Font);
            TestNotNull(TEXT("CommonUI text style has an explicit font"), Font.FontObject.Get());
            TestEqual(TEXT("CommonUI text style selects Regular typeface"), Font.TypefaceFontName, FName(TEXT("Regular")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RhStartScreenFlow,
    "GV2.Runtime.Presentation.RhStartOpensLocationScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RhStartScreenFlow::RunTest(const FString& Parameters)
{
    const UGV2RuntimeSettings* RuntimeSettings = GetDefault<UGV2RuntimeSettings>();
    TestNotNull(TEXT("Runtime development settings are available"), RuntimeSettings);
    if (RuntimeSettings != nullptr)
    {
        TestTrue(
            TEXT("Editor startup profile uses RH"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/rh")));
        TestFalse(
            TEXT("Editor startup profile excludes the sample test screen"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/sample")));
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the runtime"), Runtime);
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
        UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("RH startup opens the registered LocationScreen"), Screen);
        if (Screen != nullptr)
        {
            UClass* LocationScreenClass = LoadClass<UUserWidget>(
                nullptr,
                TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
            TestNotNull(TEXT("LocationScreen class is loadable"), LocationScreenClass);
            TestTrue(
                TEXT("RH startup presents WBP_LocationScreen"),
                LocationScreenClass != nullptr && Screen->IsA(LocationScreenClass));

            // 1. Verify startup tavern scene has 1 character from Lua presentation
            // DCA-05: Scene is now the generic declared composite -- matched by
            // HostIdentity, not a dedicated C++ class, since several other declared
            // composites could also appear in this tree.
            UGV2DeclaredCompositeWidgetBase* SceneWidget = nullptr;
            UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;
            if (Screen->WidgetTree != nullptr)
            {
                Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                {
                    if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                    {
                        SceneWidget = Scene;
                    }
                    else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                    {
                        CommandWidget = Cmd;
                    }
                });
            }

            // M2 (DCA-05...07): the three composites are declarations now, so the risk
            // the migration carries is not a missing widget -- structure and entry counts
            // stay right -- but a property that silently stops arriving at its leaf. The
            // set checked here is enumerated from each composite's own DeclaredCapabilities,
            // not from a hand-written list of properties, so a capability added to a
            // declaration later falls under this check without anyone updating the test.
            UGV2DeclaredCompositeWidgetBase* StatusWidget = nullptr;
            if (Screen->WidgetTree != nullptr)
            {
                Screen->WidgetTree->ForEachWidget([&StatusWidget](UWidget* Widget)
                {
                    if (auto* Status = Cast<UGV2DeclaredCompositeWidgetBase>(Widget);
                        Status != nullptr && Status->GetHostIdentity() == FName(TEXT("player_status")))
                    {
                        StatusWidget = Status;
                    }
                });
            }
            TestNotNull(TEXT("LocationScreen contains PlayerStatus component"), StatusWidget);

            auto VerifyDeclaredValuesArrived =
                [this](UGV2DeclaredCompositeWidgetBase* Composite, const TCHAR* Label) -> int32
            {
                if (Composite == nullptr)
                {
                    return 0;
                }
                const FGV2UiHostCommittedSnapshot Snapshot =
                    GetUiHostSemanticState(Composite->GetPropertyHostState()).GetCommittedSnapshot();
                if (!Snapshot.Schema)
                {
                    TestTrue(
                        *FString::Printf(TEXT("M2: [%s] committed a schema after the Lua-driven revision"), Label),
                        false);
                    return 0;
                }

                // The set is the intersection of two independently produced sides: what the
                // Designer declaration binds, and what the committed schema requires. Both
                // sides are read, not written here. Schema-optional fields are excluded on
                // the schema's own say-so -- the composite's identity `key` is declared
                // `required: false` and is never published by the document, so demanding a
                // committed value for it would assert the opposite of the schema.
                TSet<FString> SchemaFieldNames;
                TSet<FString> RequiredSchemaFieldNames;
                for (const auto& FieldEntry : Snapshot.Schema->Fields)
                {
                    const FString FieldName = UTF8_TO_TCHAR(FieldEntry.Name.c_str());
                    SchemaFieldNames.Add(FieldName);
                    if (FieldEntry.bRequired)
                    {
                        RequiredSchemaFieldNames.Add(FieldName);
                    }
                }

                int32 Arrived = 0;
                for (const FGV2DeclaredUiCapability& Declared : Composite->DeclaredCapabilities)
                {
                    const FString PropertyName = Declared.PropertyName.ToString();
                    if (!SchemaFieldNames.Contains(PropertyName))
                    {
                        continue;
                    }
                    // A declaration-optional property whose child is unbound on this asset
                    // is not declared at all for this instance (DCA-01), so requiring a
                    // committed value for it would assert the opposite of that contract.
                    if (Declared.bOptional
                        && Declared.ChildWidgetName != NAME_None
                        && Composite->GetWidgetFromName(Declared.ChildWidgetName) == nullptr)
                    {
                        continue;
                    }
                    const bool bCommitted = Snapshot.Properties.FindField(PropertyName) != nullptr;
                    if (bCommitted)
                    {
                        ++Arrived;
                    }
                    // Schema-required is the only case the contract lets us demand. The
                    // count returned below covers the rest: a revision where nothing at
                    // all arrived would satisfy every required check of a schema whose
                    // fields are all optional, which is exactly the scene's situation.
                    if (RequiredSchemaFieldNames.Contains(PropertyName))
                    {
                        TestTrue(
                            *FString::Printf(
                                TEXT("M2: [%s] schema-required declared property '%s' has a committed value after the Lua-driven revision"),
                                Label,
                                *PropertyName),
                            bCommitted);
                    }
                }
                return Arrived;
            };

            const int32 SceneArrived = VerifyDeclaredValuesArrived(SceneWidget, TEXT("scene"));
            const int32 StatusArrived = VerifyDeclaredValuesArrived(StatusWidget, TEXT("player_status"));
            const int32 CommandsArrived = VerifyDeclaredValuesArrived(CommandWidget, TEXT("commands"));
            TestTrue(
                *FString::Printf(
                    TEXT("M2: every migrated composite received at least one declared value from Lua (scene=%d, player_status=%d, commands=%d)"),
                    SceneArrived, StatusArrived, CommandsArrived),
                SceneArrived > 0 && StatusArrived > 0 && CommandsArrived > 0);

            // Accounting alone is not enough: the value must reach the primitive the
            // declaration binds. The leaf is resolved through the declaration itself,
            // so this does not hard-code any widget name.
            auto DeclaredTextLeafContent =
                [](UGV2DeclaredCompositeWidgetBase* Composite, const TCHAR* PropertyName) -> FText
            {
                if (Composite == nullptr)
                {
                    return FText::GetEmpty();
                }
                for (const FGV2DeclaredUiCapability& Declared : Composite->DeclaredCapabilities)
                {
                    if (Declared.PropertyName != FName(PropertyName)
                        || Declared.Kind != EGV2DeclaredUiCapabilityKind::Text)
                    {
                        continue;
                    }
                    if (UGV2TextWidgetBase* Leaf =
                            Cast<UGV2TextWidgetBase>(Composite->GetWidgetFromName(Declared.ChildWidgetName)))
                    {
                        return Leaf->GetTextContent();
                    }
                }
                return FText::GetEmpty();
            };

            const FText SceneContext = DeclaredTextLeafContent(SceneWidget, TEXT("context_text"));
            TestFalse(
                TEXT("M2: scene context text published by Lua reached its text primitive"),
                SceneContext.IsEmpty());
            const FText StatusName = DeclaredTextLeafContent(StatusWidget, TEXT("name"));
            TestFalse(
                TEXT("M2: player_status name published by Lua reached its text primitive"),
                StatusName.IsEmpty());

            TestNotNull(TEXT("LocationScreen contains SceneView component"), SceneWidget);
            UGV2ListViewWidgetBase* CharRep = SceneWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(SceneWidget->GetWidgetFromName(TEXT("CharacterRepeater")))
                : nullptr;
            if (CharRep != nullptr)
            {
                TestEqual(TEXT("Initial tavern scene has 1 character"), CharRep->GetEntryCount(), 1);
                TestNotNull(TEXT("Initial tavern character widget matches keeper"), CharRep->GetEntryWidget(FName(TEXT("tavern_keeper"))));
            }

            // 2. Find travel button to market in CommandPanel and submit interaction
            TestNotNull(TEXT("LocationScreen contains CommandPanel component"), CommandWidget);
            UGV2ListViewWidgetBase* CmdRep = CommandWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                : nullptr;
            if (CmdRep != nullptr)
            {
                UGV2ButtonWidgetBase* TravelMarketBtn = Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))));
                TestNotNull(TEXT("Travel to market button found in tavern CommandPanel"), TravelMarketBtn);
                if (TravelMarketBtn != nullptr)
                {
                    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
                    TestEqual(TEXT("Travel to market interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                }
            }

            // 3. Verify Market presentation has 0 characters
            UGV2ScreenWidgetBase* MarketScreen = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Market LocationScreen is presented"), MarketScreen);
            if (MarketScreen != nullptr)
            {
                UGV2DeclaredCompositeWidgetBase* MarketScene = nullptr;
                UGV2DeclaredCompositeWidgetBase* MarketCommandsWidget = nullptr;
                if (MarketScreen->WidgetTree != nullptr)
                {
                    MarketScreen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            MarketScene = Scene;
                        }
                        else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                        {
                            MarketCommandsWidget = Cmd;
                        }
                    });
                }
                TestNotNull(TEXT("Market Screen contains SceneView component"), MarketScene);
                UGV2ListViewWidgetBase* MarketCharRep = MarketScene != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(MarketScene->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (MarketCharRep != nullptr)
                {
                    TestEqual(TEXT("Market scene has 0 characters"), MarketCharRep->GetEntryCount(), 0);
                }

                // 4. Travel back to tavern
                TestNotNull(TEXT("Market Screen contains CommandPanel component"), MarketCommandsWidget);
                UGV2ListViewWidgetBase* MarketCmdRep = MarketCommandsWidget != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(MarketCommandsWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                    : nullptr;
                if (MarketCmdRep != nullptr)
                {
                    UGV2ButtonWidgetBase* TravelTavernBtn = Cast<UGV2ButtonWidgetBase>(MarketCmdRep->GetEntryWidget(FName(TEXT("travel_city_tavern"))));
                    TestNotNull(TEXT("Travel to tavern button found in market CommandPanel"), TravelTavernBtn);
                    if (TravelTavernBtn != nullptr)
                    {
                        const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelTavernBtn->GetBindingHandle(), {});
                        TestEqual(TEXT("Travel back to tavern interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                    }
                }
            }

            // 5. Verify returned Tavern has 1 character restored
            UGV2ScreenWidgetBase* TavernScreen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Returned Tavern LocationScreen is presented"), TavernScreen2);
            if (TavernScreen2 != nullptr)
            {
                UGV2DeclaredCompositeWidgetBase* TavernScene2 = nullptr;
                if (TavernScreen2->WidgetTree != nullptr)
                {
                    TavernScreen2->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            TavernScene2 = Scene;
                        }
                    });
                }
                TestNotNull(TEXT("Returned Tavern Screen contains SceneView component"), TavernScene2);
                UGV2ListViewWidgetBase* CharRep2 = TavernScene2 != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(TavernScene2->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (CharRep2 != nullptr)
                {
                    TestEqual(TEXT("Returned tavern scene has 1 character"), CharRep2->GetEntryCount(), 1);
                    TestNotNull(TEXT("Returned tavern character widget matches keeper"), CharRep2->GetEntryWidget(FName(TEXT("tavern_keeper"))));
                }

                // 6. CFC-11: Verify missing parent field 'scene' is rejected by PrepareScreenFields
                FGV2ScreenMutationPlan IncompletePlan;
                FString IncompleteError;
                const bool bPreparedIncomplete = TavernScreen2->PrepareScreenFields(
                    {}, IncompletePlan, IncompleteError);
                TestFalse(TEXT("CFC-11: PrepareScreenFields rejects payload missing configured hosts"), bPreparedIncomplete);
                TestTrue(TEXT("CFC-11: Incomplete error mentions host has no value"),
                    IncompleteError.Contains(TEXT("has no value in the payload")));
            }
        }
        Runtime->EndSession();
    }

    return true;
}


// =========================================================================
// UIH-15: LocationScreen Transition Contract Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenTransitionContractTest,
    "GV2.Runtime.UI.LocationScreenTransitionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenTransitionContractTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("RuntimeSubsystem initialized"), Runtime);

    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);

        const FString GameNs = TEXT("r") TEXT("h");
        const FString TavernTitleTextId = GameNs + TEXT(":text.location.tavern.title");
        const FString MarketTitleTextId = GameNs + TEXT(":text.location.market.title");
        const FString TavernBgResId = GameNs + TEXT(":resource.location.tavern");
        const FString MarketBgResId = GameNs + TEXT(":resource.location.market");

        // 1. Initial screen in Tavern
        UGV2ScreenWidgetBase* Screen1 = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("Initial location screen presented in Tavern"), Screen1);

        if (Screen1 != nullptr)
        {
            // 2. Perform location transition specifically to Market (Tavern -> Market)
            // Find the explicit travel command button binding handle for Market
            TArray<UWidget*> ChildWidgets;
            Screen1->WidgetTree->GetAllWidgets(ChildWidgets);
            FGV2UiBindingHandle TravelMarketHandle;

            for (UWidget* Child : ChildWidgets)
            {
                if (auto* CmdPanel = Cast<UGV2DeclaredCompositeWidgetBase>(Child); CmdPanel != nullptr && CmdPanel->GetHostIdentity() == FName(TEXT("commands")))
                {
                    if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(CmdPanel->GetWidgetFromName(TEXT("ButtonRepeater"))))
                    {
                        if (auto* Btn = Cast<UGV2ButtonWidgetBase>(Repeater->GetEntryWidget(FName(TEXT("travel_city_market")))))
                        {
                            TravelMarketHandle = Btn->GetBindingHandle();
                            break;
                        }
                    }
                }
            }

            TestTrue(TEXT("Found travel_city_market button binding in Tavern screen"), TravelMarketHandle.IsValid());

            if (TravelMarketHandle.IsValid())
            {
                const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelMarketHandle, {});
                TestEqual(TEXT("Travel command interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
            }

            // 3. Screen instance reuse verification
            UGV2ScreenWidgetBase* Screen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Location screen active after travel to market"), Screen2);
            TestEqual(TEXT("Screen instance is preserved and reused across location transition"), Screen1, Screen2);

            // 4. Verify location title and background updated to Market
            TArray<UWidget*> MarketWidgets;
            Screen2->WidgetTree->GetAllWidgets(MarketWidgets);
            bool bFoundMarketTopBar = false;
            bool bFoundMarketScene = false;
            bool bFoundMarketCommands = false;
            bool bTavernTravelButtonPresentInMarket = false;

            for (UWidget* Child : MarketWidgets)
            {
                if (Child != nullptr)
                {
                    if (auto* TopBar = Cast<UGV2DeclaredCompositeWidgetBase>(Child); TopBar != nullptr && TopBar->GetHostIdentity() == FName(TEXT("top_bar")))
                    {
                        bFoundMarketTopBar = true;
                    }
                    else if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Child); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                    {
                        bFoundMarketScene = true;
                        UGV2ImageWidgetBase* Bg = Cast<UGV2ImageWidgetBase>(Scene->GetWidgetFromName(FName(TEXT("Background"))));
                        if (Bg != nullptr)
                        {
                            TestEqual(
                                TEXT("CCF-21: Market Scene background resource ID"),
                                Bg->GetAppliedResourceId(),
                                MarketBgResId);
                        }
                    }
                    else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Child); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                    {
                        bFoundMarketCommands = true;
                        if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(Cmd->GetWidgetFromName(TEXT("ButtonRepeater"))))
                        {
                            bTavernTravelButtonPresentInMarket = Repeater->GetEntryWidget(FName(TEXT("travel_city_market"))) != nullptr;
                        }
                    }
                }
            }

            TestTrue(TEXT("CCF-21: Market TopBar verified"), bFoundMarketTopBar);
            TestTrue(TEXT("CCF-21: Market Scene verified"), bFoundMarketScene);
            TestTrue(TEXT("CCF-21: Market Commands field captured"), bFoundMarketCommands);
            TestFalse(TEXT("CCF-21: Old Tavern travel command button removed in Market"), bTavernTravelButtonPresentInMarket);
        }

        Runtime->EndSession();
    }

    return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
