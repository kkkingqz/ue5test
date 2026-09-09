#include "UI/GV2TextPipeline.h"

#include "Application/GV2SessionContentSnapshot.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2UiTheme.h"

namespace
{
bool IsToken(const FString& Value)
{
    if (Value.IsEmpty() || Value[0] < TEXT('a') || Value[0] > TEXT('z'))
    {
        return false;
    }
    for (const TCHAR C : Value)
    {
        if (!((C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9'))
            || C == TEXT('_')))
        {
            return false;
        }
    }
    return true;
}

FString EscapeMarkup(FString Value)
{
    Value.ReplaceInline(TEXT("&"), TEXT("&amp;"));
    Value.ReplaceInline(TEXT("<"), TEXT("&lt;"));
    Value.ReplaceInline(TEXT(">"), TEXT("&gt;"));
    Value.ReplaceInline(TEXT("\""), TEXT("&quot;"));
    return Value;
}

FString EscapeResolvedText(const FString& Value)
{
    FString Result;
    Result.Reserve(Value.Len());
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        if (Value[Index] == TEXT('&'))
        {
            const FStringView Remaining(Value.GetCharArray().GetData() + Index, Value.Len() - Index);
            int32 EntityLength = 0;
            if (Remaining.StartsWith(TEXT("&amp;"))) EntityLength = 5;
            else if (Remaining.StartsWith(TEXT("&lt;")) || Remaining.StartsWith(TEXT("&gt;"))) EntityLength = 4;
            else if (Remaining.StartsWith(TEXT("&quot;"))) EntityLength = 6;
            if (EntityLength > 0)
            {
                Result.AppendChars(Value.GetCharArray().GetData() + Index, EntityLength);
                Index += EntityLength - 1;
                continue;
            }
            Result += TEXT("&amp;");
        }
        else if (Value[Index] == TEXT('<')) Result += TEXT("&lt;");
        else if (Value[Index] == TEXT('>')) Result += TEXT("&gt;");
        else if (Value[Index] == TEXT('"')) Result += TEXT("&quot;");
        else Result.AppendChar(Value[Index]);
    }
    return Result;
}

struct FMarkupFrame
{
    FString Tag;
    FName Style;
    FName Color;
    FName Size;
    FName Interactive;
};

bool ReadAssignedValue(const FString& Body, FString& OutValue)
{
    int32 Equals = INDEX_NONE;
    if (!Body.FindChar(TEXT('='), Equals))
    {
        return false;
    }
    OutValue = Body.Mid(Equals + 1).TrimStartAndEnd();
    if (OutValue.Len() >= 2 && OutValue[0] == TEXT('"')
        && OutValue[OutValue.Len() - 1] == TEXT('"'))
    {
        OutValue = OutValue.Mid(1, OutValue.Len() - 2);
    }
    OutValue.ToLowerInline();
    return IsToken(OutValue);
}

bool ReadInteractiveId(const FString& Body, FString& OutValue)
{
    const int32 IdStart = Body.Find(TEXT("id=\""), ESearchCase::IgnoreCase);
    if (IdStart == INDEX_NONE)
    {
        return false;
    }
    const int32 ValueStart = IdStart + 4;
    const int32 ValueEnd = Body.Find(TEXT("\""), ESearchCase::CaseSensitive,
        ESearchDir::FromStart, ValueStart);
    if (ValueEnd == INDEX_NONE)
    {
        return false;
    }
    OutValue = Body.Mid(ValueStart, ValueEnd - ValueStart);
    OutValue.ToLowerInline();
    return IsToken(OutValue);
}

// PSC-10A: Theme-parameterized cores, factored out of ResolveStyleClass/
// ResolveStyleForHeight so Resolve() (which already has a Theme pointer in scope --
// either from PrepareContext or the legacy static accessor) can reuse the exact same
// resolution logic without fetching Theme a second time.
TSubclassOf<UCommonTextStyle> ResolveStyleClassCore(const UGV2UiTheme* Theme, FName StyleToken)
{
    const FGV2TextStyleToken* Token = Theme != nullptr
        ? Theme->TextStyleTokens.Find(StyleToken.IsNone() ? Theme->DefaultTextStyleToken : StyleToken)
        : nullptr;
    return Token != nullptr ? Token->Style : nullptr;
}

bool ResolveStyleCore(const UGV2UiTheme* Theme, FName StyleToken, FTextBlockStyle& OutStyle)
{
    const FName EffectiveToken = StyleToken.IsNone()
        ? ((Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone()) ? Theme->DefaultTextStyleToken : FName("default"))
        : StyleToken;
    const FGV2TextStyleToken* Token = Theme != nullptr
        ? Theme->TextStyleTokens.Find(EffectiveToken)
        : nullptr;
    const UCommonTextStyle* Style = Token != nullptr && Token->Style != nullptr
        ? Cast<UCommonTextStyle>(Token->Style->GetDefaultObject())
        : (Theme != nullptr && Theme->TextStyle != nullptr ? Cast<UCommonTextStyle>(Theme->TextStyle->GetDefaultObject()) : nullptr);
    if (Style == nullptr)
    {
        return false;
    }
    Style->ToTextBlockStyle(OutStyle);
    return true;
}
}

