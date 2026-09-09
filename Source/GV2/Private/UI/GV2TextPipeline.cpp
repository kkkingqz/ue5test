#include "UI/GV2TextPipeline.h"

#include "UI/GV2ApplyTransaction.h"
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

// Pure Theme-parameterized core shared by semantic Prepare and test fixtures. Runtime
// callers obtain Theme only from FGV2PresentationPrepareContext before building an
// operation; the apply side cannot reach this function.
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

// PSC-10B: fills every resolved-presentation field of OutText from an explicit Theme, and
// is the ONLY implementation of that filling. Both the production entry point and the
// automation-test seams below funnel through it, so a test can never observe a resolution
// this function would not have produced.
void FillResolvedPresentation(const UGV2UiTheme& Theme, FName StyleToken, FGV2TextViewModel& OutText)
{
    OutText.StyleToken = StyleToken;
    OutText.bHasResolvedPresentation = true;
    OutText.ResolvedStyleClass = ResolveStyleClassCore(&Theme, StyleToken);
    OutText.ResolvedBaseFontSize = Theme.ResolveUnscaledFontSize(StyleToken);
    OutText.ResolvedMinReadableFontSize = Theme.MinReadableFontSize;
    OutText.ResolvedReferenceViewportHeight = Theme.ReferenceViewportHeight;
    OutText.ResolvedFontScaleCurve = Theme.TextScaleCurve;
    OutText.bHasResolvedDefaultStyle = ResolveStyleCore(&Theme, StyleToken, OutText.ResolvedDefaultStyle);
}

FName ResolveDeclaredStyleToken(const UGV2UiTheme& Theme, FName StyleToken)
{
    if (!StyleToken.IsNone())
    {
        return StyleToken;
    }
    return !Theme.DefaultTextStyleToken.IsNone() ? Theme.DefaultTextStyleToken : FName(TEXT("default"));
}

