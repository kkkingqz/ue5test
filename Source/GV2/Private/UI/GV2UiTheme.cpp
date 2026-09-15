#include "UI/GV2UiTheme.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UObject/UnrealType.h"

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

float UGV2UiTheme::ResolveUnscaledFontSize(FName TextSizeToken) const
{
    float UnscaledSize = 0.0f;
    if (const float* BaseSize = TextSizeTokens.Find(TextSizeToken))
    {
        if (*BaseSize > 0.0f)
        {
            UnscaledSize = *BaseSize;
        }
    }
    if (UnscaledSize <= 0.0f)
    {
        if (const FGV2TextStyleToken* StyleToken = TextStyleTokens.Find(TextSizeToken))
        {
            if (StyleToken->Style != nullptr)
            {
                if (const UCommonTextStyle* StyleCDO = Cast<UCommonTextStyle>(StyleToken->Style->GetDefaultObject()))
                {
                    FSlateFontInfo FontInfo;
                    StyleCDO->GetFont(FontInfo);
                    UnscaledSize = FontInfo.Size;
                }
            }
        }
    }
    if (UnscaledSize <= 0.0f)
    {
        if (TextSizeToken == TEXT("title")) UnscaledSize = 20.0f;
        else if (TextSizeToken == TEXT("small")) UnscaledSize = 12.0f;
        else UnscaledSize = 14.0f;
    }
    return UnscaledSize;
}

float UGV2UiTheme::GetEffectiveFontSize(FName TextSizeToken, float ViewportHeight) const
{
    const float UnscaledSize = ResolveUnscaledFontSize(TextSizeToken);
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
    Theme->TextCatalog.Add(TEXT("core:text.common.ok"), FText::FromString(TEXT("OK")));

    CachedMinimalTheme.Reset(Theme);
    return Theme;
}

FName UGV2UiThemeSettings::GetCategoryName() const
{
    return TEXT("Game");
}

bool UGV2UiTheme::CompileResolvedTheme(
    const UGV2UiTheme* FallbackTheme,
    FGV2ResolvedUiTheme& OutResolved,
    FString& OutError) const
{
    return FGV2ResolvedUiTheme::Compile(this, FallbackTheme, OutResolved, OutError);
}

// -----------------------------------------------------------------------------
// FGV2ResolvedUiTheme
// -----------------------------------------------------------------------------

GV2ContentCore::FValue FGV2ResolvedUiTheme::ComputeThemeCanonicalValue(const UGV2UiTheme* InTheme)
{
    if (InTheme == nullptr)
    {
        return GV2ContentCore::FValue::MakeNull();
    }

    TArray<const FProperty*> Properties;
    for (TFieldIterator<FProperty> It(UGV2UiTheme::StaticClass(), EFieldIterationFlags::Default); It; ++It)
    {
        const FProperty* Prop = *It;
        if (Prop != nullptr && Prop->GetOwnerClass() == UGV2UiTheme::StaticClass())
        {
            Properties.Add(Prop);
        }
    }

    Properties.Sort([](const FProperty& A, const FProperty& B)
    {
        return A.GetName() < B.GetName();
    });

    std::vector<std::pair<std::string, GV2ContentCore::FValue>> Fields;
    Fields.reserve(Properties.Num());

    for (const FProperty* Prop : Properties)
    {
        FString ExportedText;
        Prop->ExportTextItem_Direct(
            ExportedText,
            Prop->ContainerPtrToValuePtr<void>(InTheme),
            nullptr,
            const_cast<UGV2UiTheme*>(InTheme),
            PPF_None);

        Fields.emplace_back(
            TCHAR_TO_UTF8(*Prop->GetName()),
            GV2ContentCore::FValue::MakeString(TCHAR_TO_UTF8(*ExportedText)));
    }

    return GV2ContentCore::FValue::MakeObject(std::move(Fields));
}