bool UGV2TextPipeline::Resolve(
    const FString& TextId,
    const TArray<FGV2UiControlValue>& Args,
    FName StyleToken,
    FGV2TextViewModel& OutText,
    FString& OutError,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    OutText = {};
    OutError.Reset();
    const UGV2UiTheme* Theme = PrepareContext != nullptr
        ? PrepareContext->GetTheme().Theme.Get()
        : UGV2UiThemeSettings::GetConfiguredTheme();
    const FText* Template = Theme != nullptr ? Theme->TextCatalog.Find(TextId) : nullptr;
    if (Template == nullptr && Theme != nullptr)
    {
        Template = Theme->FallbackTextCatalog.Find(TextId);
    }
    if (Template == nullptr)
    {
        if (const UGV2UiTheme* MinimalTheme = UGV2UiTheme::GetCoreMinimalTheme())
        {
            Template = MinimalTheme->TextCatalog.Find(TextId);
            if (Template == nullptr)
            {
                Template = MinimalTheme->FallbackTextCatalog.Find(TextId);
            }
        }
    }
    if (Template == nullptr)
    {
        OutError = FString::Printf(TEXT("Unknown text_id: %s"), *TextId);
        return false;
    }
    if (StyleToken.IsNone())
    {
        StyleToken = (Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone())
            ? Theme->DefaultTextStyleToken
            : FName("default");
    }
    if (Theme != nullptr && !Theme->TextStyleTokens.IsEmpty() && !Theme->TextStyleTokens.Contains(StyleToken) && StyleToken != FName("default"))
    {
        OutError = FString::Printf(TEXT("Unknown text style token: %s"), *StyleToken.ToString());
        return false;
    }

    FFormatNamedArguments FormatArgs;
    for (const FGV2UiControlValue& Arg : Args)
    {
        switch (Arg.Type)
        {
        case EGV2UiControlValueType::Boolean:
            FormatArgs.Add(Arg.Name.ToString(), FText::FromString(Arg.BooleanValue ? TEXT("true") : TEXT("false")));
            break;
        case EGV2UiControlValueType::Integer:
            FormatArgs.Add(Arg.Name.ToString(), Arg.IntegerValue);
            break;
        case EGV2UiControlValueType::Number:
            FormatArgs.Add(Arg.Name.ToString(), Arg.NumberValue);
            break;
        case EGV2UiControlValueType::String:
            FormatArgs.Add(Arg.Name.ToString(), FText::FromString(EscapeMarkup(Arg.StringValue)));
            break;
        case EGV2UiControlValueType::Null:
            OutError = TEXT("Text arguments must be scalar non-null values.");
            return false;
        }
    }
    OutText.Text = FormatArgs.IsEmpty() ? *Template : FText::Format(*Template, FormatArgs);
    OutText.StyleToken = StyleToken;

    if (PrepareContext != nullptr)
    {
        // StyleToken above is already the effective (non-None) token. Resolved here,
        // once, so Apply()/ApplyRichText() touch no Theme accessor of their own.
        OutText.bHasResolvedPresentation = true;
        OutText.ResolvedStyleClass = ResolveStyleClassCore(Theme, StyleToken);
        OutText.ResolvedBaseFontSize = Theme != nullptr ? Theme->ResolveUnscaledFontSize(StyleToken) : 14.0f;
        OutText.ResolvedMinReadableFontSize = Theme != nullptr ? Theme->MinReadableFontSize : 10.0f;
        OutText.ResolvedReferenceViewportHeight = Theme != nullptr ? Theme->ReferenceViewportHeight : 1080.0f;
        OutText.ResolvedFontScaleCurve = Theme != nullptr ? Theme->TextScaleCurve : FRuntimeFloatCurve();
        OutText.bHasResolvedDefaultStyle = ResolveStyleCore(Theme, StyleToken, OutText.ResolvedDefaultStyle);
    }

    return NormalizeMarkup(OutText.Text.ToString(), OutText.NormalizedMarkup, OutError);
}

