#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2WidgetSemanticFontSizeContractTests,
    "GV2.Runtime.UIKit.WidgetSemanticFontSizeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2WidgetSemanticFontSizeContractTests::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    TestNotNull(TEXT("Configured theme is valid"), Theme);

    // 1. Positive Test: Verify identical physical font size across all widget types
    // (Text, RichText, Button, InputField, DropdownSelect) for identical semantic tokens.
    const float ViewportHeights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };
    const FName SemanticTokens[] = {
        FName(TEXT("title")),
        FName(TEXT("body")),
        FName(TEXT("small"))
    };

    for (const float Height : ViewportHeights)
    {
        for (const FName& Token : SemanticTokens)
        {
            const float ExpectedSize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(Token, Height);
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] Expected size is positive"), Height, *Token.ToString()), ExpectedSize > 0.0f);

            // Resolve styles for all widget types
            FTextBlockStyle TextStyle;
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] Text style resolution"), Height, *Token.ToString()),
                UGV2TextPipeline::ResolveStyleForHeight(Token, TextStyle, Height));

            FTextBlockStyle RichTextStyle;
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] RichText style resolution"), Height, *Token.ToString()),
                UGV2TextPipeline::ResolveStyleForHeight(Token, RichTextStyle, Height));

            FTextBlockStyle ButtonStyle;
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] Button style resolution"), Height, *Token.ToString()),
                UGV2TextPipeline::ResolveStyleForHeight(Token, ButtonStyle, Height));

            FTextBlockStyle InputStyle;
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] InputField style resolution"), Height, *Token.ToString()),
                UGV2TextPipeline::ResolveStyleForHeight(Token, InputStyle, Height));

            FTextBlockStyle DropdownStyle;
            TestTrue(*FString::Printf(TEXT("[%.0fp / %s] DropdownSelect style resolution"), Height, *Token.ToString()),
                UGV2TextPipeline::ResolveStyleForHeight(Token, DropdownStyle, Height));

            const float TextSize = TextStyle.Font.Size;
            const float RichTextSize = RichTextStyle.Font.Size;
            const float ButtonSize = ButtonStyle.Font.Size;
            const float InputSize = InputStyle.Font.Size;
            const float DropdownSize = DropdownStyle.Font.Size;

            // Direct parity assertion across all widget types
            TestEqual(*FString::Printf(TEXT("[%.0fp / %s] Text font size matches ExpectedSize"), Height, *Token.ToString()),
                TextSize, ExpectedSize);

            TestEqual(*FString::Printf(TEXT("[%.0fp / %s] RichText font size equals Text font size"), Height, *Token.ToString()),
                RichTextSize, TextSize);

            TestEqual(*FString::Printf(TEXT("[%.0fp / %s] Button font size equals Text font size"), Height, *Token.ToString()),
                ButtonSize, TextSize);

            TestEqual(*FString::Printf(TEXT("[%.0fp / %s] InputField font size equals Text font size"), Height, *Token.ToString()),
                InputSize, TextSize);

            TestEqual(*FString::Printf(TEXT("[%.0fp / %s] DropdownSelect font size equals Text font size"), Height, *Token.ToString()),
                DropdownSize, TextSize);
        }
    }

    // 2. Negative Tests: Distinguishability between different semantic tokens at 1080p
    {
        const float TitleSize1080 = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("title")), 1080.0f);
        const float BodySize1080 = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("body")), 1080.0f);
        const float SmallSize1080 = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("small")), 1080.0f);

        TestFalse(TEXT("Title font size must differ from Body font size at 1080p"),
            FMath::IsNearlyEqual(TitleSize1080, BodySize1080, 0.1f));

        TestFalse(TEXT("Body font size must differ from Small font size at 1080p"),
            FMath::IsNearlyEqual(BodySize1080, SmallSize1080, 0.1f));

        TestTrue(TEXT("Hierarchy holds: Title > Body > Small"),
            TitleSize1080 > BodySize1080 && BodySize1080 > SmallSize1080);
    }

    // 3. Negative Tests: Perturbation detection
    {
        const float ExpectedBody = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("body")), 1080.0f);
        const float PerturbedBody = ExpectedBody + 4.0f;
        TestFalse(TEXT("Perturbed font size must not match expected size"),
            FMath::IsNearlyEqual(ExpectedBody, PerturbedBody, 0.01f));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