bool FGV2ResolvedUiTheme::Compile(
    const UGV2UiTheme* InTheme,
    const UGV2UiTheme* InFallbackTheme,
    FGV2ResolvedUiTheme& OutResolved,
    FString& OutError)
{
    OutError.Reset();
    if (InTheme == nullptr)
    {
        OutError = TEXT("Authoring theme is null");
        return false;
    }

    OutResolved = FGV2ResolvedUiTheme();
    OutResolved.bIsValid = true;

    // Typography scaling
    OutResolved.TextScaleCurve = InTheme->TextScaleCurve;
    OutResolved.MinReadableFontSize = InTheme->MinReadableFontSize;
    OutResolved.ReferenceViewportHeight = InTheme->ReferenceViewportHeight;

    // Typography pipeline
    OutResolved.DefaultTextStyleToken = InTheme->DefaultTextStyleToken;
    OutResolved.TextColorTokens = InTheme->TextColorTokens;
    OutResolved.TextSizeTokens = InTheme->TextSizeTokens;

    for (const auto& Pair : InTheme->TextStyleTokens)
    {
        UClass* ClassPtr = Pair.Value.Style != nullptr ? Pair.Value.Style.Get() : nullptr;
        OutResolved.TextStyleTokens.Add(Pair.Key, TStrongObjectPtr<UClass>(ClassPtr));
    }

    // Localization catalogs
    OutResolved.TextCatalog = InTheme->TextCatalog;
    OutResolved.FallbackTextCatalog = InTheme->FallbackTextCatalog;

    if (InFallbackTheme != nullptr)
    {
        OutResolved.CoreMinimalFallbackTextCatalog = InFallbackTheme->TextCatalog;
        for (const auto& Pair : InFallbackTheme->FallbackTextCatalog)
        {
            OutResolved.CoreMinimalFallbackTextCatalog.FindOrAdd(Pair.Key, Pair.Value);
        }
    }

    // Typography styles
    OutResolved.TextStyle = TStrongObjectPtr<UClass>(InTheme->TextStyle != nullptr ? InTheme->TextStyle.Get() : nullptr);
    OutResolved.RichTextStyle = TStrongObjectPtr<UClass>(InTheme->RichTextStyle != nullptr ? InTheme->RichTextStyle.Get() : nullptr);
    OutResolved.RichTextInteractiveStyle = InTheme->RichTextInteractiveStyle;

    // Rich Text Popover
    UClass* PopoverClass = nullptr;
    if (!InTheme->RichTextPopoverClass.IsNull() && !IsInAsyncLoadingThread() && !IsGarbageCollecting())
    {
        PopoverClass = InTheme->RichTextPopoverClass.LoadSynchronous();
    }
    else
    {
        PopoverClass = InTheme->RichTextPopoverClass.Get();
    }
    OutResolved.RichTextPopoverClass = TStrongObjectPtr<UClass>(PopoverClass);
    OutResolved.RichTextPopoverBackground = InTheme->RichTextPopoverBackground;
    OutResolved.RichTextPopoverPadding = InTheme->RichTextPopoverPadding;
    OutResolved.RichTextPopoverMaxWidth = InTheme->RichTextPopoverMaxWidth;
    OutResolved.RichTextPopoverMaxHeight = InTheme->RichTextPopoverMaxHeight;

    // Controls
    OutResolved.ButtonStyle = TStrongObjectPtr<UClass>(InTheme->ButtonStyle != nullptr ? InTheme->ButtonStyle.Get() : nullptr);
    OutResolved.ButtonLabelStyle = TStrongObjectPtr<UClass>(InTheme->ButtonLabelStyle != nullptr ? InTheme->ButtonLabelStyle.Get() : nullptr);
    OutResolved.CheckboxStyle = InTheme->CheckboxStyle;
    OutResolved.CheckboxLabelStyle = TStrongObjectPtr<UClass>(InTheme->CheckboxLabelStyle != nullptr ? InTheme->CheckboxLabelStyle.Get() : nullptr);
    OutResolved.InputFieldStyle = InTheme->InputFieldStyle;
    OutResolved.InputFieldLabelStyle = TStrongObjectPtr<UClass>(InTheme->InputFieldLabelStyle != nullptr ? InTheme->InputFieldLabelStyle.Get() : nullptr);
    OutResolved.ButtonListItemPadding = InTheme->ButtonListItemPadding;

    // Image
    OutResolved.ImageTint = InTheme->ImageTint;

    // Progress
    OutResolved.ProgressBarStyle = InTheme->ProgressBarStyle;
    OutResolved.ProgressFillColor = InTheme->ProgressFillColor;

    // Separator
    OutResolved.SeparatorBrush = InTheme->SeparatorBrush;
    OutResolved.SeparatorThickness = InTheme->SeparatorThickness;

    // Loading indicator
    OutResolved.LoadingIndicatorBrush = InTheme->LoadingIndicatorBrush;
    OutResolved.LoadingIndicatorPieces = InTheme->LoadingIndicatorPieces;
    OutResolved.LoadingIndicatorPeriod = InTheme->LoadingIndicatorPeriod;
    OutResolved.LoadingIndicatorRadius = InTheme->LoadingIndicatorRadius;

    // Dropdown
    OutResolved.DropdownHeaderStyle = TStrongObjectPtr<UClass>(InTheme->DropdownHeaderStyle != nullptr ? InTheme->DropdownHeaderStyle.Get() : nullptr);
    OutResolved.DropdownPopupBackground = InTheme->DropdownPopupBackground;
    OutResolved.DropdownPopupPadding = InTheme->DropdownPopupPadding;
    OutResolved.DropdownMaxPopupHeight = InTheme->DropdownMaxPopupHeight;
    OutResolved.DropdownOptionItemPadding = InTheme->DropdownOptionItemPadding;

    // Canonical presentation value computed via reflection
    OutResolved.CanonicalThemeValue = ComputeThemeCanonicalValue(InTheme);

    return true;
}

