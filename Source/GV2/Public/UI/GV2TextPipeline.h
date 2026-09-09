#pragma once

#include "Bridge/GV2BridgeTypes.h"
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
    // PSC-10A: PrepareContext is optional so every existing call site keeps compiling
    // unchanged. When given, Theme is read through PrepareContext->GetTheme() (the
    // session snapshot's own pinned Theme, ADR-0043 D1) instead of the legacy
    // GetConfiguredTheme() static accessor, and OutText's ResolvedStyleClass/
    // ResolvedBaseFontSize/etc. are populated so Apply()/ApplyRichText()/ApplyHint() do
    // not need to touch Theme again at Commit time (see FGV2TextViewModel's own doc
    // comment). Without one (this function's two non-PrepareContext callers, and any
    // test/legacy caller), behavior is unchanged from before this task.
    static bool Resolve(
        const FString& TextId,
        const TArray<FGV2UiControlValue>& Args,
        FName StyleToken,
        FGV2TextViewModel& OutText,
        FString& OutError,
        const FGV2PresentationPrepareContext* PrepareContext = nullptr);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Text")
    static float GetViewportHeight(const UWidget* ContextWidget = nullptr);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Text")
    static float ResolveEffectiveFontSize(FName TextSizeToken, const UWidget* ContextWidget = nullptr);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Text")
    static float ResolveEffectiveFontSizeForHeight(FName TextSizeToken, float ViewportHeight);

    static bool ResolveStyle(FName StyleToken, FTextBlockStyle& OutStyle, const UWidget* ContextWidget = nullptr);
    static bool ResolveStyleForHeight(FName StyleToken, FTextBlockStyle& OutStyle, float ViewportHeight);
    static TSubclassOf<UCommonTextStyle> ResolveStyleClass(FName StyleToken);

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
    static bool NormalizeMarkup(const FString& Source, FString& OutMarkup, FString& OutError);
};
