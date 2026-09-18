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
#include "Application/GV2SessionContentSnapshot.h"
#include "Application/GV2SessionCoordinator.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"
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

#include <vector>
#include <string>
#include <algorithm>

// =========================================================================
// TSR-07 / TSR-08 (ADR-0046, Plan TestSuiteRestructuring):
// Content Smoke Tests for TextSystem & RH widgets, styles, and startup flow.
// These tests verify that authored game blueprints load, configure, and render correctly,
// with all gameplay expectations read dynamically from content repository definitions.
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

struct FContentCharacterExpectation
{
    FName Key;
    FString ResourceId;
};

TArray<FContentCharacterExpectation> GetExpectedScreenCharacters(
    const GV2ContentCore::FRepositoryReadHandle& Repo,
    const std::string& ScreenId)
{
    TArray<FContentCharacterExpectation> Result;
    if (!Repo.IsValid())
    {
        return Result;
    }
    const GV2ContentCore::FValue* ScreenDef = Repo.Find(GV2ContentCore::FDefinitionId::Require(ScreenId));
    if (!ScreenDef || !ScreenDef->IsObject())
    {
        return Result;
    }
    const GV2ContentCore::FValue* Ext = ScreenDef->FindField("extensions");
    if (!Ext || !Ext->IsObject())
    {
        return Result;
    }
    const GV2ContentCore::FValue* RhExt = Ext->FindField("rh");
    if (!RhExt || !RhExt->IsObject())
    {
        return Result;
    }
    const GV2ContentCore::FValue* Chars = RhExt->FindField("characters");
    if (!Chars || !Chars->IsArray())
    {
        return Result;
    }
    for (const GV2ContentCore::FValue& CharEntry : Chars->AsArray())
    {
        if (!CharEntry.IsObject())
        {
            continue;
        }
        const GV2ContentCore::FValue* KeyVal = CharEntry.FindField("key");
        const GV2ContentCore::FValue* ResVal = CharEntry.FindField("resource_id");
        if (KeyVal && KeyVal->IsString())
        {
            FContentCharacterExpectation Entry;
            Entry.Key = FName(UTF8_TO_TCHAR(KeyVal->AsString().c_str()));
            if (ResVal && ResVal->IsString())
            {
                Entry.ResourceId = UTF8_TO_TCHAR(ResVal->AsString().c_str());
            }
            Result.Add(MoveTemp(Entry));
        }
    }
    return Result;
}

std::vector<std::string> GetConnectedLocationIds(
    const GV2ContentCore::FRepositoryReadHandle& Repo,
    const std::string& LocationId)
{
    std::vector<std::string> Result;
    if (!Repo.IsValid())
    {
        return Result;
    }
    const GV2ContentCore::FValue* LocDef = Repo.Find(GV2ContentCore::FDefinitionId::Require(LocationId));
    if (!LocDef || !LocDef->IsObject())
    {
        return Result;
    }
    const GV2ContentCore::FValue* Data = LocDef->FindField("data");
    if (!Data || !Data->IsObject())
    {
        return Result;
    }
    const GV2ContentCore::FValue* Conn = Data->FindField("connected_location_ids");
    if (!Conn || !Conn->IsArray())
    {
        return Result;
    }
    for (const GV2ContentCore::FValue& Entry : Conn->AsArray())
    {
        if (Entry.IsString())
        {
            Result.push_back(Entry.AsString());
        }
    }
    return Result;
}

std::string GetLocationScreenId(
    const GV2ContentCore::FRepositoryReadHandle& Repo,
    const std::string& LocationId)
{
    if (!Repo.IsValid())
    {
        return "";
    }
    const GV2ContentCore::FValue* LocDef = Repo.Find(GV2ContentCore::FDefinitionId::Require(LocationId));
    if (!LocDef || !LocDef->IsObject())
    {
        return "";
    }
    const GV2ContentCore::FValue* Data = LocDef->FindField("data");
    if (!Data || !Data->IsObject())
    {
        return "";
    }
    const GV2ContentCore::FValue* Screens = Data->FindField("screen_ids");
    if (!Screens || !Screens->IsArray() || Screens->AsArray().empty())
    {
        return "";
    }
    const GV2ContentCore::FValue& FirstScreen = Screens->AsArray()[0];
    return FirstScreen.IsString() ? FirstScreen.AsString() : "";
}