float FGV2ResolvedUiTheme::EvaluateTextScale(float ViewportHeight) const
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

float FGV2ResolvedUiTheme::ResolveUnscaledFontSize(FName TextSizeToken) const
{
    float UnscaledSize = 0.0f;
    if (const float* BaseSize = TextSizeTokens.Find(TextSizeToken))
    {
        if (*BaseSize > 0.0f)
        {
            UnscaledSize = *BaseSize;
        }
    }
    if (UnscaledSize <= 0.0f)
    {
        if (const TStrongObjectPtr<UClass>* StyleClassPtr = TextStyleTokens.Find(TextSizeToken))
        {
            if (StyleClassPtr->IsValid())
            {
                if (const UCommonTextStyle* StyleCDO = Cast<UCommonTextStyle>((*StyleClassPtr)->GetDefaultObject()))
                {
                    FSlateFontInfo FontInfo;
                    StyleCDO->GetFont(FontInfo);
                    UnscaledSize = FontInfo.Size;
                }
            }
        }
    }
    if (UnscaledSize <= 0.0f)
    {
        if (TextSizeToken == TEXT("title")) UnscaledSize = 20.0f;
        else if (TextSizeToken == TEXT("small")) UnscaledSize = 12.0f;
        else UnscaledSize = 14.0f;
    }
    return UnscaledSize;
}

float FGV2ResolvedUiTheme::GetEffectiveFontSize(FName TextSizeToken, float ViewportHeight) const
{
    const float UnscaledSize = ResolveUnscaledFontSize(TextSizeToken);
    const float Scale = EvaluateTextScale(ViewportHeight);
    const float ScaledSize = UnscaledSize * Scale;
    return FMath::Max(MinReadableFontSize, ScaledSize);
}

const FText* FGV2ResolvedUiTheme::FindText(const FString& TextId) const
{
    if (const FText* Found = TextCatalog.Find(TextId))
    {
        return Found;
    }
    if (const FText* Found = FallbackTextCatalog.Find(TextId))
    {
        return Found;
    }
    if (const FText* Found = CoreMinimalFallbackTextCatalog.Find(TextId))
    {
        return Found;
    }
    return nullptr;
}

UClass* FGV2ResolvedUiTheme::ResolveStyleClass(FName StyleToken) const
{
    const FName EffectiveToken = StyleToken.IsNone()
        ? (!DefaultTextStyleToken.IsNone() ? DefaultTextStyleToken : FName(TEXT("default")))
        : StyleToken;
    if (const TStrongObjectPtr<UClass>* Found = TextStyleTokens.Find(EffectiveToken))
    {
        if (Found->IsValid())
        {
            return Found->Get();
        }
    }
    return TextStyle.Get();
}

bool FGV2ResolvedUiTheme::ResolveStyle(FName StyleToken, FTextBlockStyle& OutStyle) const
{
    UClass* ClassPtr = ResolveStyleClass(StyleToken);
    if (ClassPtr == nullptr)
    {
        return false;
    }
    const UCommonTextStyle* StyleCDO = Cast<UCommonTextStyle>(ClassPtr->GetDefaultObject());
    if (StyleCDO == nullptr)
    {
        return false;
    }
    StyleCDO->ToTextBlockStyle(OutStyle);
    return true;
}