static FName ResolveEffectiveStyleToken(const UGV2UiTheme* Theme, FName StyleToken)
{
    if (!StyleToken.IsNone())
    {
        return StyleToken;
    }
    return (Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone())
        ? Theme->DefaultTextStyleToken
        : FName("default");
}

TSubclassOf<UCommonTextStyle> UGV2TextPipeline::ResolveStyleClassForTheme(const UGV2UiTheme* Theme, FName StyleToken)
{
    return ResolveStyleClassCore(Theme, ResolveEffectiveStyleToken(Theme, StyleToken));
}

GV2PresentationApply::FPreparedTextScalePolicy UGV2TextPipeline::ResolveScalePolicyForTheme(const UGV2UiTheme* Theme, FName StyleToken)
{
    const FName EffectiveToken = ResolveEffectiveStyleToken(Theme, StyleToken);
    GV2PresentationApply::FPreparedTextScalePolicy Policy;
    Policy.BaseFontSize = Theme != nullptr ? Theme->ResolveUnscaledFontSize(EffectiveToken) : 14.0f;
    Policy.MinReadableFontSize = Theme != nullptr ? Theme->MinReadableFontSize : 10.0f;
    Policy.ReferenceViewportHeight = Theme != nullptr ? Theme->ReferenceViewportHeight : 1080.0f;
    Policy.ScaleCurve = Theme != nullptr ? Theme->TextScaleCurve : FRuntimeFloatCurve();
    return Policy;
}

float UGV2TextPipeline::GetViewportHeight(const UWidget* ContextWidget)
{
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    const float DefaultHeight = Theme != nullptr ? Theme->ReferenceViewportHeight : 1080.0f;

    if (GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        if (ViewportSize.Y > 0.0f)
        {
            return ViewportSize.Y;
        }
    }

    if (ContextWidget != nullptr)
    {
        if (const UWorld* World = ContextWidget->GetWorld())
        {
            if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
            {
                FVector2D ViewportSize;
                ViewportClient->GetViewportSize(ViewportSize);
                if (ViewportSize.Y > 0.0f)
                {
                    return ViewportSize.Y;
                }
            }
        }
    }

    return DefaultHeight;
}

float UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(const FName TextSizeToken, const float ViewportHeight)
{
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return 14.0f;
    }
    const FName Token = TextSizeToken.IsNone()
        ? ((Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone()) ? Theme->DefaultTextStyleToken : FName("default"))
        : TextSizeToken;
    return Theme->GetEffectiveFontSize(Token, ViewportHeight);
}

float UGV2TextPipeline::ResolveEffectiveFontSize(const FName TextSizeToken, const UWidget* ContextWidget)
{
    const float ViewportHeight = GetViewportHeight(ContextWidget);
    return ResolveEffectiveFontSizeForHeight(TextSizeToken, ViewportHeight);
}

bool UGV2TextPipeline::ResolveStyleForHeight(const FName StyleToken, FTextBlockStyle& OutStyle, const float ViewportHeight)
{
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    const FName EffectiveToken = StyleToken.IsNone()
        ? ((Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone()) ? Theme->DefaultTextStyleToken : FName("default"))
        : StyleToken;
    if (!ResolveStyleCore(Theme, StyleToken, OutStyle))
    {
        return false;
    }

    // Apply DPI scaling and clamp to MinReadableFontSize
    const float EffectiveSize = ResolveEffectiveFontSizeForHeight(EffectiveToken, ViewportHeight);
    OutStyle.SetFontSize(EffectiveSize);
    return true;
}

bool UGV2TextPipeline::ResolveStyle(const FName StyleToken, FTextBlockStyle& OutStyle, const UWidget* ContextWidget)
{
    const float ViewportHeight = GetViewportHeight(ContextWidget);
    return ResolveStyleForHeight(StyleToken, OutStyle, ViewportHeight);
}

TSubclassOf<UCommonTextStyle> UGV2TextPipeline::ResolveStyleClass(const FName StyleToken)
{
    return ResolveStyleClassCore(UGV2UiThemeSettings::GetConfiguredTheme(), StyleToken);
}

