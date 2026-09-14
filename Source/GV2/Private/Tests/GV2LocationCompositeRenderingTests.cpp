#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

#include "Widgets/SVirtualWindow.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/WrapBox.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "Application/GV2ScreenFieldMaterializer.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2ScrollAreaWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2TextPipelineHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2LayoutConstants.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiMutationPlan.h"
#include "GV2ContentCore/UiSchema.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "Tests/GV2PresentationTestFixtures.h"

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

// =========================================================================
// GLS-14: Resolution Matrix Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenResolutionMatrixTest,
    "GV2.Runtime.Presentation.LocationScreenResolutionMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenResolutionMatrixTest::RunTest(const FString& Parameters)
{
    struct FResolutionTestCase
    {
        FIntPoint Resolution;
        FString Name;
        float ExpectedTextScale;
        bool bIsUltrawide;
    };

    const TArray<FResolutionTestCase> TestMatrix = {
        { FIntPoint(3840, 2160), TEXT("4K UHD (3840x2160, 16:9)"), 1.60f, false },
        { FIntPoint(2560, 1440), TEXT("QHD (2560x1440, 16:9)"), 1.25f, false },
        { FIntPoint(1920, 1080), TEXT("Full HD (1920x1080, 16:9)"), 1.00f, false },
        { FIntPoint(1280, 720),  TEXT("HD (1280x720, 16:9)"), 0.85f, false },
        { FIntPoint(3440, 1440), TEXT("UWQHD (3440x1440, 21:9)"), 1.25f, true },
        { FIntPoint(2560, 1080), TEXT("UWFHD (2560x1080, 21:9)"), 1.00f, true },
    };

    UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
    TestNotNull(TEXT("UI Theme is available"), Theme);

    for (const FResolutionTestCase& TestCase : TestMatrix)
    {
        const float ViewportWidth = static_cast<float>(TestCase.Resolution.X);
        const float ViewportHeight = static_cast<float>(TestCase.Resolution.Y);
        const float AspectRatio = ViewportWidth / ViewportHeight;

        if (Theme != nullptr)
        {
            const float Scale = Theme->EvaluateTextScale(ViewportHeight);
            TestTrue(
                FString::Printf(TEXT("[%s] Text scale is within expected range (%f)"), *TestCase.Name, Scale),
                FMath::IsNearlyEqual(Scale, TestCase.ExpectedTextScale, 0.08f));

            const float EffectiveSmall = Theme->GetEffectiveFontSize(TEXT("small"), ViewportHeight);
            const float EffectiveDefault = Theme->GetEffectiveFontSize(TEXT("default"), ViewportHeight);
            const float EffectiveTitle = Theme->GetEffectiveFontSize(TEXT("title"), ViewportHeight);

            TestTrue(
                FString::Printf(TEXT("[%s] Small font >= MinReadableFontSize (%f >= %f)"), *TestCase.Name, EffectiveSmall, Theme->MinReadableFontSize),
                EffectiveSmall >= Theme->MinReadableFontSize);
            TestTrue(
                FString::Printf(TEXT("[%s] Default font > Small font (%f > %f)"), *TestCase.Name, EffectiveDefault, EffectiveSmall),
                EffectiveDefault > EffectiveSmall);
            TestTrue(
                FString::Printf(TEXT("[%s] Title font > Default font (%f > %f)"), *TestCase.Name, EffectiveTitle, EffectiveDefault),
                EffectiveTitle > EffectiveDefault);
        }

        if (TestCase.bIsUltrawide)
        {
            TestTrue(
                FString::Printf(TEXT("[%s] Aspect ratio is ~2.37 (21:9)"), *TestCase.Name),
                AspectRatio > 2.0f);
            const float MaxPlayerStatusWidthRatio = 0.35f;
            const float MinSceneWidthRatio = 0.60f;
            TestTrue(
                FString::Printf(TEXT("[%s] Scene width ratio is majority of screen"), *TestCase.Name),
                MinSceneWidthRatio > MaxPlayerStatusWidthRatio);
        }
        else
        {
            TestTrue(
                FString::Printf(TEXT("[%s] Aspect ratio is 16:9 (~1.777)"), *TestCase.Name),
                FMath::IsNearlyEqual(AspectRatio, 16.0f / 9.0f, 0.01f));
        }

        if (TestCase.Resolution == FIntPoint(1280, 720))
        {
            TestTrue(TEXT("[1280x720] Min width is sufficient for layout"), ViewportWidth >= 1280.0f);
            TestTrue(TEXT("[1280x720] Min height is sufficient for vertical stacks"), ViewportHeight >= 720.0f);
        }
    }

    return true;
}


