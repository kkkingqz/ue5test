#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CommonTextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2UiBindingTarget.h"

// A control-state transition is not a new desired presentation (Docs/UI/ScreenTemplates.md,
// "Нелинейная кривая масштаба текста"): after CommonUI swaps normal/hovered/pressed/selected
// the control must restore the prepared style class and font metrics, and a new size may only
// come from a new desired presentation or a ViewportRefresh.
//
// The existing source gate (Tools/Testing/validate_viewport_refresh_coverage.py) proves the
// native hook exists and calls RestorePreparedLabelPresentation(). It cannot prove the restore
// reproduces the metrics the commit applied. This test covers exactly that gap, so a label
// whose size changes on selection is a failure here rather than a visual surprise.

namespace
{
constexpr float DistinctDefaultTokenSize = 22.0f;

UGV2UiTheme* MakeThemeWithDistinctDefaultSize()
{
    UGV2UiTheme* Theme = NewObject<UGV2UiTheme>(
        GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

    // Deliberately NOT the 14.0f that UGV2UiTheme::ResolveUnscaledFontSize uses as its
    // last-resort literal: the whole point is to tell "resolved through the default token"
    // apart from "fell through to the literal".
    Theme->DefaultTextStyleToken = TEXT("default");
    Theme->TextSizeTokens.Add(TEXT("default"), DistinctDefaultTokenSize);
    Theme->MinReadableFontSize = 1.0f;
    Theme->ReferenceViewportHeight = 1080.0f;

    FGV2TextStyleToken DefaultToken;
    DefaultToken.Style = UCommonTextStyle::StaticClass();
    Theme->TextStyleTokens.Add(TEXT("default"), DefaultToken);

    return Theme;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ButtonStateStyleMetricsTests,
    "GV2.Runtime.UIKit.ButtonStateStylePreservesLabelMetrics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ButtonStateStyleMetricsTests::RunTest(const FString& Parameters)
{
    UGV2UiTheme* Theme = MakeThemeWithDistinctDefaultSize();
    TestNotNull(TEXT("Synthetic theme created"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    // --- 1. Token-less text must resolve its base size through the theme's default token.
    //
    // ResolveStyleClass maps an empty token to DefaultTextStyleToken, and so does
    // ResolveScalePolicyForTheme, which is what the central style preparer ships to a button
    // as its token-less default. If the view model's own base size does not take the same
    // mapping, one text value carries two different sizes depending on which path reads it.
    FGV2TextViewModel TokenlessText;
    FString ResolveError;
    const bool bResolved = UGV2TextPipeline::ResolveLiteralForAutomationTest(
        Theme, TEXT("Buy"), NAME_None, TokenlessText, ResolveError);
    TestTrue(
        *FString::Printf(TEXT("Token-less literal resolves [Error: %s]"), *ResolveError),
        bResolved);

    const GV2PresentationApply::FPreparedTextScalePolicy PreparedDefaultScale =
        UGV2TextPipeline::ResolveScalePolicyForTheme(Theme, NAME_None);

    TestEqual(
        TEXT("Prepared token-less scale resolves through the default text token"),
        PreparedDefaultScale.BaseFontSize,
        DistinctDefaultTokenSize);

    TestEqual(
        TEXT("Token-less view model base size equals the prepared token-less base size"),
        TokenlessText.ResolvedBaseFontSize,
        PreparedDefaultScale.BaseFontSize);

    // --- 2. Production path: commit a label, then let CommonUI change control state.
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

    UGV2ButtonWidgetBase* Button = ButtonClass != nullptr
        ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass)
        : nullptr;
    TestNotNull(TEXT("Button widget created"), Button);

    if (Button != nullptr && Button->GetLabelText() != nullptr)
    {
        // Exactly the two writes the production path performs, in the production order:
        // the central style preparer's prepared button style, then the desired text value.
        Button->ApplyButtonStyleValues(
            nullptr,
            UCommonTextStyle::StaticClass(),
            PreparedDefaultScale);
        TestTrue(TEXT("Token-less label text applies"), Button->ApplyText(TokenlessText));

        const float CommittedSize = Button->GetLabelText()->GetFont().Size;
        TestTrue(
            *FString::Printf(TEXT("Committed label font size is positive (got %f)"), CommittedSize),
            CommittedSize > 0.0f);

        // A selection change is an ordinary control-state transition: CommonUI publishes a
        // current-text-style change, which is the only hook the control may repair from.
        Button->SetIsSelectable(true);
        Button->SetIsSelected(true, false);

        const float SelectedSize = Button->GetLabelText()->GetFont().Size;
        TestEqual(
            *FString::Printf(
                TEXT("Label font size survives selection (committed %f, after selection %f)"),
                CommittedSize,
                SelectedSize),
            SelectedSize,
            CommittedSize);

        Button->SetIsSelected(false, false);
        const float DeselectedSize = Button->GetLabelText()->GetFont().Size;
        TestEqual(
            *FString::Printf(
                TEXT("Label font size survives deselection (committed %f, after deselection %f)"),
                CommittedSize,
                DeselectedSize),
            DeselectedSize,
            CommittedSize);
    }

    // --- 3. Physical desired size per control state, with the CONFIGURED theme's button
    // style. The reported defect is a change of button SIZE across click sequences, which the
    // font assertions above cannot see: a per-state brush or a style-wide padding/min
    // dimension changes the Slate desired size without touching the label font.
    const UGV2UiTheme* ConfiguredTheme = GV2PresentationTestFixtures::LoadTheme();
    TestNotNull(TEXT("Configured theme resolves"), ConfiguredTheme);

    UGV2ButtonWidgetBase* StyledButton = ButtonClass != nullptr
        ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass)
        : nullptr;

    if (StyledButton != nullptr && ConfiguredTheme != nullptr)
    {
        const GV2PresentationApply::FPreparedTextScalePolicy ThemeScale =
            UGV2TextPipeline::ResolveScalePolicyForTheme(ConfiguredTheme, NAME_None);
        StyledButton->ApplyButtonStyleValues(
            ConfiguredTheme->ButtonStyle,
            ConfiguredTheme->ButtonLabelStyle,
            ThemeScale);

        FGV2TextViewModel StyledText;
        FString StyledError;
        UGV2TextPipeline::ResolveLiteralForAutomationTest(
            ConfiguredTheme, TEXT("Rest 1 day"), NAME_None, StyledText, StyledError);
        StyledButton->ApplyText(StyledText);

        const TSharedRef<SWidget> SlateButton = StyledButton->TakeWidget();

        const auto MeasureDesiredSize = [&SlateButton]() -> FVector2D
        {
            SlateButton->MarkPrepassAsDirty();
            SlateButton->SlatePrepass(1.0f);
            return SlateButton->GetDesiredSize();
        };

        const FVector2D NormalSize = MeasureDesiredSize();

        StyledButton->SetIsSelectable(true);
        StyledButton->SetIsSelected(true, false);
        const FVector2D SelectedSize = MeasureDesiredSize();
        StyledButton->SetIsSelected(false, false);
        StyledButton->SetIsSelectable(false);

        const FVector2D RenormalizedSize = MeasureDesiredSize();

        // Production reaches the disabled state through binding validity alone
        // (UGV2ButtonWidgetBase::SetBindingHandle), so an invalid handle is an ordinary
        // revision-to-revision transition rather than a synthetic one.
        StyledButton->SetBindingHandle(FGV2UiBindingHandle());
        const FVector2D DisabledSize = MeasureDesiredSize();

        StyledButton->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("core:command.test")));
        const FVector2D ReenabledSize = MeasureDesiredSize();

        AddInfo(FString::Printf(
            TEXT("Desired size by state: normal=(%.2f, %.2f) selected=(%.2f, %.2f) ")
            TEXT("back-to-normal=(%.2f, %.2f) disabled=(%.2f, %.2f) re-enabled=(%.2f, %.2f)"),
            NormalSize.X, NormalSize.Y,
            SelectedSize.X, SelectedSize.Y,
            RenormalizedSize.X, RenormalizedSize.Y,
            DisabledSize.X, DisabledSize.Y,
            ReenabledSize.X, ReenabledSize.Y));

        // Returning to a state the button already held must reproduce that state's size.
        // A mismatch here is not styling: it is state that one transition left behind.
        TestEqual(
            *FString::Printf(
                TEXT("Normal desired size is reproducible after a selection round trip ")
                TEXT("(%.2fx%.2f then %.2fx%.2f)"),
                NormalSize.X, NormalSize.Y, RenormalizedSize.X, RenormalizedSize.Y),
            RenormalizedSize,
            NormalSize);

        TestEqual(
            *FString::Printf(
                TEXT("Normal desired size is reproducible after a disable round trip ")
                TEXT("(%.2fx%.2f then %.2fx%.2f)"),
                NormalSize.X, NormalSize.Y, ReenabledSize.X, ReenabledSize.Y),
            ReenabledSize,
            NormalSize);
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif
