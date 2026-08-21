#include "UI/GV2TextPipeline.h"

#include "CommonTextBlock.h"
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
}

bool UGV2TextPipeline::Resolve(
    const FString& TextId,
    const TArray<FGV2UiControlValue>& Args,
    FName StyleToken,
    FGV2TextViewModel& OutText,
    FString& OutError)
{
    OutText = {};
    OutError.Reset();
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
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
        StyleToken = Theme != nullptr ? Theme->DefaultTextStyleToken : FName("default");
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
        default:
            OutError = TEXT("Text arguments must be scalar non-null values.");
            return false;
        }
    }
    OutText.Text = FormatArgs.IsEmpty() ? *Template : FText::Format(*Template, FormatArgs);
    OutText.StyleToken = StyleToken;
    return NormalizeMarkup(OutText.Text.ToString(), OutText.NormalizedMarkup, OutError);
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
    const FName Token = TextSizeToken.IsNone() ? Theme->DefaultTextStyleToken : TextSizeToken;
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
    const FName EffectiveToken = StyleToken.IsNone() ? (Theme != nullptr ? Theme->DefaultTextStyleToken : FName("default")) : StyleToken;
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
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    const FGV2TextStyleToken* Token = Theme != nullptr
        ? Theme->TextStyleTokens.Find(StyleToken.IsNone() ? Theme->DefaultTextStyleToken : StyleToken)
        : nullptr;
    return Token != nullptr ? Token->Style : nullptr;
}

bool UGV2TextPipeline::Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text)
{
    const TSubclassOf<UCommonTextStyle> Style = ResolveStyleClass(Text.StyleToken);
    // Plain renderer deliberately rejects semantic styled/interactive runs instead of
    // leaking authoring markup to the player. Such content must use the RichText leaf.
    if (Widget == nullptr || Style == nullptr || Text.NormalizedMarkup.Contains(TEXT("<gv2"))) return false;
    Widget->SetStyle(Style);
    Widget->SetText(Text.Text);

    // Apply DPI-aware scaled font size without wiping out the widget's FontObject/typeface
    const float ScaledFontSize = ResolveEffectiveFontSize(Text.StyleToken, Widget);
    FSlateFontInfo FontInfo = Widget->GetFont();
    if (!FMath::IsNearlyEqual(FontInfo.Size, ScaledFontSize, 0.01f))
    {
        FontInfo.Size = ScaledFontSize;
        Widget->SetFont(FontInfo);
    }
    return true;
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