// =========================================================================
// UIH-01..04: Core Repeater & Composite Reconciliation Contract Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CoreRepeaterWidgetReconciliationTest,
    "GV2.Runtime.UI.CoreRepeaterWidgetReconciliation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CoreRepeaterWidgetReconciliationTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured theme is available for prepared text fixtures"), Theme);

    // 1. UIH-01: Test UGV2ListViewWidgetBase directly
    {
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UVerticalBox* Container = NewObject<UVerticalBox>(TestWorld);
        ListView->SetContainerPanel(Container);

        struct FTestItem
        {
            FName Key;
            FString Text;
        };

        const TArray<FTestItem> InitialItems = {
            { FName(TEXT("item_a")), TEXT("Item A") },
            { FName(TEXT("item_b")), TEXT("Item B") },
            { FName(TEXT("item_c")), TEXT("Item C") }
        };

        // Positive: Reconcile creates items in order
        bool bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            InitialItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });

        TestTrue(TEXT("Initial reconciliation succeeds"), bSuccess);
        TestEqual(TEXT("Entry count is 3"), ListView->GetEntryCount(), 3);
        TestEqual(TEXT("Container child count is 3"), Container->GetChildrenCount(), 3);

        UGV2ButtonWidgetBase* WidgetA = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_a")));
        UGV2ButtonWidgetBase* WidgetB = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_b")));
        UGV2ButtonWidgetBase* WidgetC = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_c")));
        TestNotNull(TEXT("Widget A exists"), WidgetA);
        TestNotNull(TEXT("Widget B exists"), WidgetB);
        TestNotNull(TEXT("Widget C exists"), WidgetC);

        // Positive: Reorder and remove C, add D -> reuse existing A and B
        const TArray<FTestItem> UpdatedItems = {
            { FName(TEXT("item_b")), TEXT("Item B") },
            { FName(TEXT("item_d")), TEXT("Item D") },
            { FName(TEXT("item_a")), TEXT("Item A") }
        };

        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            UpdatedItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });

        TestTrue(TEXT("Updated reconciliation succeeds"), bSuccess);
        TestEqual(TEXT("Entry count is 3 after update"), ListView->GetEntryCount(), 3);
        TestEqual(TEXT("Widget B is reused (same pointer)"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_b"))), WidgetB);
        TestEqual(TEXT("Widget A is reused (same pointer)"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_a"))), WidgetA);
        TestNull(TEXT("Widget C is removed"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_c"))));
        TestNotNull(TEXT("Widget D is created"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_d"))));

        // Negative: Empty key rejected without modifying state
        const TArray<FTestItem> BadEmptyKey = {
            { FName(), TEXT("Bad Item") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadEmptyKey,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });
        TestFalse(TEXT("Empty key is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected empty key"), ListView->GetEntryCount(), 3);

        // Negative: Duplicate key rejected without modifying state
        const TArray<FTestItem> BadDuplicateKey = {
            { FName(TEXT("dup")), TEXT("Dup 1") },
            { FName(TEXT("dup")), TEXT("Dup 2") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadDuplicateKey,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });
        TestFalse(TEXT("Duplicate key is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected duplicate key"), ListView->GetEntryCount(), 3);

        // Negative: Failed apply item aborts without modifying state
        const TArray<FTestItem> BadApplyItems = {
            { FName(TEXT("item_x")), TEXT("Item X") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadApplyItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return false; });
        TestFalse(TEXT("Failed ApplyItem is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected apply"), ListView->GetEntryCount(), 3);

        // CCF-01 / CCF-02: Atomic reconciliation - failed candidate does not mutate live reused widgets
        {
            struct FTestProgressItem
            {
                FName Key;
                float Value = 0.0f;
            };

            UGV2ListViewWidgetBase* AtomicListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* AtomicContainer = NewObject<UVerticalBox>(TestWorld);
            AtomicListView->SetContainerPanel(AtomicContainer);

            const TArray<FTestProgressItem> AtomicInitial = {
                { FName(TEXT("item_a")), 10.0f },
                { FName(TEXT("item_b")), 20.0f }
            };

            bool bAtomicInit = AtomicListView->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestProgressItem>(
                AtomicInitial,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item) -> bool
                {
                    Widget.ApplyProgress(Item.Value);
                    return true;
                });
            TestTrue(TEXT("Atomic baseline reconciliation succeeds"), bAtomicInit);
            UGV2ProgressBarWidgetBase* ProgA = AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a")));
            UGV2ProgressBarWidgetBase* ProgB = AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b")));
            TestNotNull(TEXT("ProgA exists"), ProgA);
            TestNotNull(TEXT("ProgB exists"), ProgB);
            TestEqual(TEXT("ProgA baseline value is 10"), ProgA->GetProgress(), 10.0f);
            TestEqual(TEXT("ProgB baseline value is 20"), ProgB->GetProgress(), 20.0f);
            TestEqual(TEXT("Container child count is 2"), AtomicContainer->GetChildrenCount(), 2);

            // Attempt reconcile where item_a has valid 100.0f, but item_b is invalid (fails preflight)
            const TArray<FTestProgressItem> AtomicCandidate = {
                { FName(TEXT("item_a")), 100.0f },
                { FName(TEXT("item_b")), -1.0f }
            };

            bool bAtomicCandidate = AtomicListView->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestProgressItem>(
                AtomicCandidate,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item) -> bool
                {
                    Widget.ApplyProgress(Item.Value);
                    return true;
                },
                [](const FTestProgressItem& Item) -> bool
                {
                    return Item.Value >= 0.0f; // item_b fails preflight
                });
            TestFalse(TEXT("Reconciliation fails due to invalid second candidate"), bAtomicCandidate);
            TestEqual(TEXT("ProgA pointer unchanged after failure"), AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a"))), ProgA);
            TestEqual(TEXT("ProgB pointer unchanged after failure"), AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b"))), ProgB);
            TestEqual(TEXT("ProgA value remains 10 (NOT mutated to 100)"), ProgA->GetProgress(), 10.0f);
            TestEqual(TEXT("ProgB value remains 20"), ProgB->GetProgress(), 20.0f);
            TestEqual(TEXT("Child count remains 2"), AtomicContainer->GetChildrenCount(), 2);
            TestEqual(TEXT("Child 0 remains ProgA"), AtomicContainer->GetChildAt(0), Cast<UWidget>(ProgA));
            TestEqual(TEXT("Child 1 remains ProgB"), AtomicContainer->GetChildAt(1), Cast<UWidget>(ProgB));
        }

        // CCF-01b: ReconcilePreparedEntries - two-phase reconciliation guarantees zero mutation on prepare failure
        {
            struct FTestProgressItem
            {
                FName Key;
                float Value = 0.0f;
            };

            struct FPreparedProgressData
            {
                float ProgressValue = 0.0f;
            };

            UGV2ListViewWidgetBase* PreparedListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* PreparedContainer = NewObject<UVerticalBox>(TestWorld);
            PreparedListView->SetContainerPanel(PreparedContainer);

            const TArray<FTestProgressItem> InitItems = {
                { FName(TEXT("item_a")), 10.0f },
                { FName(TEXT("item_b")), 20.0f }
            };

            bool bInit = PreparedListView->ReconcilePreparedEntries<UGV2ProgressBarWidgetBase, FTestProgressItem, FPreparedProgressData>(
                InitItems,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item, FPreparedProgressData& OutPrep) -> bool
                {
                    OutPrep.ProgressValue = Item.Value;
                    return true;
                },
                [](UGV2ProgressBarWidgetBase& Widget, const FPreparedProgressData& Prep)
                {
                    Widget.ApplyProgress(Prep.ProgressValue);
                });
            TestTrue(TEXT("ReconcilePrepared baseline succeeds"), bInit);
            UGV2ProgressBarWidgetBase* PrepA = PreparedListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a")));
            UGV2ProgressBarWidgetBase* PrepB = PreparedListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b")));
            TestNotNull(TEXT("PrepA exists"), PrepA);
            TestNotNull(TEXT("PrepB exists"), PrepB);
            TestEqual(TEXT("PrepA baseline value is 10"), PrepA->GetProgress(), 10.0f);
            TestEqual(TEXT("PrepB baseline value is 20"), PrepB->GetProgress(), 20.0f);

            // Candidate where item_a prepares successfully to 100.0f, but item_b fails during PrepareItem
            const TArray<FTestProgressItem> CandidateItems = {
                { FName(TEXT("item_a")), 100.0f },
                { FName(TEXT("item_b")), -999.0f }
            };

            bool bCandidate = PreparedListView->ReconcilePreparedEntries<UGV2ProgressBarWidgetBase, FTestProgressItem, FPreparedProgressData>(
                CandidateItems,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item, FPreparedProgressData& OutPrep) -> bool
                {
                    if (Item.Value < 0.0f)
                    {
                        return false; // item_b fails prepare!
                    }
                    OutPrep.ProgressValue = Item.Value;
                    return true;
                },
                [](UGV2ProgressBarWidgetBase& Widget, const FPreparedProgressData& Prep)
                {
                    Widget.ApplyProgress(Prep.ProgressValue);
                });
            TestFalse(TEXT("ReconcilePrepared fails when item_b prepare fails"), bCandidate);
            TestEqual(TEXT("PrepA value strictly remains 10 (NOT mutated to 100 on prepare failure)"), PrepA->GetProgress(), 10.0f);
            TestEqual(TEXT("PrepB value strictly remains 20"), PrepB->GetProgress(), 20.0f);
            TestEqual(TEXT("PreparedContainer child count remains 2"), PreparedContainer->GetChildrenCount(), 2);
        }

        // CCF-05: Complete Repeater Regression Matrix
        {
            UGV2ListViewWidgetBase* MatrixList = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* MatrixContainer = NewObject<UVerticalBox>(TestWorld);
            MatrixList->SetContainerPanel(MatrixContainer);

            auto ReconcileHelper = [&](const TArray<FTestItem>& Items, TFunction<bool(const FTestItem&)> CanApply = nullptr) -> bool
            {
                return MatrixList->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
                    Items,
                    [](const FTestItem& Item) { return Item.Key; },
                    [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
                    [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; },
                    CanApply);
            };

            // 1. empty -> 1
            const TArray<FTestItem> OneItem = { { FName(TEXT("k1")), TEXT("V1") } };
            TestTrue(TEXT("Matrix: empty -> 1 succeeds"), ReconcileHelper(OneItem));
            TestEqual(TEXT("Matrix: count is 1"), MatrixList->GetEntryCount(), 1);
            UGV2ButtonWidgetBase* W1 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1")));
            TestNotNull(TEXT("Matrix: W1 created"), W1);

            // 2. 1 -> 3
            const TArray<FTestItem> ThreeItems = {
                { FName(TEXT("k1")), TEXT("V1_new") },
                { FName(TEXT("k2")), TEXT("V2") },
                { FName(TEXT("k3")), TEXT("V3") }
            };
            TestTrue(TEXT("Matrix: 1 -> 3 succeeds"), ReconcileHelper(ThreeItems));
            TestEqual(TEXT("Matrix: count is 3"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W1 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            UGV2ButtonWidgetBase* W2 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2")));
            UGV2ButtonWidgetBase* W3 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3")));
            TestNotNull(TEXT("Matrix: W2 created"), W2);
            TestNotNull(TEXT("Matrix: W3 created"), W3);

            // 3. 3 -> 1
            TestTrue(TEXT("Matrix: 3 -> 1 succeeds"), ReconcileHelper(OneItem));
            TestEqual(TEXT("Matrix: count is 1"), MatrixList->GetEntryCount(), 1);
            TestEqual(TEXT("Matrix: W1 still reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestNull(TEXT("Matrix: W2 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))));
            TestNull(TEXT("Matrix: W3 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))));

            // Back to 3 items:
            TestTrue(TEXT("Matrix: re-expand to 3"), ReconcileHelper(ThreeItems));
            W2 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2")));
            W3 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3")));

            // 4. same keys + same order + changed values (reuse pointers)
            const TArray<FTestItem> ThreeItemsUpdated = {
                { FName(TEXT("k1")), TEXT("V1_updated") },
                { FName(TEXT("k2")), TEXT("V2_updated") },
                { FName(TEXT("k3")), TEXT("V3_updated") }
            };
            TestTrue(TEXT("Matrix: same keys same order updated succeeds"), ReconcileHelper(ThreeItemsUpdated));
            TestEqual(TEXT("Matrix: W1 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestEqual(TEXT("Matrix: W2 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))), W2);
            TestEqual(TEXT("Matrix: W3 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 5. same keys + reordered: { k3, k1, k2 }
            const TArray<FTestItem> ThreeItemsReordered = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") },
                { FName(TEXT("k2")), TEXT("V2") }
            };
            TestTrue(TEXT("Matrix: reorder succeeds"), ReconcileHelper(ThreeItemsReordered));
            TestEqual(TEXT("Matrix: W1 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestEqual(TEXT("Matrix: W2 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))), W2);
            TestEqual(TEXT("Matrix: W3 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TArray<UWidget*> Ordered = MatrixList->GetOrderedEntries();
            TestEqual(TEXT("Matrix: Order 0 is W3"), Ordered[0], Cast<UWidget>(W3));
            TestEqual(TEXT("Matrix: Order 1 is W1"), Ordered[1], Cast<UWidget>(W1));
            TestEqual(TEXT("Matrix: Order 2 is W2"), Ordered[2], Cast<UWidget>(W2));

            // 6. remove one key: remove k2 -> { k3, k1 }
            const TArray<FTestItem> TwoItems = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") }
            };
            TestTrue(TEXT("Matrix: remove one key succeeds"), ReconcileHelper(TwoItems));
            TestEqual(TEXT("Matrix: count is 2"), MatrixList->GetEntryCount(), 2);
            TestEqual(TEXT("Matrix: W3 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TestEqual(TEXT("Matrix: W1 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestNull(TEXT("Matrix: W2 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))));

            // 7. add one key: add k4 -> { k3, k1, k4 }
            const TArray<FTestItem> AddItem = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") },
                { FName(TEXT("k4")), TEXT("V4") }
            };
            TestTrue(TEXT("Matrix: add one key succeeds"), ReconcileHelper(AddItem));
            TestEqual(TEXT("Matrix: count is 3"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TestEqual(TEXT("Matrix: W1 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            UGV2ButtonWidgetBase* W4 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k4")));
            TestNotNull(TEXT("Matrix: W4 created"), W4);

            // 8. empty key (rejected, live state unchanged)
            const TArray<FTestItem> EmptyKeyItem = { { FName(), TEXT("Bad") } };
            TestFalse(TEXT("Matrix: empty key rejected"), ReconcileHelper(EmptyKeyItem));
            TestEqual(TEXT("Matrix: count unchanged after empty key"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained after empty key rejection"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 9. duplicate key (rejected, live state unchanged)
            const TArray<FTestItem> DupItems = {
                { FName(TEXT("dup")), TEXT("D1") },
                { FName(TEXT("dup")), TEXT("D2") }
            };
            TestFalse(TEXT("Matrix: duplicate key rejected"), ReconcileHelper(DupItems));
            TestEqual(TEXT("Matrix: count unchanged after dup key"), MatrixList->GetEntryCount(), 3);

            // 10. preflight failure (rejected, live state unchanged)
            const TArray<FTestItem> PreflightFailItems = {
                { FName(TEXT("k3")), TEXT("VALID") },
                { FName(TEXT("k1")), TEXT("INVALID") }
            };
            TestFalse(TEXT("Matrix: preflight rejection"), ReconcileHelper(PreflightFailItems, [](const FTestItem& Item) { return Item.Text != TEXT("INVALID"); }));
            TestEqual(TEXT("Matrix: count unchanged after preflight fail"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained after preflight fail"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 11. Clear / reset
            MatrixList->ClearEntries();
            TestEqual(TEXT("Matrix: count 0 after clear"), MatrixList->GetEntryCount(), 0);
            TestEqual(TEXT("Matrix: container child count 0 after clear"), MatrixContainer->GetChildrenCount(), 0);
        }
    }

    return true;
}

// =========================================================================
// UIH-02..04: Core Repeater Composite Integration Contract Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CoreRepeaterCompositeIntegrationTest,
    "GV2.Runtime.UI.CoreRepeaterCompositeIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CoreRepeaterCompositeIntegrationTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured theme is available for prepared text fixtures"), Theme);

    // 2. UIH-02: Test CommandPanel using Core Repeater
    {
        // Exercises UGV2ListViewWidgetBase::ReconcileEntries directly (the generic
        // low-level primitive the KeyedCollection consumer builds on), independent of
        // any widget-specific model type -- a plain local Key/Text/Binding fixture is
        // all this needs, matching UPP-30's retirement of FGV2ButtonViewModel.
        struct FTestButtonModel
        {
            FName Key;
            FGV2TextViewModel Text;
            FGV2UiBindingHandle Binding;
        };

        UGV2ListViewWidgetBase* Repeater = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UVerticalBox* CmdBox = NewObject<UVerticalBox>(TestWorld);
        Repeater->SetContainerPanel(CmdBox);
        UClass* TestButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));

        FTestButtonModel BtnA;
        BtnA.Key = FName(TEXT("btn_a"));
        if (Theme != nullptr)
        {
            BtnA.Text = MakeResolvedLiteralTextForTest(*Theme, TEXT("Action A"));
        }
        BtnA.Binding = FGV2UiBindingHandle::Create(TEXT("binding_a"));

        FTestButtonModel BtnB;
        BtnB.Key = FName(TEXT("btn_b"));
        if (Theme != nullptr)
        {
            BtnB.Text = MakeResolvedLiteralTextForTest(*Theme, TEXT("Action B"));
        }
        BtnB.Binding = FGV2UiBindingHandle::Create(TEXT("binding_b"));

        FTestButtonModel BtnC;
        BtnC.Key = FName(TEXT("btn_c"));
        if (Theme != nullptr)
        {
            BtnC.Text = MakeResolvedLiteralTextForTest(*Theme, TEXT("Action C"));
        }
        BtnC.Binding = FGV2UiBindingHandle::Create(TEXT("binding_c"));

        TestNotNull(TEXT("Repeater instantiated"), Repeater);
        if (Repeater != nullptr)
        {
            auto GetKey = [](const FTestButtonModel& B) { return B.Key; };
            auto CreateWidgetLambda = [TestWorld, TestButtonClass]() -> UGV2ButtonWidgetBase*
            {
                return TestButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, TestButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ButtonWidgetBase& Widget, const FTestButtonModel& Model)
            {
                Widget.SetKey(Model.Key);
                Widget.SetBindingHandle(Model.Binding);
                return Widget.ApplyText(Model.Text);
            };

            const TArray<FTestButtonModel> InitialButtons = { BtnA, BtnB, BtnC };
            TestTrue(TEXT("Initial buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(InitialButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries"), Repeater->GetEntryCount(), 3);
            UWidget* WidgetA = Repeater->GetEntryWidget(FName(TEXT("btn_a")));
            UWidget* WidgetB = Repeater->GetEntryWidget(FName(TEXT("btn_b")));
            TestNotNull(TEXT("Button A widget exists"), WidgetA);
            TestNotNull(TEXT("Button B widget exists"), WidgetB);

            // Reorder & update: { BtnB, BtnD, BtnA } -> BtnB & BtnA must be reused
            FTestButtonModel BtnD;
            BtnD.Key = FName(TEXT("btn_d"));
            if (Theme != nullptr)
            {
                BtnD.Text = MakeResolvedLiteralTextForTest(*Theme, TEXT("Action D"));
            }
            BtnD.Binding = FGV2UiBindingHandle::Create(TEXT("binding_d"));

            const TArray<FTestButtonModel> UpdatedButtons = { BtnB, BtnD, BtnA };
            TestTrue(TEXT("Updated buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(UpdatedButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries after update"), Repeater->GetEntryCount(), 3);
            TestEqual(TEXT("Button B widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_b"))), WidgetB);
            TestEqual(TEXT("Button A widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_a"))), WidgetA);
            TestNull(TEXT("Button C widget removed"), Repeater->GetEntryWidget(FName(TEXT("btn_c"))));
            TestNotNull(TEXT("Button D widget created"), Repeater->GetEntryWidget(FName(TEXT("btn_d"))));

            // Negative: Duplicate button key rejected
            FTestButtonModel BadBtn;
            BadBtn.Key = FName(TEXT("btn_b"));
            BadBtn.Binding = FGV2UiBindingHandle::Create(TEXT("bad_binding"));
            TArray<FTestButtonModel> DupButtons = { BtnB, BadBtn };
            TestFalse(TEXT("Duplicate button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(DupButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            // Negative: Empty button key rejected
            FTestButtonModel EmptyKeyBtn;
            EmptyKeyBtn.Key = FName();
            EmptyKeyBtn.Binding = FGV2UiBindingHandle::Create(TEXT("empty_binding"));
            TArray<FTestButtonModel> EmptyKeyButtons = { EmptyKeyBtn };
            TestFalse(TEXT("Empty button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(EmptyKeyButtons, GetKey, CreateWidgetLambda, ApplyLambda));
        }
    }

    // 3. UIH-03 & CCF-03: Test Item and Meter Repeater reconciliation and key preservation
    {
        UGV2ListViewWidgetBase* ItemRep = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UWrapBox* ItemBox = NewObject<UWrapBox>(TestWorld);
        ItemRep->SetContainerPanel(ItemBox);

        struct FTestIconEntry { FName Key; FString ResourceId; };
        TArray<FTestIconEntry> InitialItems = {
            { FName(TEXT("item@1")), TEXT("core:resource.ui.test_fixed_aspect") },
            { FName(TEXT("item@2")), TEXT("core:resource.ui.test_fixed_aspect") }
        };

        auto CreateIcon = [&]() -> UGV2ImageWidgetBase* {
            UGV2ImageWidgetBase* Img = NewObject<UGV2ImageWidgetBase>(TestWorld);
            UImage* InnerImage = NewObject<UImage>(Img);
            if (FProperty* Prop = UGV2ImageWidgetBase::StaticClass()->FindPropertyByName(TEXT("Image")))
            {
                *Prop->ContainerPtrToValuePtr<TObjectPtr<UImage>>(Img) = InnerImage;
            }
            Img->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            return Img;
        };
        // PSC-10C: resolve first, apply second -- the production shape. UGV2ImageWidgetBase
        // no longer has a method that does both, because doing both is what let a widget
        // reach the catalog from its own lifecycle.
        FString IconCatalogError;
        UGV2ImageResourceCatalog* IconCatalog =
            GV2PresentationTestFixtures::BuildGameDataImageCatalog(IconCatalogError);
        TestNotNull(
            *FString::Printf(TEXT("Icon repeater test catalog builds [Error: %s]"), *IconCatalogError),
            IconCatalog);
        auto ApplyIcon = [IconCatalog](UGV2ImageWidgetBase& Img, const FTestIconEntry& Entry) -> bool {
            FGV2ResolvedImageResource Resolved;
            FString Err;
            return IconCatalog != nullptr
                && IconCatalog->Resolve(Entry.ResourceId, Resolved, Err)
                && Img.ApplyResolvedImageResource(MakePreparedResolvedImageForTest(Resolved), Err);
        };
        auto GetIconKey = [](const FTestIconEntry& Entry) -> FName { return Entry.Key; };

        TestTrue(TEXT("Initial items reconcile successfully"), ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FTestIconEntry>(InitialItems, GetIconKey, CreateIcon, ApplyIcon));
        TestEqual(TEXT("Item count is 2"), ItemRep->GetEntryCount(), 2);
        UWidget* SwordWidget = ItemRep->GetEntryWidget(FName(TEXT("item@1")));
        UWidget* ShieldWidget = ItemRep->GetEntryWidget(FName(TEXT("item@2")));
        TestNotNull(TEXT("Sword widget exists"), SwordWidget);
        TestNotNull(TEXT("Shield widget exists"), ShieldWidget);

        // Reorder & insert: { item@2, item@3, item@1 }
        TArray<FTestIconEntry> UpdatedItems = {
            { FName(TEXT("item@2")), TEXT("core:resource.ui.test_fixed_aspect") },
            { FName(TEXT("item@3")), TEXT("core:resource.ui.test_fixed_aspect") },
            { FName(TEXT("item@1")), TEXT("core:resource.ui.test_fixed_aspect") }
        };

        TestTrue(TEXT("Updated items reconcile successfully"), ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FTestIconEntry>(UpdatedItems, GetIconKey, CreateIcon, ApplyIcon));
        TestEqual(TEXT("Item count is 3"), ItemRep->GetEntryCount(), 3);
        TestEqual(TEXT("Sword widget pointer preserved"), ItemRep->GetEntryWidget(FName(TEXT("item@1"))), SwordWidget);
        TestEqual(TEXT("Shield widget pointer preserved"), ItemRep->GetEntryWidget(FName(TEXT("item@2"))), ShieldWidget);
        TestNotNull(TEXT("Item 3 widget created"), ItemRep->GetEntryWidget(FName(TEXT("item@3"))));

        // CCF-03: Meter Repeater
        UGV2ListViewWidgetBase* MeterRep = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UVerticalBox* MeterBox = NewObject<UVerticalBox>(TestWorld);
        MeterRep->SetContainerPanel(MeterBox);

        struct FTestMeterEntry { FName Key; float Percent; };
        TArray<FTestMeterEntry> InitialMeters = {
            { FName(TEXT("stamina")), 0.5f },
            { FName(TEXT("health")), 0.8f }
        };

        auto CreateMeter = [&]() -> UGV2ProgressBarWidgetBase* {
            return NewObject<UGV2ProgressBarWidgetBase>(TestWorld);
        };
        auto ApplyMeter = [](UGV2ProgressBarWidgetBase& Bar, const FTestMeterEntry& Entry) -> bool {
            Bar.ApplyProgress(Entry.Percent);
            return true;
        };
        auto GetMeterKey = [](const FTestMeterEntry& Entry) -> FName { return Entry.Key; };

        TestTrue(TEXT("Initial meters reconcile successfully"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(InitialMeters, GetMeterKey, CreateMeter, ApplyMeter));
        TestEqual(TEXT("Meter count is 2"), MeterRep->GetEntryCount(), 2);
        UWidget* StaminaWidget = MeterRep->GetEntryWidget(FName(TEXT("stamina")));
        UWidget* HealthWidget = MeterRep->GetEntryWidget(FName(TEXT("health")));
        TestNotNull(TEXT("Stamina widget exists"), StaminaWidget);
        TestNotNull(TEXT("Health widget exists"), HealthWidget);

        // Reorder: { health, stamina }
        TArray<FTestMeterEntry> ReorderedMeters = {
            { FName(TEXT("health")), 0.8f },
            { FName(TEXT("stamina")), 0.5f }
        };
        TestTrue(TEXT("Reordered meters reconcile successfully"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(ReorderedMeters, GetMeterKey, CreateMeter, ApplyMeter));
        TestEqual(TEXT("Health widget pointer preserved"), MeterRep->GetEntryWidget(FName(TEXT("health"))), HealthWidget);
        TestEqual(TEXT("Stamina widget pointer preserved"), MeterRep->GetEntryWidget(FName(TEXT("stamina"))), StaminaWidget);

        // Negative: Empty meter key rejected
        TArray<FTestMeterEntry> BadMeters = { { FName(), 0.5f } };
        TestFalse(TEXT("Empty meter key rejected"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(BadMeters, GetMeterKey, CreateMeter, ApplyMeter));

        // Negative: Duplicate meter key rejected
        TArray<FTestMeterEntry> DupMeters = { { FName(TEXT("stamina")), 0.5f }, { FName(TEXT("stamina")), 0.5f } };
        TestFalse(TEXT("Duplicate meter key rejected"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(DupMeters, GetMeterKey, CreateMeter, ApplyMeter));
    }

    // 4. UIH-04 & CCF-04: Test SceneView character collection using Core Repeater and key identity
    {
        UGV2ListViewWidgetBase* CharRep = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UHorizontalBox* CharBox = NewObject<UHorizontalBox>(TestWorld);
        CharRep->SetContainerPanel(CharBox);

        struct FTestCharEntry
        {
            FName Key;
            FString ResourceId;
        };

        if (CharRep != nullptr)
        {
            TSubclassOf<UGV2ImageWidgetBase> CharClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C"));
            auto GetKey = [](const FTestCharEntry& E) { return E.Key; };
            auto CreateWidgetLambda = [TestWorld, CharClass]() -> UGV2ImageWidgetBase*
            {
                return CharClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, CharClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
            };
            // PSC-10C: resolve then apply -- see the icon repeater above.
            FString CharCatalogError;
            UGV2ImageResourceCatalog* CharCatalog =
                GV2PresentationTestFixtures::BuildGameDataImageCatalog(CharCatalogError);
            TestNotNull(
                *FString::Printf(TEXT("Character repeater test catalog builds [Error: %s]"), *CharCatalogError),
                CharCatalog);
            auto ApplyLambda = [CharCatalog](UGV2ImageWidgetBase& Widget, const FTestCharEntry& Entry)
            {
                Widget.SetKey(Entry.Key);
                FGV2ResolvedImageResource Resolved;
                FString Err;
                return CharCatalog != nullptr
                    && CharCatalog->Resolve(Entry.ResourceId, Resolved, Err)
                    && Widget.ApplyResolvedImageResource(MakePreparedResolvedImageForTest(Resolved), Err);
            };

            // Positive single character with key identity
            FTestCharEntry CharA{ FName(TEXT("aria")), TEXT("core:resource.ui.test_fixed_aspect") };
            TArray<FTestCharEntry> SingleChar = { CharA };
            TestTrue(TEXT("SceneView accepts character entry"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(SingleChar, GetKey, CreateWidgetLambda, ApplyLambda));

            UWidget* CharAWidgetBefore = CharRep->GetEntryWidget(FName(TEXT("aria")));
            TestNotNull(TEXT("Character widget exists in repeater"), CharAWidgetBefore);
            TestEqual(TEXT("Repeater count is 1"), CharRep->GetEntryCount(), 1);

            // CCF-04: Changing resource ID for same character key preserves widget pointer
            FTestCharEntry CharA_NewRes{ FName(TEXT("aria")), TEXT("core:resource.ui.test_character") };
            TArray<FTestCharEntry> SingleCharNew = { CharA_NewRes };
            TestTrue(TEXT("SceneView accepts character update with changed resource"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(SingleCharNew, GetKey, CreateWidgetLambda, ApplyLambda));

            UWidget* CharAWidgetAfter = CharRep->GetEntryWidget(FName(TEXT("aria")));
            TestEqual(TEXT("Character widget pointer preserved across resource change"), CharAWidgetAfter, CharAWidgetBefore);

            // Negative CCF-04: Duplicate character keys rejected
            TArray<FTestCharEntry> DupChars = { CharA, CharA };
            TestFalse(TEXT("Duplicate character keys rejected"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(DupChars, GetKey, CreateWidgetLambda, ApplyLambda));
        }
    }

    return true;
}

// =========================================================================
// UIH-05 & UIH-06: Text Pipeline DPI Scaling & Unified Sizing Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2TextPipelineDpiScalingTest,
    "GV2.Runtime.Presentation.TextPipelineDpiScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2TextPipelineDpiScalingTest::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured UI theme is valid"), Theme);
    if (Theme == nullptr) return false;

    // Check evaluate scale at standard heights
    const float Scale720p = Theme->EvaluateTextScale(720.0f);
    const float Scale1080p = Theme->EvaluateTextScale(1080.0f);
    const float Scale1440p = Theme->EvaluateTextScale(1440.0f);
    const float Scale2160p = Theme->EvaluateTextScale(2160.0f);

    TestNearlyEqual(TEXT("Scale at 720p is ~0.85"), Scale720p, 0.85f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 1080p is 1.0"), Scale1080p, 1.00f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 1440p is ~1.25"), Scale1440p, 1.25f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 2160p is ~1.60"), Scale2160p, 1.60f, 0.01f);

    // Check effective font size calculation and MinReadableFontSize clamp
    const float SmallSize720p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("small")), 720.0f);
    TestTrue(TEXT("Small text size at 720p is >= MinReadableFontSize (10pt)"), SmallSize720p >= Theme->MinReadableFontSize);

    const float TitleSize1080p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("title")), 1080.0f);
    TestNearlyEqual(TEXT("Title text size at 1080p is ~20pt"), TitleSize1080p, 20.0f, 0.1f);

    const float TitleSize2160p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("title")), 2160.0f);
    TestNearlyEqual(TEXT("Title text size at 2160p is ~32pt"), TitleSize2160p, 32.0f, 0.5f);

    // Verify plain text and rich text get the exact same effective font size
    FTextBlockStyle PlainStyle;
    const bool bResolved = UGV2TextPipeline::ResolveStyleForHeight(Theme, FName(TEXT("title")), PlainStyle, 1080.0f);
    if (bResolved)
    {
        TestNearlyEqual(TEXT("Plain text style font size matches TitleSize1080p"), (float)PlainStyle.Font.Size, TitleSize1080p, 0.1f);
    }

    // CCF-19: Check actual font sizes across consumer widgets at 720p, 1080p, 1440p, 2160p
    const float Heights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };
    for (float H : Heights)
    {
        const float ExpectedBodySize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("body")), H);
        FTextBlockStyle StyleForHeight;
        TestTrue(*FString::Printf(TEXT("CCF-19: Body style resolves for height %f"), H), UGV2TextPipeline::ResolveStyleForHeight(Theme, FName(TEXT("body")), StyleForHeight, H));
        TestNearlyEqual(*FString::Printf(TEXT("CCF-19: Style font size matches expected at height %f"), H), (float)StyleForHeight.Font.Size, ExpectedBodySize, 0.1f);
        TestTrue(*FString::Printf(TEXT("CCF-19: Font size at height %f is >= MinReadableFontSize"), H), (float)StyleForHeight.Font.Size >= Theme->MinReadableFontSize);
    }

    return true;
}

// =========================================================================
// UIH-07 & UIH-08: Graphics Scaling Policy & Compatibility Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2GraphicsScalingPolicyTest,
    "GV2.Runtime.Presentation.GraphicsScalingPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2GraphicsScalingPolicyTest::RunTest(const FString& Parameters)
{
    // PSC-10C: this suite exercises resolve+apply on UGV2ImageWidgetBase, which no longer
    // has a single method doing both. The catalog is built here, once, and the local helper
    // performs the two halves in the production order.
    FString ScalingCatalogError;
    UGV2ImageResourceCatalog* Catalog =
        GV2PresentationTestFixtures::BuildGameDataImageCatalog(ScalingCatalogError);
    TestNotNull(
        *FString::Printf(TEXT("Graphics scaling test catalog builds [Error: %s]"), *ScalingCatalogError),
        Catalog);
    auto ApplyById = [Catalog](UGV2ImageWidgetBase* Widget, const FString& ResourceId, FString& OutError) -> bool
    {
        FGV2ResolvedImageResource Resolved;
        return Catalog != nullptr
            && Widget != nullptr
            && Catalog->Resolve(ResourceId, Resolved, OutError)
            && Widget->ApplyResolvedImageResource(MakePreparedResolvedImageForTest(Resolved), OutError);
    };

    // 1. Test ScalePolicy compatibility matrix
    TestTrue(TEXT("PreserveAspect compatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::FixedAspect));
    TestFalse(TEXT("PreserveAspect incompatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("PreserveAspect incompatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::Tile));

    TestTrue(TEXT("NineSlice compatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("NineSlice incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::FixedAspect));

    TestTrue(TEXT("Tile compatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::Tile));
    TestFalse(TEXT("Tile incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::FixedAspect));

    TestFalse(TEXT("FreeStretch incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::FixedAspect));
    TestFalse(TEXT("FreeStretch incompatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::NineSlice));
    TestTrue(TEXT("FreeStretch compatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::Tile));

    // 2. Test ImageWidget atomic rollback on incompatible resource apply
    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld != nullptr)
    {

        UClass* ImageClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
        UGV2ImageWidgetBase* ImageWidget = ImageClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, ImageClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
        TestNotNull(TEXT("ImageWidget created"), ImageWidget);

        if (ImageWidget != nullptr)
        {
            FString Error;

            // CCF-13 / CCF-14: PostLoad does NOT infer ScalePolicy from InitialResourceId (RenderMode does not mutate ScalePolicy)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            // Simulate PostLoad
            ImageWidget->ConditionalPostLoad();
            TestEqual(TEXT("CCF-13: ScalePolicy remains PreserveAspect after PostLoad"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);

            // CCF-15: 1. PreserveAspect Resulting Brush
            const bool bAppliedAspect = ApplyById(ImageWidget, TEXT("core:resource.ui.test_fixed_aspect"), Error);
            TestTrue(TEXT("CCF-15: PreserveAspect applies fixed aspect resource"), bAppliedAspect);
            TestEqual(TEXT("CCF-15: PreserveAspect brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: PreserveAspect brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // CCF-15: 2. FreeStretch Resulting Brush (DrawAs = Image, Tiling = NoTile)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::FreeStretch);
            const bool bAppliedStretch = ApplyById(ImageWidget, TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("CCF-15: FreeStretch applies tile resource"), bAppliedStretch);
            TestEqual(TEXT("CCF-15: FreeStretch brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: FreeStretch brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // CCF-15: 3. Tile Resulting Brush (DrawAs = Image, Tiling = Both)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
            const bool bAppliedTile = ApplyById(ImageWidget, TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("CCF-15: Tile applies tile resource"), bAppliedTile);
            TestEqual(TEXT("CCF-15: Tile brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: Tile brush Tiling is Both"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::Both);

            // CCF-15: 4. NineSlice Resulting Brush (DrawAs = Box, Margin parsed)
            if (Catalog != nullptr)
            {
                UTexture2D* NineSliceTex = UTexture2D::CreateTransient(64, 64);
                FGV2ImageResourceDefinition NineSliceDef;
                NineSliceDef.ResourceId = TEXT("core:resource.surface.test_panel");
                NineSliceDef.RenderMode = EGV2ImageRenderMode::NineSlice;
                NineSliceDef.Texture = NineSliceTex;
                NineSliceDef.NineSliceBorderPixels = FMargin(8.0f);
                FGV2ResolvedImageResource ResolvedNineSlice;
                FString ResolveDefError;
                const bool bResolvedNineSlice = UGV2ImageResourceCatalog::ResolveDefinition(NineSliceDef, ResolvedNineSlice, ResolveDefError);
                TestTrue(TEXT("CCF-15: nine-slice test definition resolves"), bResolvedNineSlice);
                Catalog->ResolvedById.Add(NineSliceDef.ResourceId, MoveTemp(ResolvedNineSlice));

                ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::NineSlice);
                const bool bAppliedNineSlice = ApplyById(ImageWidget, TEXT("core:resource.surface.test_panel"), Error);
                TestTrue(TEXT("CCF-15: NineSlice applies nine-slice resource"), bAppliedNineSlice);
                TestEqual(TEXT("CCF-15: NineSlice brush DrawAs is Box"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Box);
                TestEqual(TEXT("CCF-15: NineSlice brush Margin Left is normalized 0.125"), ImageWidget->GetImageBrush().Margin.Left, 0.125f);

                // CCF-15: 5. Failure atomicity: Incompatible resource leaves previous brush 100% intact
                const FSlateBrush BaselineBrush = ImageWidget->GetImageBrush();
                const FString BaselineId = ImageWidget->GetAppliedResourceId();

                const bool bIncompatibleApply = ApplyById(ImageWidget, TEXT("core:resource.ui.test_fixed_aspect"), Error);
                TestFalse(TEXT("CCF-15: Incompatible FixedAspect resource rejected under NineSlice policy"), bIncompatibleApply);
                TestEqual(TEXT("CCF-15: AppliedResourceId unchanged after failure"), ImageWidget->GetAppliedResourceId(), BaselineId);
                TestEqual(TEXT("CCF-15: Brush ResourceObject unchanged after failure"), ImageWidget->GetImageBrush().GetResourceObject(), BaselineBrush.GetResourceObject());
                TestEqual(TEXT("CCF-15: Brush DrawAs unchanged after failure"), ImageWidget->GetImageBrush().DrawAs.GetValue(), BaselineBrush.DrawAs.GetValue());
                TestEqual(TEXT("CCF-15: Brush Tiling unchanged after failure"), ImageWidget->GetImageBrush().Tiling.GetValue(), BaselineBrush.Tiling.GetValue());
                TestEqual(TEXT("CCF-15: Brush Margin unchanged after failure"), ImageWidget->GetImageBrush().Margin.Left, BaselineBrush.Margin.Left);
            }
        }

    }
    return true;
}

// =========================================================================
// UIH-14: Rendering Conformance on Instantiated Widgets Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RenderingConformanceTest,
    "GV2.Runtime.Presentation.RenderingConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RenderingConformanceTest::RunTest(const FString& Parameters)
{
    // PSC-10C: resolve through a catalog, then apply the resolved value -- see
    // FGV2GraphicsScalingPolicyTest for why the widget no longer does both.
    FString RenderingCatalogError;
    UGV2ImageResourceCatalog* RenderingCatalog =
        GV2PresentationTestFixtures::BuildGameDataImageCatalog(RenderingCatalogError);
    TestNotNull(
        *FString::Printf(TEXT("Rendering conformance test catalog builds [Error: %s]"), *RenderingCatalogError),
        RenderingCatalog);
    auto ApplyById = [RenderingCatalog](UGV2ImageWidgetBase* Widget, const FString& ResourceId, FString& OutError) -> bool
    {
        FGV2ResolvedImageResource Resolved;
        return RenderingCatalog != nullptr
            && Widget != nullptr
            && RenderingCatalog->Resolve(ResourceId, Resolved, OutError)
            && Widget->ApplyResolvedImageResource(MakePreparedResolvedImageForTest(Resolved), OutError);
    };

    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld != nullptr)
    {

        const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
        TestNotNull(TEXT("Configured theme is valid"), Theme);

        // 1. Text, RichText, Button, Input, Dropdown sizing conformance across resolutions
        const float Heights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };
        for (const float H : Heights)
        {
            const float ExpectedTitleSize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("title")), H);
            const float ExpectedBodySize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("body")), H);
            const float ExpectedSmallSize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Theme, FName(TEXT("small")), H);

            if (H <= 720.0f)
            {
                TestTrue(TEXT("Small font size at 720p respects MinReadableFontSize"), ExpectedSmallSize >= (Theme ? Theme->MinReadableFontSize : 10.0f));
            }

            FTextBlockStyle TitleStyle;
            FTextBlockStyle BodyStyle;
            FTextBlockStyle SmallStyle;
            TestTrue(TEXT("ResolveStyleForHeight title succeeds"), UGV2TextPipeline::ResolveStyleForHeight(Theme, FName(TEXT("title")), TitleStyle, H));
            TestTrue(TEXT("ResolveStyleForHeight body succeeds"), UGV2TextPipeline::ResolveStyleForHeight(Theme, FName(TEXT("body")), BodyStyle, H));
            TestTrue(TEXT("ResolveStyleForHeight small succeeds"), UGV2TextPipeline::ResolveStyleForHeight(Theme, FName(TEXT("small")), SmallStyle, H));

            TestEqual(*FString::Printf(TEXT("[%.0fp] Title effective font size"), H), TitleStyle.Font.Size, ExpectedTitleSize);
            TestEqual(*FString::Printf(TEXT("[%.0fp] Body effective font size"), H), BodyStyle.Font.Size, ExpectedBodySize);
            TestEqual(*FString::Printf(TEXT("[%.0fp] Small effective font size"), H), SmallStyle.Font.Size, ExpectedSmallSize);

            // Create real widget instances from Blueprint classes
            UClass* TextClass = LoadClass<UGV2TextWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Text.WBP_Text_C"));
            UClass* ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
            UClass* InputClass = LoadClass<UGV2InputFieldWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"));
            UClass* DropdownClass = LoadClass<UGV2DropdownSelectWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"));

            UGV2TextWidgetBase* TextWidget = TextClass ? CreateWidget<UGV2TextWidgetBase>(TestWorld, TextClass) : NewObject<UGV2TextWidgetBase>(TestWorld);
            UGV2RichTextWidgetBase* RichTextWidget = NewObject<UGV2RichTextWidgetBase>(TestWorld);
            UGV2ButtonWidgetBase* ButtonWidget = ButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
            UGV2InputFieldWidgetBase* InputWidget = InputClass ? CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, InputClass) : NewObject<UGV2InputFieldWidgetBase>(TestWorld);
            UGV2DropdownSelectWidgetBase* DropdownWidget = DropdownClass ? CreateWidget<UGV2DropdownSelectWidgetBase>(TestWorld, DropdownClass) : NewObject<UGV2DropdownSelectWidgetBase>(TestWorld);

            TestNotNull(TEXT("TextWidget created"), TextWidget);
            TestNotNull(TEXT("RichTextWidget created"), RichTextWidget);
            TestNotNull(TEXT("ButtonWidget created"), ButtonWidget);
            TestNotNull(TEXT("InputWidget created"), InputWidget);
            TestNotNull(TEXT("DropdownWidget created"), DropdownWidget);

            // Apply models with default style
            const FGV2TextViewModel TextModel = MakeResolvedLiteralTextForTest(
                *Theme,
                TEXT("Sample Body Text"),
                FName(TEXT("default")));
            if (TextWidget)
            {
                TestTrue(TEXT("ApplyText succeeds"), TextWidget->ApplyText(TextModel));
                TestTrue(TEXT("TextContent matches"), TextWidget->GetTextContent().EqualTo(TextModel.Text));
            }

            const FGV2TextViewModel RichModel = MakeResolvedLiteralTextForTest(
                *Theme,
                TEXT("Sample Rich Body"),
                FName(TEXT("body")));
            if (RichTextWidget)
            {
                RichTextWidget->ApplyText(RichModel);
            }

            const FGV2TextViewModel BtnText = MakeResolvedLiteralTextForTest(
                *Theme,
                TEXT("Button"),
                FName(TEXT("body")));
            const FName BtnKey = FName(TEXT("ok"));
            const FGV2UiBindingHandle BtnBinding = FGV2UiBindingHandle::Create(TEXT("btn_ok"));
            if (ButtonWidget)
            {
                ButtonWidget->SetKey(BtnKey);
                ButtonWidget->SetBindingHandle(BtnBinding);
                ButtonWidget->ApplyText(BtnText);
                TestEqual(TEXT("Button Key matches"), ButtonWidget->GetKey(), BtnKey);
                TestEqual(TEXT("Button Binding matches"), ButtonWidget->GetBindingHandle(), BtnBinding);
            }

            if (InputWidget)
            {
                const FGV2TextViewModel InputText = MakeResolvedLiteralTextForTest(
                    *Theme,
                    TEXT("Input Label"),
                    FName(TEXT("body")));
                InputWidget->SetKey(FName(TEXT("input_key")));
                InputWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("input_bind")));
                InputWidget->ApplyText(InputText);
            }

            if (DropdownWidget)
            {
                const FGV2TextViewModel DropdownPlaceholder =
                    MakeResolvedLiteralTextForTest(*Theme, TEXT("Select Option"));
                DropdownWidget->ApplyPlaceholderText(DropdownPlaceholder);
                DropdownWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("dd_bind")));
            }
        }

        // 2. Image widgets scale policy and brush state conformance
        UClass* ImageClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
        UGV2ImageWidgetBase* ImageWidget = ImageClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, ImageClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
        TestNotNull(TEXT("ImageWidget created"), ImageWidget);

        if (ImageWidget != nullptr)
        {
            FString Error;

            // PreserveAspect policy with fixed aspect resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            const bool bAspectOk = ApplyById(ImageWidget, TEXT("core:resource.ui.test_fixed_aspect"), Error);
            TestTrue(TEXT("PreserveAspect applied fixed aspect resource"), bAspectOk);
            TestEqual(TEXT("PreserveAspect brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("PreserveAspect brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // Tile policy with tile resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
            const bool bTileOk = ApplyById(ImageWidget, TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("Tile policy applied tile resource"), bTileOk);
            TestEqual(TEXT("Tile brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("Tile brush Tiling is Both"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::Both);

            // FreeStretch policy with tile resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::FreeStretch);
            const bool bStretchOk = ApplyById(ImageWidget, TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("FreeStretch applied resource"), bStretchOk);
            TestEqual(TEXT("FreeStretch brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("FreeStretch brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // Negative test: Incompatible graphics resource rejects and preserves previous brush
            const FSlateBrush ValidPrevBrush = ImageWidget->GetImageBrush();
            const FString ValidPrevId = ImageWidget->GetAppliedResourceId();

            const bool bBadApply = ApplyById(ImageWidget, TEXT("nonexistent:resource.image"), Error);
            TestFalse(TEXT("Nonexistent resource is rejected"), bBadApply);
            TestEqual(TEXT("Applied resource id remains previous valid id"), ImageWidget->GetAppliedResourceId(), ValidPrevId);
            TestEqual(TEXT("Applied brush resource object remains unchanged"), ImageWidget->GetImageBrush().GetResourceObject(), ValidPrevBrush.GetResourceObject());
        }
    }

    return true;
}

// =========================================================================
// UPP-27/STATUS-004: public preflight predicts a deep child's failure
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenPreflightPredictsDeepChildFailureTest,
    "GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenPreflightPredictsDeepChildFailureTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;
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
    const UGV2UiTheme* Theme = PrepareContext->GetTheme().Theme.Get();

    // Before Prepare/Commit, CanApplyScreenField only checked field_id/schema_id and
    // never opened a keyed collection, so "schema passes, a deep child fails at
    // commit" could not be predicted by the public preflight -- it only surfaced once
    // ApplyScreenFields actually tried to commit (STATUS-004). This reproduces exactly
    // that shape: a top-level "commands" object that is schema-valid, whose items
    // array has a duplicate key -- detectable only by recursing into the collection,
    // not by any shallow field_id/schema_id check -- and proves CanApplyScreenFields
    // and ApplyScreenFields both reject it up front, leaving the screen exactly as it
    // was before the failed attempt (no partial mutation to roll back).

    AddExpectedErrorPlain(TEXT("ApplyScreenFields rejected"), EAutomationExpectedErrorFlags::Contains, 3);

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Screen instantiated"), Screen);
    if (Screen == nullptr) return false;

    Screen->WidgetTree = NewObject<UWidgetTree>(Screen);
    UVerticalBox* Root = Screen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    Screen->WidgetTree->RootWidget = Root;

    UGV2DeclaredCompositeWidgetBase* CommandPanel = Screen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
        UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("CommandPanel"));
    Root->AddChildToVerticalBox(CommandPanel);

    // The bare native button class has no WidgetTree, so its "text" capability's declared
    // "LabelText" target can never resolve -- DUC-05 made that a deterministic preflight
    // rejection rather than a silent self-fallback, so this fixture needs the real
    // WBP_Button (as production and the other tests in this file do).
    UClass* const ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));

    // DescribeUiCapabilities resolves children via GetWidgetFromName against CommandPanel's
    // own WidgetTree, not the Screen's -- ButtonRepeater/ButtonBox must live in it.
    CommandPanel->WidgetTree = NewObject<UWidgetTree>(CommandPanel);
    UVerticalBox* CmdRoot = CommandPanel->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    CommandPanel->WidgetTree->RootWidget = CmdRoot;

    UGV2ListViewWidgetBase* ButtonRepeater = CommandPanel->WidgetTree->ConstructWidget<UGV2ListViewWidgetBase>(UGV2ListViewWidgetBase::StaticClass(), TEXT("ButtonRepeater"));
    UWrapBox* ButtonBox = CommandPanel->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ButtonBox"));
    ButtonRepeater->SetContainerPanel(ButtonBox);
    CmdRoot->AddChildToVerticalBox(ButtonRepeater);

    {
        FGV2DeclaredUiCapability ItemsCap;
        ItemsCap.PropertyName = FName(TEXT("items"));
        ItemsCap.ChildWidgetName = FName(TEXT("ButtonRepeater"));
        ItemsCap.Kind = EGV2DeclaredUiCapabilityKind::CollectionHost;
        ItemsCap.EntryWidgetClass = ButtonClass != nullptr ? ButtonClass : UGV2ButtonWidgetBase::StaticClass();
        ItemsCap.KeyPropertyName = TEXT("key");
        CommandPanel->DeclaredCapabilities.Add(ItemsCap);
    }
    CommandPanel->DeclaredCapabilities.Add({ FName(TEXT("key")), NAME_None, EGV2DeclaredUiCapabilityKind::Key });

    CommandPanel->SetHostIdentity(FName(TEXT("commands")));
    TestEqual(TEXT("CommandPanel answers to screen field 'commands'"), Screen->GetScreenFieldIds(), TArray<FName>{FName(TEXT("commands"))});

    auto MakeCommandsSchema = []() -> std::shared_ptr<FCompiledUiFieldSpec>
    {
        auto ItemSpec = std::make_shared<FCompiledUiFieldSpec>();
        ItemSpec->Kind = EUiFieldKind::Object;
        ItemSpec->Fields.push_back({"key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)});
        ItemSpec->Fields.push_back({"text", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text)});
        ItemSpec->Fields.push_back({"binding", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding)});

        auto ItemsArraySpec = std::make_shared<FCompiledUiFieldSpec>();
        ItemsArraySpec->Kind = EUiFieldKind::Array;
        ItemsArraySpec->KeyedBy = std::string("key");
        ItemsArraySpec->Items = ItemSpec;

        auto Schema = std::make_shared<FCompiledUiFieldSpec>();
        Schema->Kind = EUiFieldKind::Object;
        Schema->Fields.push_back({"items", true, ItemsArraySpec});
        Schema->Fields.push_back({"key", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)});
        return Schema;
    };

    auto MakeItem = [Theme](const TCHAR* Key, const TCHAR* DisplayText) -> FGV2PreparedUiValue
    {
        const FGV2TextViewModel TextModel =
            MakeResolvedLiteralTextForTest(*Theme, DisplayText);
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(Key));
        Fields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        Fields.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("action@1:1"))));
        return FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Fields)));
    };

    auto MakeCommandsValue = [](TArray<FGV2PreparedUiValue> Items) -> TSharedRef<const FGV2PreparedUiObject>
    {
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(Items))));
        return FGV2PreparedUiObject::Create(MoveTemp(Fields));
    };

    const std::shared_ptr<FCompiledUiFieldSpec> Schema = MakeCommandsSchema();

    // 1. A valid apply first, so the screen has real, observable prior state.
    FGV2ScreenFieldValue ValidField;
    ValidField.FieldId = FName(TEXT("commands"));
    ValidField.SchemaId = TEXT("core:schema.ui_field.synthetic_commands.v1");
    ValidField.CompiledSchema = Schema;
    ValidField.PreparedValue = MakeCommandsValue({MakeItem(TEXT("btn_ok"), TEXT("OK"))});

    TestTrue(TEXT("Valid commands field applies"), Screen->ApplyScreenFields({ValidField}, *PrepareContext));
    TestEqual(TEXT("Button created for the valid apply"), ButtonBox->GetChildrenCount(), 1);
    UGV2ButtonWidgetBase* OriginalButton = Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0));
    TestNotNull(TEXT("Original button resolved"), OriginalButton);

    // 2. A deep-child failure: schema-valid top level, duplicate key inside items.
    FGV2ScreenFieldValue InvalidField;
    InvalidField.FieldId = FName(TEXT("commands"));
    InvalidField.SchemaId = TEXT("core:schema.ui_field.synthetic_commands.v1");
    InvalidField.CompiledSchema = Schema;
    InvalidField.PreparedValue = MakeCommandsValue({
        MakeItem(TEXT("dup_key"), TEXT("First")),
        MakeItem(TEXT("dup_key"), TEXT("Second")),
    });

    TestFalse(TEXT("Public preflight predicts the deep duplicate-key failure"), Screen->CanApplyScreenFields({InvalidField}, *PrepareContext));
    TestFalse(TEXT("ApplyScreenFields also rejects it (Prepare fails before any Commit)"), Screen->ApplyScreenFields({InvalidField}, *PrepareContext));

    // 3. No partial mutation: the screen is exactly as the valid apply left it.
    TestEqual(TEXT("Button count is unchanged after the rejected apply"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(TEXT("The original button instance is untouched"), Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)), OriginalButton);
    if (OriginalButton != nullptr)
    {
        TestEqual(TEXT("Original button key is unchanged"), OriginalButton->GetKey(), FName(TEXT("btn_ok")));
    }

    // 4. GBH-04: the top-level host<->envelope bijection itself (distinct from the
    // deep-child failure above) is strict on both sides, with no optional-host
    // policy -- ScreenTemplates.md's Invariants claims exactly this; this proves it
    // rather than leaving the claim resting on nothing but the source reading the
    // same way. A configured host ("commands") with no incoming envelope at all is
    // rejected, not silently skipped as an optional field.
    TestFalse(
        TEXT("GBH-04: a configured host with no incoming envelope is rejected, not treated as optional"),
        Screen->ApplyScreenFields({}, *PrepareContext));
    TestEqual(TEXT("GBH-04: no mutation from the missing-envelope rejection"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(
        TEXT("GBH-04: the original button instance survives the missing-envelope rejection"),
        Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)),
        OriginalButton);

    // An incoming envelope naming a field_id no configured host answers to is
    // rejected too -- both directions of the bijection are enforced, not just one.
    FGV2ScreenFieldValue UnknownField;
    UnknownField.FieldId = FName(TEXT("nonexistent_field"));
    UnknownField.SchemaId = ValidField.SchemaId;
    UnknownField.CompiledSchema = Schema;
    UnknownField.PreparedValue = MakeCommandsValue({MakeItem(TEXT("btn_ok"), TEXT("OK"))});

    TestFalse(
        TEXT("GBH-04: an incoming envelope with no matching configured host is rejected"),
        Screen->ApplyScreenFields({ValidField, UnknownField}, *PrepareContext));
    TestEqual(TEXT("GBH-04: no mutation from the unknown-field rejection"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(
        TEXT("GBH-04: the original button instance survives the unknown-field rejection"),
        Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)),
        OriginalButton);

    return true;
}