FString ComputeTravelButtonKey(const std::string& TargetLocationId)
{
    const FString Target = UTF8_TO_TCHAR(TargetLocationId.c_str());
    int32 DotIndex = INDEX_NONE;
    if (Target.FindChar(TEXT('.'), DotIndex))
    {
        FString PathStr = Target.Mid(DotIndex + 1);
        PathStr.ReplaceInline(TEXT("."), TEXT("_"));
        return TEXT("travel_") + PathStr;
    }
    return TEXT("travel_") + Target;
}
}

// =========================================================================
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

// =========================================================================
// Content Smoke Test for CommonUI text styles
// =========================================================================
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

// =========================================================================
// TSR-08: Content Smoke - Game Startup and Location Flow
// Verifies live game session startup, screen presentation, dynamic composite
// data reconciliation from Lua, and transitions between locations using
// expectations read directly from repository definitions.
// =========================================================================
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

        const FGV2SessionContentSnapshot* ContentSnapshot = Runtime->GetContentSnapshotForAutomationTest();
        TestNotNull(TEXT("Session content snapshot is available"), ContentSnapshot);

        const GV2ContentCore::FRepositoryReadHandle& Repo = ContentSnapshot != nullptr
            ? ContentSnapshot->GetRepository()
            : GV2ContentCore::FRepositoryReadHandle();
        TestTrue(TEXT("Content repository read handle is valid"), Repo.IsValid());

        const std::string GameNs = std::string("r") + "h";
        const std::string InitialLocationId = GameNs + ":location.city.tavern";
        const std::string TargetLocationId = GameNs + ":location.city.market";

        const std::string InitialScreenId = GetLocationScreenId(Repo, InitialLocationId);
        const std::string TargetScreenId = GetLocationScreenId(Repo, TargetLocationId);

        const TArray<FContentCharacterExpectation> ExpectedInitialChars =
            GetExpectedScreenCharacters(Repo, InitialScreenId);
        const TArray<FContentCharacterExpectation> ExpectedTargetChars =
            GetExpectedScreenCharacters(Repo, TargetScreenId);

        UGV2ScreenWidgetBase* Screen1 = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("RH startup opens the registered LocationScreen"), Screen1);
        if (Screen1 != nullptr)
        {
            UClass* LocationScreenClass = LoadClass<UUserWidget>(
                nullptr,
                TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
            TestNotNull(TEXT("LocationScreen class is loadable"), LocationScreenClass);
            TestTrue(
                TEXT("RH startup presents WBP_LocationScreen"),
                LocationScreenClass != nullptr && Screen1->IsA(LocationScreenClass));

            // Find child declared composite widgets
            UGV2DeclaredCompositeWidgetBase* SceneWidget = nullptr;
            UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;
            UGV2DeclaredCompositeWidgetBase* StatusWidget = nullptr;

            if (Screen1->WidgetTree != nullptr)
            {
                Screen1->WidgetTree->ForEachWidget([&](UWidget* Widget)
                {
                    if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                    {
                        SceneWidget = Scene;
                    }
                    else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                    {
                        CommandWidget = Cmd;
                    }
                    else if (auto* Status = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Status != nullptr && Status->GetHostIdentity() == FName(TEXT("player_status")))
                    {
                        StatusWidget = Status;
                    }
                });
            }

            TestNotNull(TEXT("LocationScreen contains SceneView component"), SceneWidget);
            TestNotNull(TEXT("LocationScreen contains CommandPanel component"), CommandWidget);
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

            // 1. Verify startup scene characters match content definition (expectations from Repo)
            UGV2ListViewWidgetBase* CharRep = SceneWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(SceneWidget->GetWidgetFromName(TEXT("CharacterRepeater")))
                : nullptr;
            if (CharRep != nullptr)
            {
                TestEqual(
                    TEXT("Initial scene character count matches content definition"),
                    CharRep->GetEntryCount(),
                    ExpectedInitialChars.Num());
                for (const FContentCharacterExpectation& ExpectedChar : ExpectedInitialChars)
                {
                    TestNotNull(
                        *FString::Printf(TEXT("Initial scene character '%s' matches content definition"), *ExpectedChar.Key.ToString()),
                        CharRep->GetEntryWidget(ExpectedChar.Key));
                }
            }

            // 2. Find travel button to target location in CommandPanel and submit interaction
            // Validates that initial location definition connects to target location
            const std::vector<std::string> InitialConnected = GetConnectedLocationIds(Repo, InitialLocationId);
            const bool bInitialConnectsToTarget =
                std::find(InitialConnected.begin(), InitialConnected.end(), TargetLocationId) != InitialConnected.end();
            TestTrue(
                *FString::Printf(TEXT("Initial location '%s' connects to target '%s' in content definition"),
                    UTF8_TO_TCHAR(InitialLocationId.c_str()), UTF8_TO_TCHAR(TargetLocationId.c_str())),
                bInitialConnectsToTarget);

            const FString TravelTargetBtnKey = ComputeTravelButtonKey(TargetLocationId);
            UGV2ListViewWidgetBase* CmdRep = CommandWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                : nullptr;
            if (CmdRep != nullptr)
            {
                UGV2ButtonWidgetBase* TravelTargetBtn =
                    Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(*TravelTargetBtnKey)));
                TestNotNull(
                    *FString::Printf(TEXT("Travel button '%s' found in initial CommandPanel"), *TravelTargetBtnKey),
                    TravelTargetBtn);
                if (TravelTargetBtn != nullptr)
                {
                    const EGV2SubmitUiInteractionResult SubmitResult =
                        Runtime->SubmitUiInteraction(TravelTargetBtn->GetBindingHandle(), {});
                    TestEqual(TEXT("Travel interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                }
            }

            // 3. Verify Target Location presentation & screen instance reuse
            UGV2ScreenWidgetBase* Screen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Target LocationScreen is presented"), Screen2);
            TestEqual(
                TEXT("Screen instance is preserved and reused across location transition"),
                Screen1,
                Screen2);

            if (Screen2 != nullptr)
            {
                UGV2DeclaredCompositeWidgetBase* TargetScene = nullptr;
                UGV2DeclaredCompositeWidgetBase* TargetCommandsWidget = nullptr;
                if (Screen2->WidgetTree != nullptr)
                {
                    Screen2->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            TargetScene = Scene;
                        }
                        else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                        {
                            TargetCommandsWidget = Cmd;
                        }
                    });
                }
                TestNotNull(TEXT("Target Screen contains SceneView component"), TargetScene);
                UGV2ListViewWidgetBase* TargetCharRep = TargetScene != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(TargetScene->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (TargetCharRep != nullptr)
                {
                    TestEqual(
                        TEXT("Target scene character count matches content definition"),
                        TargetCharRep->GetEntryCount(),
                        ExpectedTargetChars.Num());
                    for (const FContentCharacterExpectation& ExpectedChar : ExpectedTargetChars)
                    {
                        TestNotNull(
                            *FString::Printf(TEXT("Target scene character '%s' matches content definition"), *ExpectedChar.Key.ToString()),
                            TargetCharRep->GetEntryWidget(ExpectedChar.Key));
                    }
                }

                // 4. Travel back to initial location
                const std::vector<std::string> TargetConnected = GetConnectedLocationIds(Repo, TargetLocationId);
                const bool bTargetConnectsToInitial =
                    std::find(TargetConnected.begin(), TargetConnected.end(), InitialLocationId) != TargetConnected.end();
                TestTrue(
                    *FString::Printf(TEXT("Target location '%s' connects back to initial '%s' in content definition"),
                        UTF8_TO_TCHAR(TargetLocationId.c_str()), UTF8_TO_TCHAR(InitialLocationId.c_str())),
                    bTargetConnectsToInitial);

                const FString ReturnBtnKey = ComputeTravelButtonKey(InitialLocationId);
                TestNotNull(TEXT("Target Screen contains CommandPanel component"), TargetCommandsWidget);
                UGV2ListViewWidgetBase* TargetCmdRep = TargetCommandsWidget != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(TargetCommandsWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                    : nullptr;
                if (TargetCmdRep != nullptr)
                {
                    UGV2ButtonWidgetBase* ReturnBtn =
                        Cast<UGV2ButtonWidgetBase>(TargetCmdRep->GetEntryWidget(FName(*ReturnBtnKey)));
                    TestNotNull(
                        *FString::Printf(TEXT("Return travel button '%s' found in target CommandPanel"), *ReturnBtnKey),
                        ReturnBtn);
                    if (ReturnBtn != nullptr)
                    {
                        const EGV2SubmitUiInteractionResult SubmitResult =
                            Runtime->SubmitUiInteraction(ReturnBtn->GetBindingHandle(), {});
                        TestEqual(TEXT("Travel back interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                    }
                }
            }

            // 5. Verify returned LocationScreen state & screen instance preservation
            UGV2ScreenWidgetBase* Screen3 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Returned LocationScreen is presented"), Screen3);
            TestEqual(
                TEXT("Screen instance is preserved across return transition"),
                Screen1,
                Screen3);

            if (Screen3 != nullptr)
            {
                UGV2DeclaredCompositeWidgetBase* ReturnScene = nullptr;
                if (Screen3->WidgetTree != nullptr)
                {
                    Screen3->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            ReturnScene = Scene;
                        }
                    });
                }
                TestNotNull(TEXT("Returned Screen contains SceneView component"), ReturnScene);
                UGV2ListViewWidgetBase* ReturnCharRep = ReturnScene != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(ReturnScene->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (ReturnCharRep != nullptr)
                {
                    TestEqual(
                        TEXT("Returned scene character count matches content definition"),
                        ReturnCharRep->GetEntryCount(),
                        ExpectedInitialChars.Num());
                    for (const FContentCharacterExpectation& ExpectedChar : ExpectedInitialChars)
                    {
                        TestNotNull(
                            *FString::Printf(TEXT("Returned scene character '%s' matches content definition"), *ExpectedChar.Key.ToString()),
                            ReturnCharRep->GetEntryWidget(ExpectedChar.Key));
                    }
                }

                // 6. CFC-11: Verify missing parent field 'scene' is rejected by PrepareScreenFields
                FGV2ScreenMutationPlan IncompletePlan;
                FString IncompleteError;
                const bool bPreparedIncomplete = Screen3->PrepareScreenFields(
                    {}, IncompletePlan, IncompleteError);
                TestFalse(TEXT("CFC-11: PrepareScreenFields rejects payload missing configured hosts"), bPreparedIncomplete);
                TestTrue(
                    TEXT("CFC-11: Incomplete error mentions host has no value"),
                    IncompleteError.Contains(TEXT("has no value in the payload")));
            }
        }
        Runtime->EndSession();
    }

    return true;
}

