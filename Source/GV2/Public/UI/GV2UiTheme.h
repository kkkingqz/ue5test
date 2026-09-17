#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "GV2ContentCore/Value.h"
#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "GV2UiTheme.generated.h"

class UCommonButtonStyle;
class UCommonTextStyle;

USTRUCT(BlueprintType)
struct GV2_API FGV2TextStyleToken
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
    TSubclassOf<UCommonTextStyle> Style;
};

UCLASS(BlueprintType)
class GV2_API UGV2UiTheme : public UDataAsset
{
    GENERATED_BODY()

public:
    UGV2UiTheme();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Scaling")
    FRuntimeFloatCurve TextScaleCurve;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Scaling", meta = (ClampMin = "6.0"))
    float MinReadableFontSize = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Scaling", meta = (ClampMin = "360.0"))
    float ReferenceViewportHeight = 1080.0f;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Typography")
    float EvaluateTextScale(float ViewportHeight) const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Typography")
    float GetEffectiveFontSize(FName TextSizeToken, float ViewportHeight) const;

    // PSC-10A: the UnscaledSize half of GetEffectiveFontSize, extracted so Prepare-phase
    // callers (UGV2TextPipeline::Resolve(), given a PrepareContext) can resolve a base
    // font size once, without needing this Theme object again at Apply/Commit time --
    // GetEffectiveFontSize itself now calls this, unchanged behavior.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Typography")
    float ResolveUnscaledFontSize(FName TextSizeToken) const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Theme")
    static UGV2UiTheme* GetCoreMinimalTheme(UObject* WorldContextObject = nullptr);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Pipeline")
    FName DefaultTextStyleToken = TEXT("default");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Pipeline")
    TMap<FName, FGV2TextStyleToken> TextStyleTokens;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Pipeline")
    TMap<FName, FLinearColor> TextColorTokens;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Pipeline")
    TMap<FName, float> TextSizeTokens;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Localization")
    TMap<FString, FText> TextCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography|Localization")
    TMap<FString, FText> FallbackTextCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
    TSubclassOf<UCommonTextStyle> TextStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
    TSubclassOf<UCommonTextStyle> RichTextStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
    FHyperlinkStyle RichTextInteractiveStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    TSubclassOf<UCommonButtonStyle> ButtonStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    TSubclassOf<UCommonTextStyle> ButtonLabelStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    FCheckBoxStyle CheckboxStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    TSubclassOf<UCommonTextStyle> CheckboxLabelStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    FEditableTextBoxStyle InputFieldStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    TSubclassOf<UCommonTextStyle> InputFieldLabelStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
    FMargin ButtonListItemPadding = FMargin(0.0f, 0.0f, 0.0f, 8.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Image")
    FLinearColor ImageTint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress")
    FProgressBarStyle ProgressBarStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress")
    FLinearColor ProgressFillColor = FLinearColor(0.12f, 0.65f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Separator")
    FSlateBrush SeparatorBrush;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Separator", meta = (ClampMin = "0.5"))
    float SeparatorThickness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Indicator")
    FSlateBrush LoadingIndicatorBrush;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Indicator", meta = (ClampMin = "1", ClampMax = "25"))
    int32 LoadingIndicatorPieces = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Indicator", meta = (ClampMin = "0.01"))
    float LoadingIndicatorPeriod = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Indicator", meta = (ClampMin = "1.0"))
    float LoadingIndicatorRadius = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dropdown")
    TSubclassOf<UCommonButtonStyle> DropdownHeaderStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dropdown")
    FSlateBrush DropdownPopupBackground;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dropdown")
    FMargin DropdownPopupPadding = FMargin(4.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dropdown", meta = (ClampMin = "32.0"))
    float DropdownMaxPopupHeight = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dropdown")
    FMargin DropdownOptionItemPadding = FMargin(0.0f, 0.0f, 0.0f, 2.0f);

    bool CompileResolvedTheme(
        const UGV2UiTheme* FallbackTheme,
        class FGV2ResolvedUiTheme& OutResolved,
        FString& OutError) const;
};

// SAC-04 (ADR-0043 D1, ADR-0042, CFC-AF-21):
// Independent, immutable resolved UI theme owned by a session content snapshot.
// Compiled read-only from authoring UGV2UiTheme; holds resolved value copies (colors,
// margins, brushes, curves, text catalogs) and GC-safe strong references to resolved
// UClasses (TStrongObjectPtr<UClass>).
// Retains NO pointer or reference to the authoring UGV2UiTheme DataAsset.
class GV2_API FGV2ResolvedUiTheme
{
public:
    FGV2ResolvedUiTheme() = default;
    ~FGV2ResolvedUiTheme() = default;
    FGV2ResolvedUiTheme(const FGV2ResolvedUiTheme&) = default;
    FGV2ResolvedUiTheme& operator=(const FGV2ResolvedUiTheme&) = default;
    FGV2ResolvedUiTheme(FGV2ResolvedUiTheme&&) = default;
    FGV2ResolvedUiTheme& operator=(FGV2ResolvedUiTheme&&) = default;

    bool IsValid() const { return bIsValid; }

    static bool Compile(
        const UGV2UiTheme* InTheme,
        const UGV2UiTheme* InFallbackTheme,
        FGV2ResolvedUiTheme& OutResolved,
        FString& OutError);

    static GV2ContentCore::FValue ComputeThemeCanonicalValue(const UGV2UiTheme* InTheme);

    float EvaluateTextScale(float ViewportHeight) const;
    float ResolveUnscaledFontSize(FName TextSizeToken) const;
    float GetEffectiveFontSize(FName TextSizeToken, float ViewportHeight) const;

    UClass* ResolveStyleClass(FName StyleToken) const;
    bool ResolveStyle(FName StyleToken, FTextBlockStyle& OutStyle) const;
    const FText* FindText(const FString& TextId) const;

    const GV2ContentCore::FValue& GetCanonicalValue() const { return CanonicalThemeValue; }

    bool bIsValid = false;

    // Typography scaling
    FRuntimeFloatCurve TextScaleCurve;
    float MinReadableFontSize = 10.0f;
    float ReferenceViewportHeight = 1080.0f;

    // Typography pipeline
    FName DefaultTextStyleToken = TEXT("default");
    TMap<FName, TStrongObjectPtr<UClass>> TextStyleTokens;
    TMap<FName, FLinearColor> TextColorTokens;
    TMap<FName, float> TextSizeTokens;

    // Localization catalogs
    TMap<FString, FText> TextCatalog;
    TMap<FString, FText> FallbackTextCatalog;
    TMap<FString, FText> CoreMinimalFallbackTextCatalog;

    // Typography styles
    TStrongObjectPtr<UClass> TextStyle;
    TStrongObjectPtr<UClass> RichTextStyle;
    FHyperlinkStyle RichTextInteractiveStyle;

    // Controls
    TStrongObjectPtr<UClass> ButtonStyle;
    TStrongObjectPtr<UClass> ButtonLabelStyle;
    FCheckBoxStyle CheckboxStyle;
    TStrongObjectPtr<UClass> CheckboxLabelStyle;
    FEditableTextBoxStyle InputFieldStyle;
    TStrongObjectPtr<UClass> InputFieldLabelStyle;
    FMargin ButtonListItemPadding = FMargin(0.0f, 0.0f, 0.0f, 8.0f);

    // Image
    FLinearColor ImageTint = FLinearColor::White;

    // Progress
    FProgressBarStyle ProgressBarStyle;
    FLinearColor ProgressFillColor = FLinearColor(0.12f, 0.65f, 1.0f, 1.0f);

    // Separator
    FSlateBrush SeparatorBrush;
    float SeparatorThickness = 1.0f;

    // Loading indicator
    FSlateBrush LoadingIndicatorBrush;
    int32 LoadingIndicatorPieces = 8;
    float LoadingIndicatorPeriod = 0.75f;
    float LoadingIndicatorRadius = 16.0f;

    // Dropdown
    TStrongObjectPtr<UClass> DropdownHeaderStyle;
    FSlateBrush DropdownPopupBackground;
    FMargin DropdownPopupPadding = FMargin(4.0f);
    float DropdownMaxPopupHeight = 200.0f;
    FMargin DropdownOptionItemPadding = FMargin(0.0f, 0.0f, 0.0f, 2.0f);

private:
    GV2ContentCore::FValue CanonicalThemeValue;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GV2 UI Theme"))
class GV2_API UGV2UiThemeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Theme", meta = (AllowedClasses = "/Script/GV2.GV2UiTheme"))
    TSoftObjectPtr<UGV2UiTheme> ThemeAsset;
};
