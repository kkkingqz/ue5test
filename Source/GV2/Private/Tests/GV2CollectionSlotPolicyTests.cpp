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

// ADR-0049: a button is the size its surface says, identical for every button on that
// surface, scaled only by the surface's own viewport curve. Its label is content placed into
// that size and is never an input to it; a label that does not fit is a reported error.
//
// Before this contract the size came from UCommonButtonStyle's MinWidth/MinHeight, which are
// a MINIMUM: width was max(160, label + padding), so one long caption made one button wider
// than its neighbours, and which captions were on screen changed with every revision. The
// measurement that found it is kept below as the first assertion.

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
    GV2PresentationApply::FPreparedViewportScalePolicy SizeScale;
    SizeScale.ScaleCurve = Theme->TextScaleCurve;
    SizeScale.ReferenceViewportHeight = Theme->ReferenceViewportHeight;

    TestFalse(
        TEXT("Theme declares a non-zero authored button size"),
        Theme->ButtonSize.IsNearlyZero());

    const auto MakeButton = [&](const FString& Label) -> UGV2ButtonWidgetBase*
    {
        UGV2ButtonWidgetBase* Button = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass);
        if (Button == nullptr)
        {
            return nullptr;
        }
        Button->ApplyButtonStyleValues(
            Theme->ButtonStyle, Theme->ButtonLabelStyle, ThemeScale, Theme->ButtonSize, SizeScale);
        FGV2TextViewModel Text;
        FString TextError;
        UGV2TextPipeline::ResolveLiteralForAutomationTest(Theme, Label, NAME_None, Text, TextError);
        Button->ApplyText(Text);
        return Button;
    };

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
            if (UGV2ButtonWidgetBase* Button = MakeButton(Label))
            {
                // Production adds collection children exactly like this -- a bare AddChild
                // with no slot configuration of any kind.
                WrapBox->AddChild(Button);
            }
        }
        return WrapBox;
    };

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
    const FString LongLabel = TEXT("Buy leather armor for twenty five gold pieces right now");

    // --- 1. Size is a property of the surface, not of the caption.
    UWorld* const Unused = TestWorld;
    (void)Unused;

    UWrapBox* MixedBox = BuildWrapBox({ShortLabel, LongLabel, ShortLabel});
    TArray<FVector2D> MixedSizes;
    ArrangeAndReport(MixedBox, 1200.0f, MixedSizes);

    if (MixedSizes.Num() == 3)
    {
        AddInfo(FString::Printf(
            TEXT("Arranged sizes: short=%.2fx%.2f long=%.2fx%.2f short=%.2fx%.2f"),
            MixedSizes[0].X, MixedSizes[0].Y,
            MixedSizes[1].X, MixedSizes[1].Y,
            MixedSizes[2].X, MixedSizes[2].Y));

        TestEqual(
            *FString::Printf(
                TEXT("A long caption does not widen its button (short %.2f, long %.2f)"),
                MixedSizes[0].X, MixedSizes[1].X),
            MixedSizes[1],
            MixedSizes[0]);
        TestEqual(
            TEXT("Every button on the surface is the same size"),
            MixedSizes[2],
            MixedSizes[0]);
    }
    else
    {
        AddError(FString::Printf(TEXT("Arrangement produced %d children, expected 3"), MixedSizes.Num()));
    }

    // --- 2. Membership of a wrapped line still changes nothing.
    UWrapBox* AloneBox = BuildWrapBox({ShortLabel});
    TArray<FVector2D> AloneSizes;
    ArrangeAndReport(AloneBox, 1200.0f, AloneSizes);
    if (AloneSizes.Num() >= 1 && MixedSizes.Num() >= 1)
    {
        TestEqual(
            TEXT("A button's arranged size does not depend on its line's other items"),
            MixedSizes[0],
            AloneSizes[0]);
    }

    // --- 3. The size scales by the surface curve, and only by it.
    UGV2ButtonWidgetBase* ScaledButton = MakeButton(ShortLabel);
    TestNotNull(TEXT("Scaled button created"), ScaledButton);
    if (ScaledButton != nullptr)
    {
        const auto ExpectedAt = [&](float Height) -> FIntPoint
        {
            const float Scale = GV2PresentationApply::EvaluatePreparedViewportScale(SizeScale, Height);
            return FIntPoint(
                FMath::RoundToInt(Theme->ButtonSize.X * Scale),
                FMath::RoundToInt(Theme->ButtonSize.Y * Scale));
        };

        for (const float Height : {720.0f, 1080.0f, 1440.0f, 2160.0f})
        {
            ScaledButton->RefreshPreparedViewportPresentation(Height);
            const FIntPoint Expected = ExpectedAt(Height);
            const FIntPoint Actual = ScaledButton->GetLastEnforcedDimensions();
            TestEqual(
                *FString::Printf(TEXT("[%.0fp] enforced dimensions follow the surface curve"), Height),
                Actual,
                Expected);
        }
        AddInfo(FString::Printf(
            TEXT("Authored virtual size %.0fx%.0f; at 720p %dx%d, at 2160p %dx%d"),
            Theme->ButtonSize.X, Theme->ButtonSize.Y,
            ExpectedAt(720.0f).X, ExpectedAt(720.0f).Y,
            ExpectedAt(2160.0f).X, ExpectedAt(2160.0f).Y));
    }

    // --- 4. A caption that does not fit is reported, not absorbed.
    UGV2ButtonWidgetBase* OverflowButton = MakeButton(LongLabel);
    UGV2ButtonWidgetBase* FittingButton = MakeButton(TEXT("Go"));
    if (OverflowButton != nullptr && FittingButton != nullptr)
    {
        FString OverflowError;
        const bool bOverflowAccepted = OverflowButton->MeasureLabelOverflow(OverflowError);
        TestFalse(
            *FString::Printf(TEXT("An over-long caption is refused (error: %s)"), *OverflowError),
            bOverflowAccepted);
        TestTrue(
            TEXT("The refusal names the label_overflow diagnostic"),
            OverflowError.Contains(TEXT("core:diagnostic.ui_button.label_overflow")));

        FString FittingError;
        TestTrue(
            *FString::Printf(TEXT("A caption that fits is accepted (error: %s)"), *FittingError),
            FittingButton->MeasureLabelOverflow(FittingError));
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif
