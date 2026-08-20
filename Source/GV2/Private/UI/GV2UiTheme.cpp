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

FName UGV2UiThemeSettings::GetCategoryName() const
{
    return TEXT("Game");
}

UGV2UiTheme* UGV2UiThemeSettings::GetConfiguredTheme()
{
    const UGV2UiThemeSettings* Settings = GetDefault<UGV2UiThemeSettings>();
    return Settings != nullptr ? Settings->ThemeAsset.LoadSynchronous() : nullptr;
}