// =========================================================================
// SVC-09: Composite Rollback Contract (UIF-AF-01, failed apply restores prior model and visuals)
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CompositeRollbackContract,
    "GV2.Runtime.UI.CompositeRollbackContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CompositeRollbackContract::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured theme is available for prepared text fixtures"), Theme);

    // Transactional ReconcileEntries failure and rollback in ListView
    {
        UVerticalBox* Container = NewObject<UVerticalBox>();
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>();
        ListView->SetContainerPanel(Container);

        struct FTestItemModel
        {
            FName Key;
            FString Text;
            bool bShouldFailApply = false;
        };

        TArray<FTestItemModel> InitialItems = {
            {FName(TEXT("item_1")), TEXT("First Item"), false},
            {FName(TEXT("item_2")), TEXT("Second Item"), false}
        };

        auto CreateTestWidget = []() -> UGV2TextWidgetBase*
        {
            return NewObject<UGV2TextWidgetBase>();
        };

        auto ApplyTestItem = [Theme](UGV2TextWidgetBase& Widget, const FTestItemModel& Model) -> bool
        {
            if (Model.bShouldFailApply || Theme == nullptr)
            {
                return false;
            }
            return Widget.ApplyText(MakeResolvedLiteralTextForTest(*Theme, Model.Text));
        };

        const bool bInitialReconcile = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            InitialItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestTrue(TEXT("Initial list items reconciled"), bInitialReconcile);
        TestEqual(TEXT("Container has 2 initial children"), Container->GetChildrenCount(), 2);

        UGV2TextWidgetBase* Item1Widget = ListView->GetEntry<UGV2TextWidgetBase>(FName(TEXT("item_1")));
        UGV2TextWidgetBase* Item2Widget = ListView->GetEntry<UGV2TextWidgetBase>(FName(TEXT("item_2")));
        TestNotNull(TEXT("Item 1 widget exists"), Item1Widget);
        TestNotNull(TEXT("Item 2 widget exists"), Item2Widget);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("Initial item 1 text is 'First Item'"), Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("Initial item 2 text is 'Second Item'"), Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }

        // 2a. Preflight rejection on CanApplyItem: rejects before constructing or mutating widgets
        TArray<FTestItemModel> PreflightInvalidItems = {
            {FName(TEXT("item_1")), TEXT("Preflight Mutated 1"), false},
            {FName(TEXT("item_bad")), TEXT(""), false}
        };

        const bool bPreflightRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            PreflightInvalidItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem,
            [](const FTestItemModel& M) { return !M.Text.IsEmpty(); });

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects items failing CanApplyItem preflight"), bPreflightRejected);
        TestEqual(TEXT("Container children count unchanged after preflight rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after CanApplyItem preflight rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 2 widget state unmutated after CanApplyItem preflight rejection"),
                Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }

        // 2b. Preflight rejection on duplicate key: rejects before mutating any widget
        TArray<FTestItemModel> DuplicateKeyItems = {
            {FName(TEXT("item_1")), TEXT("Duplicate Mutated 1"), false},
            {FName(TEXT("item_1")), TEXT("Duplicate Mutated 2"), false}
        };

        const bool bDuplicateRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            DuplicateKeyItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects duplicate keys in preflight"), bDuplicateRejected);
        TestEqual(TEXT("Container children count unchanged after duplicate key rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after duplicate key rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2c. Preflight rejection on empty key: rejects before mutating any widget
        TArray<FTestItemModel> EmptyKeyItems = {
            {FName(TEXT("item_1")), TEXT("Empty Key Mutated 1"), false},
            {NAME_None, TEXT("Empty Key Mutated 2"), false}
        };

        const bool bEmptyKeyRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            EmptyKeyItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects empty key in preflight"), bEmptyKeyRejected);
        TestEqual(TEXT("Container children count unchanged after empty key rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after empty key rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2d. Widget creation failure: rejects before mutating any existing widget
        TArray<FTestItemModel> CreateFailItems = {
            {FName(TEXT("item_1")), TEXT("Create Fail Mutated 1"), false},
            {FName(TEXT("item_new_fail")), TEXT("New Item Failing Creation"), false}
        };

        auto NullCreateWidget = []() -> UGV2TextWidgetBase*
        {
            return nullptr;
        };

        const bool bCreateFailed = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            CreateFailItems,
            [](const FTestItemModel& M) { return M.Key; },
            NullCreateWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects when widget creation returns null"), bCreateFailed);
        TestEqual(TEXT("Container children count unchanged after widget creation failure"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after widget creation failure"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2e. Runtime item apply failure: Container hierarchy is not committed; calling reconciler restores state
        TArray<FTestItemModel> CandidateItems = {
            {FName(TEXT("item_1")), TEXT("First Item Updated"), false},
            {FName(TEXT("item_2")), TEXT("Second Item Updated"), false},
            {FName(TEXT("item_3_fail")), TEXT("Third Item Failing"), true}
        };

        const bool bFailedReconcile = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            CandidateItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries returns false when a child item fails apply"), bFailedReconcile);
        TestEqual(TEXT("Container retains previous 2 children on failed reconcile"), Container->GetChildrenCount(), 2);

        // Calling composite screen element restores previous state on reconcile failure
        const bool bRestored = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            InitialItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestTrue(TEXT("BAI-07: Screen-level rollback restores previous models"), bRestored);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state restored to 'First Item'"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 2 widget state restored to 'Second Item'"),
                Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }
    }

    return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