bool ResolveTextWithTheme(
    const UGV2UiTheme& Theme,
    const UGV2UiTheme* FallbackTheme,
    const FString& TextId,
    const TArray<FGV2UiControlValue>& Args,
    FName StyleToken,
    FGV2TextViewModel& OutText,
    FString& OutError)
{
    OutText = {};
    OutError.Reset();
    const FText* Template = Theme.TextCatalog.Find(TextId);
    if (Template == nullptr)
    {
        Template = Theme.FallbackTextCatalog.Find(TextId);
    }
    // PSC-10B: a text id the authored Theme does not carry falls back to the snapshot's
    // pinned core-minimal Theme -- the same substitution the retired accessor-based Resolve
    // performed, but against a value the session resolved at build time instead of a
    // process-global lookup reached from the Commit-facing side.
    if (Template == nullptr && FallbackTheme != nullptr)
    {
        Template = FallbackTheme->TextCatalog.Find(TextId);
        if (Template == nullptr)
        {
            Template = FallbackTheme->FallbackTextCatalog.Find(TextId);
        }
    }
    if (Template == nullptr)
    {
        OutError = FString::Printf(TEXT("Unknown text_id: %s"), *TextId);
        return false;
    }
    StyleToken = ResolveDeclaredStyleToken(Theme, StyleToken);
    if (!Theme.TextStyleTokens.IsEmpty()
        && !Theme.TextStyleTokens.Contains(StyleToken)
        && StyleToken != FName(TEXT("default")))
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
    FillResolvedPresentation(Theme, StyleToken, OutText);
    return UGV2TextPipeline::NormalizeMarkup(
        &Theme,
        OutText.Text.ToString(),
        OutText.NormalizedMarkup,
        OutError);
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
    const UGV2UiTheme* Theme = PrepareContext != nullptr
        ? PrepareContext->GetTheme().Theme.Get()
        : nullptr;
    if (Theme == nullptr)
    {
        OutError = TEXT("core:diagnostic.ui_text.missing_prepare_context: session Theme is unavailable");
        return false;
    }
    const UGV2UiTheme* FallbackTheme = PrepareContext->GetTheme().FallbackTheme.Get();
    return ResolveTextWithTheme(*Theme, FallbackTheme, TextId, Args, StyleToken, OutText, OutError);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UGV2TextPipeline::ResolveForAutomationTest(
    const UGV2UiTheme* Theme,
    const FString& TextId,
    const TArray<FGV2UiControlValue>& Args,
    FName StyleToken,
    FGV2TextViewModel& OutText,
    FString& OutError)
{
    if (Theme == nullptr)
    {
        OutText = {};
        OutError = TEXT("Automation-test Theme is unavailable.");
        return false;
    }
    return ResolveTextWithTheme(*Theme, nullptr, TextId, Args, StyleToken, OutText, OutError);
}

// PSC-10B: the one sanctioned way for a test to obtain a resolved view model from LITERAL
// text rather than a catalog id. It shares FillResolvedPresentation and NormalizeMarkup with
// the production path, so a fixture cannot invent a resolution the production resolver would
// not produce -- the defect that a hand-rolled test helper reintroduced once already.
bool UGV2TextPipeline::ResolveLiteralForAutomationTest(
    const UGV2UiTheme* Theme,
    const FString& LiteralText,
    FName StyleToken,
    FGV2TextViewModel& OutText,
    FString& OutError)
{
    OutText = {};
    OutError.Reset();
    if (Theme == nullptr)
    {
        OutError = TEXT("Automation-test Theme is unavailable.");
        return false;
    }
    OutText.Text = FText::FromString(LiteralText);
    FillResolvedPresentation(*Theme, ResolveDeclaredStyleToken(*Theme, StyleToken), OutText);
    return NormalizeMarkup(Theme, LiteralText, OutText.NormalizedMarkup, OutError);
}
#endif

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

float UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(
    const UGV2UiTheme* Theme,
    const FName TextSizeToken,
    const float ViewportHeight)
{
    if (Theme == nullptr)
    {
        return 14.0f;
    }
    const FName Token = TextSizeToken.IsNone()
        ? ((Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone()) ? Theme->DefaultTextStyleToken : FName("default"))
        : TextSizeToken;
    return Theme->GetEffectiveFontSize(Token, ViewportHeight);
}

bool UGV2TextPipeline::ResolveStyleForHeight(
    const UGV2UiTheme* Theme,
    const FName StyleToken,
    FTextBlockStyle& OutStyle,
    const float ViewportHeight)
{
    const FName EffectiveToken = StyleToken.IsNone()
        ? ((Theme != nullptr && !Theme->DefaultTextStyleToken.IsNone()) ? Theme->DefaultTextStyleToken : FName("default"))
        : StyleToken;
    if (!ResolveStyleCore(Theme, StyleToken, OutStyle))
    {
        return false;
    }

    // Apply DPI scaling and clamp to MinReadableFontSize
    const float EffectiveSize = ResolveEffectiveFontSizeForHeight(Theme, EffectiveToken, ViewportHeight);
    OutStyle.SetFontSize(EffectiveSize);
    return true;
}

// Apply accepts only a presentation resolved during Prepare and translates it to a lower
// module value-only transaction. It performs no Theme or token lookup.
bool UGV2TextPipeline::Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text)
{
    const TSubclassOf<UCommonTextStyle> Style = Text.ResolvedStyleClass;
    // Plain renderer deliberately rejects semantic styled/interactive runs instead of
    // leaking authoring markup to the player. Such content must use the RichText leaf.
    if (Widget == nullptr || !Text.bHasResolvedPresentation || Style == nullptr
        || Text.NormalizedMarkup.Contains(TEXT("<gv2"))) return false;

    GV2PresentationApply::FPreparedTextScalePolicy ScalePolicy;
    ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
    ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
    ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
    ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;

    GV2PresentationApply::FPreparedPlainTextOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Style = Style;
    Operation.Text = Text.Text;
    Operation.ScalePolicy = ScalePolicy;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddPlainTextOperation(MoveTemp(Operation));
    FString ApplyError;
    return GV2ApplyTransaction(Transaction, ApplyError);
}

// RichText follows the same resolved-only boundary. Markup, style and live-geometry scale
// policy are all values prepared before this physical-effect path is entered.
bool UGV2TextPipeline::ApplyRichText(
    UCommonRichTextBlock* Widget,
    const FGV2TextViewModel& Text,
    const UWidget* /*ContextWidget*/)
{
    if (Widget == nullptr || !Text.bHasResolvedPresentation)
    {
        return false;
    }

    TSubclassOf<UCommonTextStyle> Style;
    FTextBlockStyle DefaultStyle;
    bool bHasDefaultStyle = false;
    GV2PresentationApply::FPreparedTextScalePolicy ScalePolicy;

    Style = Text.ResolvedStyleClass;
    bHasDefaultStyle = Text.bHasResolvedDefaultStyle;
    DefaultStyle = Text.ResolvedDefaultStyle;
    ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
    ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
    ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
    ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;

    FString Markup = Text.NormalizedMarkup;
    if (Markup.IsEmpty() && !Text.Text.IsEmpty()) return false;

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
    return GV2ApplyTransaction(Transaction, ApplyError);
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
    return GV2ApplyTransaction(Transaction, ApplyError);
}

bool UGV2TextPipeline::NormalizeMarkup(
    const UGV2UiTheme* Theme,
    const FString& Source,
    FString& OutMarkup,
    FString& OutError)
{
    OutMarkup.Reset();
    OutError.Reset();
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
