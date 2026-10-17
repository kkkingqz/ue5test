#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Layout/ArrangedChildren.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"

// A keyed-collection commit rebuilds its panel's children with ClearChildren()/AddChild()
// (FGV2PresentationApply's FPreparedKeyedCollectionOperation, and every collection host's own
// Reset). ClearChildren destroys the UPanelSlot objects, so AddChild creates new ones carrying
// nothing but their UCLASS defaults. FGV2KeyedCollection takes a ConfigureSlot callback for
// exactly this reason ("rebuild must reapply caller-owned layout policy to that new slot in the
// same physical commit"), and that callback currently has no caller anywhere in the repository.
//
// This test measures what the defaults actually produce: whether one item's size depends on the
// other items sharing its wrapped line. If it does, an item's geometry is a function of the
// collection's membership, which changes on every revision -- i.e. on clicks.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CollectionSlotPolicyTests,
    "GV2.Runtime.UIKit.CollectionSlotPolicyIsIndependentOfLineMembership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CollectionSlotPolicyTests::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = GV2PresentationTestFixtures::LoadTheme();
    TestNotNull(TEXT("Configured theme resolves"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("TestWorld created"), TestWorld);
    if (TestWorld == nullptr)
    {
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(TestWorld);

    UClass* ButtonClass = LoadClass<UGV2ButtonWidgetBase>(
        nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
    TestNotNull(TEXT("WBP_Button class loaded"), ButtonClass);
    if (ButtonClass == nullptr)
    {
        return false;
    }

    const GV2PresentationApply::FPreparedTextScalePolicy ThemeScale =
        UGV2TextPipeline::ResolveScalePolicyForTheme(Theme, NAME_None);

    // The authored container: WBP_ListView_WrapButtons' ContainerPanel is a UWrapBox in
    // dynamic mode (bExplicitWrapSize=false, HAlign_Left, InnerSlotPadding=0), read off the
    // asset itself rather than assumed here.
    const auto BuildWrapBox = [&](const TArray<FString>& Labels) -> UWrapBox*
    {
        UWrapBox* WrapBox = NewObject<UWrapBox>(TestWorld);
        WrapBox->SetHorizontalAlignment(HAlign_Left);
        WrapBox->SetInnerSlotPadding(FVector2D::ZeroVector);
        for (const FString& Label : Labels)
        {
            UGV2ButtonWidgetBase* Button = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass);
            if (Button == nullptr)
            {
                continue;
            }
            Button->ApplyButtonStyleValues(Theme->ButtonStyle, Theme->ButtonLabelStyle, ThemeScale);
            FGV2TextViewModel Text;
            FString TextError;
            UGV2TextPipeline::ResolveLiteralForAutomationTest(Theme, Label, NAME_None, Text, TextError);
            Button->ApplyText(Text);
            // Production adds collection children exactly like this -- a bare AddChild with
            // no slot configuration of any kind.
            WrapBox->AddChild(Button);
        }
        return WrapBox;
    };

    // Measures the arranged (not merely desired) local size of each child, which is where a
    // Fill alignment turns a line's tallest item into every sibling's height.
    const auto ArrangeAndReport = [&](UWrapBox* WrapBox, float AvailableWidth, TArray<FVector2D>& OutSizes)
    {
        OutSizes.Reset();
        if (WrapBox == nullptr)
        {
            return;
        }
        const TSharedRef<SWidget> Slate = WrapBox->TakeWidget();
        Slate->MarkPrepassAsDirty();
        Slate->SlatePrepass(1.0f);
        const FGeometry Geometry = FGeometry::MakeRoot(
            FVector2D(AvailableWidth, 4000.0f), FSlateLayoutTransform());
        FArrangedChildren Arranged(EVisibility::All);
        Slate->ArrangeChildren(Geometry, Arranged);
        for (int32 Index = 0; Index < Arranged.Num(); ++Index)
        {
            OutSizes.Add(FVector2D(Arranged[Index].Geometry.GetLocalSize()));
        }
    };

    const FString ShortLabel = TEXT("Work");
    // Long enough that its own label forces a taller button than the short ones at the
    // widths exercised below -- which is what makes line membership observable at all.
    const FString LongLabel = TEXT("Buy leather armor for twenty five gold pieces right now");

    // A: the short button alone on its line.
    UWrapBox* AloneBox = BuildWrapBox({ShortLabel});
    TArray<FVector2D> AloneSizes;
    ArrangeAndReport(AloneBox, 1200.0f, AloneSizes);

    // B: the same short button sharing a line with a tall sibling.
    UWrapBox* SharedBox = BuildWrapBox({ShortLabel, LongLabel});
    TArray<FVector2D> SharedSizes;
    ArrangeAndReport(SharedBox, 1200.0f, SharedSizes);

    if (AloneSizes.Num() >= 1 && SharedSizes.Num() >= 2)
    {
        AddInfo(FString::Printf(
            TEXT("Short button alone: %.2fx%.2f | sharing a line: %.2fx%.2f | tall sibling: %.2fx%.2f"),
            AloneSizes[0].X, AloneSizes[0].Y,
            SharedSizes[0].X, SharedSizes[0].Y,
            SharedSizes[1].X, SharedSizes[1].Y));

        TestEqual(
            *FString::Printf(
                TEXT("A collection item's arranged size does not depend on its line's other items ")
                TEXT("(alone %.2fx%.2f, shared %.2fx%.2f)"),
                AloneSizes[0].X, AloneSizes[0].Y, SharedSizes[0].X, SharedSizes[0].Y),
            SharedSizes[0],
            AloneSizes[0]);
    }
    else
    {
        AddError(FString::Printf(
            TEXT("Arrangement produced too few children (alone=%d, shared=%d)"),
            AloneSizes.Num(), SharedSizes.Num()));
    }

    // C: the same membership, narrower panel. Wrapping moves the tall item to its own line,
    // so the short button's size must not change with it.
    TArray<FVector2D> NarrowSizes;
    UWrapBox* NarrowBox = BuildWrapBox({ShortLabel, LongLabel});
    ArrangeAndReport(NarrowBox, 400.0f, NarrowSizes);
    if (NarrowSizes.Num() >= 1 && AloneSizes.Num() >= 1)
    {
        AddInfo(FString::Printf(
            TEXT("Short button on a 400-unit panel (tall sibling wraps away): %.2fx%.2f"),
            NarrowSizes[0].X, NarrowSizes[0].Y));
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif
