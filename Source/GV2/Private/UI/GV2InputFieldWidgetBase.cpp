#include "UI/GV2InputFieldWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"

void UGV2InputFieldWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
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

bool UGV2InputFieldWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || EditableTextBox == nullptr
        || (LabelText != nullptr && Theme->InputFieldLabelStyle == nullptr))
    {
        return false;
    }

    EditableTextBox->WidgetStyle = Theme->InputFieldStyle;
    const float ScaledFontSize = UGV2TextPipeline::ResolveEffectiveFontSize(
        AppliedLabelText.StyleToken.IsNone() ? FName(TEXT("body")) : AppliedLabelText.StyleToken,
        this);
    EditableTextBox->WidgetStyle.TextStyle.Font.Size = ScaledFontSize;

    if (LabelText != nullptr && Theme->InputFieldLabelStyle != nullptr)
    {
        const TSubclassOf<UCommonTextStyle> LabelStyle = AppliedLabelText.StyleToken.IsNone()
            ? Theme->InputFieldLabelStyle
            : UGV2TextPipeline::ResolveStyleClass(AppliedLabelText.StyleToken);

        if (LabelStyle != nullptr)
        {
            LabelText->SetStyle(LabelStyle);
            FSlateFontInfo FontInfo = LabelText->GetFont();
            if (!FMath::IsNearlyEqual(FontInfo.Size, ScaledFontSize, 0.01f))
            {
                FontInfo.Size = ScaledFontSize;
                LabelText->SetFont(FontInfo);
            }
        }
    }

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
