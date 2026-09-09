#include "UI/GV2InputFieldWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"

void UGV2InputFieldWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedInputFieldStyle; the serialized box and
    // label styles already render at design time. See UGV2SeparatorWidgetBase.
}

void UGV2InputFieldWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (EditableTextBox)
    {
        EditableTextBox->OnTextCommitted.RemoveDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
        EditableTextBox->OnTextCommitted.AddDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
    }
}

void UGV2InputFieldWidgetBase::NativeDestruct()
{
    if (EditableTextBox)
    {
        EditableTextBox->OnTextCommitted.RemoveDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
    }
    Super::NativeDestruct();
}

void UGV2InputFieldWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (LabelText != nullptr)
    {
        OutBuilder.AddText(TEXT("text"), FName(TEXT("LabelText")));
    }
    OutBuilder.AddText(TEXT("placeholder_text"), FName(TEXT("EditableTextBox")));
    OutBuilder.AddString(TEXT("value"), FName(TEXT("EditableTextBox")));
    OutBuilder.AddBoolean(TEXT("is_read_only"), FName(TEXT("EditableTextBox")));
    OutBuilder.AddInteger(TEXT("max_length"), FName(TEXT("EditableTextBox")), 0, 10000);
    OutBuilder.AddBinding(TEXT("binding"), NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

void UGV2InputFieldWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InHandle)
{
    BindingHandle = InHandle;
    SetIsEnabled(BindingHandle.IsValid());
}

FGV2UiBindingHandle UGV2InputFieldWidgetBase::GetBindingHandle() const
{
    return BindingHandle;
}

void UGV2InputFieldWidgetBase::SetIsReadOnly(bool bInIsReadOnly)
{
    if (EditableTextBox)
    {
        EditableTextBox->SetIsReadOnly(bInIsReadOnly);
    }
}

bool UGV2InputFieldWidgetBase::GetIsReadOnly() const
{
    return EditableTextBox ? EditableTextBox->GetIsReadOnly() : false;
}

void UGV2InputFieldWidgetBase::SetMaxLength(int64 InMaxLength)
{
    MaxLength = InMaxLength;
    if (MaxLength > 0 && EditableTextBox)
    {
        const FString Current = EditableTextBox->GetText().ToString();
        if (Current.Len() > MaxLength)
        {
            EditableTextBox->SetText(FText::FromString(Current.Left(static_cast<int32>(MaxLength))));
        }
    }
}

void UGV2InputFieldWidgetBase::SetValue(const FString& InValue)
{
    if (EditableTextBox)
    {
        FString Truncated = InValue;
        if (MaxLength > 0 && Truncated.Len() > MaxLength)
        {
            Truncated = Truncated.Left(static_cast<int32>(MaxLength));
        }
        EditableTextBox->SetText(FText::FromString(Truncated));
    }
}

FString UGV2InputFieldWidgetBase::GetValue() const
{
    return EditableTextBox ? EditableTextBox->GetText().ToString() : FString();
}

bool UGV2InputFieldWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    if (InText.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    AppliedLabelText = InText;

    // PSC-10B: the box's OWN text size follows this field's label style token, so it is
    // applied where the token actually arrives -- with the text, in the same transaction --
    // instead of being re-derived from a Theme later inside ApplyCentralStyle. The
    // resolved-presentation-first split below is UGV2TextPipeline::Apply's own split; the
    // legacy branch's ResolveEffectiveFontSize is the same fallback that function still
    // has, and dies with the configured accessor rather than separately.
    if (EditableTextBox != nullptr)
    {
        GV2PresentationApply::FPreparedTextScalePolicy ScalePolicy;
        if (InText.bHasResolvedPresentation)
        {
            ScalePolicy.BaseFontSize = InText.ResolvedBaseFontSize;
            ScalePolicy.MinReadableFontSize = InText.ResolvedMinReadableFontSize;
            ScalePolicy.ReferenceViewportHeight = InText.ResolvedReferenceViewportHeight;
            ScalePolicy.ScaleCurve = InText.ResolvedFontScaleCurve;
        }
        else
        {
            ScalePolicy.BaseFontSize = UGV2TextPipeline::ResolveEffectiveFontSize(
                InText.StyleToken.IsNone() ? FName(TEXT("body")) : InText.StyleToken, this);
            ScalePolicy.bIsAlreadyScaled = true;
        }
        EditableTextBox->WidgetStyle.TextStyle.Font.Size = GV2PresentationApply::EvaluatePreparedFontSize(
            ScalePolicy,
            GV2PresentationApply::ResolveLiveViewportHeight(this, ScalePolicy.ReferenceViewportHeight));
    }

    if (LabelText != nullptr)
    {
        return UGV2TextPipeline::Apply(LabelText, InText);
    }
    return true;
}