// PSC-09B/10A (ADR-0043 D2/D3, PAH-R1): split into upper preparation (this function)
// and lower widget application (GV2PresentationApply::Apply, which performs no
// decision, only the CommonUI/UMG-native SetStyle/SetText/SetFont calls). When Text
// carries a resolved presentation (Resolve() was given a PrepareContext -- the
// operation-kind pipeline's own path), Style/ScalePolicy are read directly from it and
// this function touches no Theme accessor at all. Otherwise (every call site outside
// the operation-kind pipeline -- PSC-10B's own scope, not yet converted) this falls
// back to the exact same Theme-touching resolution as before this task, unchanged.
bool UGV2TextPipeline::Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text)
{
    const TSubclassOf<UCommonTextStyle> Style = Text.bHasResolvedPresentation
        ? Text.ResolvedStyleClass
        : ResolveStyleClass(Text.StyleToken);
    // Plain renderer deliberately rejects semantic styled/interactive runs instead of
    // leaking authoring markup to the player. Such content must use the RichText leaf.
    if (Widget == nullptr || Style == nullptr || Text.NormalizedMarkup.Contains(TEXT("<gv2"))) return false;

    GV2PresentationApply::FPreparedTextScalePolicy ScalePolicy;
    if (Text.bHasResolvedPresentation)
    {
        ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
        ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
        ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
        ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;
    }
    else
    {
        ScalePolicy.BaseFontSize = ResolveEffectiveFontSize(Text.StyleToken, Widget);
        ScalePolicy.bIsAlreadyScaled = true;
    }

    GV2PresentationApply::FPreparedPlainTextOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Style = Style;
    Operation.Text = Text.Text;
    Operation.ScalePolicy = ScalePolicy;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddPlainTextOperation(MoveTemp(Operation));
    FString ApplyError;
    return GV2PresentationApply::Apply(Transaction, ApplyError);
}

// PSC-09B/10A: same split as Apply() above -- resolved-presentation-first, legacy
// Theme-touching fallback otherwise. See Apply()'s own doc comment for the split
// rationale; StyleToken.IsNone() is never true on the resolved path (Resolve() always
// stores the already-effective, non-None token), so the RichTextStyle-specific
// None-token fallback below is only ever reached by the legacy path, unchanged.
bool UGV2TextPipeline::ApplyRichText(
    UCommonRichTextBlock* Widget,
    const FGV2TextViewModel& Text,
    const UWidget* ContextWidget)
{
    if (Widget == nullptr)
    {
        return false;
    }

    TSubclassOf<UCommonTextStyle> Style;
    FTextBlockStyle DefaultStyle;
    bool bHasDefaultStyle = false;
    GV2PresentationApply::FPreparedTextScalePolicy ScalePolicy;

    if (Text.bHasResolvedPresentation)
    {
        Style = Text.ResolvedStyleClass;
        bHasDefaultStyle = Text.bHasResolvedDefaultStyle;
        DefaultStyle = Text.ResolvedDefaultStyle;
        ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
        ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
        ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
        ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;
    }
    else
    {
        const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
        Style = Text.StyleToken.IsNone()
            ? (Theme != nullptr ? Theme->RichTextStyle : nullptr)
            : ResolveStyleClass(Text.StyleToken);
        bHasDefaultStyle = ResolveStyle(Text.StyleToken, DefaultStyle, ContextWidget != nullptr ? ContextWidget : Widget);
        // DefaultStyle already carries its final, scaled font size from ResolveStyle --
        // ScalePolicy.bIsAlreadyScaled makes Apply's own SetFontSize call a same-value
        // no-op instead of a second scaling pass.
        ScalePolicy.BaseFontSize = bHasDefaultStyle ? DefaultStyle.Font.Size : 0.0f;
        ScalePolicy.bIsAlreadyScaled = true;
    }

    FString Markup = Text.NormalizedMarkup;
    if (Markup.IsEmpty() && !Text.Text.IsEmpty())
    {
        FString Error;
        if (!NormalizeMarkup(Text.Text.ToString(), Markup, Error))
        {
            return false;
        }
    }

    GV2PresentationApply::FPreparedRichTextRenderOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Style = Style;
    Operation.DefaultStyle = DefaultStyle;
    Operation.bHasDefaultStyle = bHasDefaultStyle;
    Operation.Markup = MoveTemp(Markup);
    Operation.ScalePolicy = ScalePolicy;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddRichTextRenderOperation(MoveTemp(Operation));
    FString ApplyError;
    return GV2PresentationApply::Apply(Transaction, ApplyError);
}

// PSC-09B: same split -- no Theme lookup needed here beyond the guard already checked,
// so this is the smallest of the three, but still goes through the same protocol as the
// other two, not a direct SetHintText call.
bool UGV2TextPipeline::ApplyHint(UEditableTextBox* Widget, const FGV2TextViewModel& Text)
{
    if (Widget == nullptr || Text.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }

    GV2PresentationApply::FPreparedTextHintOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Text = Text.Text;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddTextHintOperation(MoveTemp(Operation));
    FString ApplyError;
    return GV2PresentationApply::Apply(Transaction, ApplyError);
}

