#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateTypes.h"
#include "GV2TextPipeline.generated.h"

class UCommonTextBlock;
class UCommonRichTextBlock;
class UCommonTextStyle;
class UEditableTextBox;
class UWidget;

class FGV2PresentationPrepareContext;
class UGV2UiTheme;

UCLASS()
class GV2_API UGV2TextPipeline : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Runtime text resolution requires the active session's pinned presentation context.
    static bool Resolve(
        const FString& TextId,
        const TArray<FGV2UiControlValue>& Args,
        FName StyleToken,
        FGV2TextViewModel& OutText,
        FString& OutError,
        const FGV2PresentationPrepareContext* PrepareContext);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Text")
    static float ResolveEffectiveFontSizeForHeight(const UGV2UiTheme* Theme, FName TextSizeToken, float ViewportHeight);

    static bool ResolveStyleForHeight(const UGV2UiTheme* Theme, FName StyleToken, FTextBlockStyle& OutStyle, float ViewportHeight);

    // PSC-10B: the same theme -> style-class / scale-policy resolution Resolve() performs
    // for a text operation, against an explicitly supplied Theme rather than the configured
    // one. GV2CentralStylePreparer needs exactly this to build a central-style role's
    // default-label fields, and taking it from here keeps one implementation of the math
    // instead of a second copy in the preparer. A null StyleToken resolves the same way
    // Resolve() resolves it: the theme's DefaultTextStyleToken, else "default".
    static TSubclassOf<UCommonTextStyle> ResolveStyleClassForTheme(const UGV2UiTheme* Theme, FName StyleToken);
    static GV2PresentationApply::FPreparedTextScalePolicy ResolveScalePolicyForTheme(const UGV2UiTheme* Theme, FName StyleToken);
    static bool Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text);
    static bool ApplyRichText(UCommonRichTextBlock* Widget, const FGV2TextViewModel& Text, const UWidget* ContextWidget = nullptr);
    static bool ApplyHint(UEditableTextBox* Widget, const FGV2TextViewModel& Text);
    static bool NormalizeMarkup(const UGV2UiTheme* Theme, const FString& Source, FString& OutMarkup, FString& OutError);

#if WITH_DEV_AUTOMATION_TESTS
    static bool ResolveForAutomationTest(
        const UGV2UiTheme* Theme,
        const FString& TextId,
        const TArray<FGV2UiControlValue>& Args,
        FName StyleToken,
        FGV2TextViewModel& OutText,
        FString& OutError);

    static bool ResolveLiteralForAutomationTest(
        const UGV2UiTheme* Theme,
        const FString& LiteralText,
        FName StyleToken,
        FGV2TextViewModel& OutText,
        FString& OutError);
#endif
};