bool UGV2InputFieldWidgetBase::ApplyPlaceholderText(const FGV2TextViewModel& InPlaceholder)
{
    if (!EditableTextBox)
    {
        return false;
    }
    AppliedPlaceholderText = InPlaceholder;
    return UGV2TextPipeline::ApplyHint(EditableTextBox, InPlaceholder);
}

void UGV2InputFieldWidgetBase::ApplyInputFieldStyleValues(
    const FEditableTextBoxStyle& InWidgetStyle,
    TSubclassOf<UCommonTextStyle> InDefaultLabelStyle,
    const GV2PresentationApply::FPreparedTextScalePolicy& InDefaultLabelScale)
{
    if (EditableTextBox == nullptr)
    {
        return;
    }

    const bool bHasStyleToken = !AppliedLabelText.StyleToken.IsNone();
    // Assigning the theme's whole box style would drop the text size ApplyText just set for
    // this field's token, so that one field is carried across the assignment. Without a
    // token nothing established a size and the role's own default applies.
    const float SizeFromTextOperation = EditableTextBox->WidgetStyle.TextStyle.Font.Size;
    EditableTextBox->WidgetStyle = InWidgetStyle;
    EditableTextBox->WidgetStyle.TextStyle.Font.Size = bHasStyleToken
        ? SizeFromTextOperation
        : GV2PresentationApply::EvaluatePreparedFontSize(
            InDefaultLabelScale,
            GV2PresentationApply::ResolveLiveViewportHeight(this, InDefaultLabelScale.ReferenceViewportHeight));

    // See UGV2ButtonWidgetBase::ApplyButtonStyleValues for why a token-carrying label is
    // left to its own text operation.
    if (LabelText != nullptr && InDefaultLabelStyle != nullptr && !bHasStyleToken)
    {
        LabelText->SetStyle(InDefaultLabelStyle);
        FSlateFontInfo FontInfo = LabelText->GetFont();
        const float DefaultSize = EditableTextBox->WidgetStyle.TextStyle.Font.Size;
        if (!FMath::IsNearlyEqual(FontInfo.Size, DefaultSize, 0.01f))
        {
            FontInfo.Size = DefaultSize;
            LabelText->SetFont(FontInfo);
        }
    }
}

bool UGV2InputFieldWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: carried by FPreparedInputFieldStyle, written by ApplyInputFieldStyleValues.
    return true;
}

void UGV2InputFieldWidgetBase::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    FString CommittedString = Text.ToString();
    if (MaxLength > 0 && CommittedString.Len() > MaxLength)
    {
        CommittedString = CommittedString.Left(static_cast<int32>(MaxLength));
        if (EditableTextBox)
        {
            EditableTextBox->SetText(FText::FromString(CommittedString));
        }
    }
    SubmitTextValue(CommittedString);
}

EGV2SubmitUiInteractionResult UGV2InputFieldWidgetBase::SubmitTextValue(const FString& NewTextValue)
{
    if (!BindingHandle.IsValid())
    {
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    FGV2UiControlValue ControlValue;
    ControlValue.Name = TEXT("value");
    ControlValue.Type = EGV2UiControlValueType::String;
    ControlValue.StringValue = NewTextValue;

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, BindingHandle, {ControlValue});

    OnBindingInvoked.Broadcast(BindingHandle, NewTextValue, Result);
    return Result;
}
