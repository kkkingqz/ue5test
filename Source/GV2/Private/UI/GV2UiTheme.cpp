#include "UI/GV2UiTheme.h"

UGV2UiTheme::UGV2UiTheme()
{
    FRichCurve* Curve = TextScaleCurve.GetRichCurve();
    if (Curve != nullptr)
    {
        Curve->AddKey(720.0f, 0.85f);
        Curve->AddKey(1080.0f, 1.0f);
        Curve->AddKey(1440.0f, 1.25f);
        Curve->AddKey(2160.0f, 1.60f);
    }
}

float UGV2UiTheme::EvaluateTextScale(float ViewportHeight) const
{
    if (ViewportHeight <= 0.0f)
    {
        return 1.0f;
    }
    const FRichCurve* Curve = TextScaleCurve.GetRichCurveConst();
    if (Curve != nullptr && Curve->GetNumKeys() > 0)
    {
        return FMath::Max(0.1f, Curve->Eval(ViewportHeight));
    }
    if (ViewportHeight < 1080.0f)
    {
        const float Alpha = FMath::Clamp((ViewportHeight - 720.0f) / (1080.0f - 720.0f), 0.0f, 1.0f);
        return FMath::Lerp(0.85f, 1.0f, Alpha);
    }
    else
    {
        const float Alpha = FMath::Clamp((ViewportHeight - 1080.0f) / (2160.0f - 1080.0f), 0.0f, 1.0f);
        return FMath::Lerp(1.0f, 1.60f, Alpha);
    }
}

float UGV2UiTheme::GetEffectiveFontSize(FName TextSizeToken, float ViewportHeight) const
{
    const float* BaseSize = TextSizeTokens.Find(TextSizeToken);
    const float UnscaledSize = (BaseSize != nullptr && *BaseSize > 0.0f) ? *BaseSize : 14.0f;
    const float Scale = EvaluateTextScale(ViewportHeight);
    const float ScaledSize = UnscaledSize * Scale;
    return FMath::Max(MinReadableFontSize, ScaledSize);
}

UGV2UiTheme* UGV2UiTheme::GetCoreMinimalTheme(UObject* WorldContextObject)
{
    static TStrongObjectPtr<UGV2UiTheme> CachedMinimalTheme;
    if (CachedMinimalTheme.IsValid())
    {
        return CachedMinimalTheme.Get();
    }

    UGV2UiTheme* Theme = NewObject<UGV2UiTheme>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public | RF_Standalone);
    Theme->AddToRoot();

    Theme->DefaultTextStyleToken = TEXT("default");
    Theme->TextSizeTokens.Add(TEXT("default"), 14.0f);
    Theme->TextSizeTokens.Add(TEXT("title"), 20.0f);
    Theme->TextSizeTokens.Add(TEXT("body"), 14.0f);
    Theme->TextSizeTokens.Add(TEXT("small"), 12.0f);

    Theme->TextColorTokens.Add(TEXT("default"), FLinearColor::White);
    Theme->TextColorTokens.Add(TEXT("title"), FLinearColor::White);
    Theme->TextColorTokens.Add(TEXT("error"), FLinearColor(1.0f, 0.2f, 0.2f, 1.0f));
    Theme->TextColorTokens.Add(TEXT("subdued"), FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));

    Theme->MinReadableFontSize = 10.0f;
    Theme->ReferenceViewportHeight = 1080.0f;

    Theme->TextCatalog.Add(TEXT("core:text.screen.error.title"), FText::FromString(TEXT("Error")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.error.description"), FText::FromString(TEXT("An unexpected error has occurred.")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.loading.title"), FText::FromString(TEXT("Loading...")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.recovery.title"), FText::FromString(TEXT("Recovery")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.recovery.description"), FText::FromString(TEXT("Attempting session recovery.")));
    Theme->TextCatalog.Add(TEXT("core:text.button.close"), FText::FromString(TEXT("Close")));
    Theme->TextCatalog.Add(TEXT("core:text.button.retry"), FText::FromString(TEXT("Retry")));

    CachedMinimalTheme.Reset(Theme);
    return Theme;
}

FName UGV2UiThemeSettings::GetCategoryName() const
{
    return TEXT("Game");
}

UGV2UiTheme* UGV2UiThemeSettings::GetConfiguredTheme()
{
    const UGV2UiThemeSettings* Settings = GetDefault<UGV2UiThemeSettings>();
    if (Settings != nullptr && !Settings->ThemeAsset.IsNull())
    {
        if (UGV2UiTheme* Loaded = Settings->ThemeAsset.Get())
        {
            return Loaded;
        }
        if (!IsInAsyncLoadingThread() && !IsGarbageCollecting())
        {
            return Settings->ThemeAsset.LoadSynchronous();
        }
    }
    return UGV2UiTheme::GetCoreMinimalTheme();
}