bool UGV2TextPipeline::NormalizeMarkup(
    const FString& Source,
    FString& OutMarkup,
    FString& OutError)
{
    OutMarkup.Reset();
    OutError.Reset();
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        OutError = TEXT("Text theme is unavailable.");
        return false;
    }

    TArray<FMarkupFrame> Stack;
    Stack.AddDefaulted();
    auto Emit = [&OutMarkup, &Stack](const FString& Text)
    {
        if (Text.IsEmpty()) return;
        const FString Escaped = EscapeResolvedText(Text);
        const FMarkupFrame& Frame = Stack.Last();
        if (Frame.Style.IsNone() && Frame.Color.IsNone() && Frame.Size.IsNone()
            && Frame.Interactive.IsNone())
        {
            OutMarkup += Escaped;
            return;
        }
        OutMarkup += TEXT("<gv2");
        if (!Frame.Style.IsNone()) OutMarkup += FString::Printf(TEXT(" style=\"%s\""), *Frame.Style.ToString());
        if (!Frame.Color.IsNone()) OutMarkup += FString::Printf(TEXT(" color=\"%s\""), *Frame.Color.ToString());
        if (!Frame.Size.IsNone()) OutMarkup += FString::Printf(TEXT(" size=\"%s\""), *Frame.Size.ToString());
        if (!Frame.Interactive.IsNone()) OutMarkup += FString::Printf(TEXT(" interactive=\"%s\""), *Frame.Interactive.ToString());
        OutMarkup += TEXT(">");
        OutMarkup += Escaped;
        OutMarkup += TEXT("</>");
    };

    int32 Cursor = 0;
    while (Cursor < Source.Len())
    {
        const int32 Open = Source.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
        if (Open == INDEX_NONE)
        {
            Emit(Source.Mid(Cursor));
            break;
        }
        Emit(Source.Mid(Cursor, Open - Cursor));
        const int32 Close = Source.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
        if (Close == INDEX_NONE)
        {
            OutError = TEXT("Unclosed text markup tag.");
            return false;
        }
        FString Body = Source.Mid(Open + 1, Close - Open - 1).TrimStartAndEnd();
        if (Body.Equals(TEXT("br"), ESearchCase::IgnoreCase)
            || Body.Equals(TEXT("br/"), ESearchCase::IgnoreCase)
            || Body == TEXT("\\n"))
        {
            OutMarkup += TEXT("\n");
            Cursor = Close + 1;
            continue;
        }
        if (Body.StartsWith(TEXT("/")))
        {
            const FString Closing = Body.Mid(1).ToLower();
            if (Stack.Num() <= 1 || (!Closing.IsEmpty() && Closing != Stack.Last().Tag))
            {
                OutError = TEXT("Mismatched text markup closing tag.");
                return false;
            }
            Stack.Pop();
            Cursor = Close + 1;
            continue;
        }

        FMarkupFrame Frame = Stack.Last();
        FString Value;
        int32 TagLength = 0;
        while (TagLength < Body.Len() && Body[TagLength] != TEXT('=')
            && !FChar::IsWhitespace(Body[TagLength]))
        {
            ++TagLength;
        }
        FString Tag = Body.Left(TagLength).ToLower();
        if (Tag == TEXT("color") && ReadAssignedValue(Body, Value)
            && Theme->TextColorTokens.Contains(FName(Value)))
        {
            Frame.Color = FName(Value);
        }
        else if (Tag == TEXT("size") && ReadAssignedValue(Body, Value)
            && Theme->TextSizeTokens.Contains(FName(Value)))
        {
            Frame.Size = FName(Value);
        }
        else if (Tag == TEXT("style") && ReadAssignedValue(Body, Value)
            && Theme->TextStyleTokens.Contains(FName(Value)))
        {
            Frame.Style = FName(Value);
        }
        else if (Tag == TEXT("interactive") && ReadInteractiveId(Body, Value))
        {
            Frame.Interactive = FName(Value);
        }
        else
        {
            OutError = FString::Printf(TEXT("Unknown or invalid text markup token: <%s>"), *Body);
            return false;
        }
        Frame.Tag = Tag;
        Stack.Add(Frame);
        Cursor = Close + 1;
    }
    if (Stack.Num() != 1)
    {
        OutError = TEXT("Unclosed text markup scope.");
        return false;
    }
    return true;
}