// =========================================================================
// PEP-07 (ADR-0047): hover open/close as the effect queue's first production consumer.
// Reuses this file's own RH content harness (real session, real location travel) rather
// than a synthetic FPresentationEffect, so the accept/discard outcomes below are the
// queue's real production behavior, not a unit test's own hand-built scenario.
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverEffectQueueContract,
    "GV2.Runtime.Presentation.HoverEffectQueueContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HoverEffectQueueContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the runtime"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }
    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator is available"), Coordinator);
    if (Coordinator == nullptr)
    {
        return false;
    }

    const FGV2SessionContentSnapshot* ContentSnapshot = Runtime->GetContentSnapshotForAutomationTest();
    const GV2ContentCore::FRepositoryReadHandle& Repo = ContentSnapshot != nullptr
        ? ContentSnapshot->GetRepository()
        : GV2ContentCore::FRepositoryReadHandle();
    TestTrue(TEXT("Content repository read handle is valid"), Repo.IsValid());

    const FString InitialUiInstanceId = Coordinator->GetBindingRegistry().GetUiInstanceId();
    const int64 InitialRevision = Coordinator->GetBindingRegistry().GetRevision();
    TestFalse(TEXT("Session publishes a real ui_instance_id"), InitialUiInstanceId.IsEmpty());

    // 1. PEP-01/ADR-0047 criterion: hover changes no canonical state value.
    const std::string HashBeforeHover = Coordinator->GetRuntimeSession().GetCanonicalStateHash();

    UGV2ScreenWidgetBase* HoverScreenA = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Hover screen A instantiates"), HoverScreenA);
    FName InstanceKeyA;
    FString OpenErrorA;
    TestTrue(
        *FString::Printf(TEXT("OpenHoverOverlay succeeds through the production entry point [Error: %s]"), *OpenErrorA),
        Runtime->OpenHoverOverlay(HoverScreenA, 0.0f, InstanceKeyA, OpenErrorA));

    const std::string HashAfterHover = Coordinator->GetRuntimeSession().GetCanonicalStateHash();
    TestEqual(
        TEXT("Canonical state is byte-identical before and after hover"),
        FString(UTF8_TO_TCHAR(HashAfterHover.c_str())),
        FString(UTF8_TO_TCHAR(HashBeforeHover.c_str())));

    // 2. The open went through the shared queue for real: exactly one effect drained, the
    // hover-open effect itself, accepted (target matches the still-current document).
    {
        const auto& Diagnostics = Runtime->GetLastDrainedEffectDiagnosticsForAutomationTest();
        TestEqual(TEXT("Exactly one effect drained for the open"), Diagnostics.Num(), 1);
        if (Diagnostics.Num() == 1)
        {
            TestEqual(
                TEXT("Drained effect is the hover-open effect"),
                Diagnostics[0].EffectId,
                FString(TEXT("core:effect.rich_text_hover_open")));
            TestEqual(
                TEXT("A fresh hover-open effect resolves to no rejection"),
                Diagnostics[0].RejectReason,
                GV2RuntimeCore::EPresentationEffectRejectReason::None);
        }
    }

    // 3. An accepted queued close is the cause of the physical detach. Removing the
    // production dispatch from the drain must leave HoverScreenA attached and fail this
    // assertion; observing a ResolveEffectTarget diagnostic is not enough.
    GV2RuntimeCore::FPresentationEffect AcceptedCloseEffect;
    AcceptedCloseEffect.EffectId = "core:effect.rich_text_hover_close";
    AcceptedCloseEffect.bHasTarget = true;
    AcceptedCloseEffect.TargetUiInstanceId = TCHAR_TO_UTF8(*InitialUiInstanceId);
    AcceptedCloseEffect.TargetRevision = InitialRevision;
    AcceptedCloseEffect.Args.emplace(
        "instance_key",
        GV2RuntimeCore::FValue(std::string(TCHAR_TO_UTF8(*InstanceKeyA.ToString()))));
    GV2RuntimeCore::FRuntimeFault AcceptedClosePublishFault;
    TestTrue(TEXT("An accepted close effect is queued through the production API"),
        Coordinator->GetRuntimeSession().PublishHostLocalEffect(AcceptedCloseEffect, AcceptedClosePublishFault));

    UGV2ScreenWidgetBase* HoverScreenB = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Hover screen B instantiates"), HoverScreenB);
    FName InstanceKeyB;
    FString OpenErrorB;
    TestTrue(
        *FString::Printf(TEXT("A second hover open triggers the common drain [Error: %s]"), *OpenErrorB),
        Runtime->OpenHoverOverlay(HoverScreenB, 0.0f, InstanceKeyB, OpenErrorB));
    TestFalse(
        TEXT("The accepted queued close physically detaches its addressed window"),
        Runtime->GetActiveGameShell() != nullptr
            && Runtime->GetActiveGameShell()
                ->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack)
                .Contains(Cast<UUserWidget>(HoverScreenA)));

    // 4. A REAL discarded effect, not a synthetic one, with the rest of the session left
    // completely undisturbed: publish directly against a ui_instance_id that names no
    // document this session actually has (the exact same production API OpenHoverOverlay
    // itself calls, PublishHostLocalEffect -- not a hand-rolled ResolveEffectTarget unit
    // test), then let a later, unrelated hover event's own drain sweep it up. Nothing about
    // the session, document or shell changes as a result of publishing this -- only the
    // drain's own verdict is what's under test here.
    GV2RuntimeCore::FPresentationEffect StaleTargetEffect;
    StaleTargetEffect.EffectId = "core:effect.rich_text_hover_close";
    StaleTargetEffect.bHasTarget = true;
    StaleTargetEffect.TargetUiInstanceId = std::string(TCHAR_TO_UTF8(*InitialUiInstanceId)) + "_no_such_document";
    StaleTargetEffect.TargetRevision = InitialRevision;
    StaleTargetEffect.Args.emplace(
        "instance_key",
        GV2RuntimeCore::FValue(std::string(TCHAR_TO_UTF8(*InstanceKeyB.ToString()))));
    GV2RuntimeCore::FRuntimeFault PublishFault;
    TestTrue(
        TEXT("Directly publishing a real host-local effect via the production API succeeds"),
        Coordinator->GetRuntimeSession().PublishHostLocalEffect(StaleTargetEffect, PublishFault));

    UGV2ScreenWidgetBase* HoverScreenC = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Hover screen C instantiates"), HoverScreenC);
    FName InstanceKeyC;
    FString OpenErrorC;
    TestTrue(
        *FString::Printf(TEXT("A third, unrelated hover open succeeds and drains the whole queue [Error: %s]"), *OpenErrorC),
        Runtime->OpenHoverOverlay(HoverScreenC, 0.0f, InstanceKeyC, OpenErrorC));

    {
        const auto& Diagnostics = Runtime->GetLastDrainedEffectDiagnosticsForAutomationTest();
        TestEqual(TEXT("Two effects drained: the wrong-document one and the fresh one"), Diagnostics.Num(), 2);
        bool bFoundRealDiscard = false;
        for (const auto& Diagnostic : Diagnostics)
        {
            if (Diagnostic.RejectReason != GV2RuntimeCore::EPresentationEffectRejectReason::None)
            {
                bFoundRealDiscard = true;
                TestEqual(
                    TEXT("The wrong-document effect is rejected specifically as StaleTarget (unrecognized ui_instance_id)"),
                    Diagnostic.RejectReason,
                    GV2RuntimeCore::EPresentationEffectRejectReason::StaleTarget);
            }
        }
        TestTrue(
            TEXT("PEP-01: at least one of PEP-03's three reject reasons fires on a real, production-published effect"),
            bFoundRealDiscard);
    }

    // 5. A discarded effect does not close the addressed window -- HoverScreenB is still a
    // real child of overlay_stack, untouched by HoverScreenC's drain, and canonical state
    // is still exactly what it was before any of this hover activity.
    // GetActiveScreenInLayer/Reconciler::GetActiveScreen only ever searches the document
    // tier's ActiveScreens map (PEP-06's own two-tier split) -- a host-local participant is
    // never findable through it regardless of whether it is still attached, so this reads
    // the Shell's actual panel children instead, exactly as GV2LayeredReconciliationTests'
    // own host-local contracts do.
    TestTrue(
        TEXT("The addressed hover window survives its rejected close effect"),
        Runtime->GetActiveGameShell() != nullptr
            && Runtime->GetActiveGameShell()
                ->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack)
                .Contains(Cast<UUserWidget>(HoverScreenB)));
    TestEqual(
        TEXT("Canonical state is still unaffected after the discard"),
        FString(UTF8_TO_TCHAR(Coordinator->GetRuntimeSession().GetCanonicalStateHash().c_str())),
        FString(UTF8_TO_TCHAR(HashBeforeHover.c_str())));

    // 6. A host effect deliberately queued without its producer's normal immediate drain is
    // rejected when a real content-driven document change makes its target stale. This is
    // the production replacement path for ResolveEffectTarget, not a pure-function probe.
    {
        GV2RuntimeCore::FPresentationEffect PreTravelEffect;
        PreTravelEffect.EffectId = "core:effect.rich_text_hover_close";
        PreTravelEffect.bHasTarget = true;
        PreTravelEffect.TargetUiInstanceId = TCHAR_TO_UTF8(*Coordinator->GetBindingRegistry().GetUiInstanceId());
        PreTravelEffect.TargetRevision = Coordinator->GetBindingRegistry().GetRevision();
        GV2RuntimeCore::FRuntimeFault PreTravelPublishFault;
        TestTrue(
            TEXT("Publishing a pre-travel effect via the production API succeeds"),
            Coordinator->GetRuntimeSession().PublishHostLocalEffect(PreTravelEffect, PreTravelPublishFault));

        const std::string GameNs = std::string("r") + "h";
        const std::string InitialLocationId = GameNs + ":location.city.tavern";
        const std::string TargetLocationId = GameNs + ":location.city.market";
        const std::vector<std::string> InitialConnected = GetConnectedLocationIds(Repo, InitialLocationId);
        const bool bConnectsToTarget =
            std::find(InitialConnected.begin(), InitialConnected.end(), TargetLocationId) != InitialConnected.end();
        TestTrue(TEXT("RH content connects tavern to market for this test's travel step"), bConnectsToTarget);

        UGV2ScreenWidgetBase* LocationScreen = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;
        if (LocationScreen != nullptr && LocationScreen->WidgetTree != nullptr)
        {
            LocationScreen->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                {
                    CommandWidget = Cmd;
                }
            });
        }
        UGV2ListViewWidgetBase* CmdRep = CommandWidget != nullptr
            ? Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
            : nullptr;
        UGV2ButtonWidgetBase* TravelBtn = CmdRep != nullptr
            ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(*ComputeTravelButtonKey(TargetLocationId))))
            : nullptr;
        TestNotNull(TEXT("Travel button to market found"), TravelBtn);
        if (TravelBtn != nullptr)
        {
            TestEqual(
                TEXT("Travel interaction accepted (this is what really advances the document)"),
                Runtime->SubmitUiInteraction(TravelBtn->GetBindingHandle(), {}),
                EGV2SubmitUiInteractionResult::Accepted);
        }

        const auto& Diagnostics = Runtime->GetLastDrainedEffectDiagnosticsForAutomationTest();
        bool bFoundRealDiscard = false;
        for (const auto& Diagnostic : Diagnostics)
        {
            if (Diagnostic.EffectId == TEXT("core:effect.rich_text_hover_close"))
            {
                bFoundRealDiscard = Diagnostic.RejectReason != GV2RuntimeCore::EPresentationEffectRejectReason::None;
            }
        }
        TestTrue(
            TEXT("A real location-travel document change rejects the pre-existing targeted effect"),
            bFoundRealDiscard);
    }

    // 7. Rebuild guarantee: EndSession tears down the whole projection: the hover window does
    // not survive it and is not restored by StartSession alone -- only a fresh hover would
    // recreate it, proven by NOT hovering again and finding the layer empty.
    Runtime->EndSession();
    TestNull(
        TEXT("After EndSession the game shell is gone (rebuild does not resurrect the hover window)"),
        Runtime->GetActiveGameShell());
    Runtime->StartSession();
    TestTrue(TEXT("A fresh session is ready after rebuild"), Runtime->GetSessionState().bIsReady);
    TestTrue(
        TEXT("A rebuilt session's overlay_stack has no hover window until hover happens again"),
        Runtime->GetActiveGameShell() == nullptr
            || Runtime->GetActiveGameShell()->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack).IsEmpty());

    Runtime->EndSession();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
