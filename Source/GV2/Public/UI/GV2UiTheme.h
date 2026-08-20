#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Styling/SlateTypes.h"
#include "GV2UiTheme.generated.h"

class UCommonButtonStyle;
class UCommonTextStyle;
class UGV2RichTextPopoverWidgetBase;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rich Text Popover")
    TSoftClassPtr<UGV2RichTextPopoverWidgetBase> RichTextPopoverClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rich Text Popover")
    FSlateBrush RichTextPopoverBackground;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rich Text Popover")
    FMargin RichTextPopoverPadding = FMargin(12.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rich Text Popover", meta = (ClampMin = "64.0"))
    float RichTextPopoverMaxWidth = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rich Text Popover", meta = (ClampMin = "64.0"))
    float RichTextPopoverMaxHeight = 480.0f;

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
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GV2 UI Theme"))
class GV2_API UGV2UiThemeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Style")
    static UGV2UiTheme* GetConfiguredTheme();

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Theme", meta = (AllowedClasses = "/Script/GV2.GV2UiTheme"))
    TSoftObjectPtr<UGV2UiTheme> ThemeAsset;
};
