#include "UI/GV2PropertyConsumers.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"

// --- FGV2TextPropertyConsumer ---

bool FGV2TextPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsText();
}

bool FGV2TextPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    UWidget* ResolvedWidget = TargetWidget;
    if (UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = TW->GetTextBlock();
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock();
    }

    if (!ResolvedWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for text capability");
        return false;
    }

    if (!Cast<UCommonTextBlock>(ResolvedWidget) && !Cast<UCommonRichTextBlock>(ResolvedWidget) && !Cast<UEditableTextBox>(ResolvedWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a supported text renderer");
        return false;
    }

    if (!Value.IsText())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Text value");
        return false;
    }

    PreparedText = Value.AsText();
    if (Cast<UEditableTextBox>(ResolvedWidget) || Cast<UCommonTextBlock>(ResolvedWidget))
    {
        if (PreparedText.NormalizedMarkup.Contains(TEXT("<gv2")))
        {
            OutError = TEXT("core:diagnostic.ui_consumer.unsupported_text_markup: Plain text renderer does not support formatted markup");
            return false;
        }
    }
    return true;
}

bool FGV2TextPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    UWidget* ResolvedWidget = TargetWidget;
    if (UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = TW->GetTextBlock();
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock();
    }

    if (!ResolvedWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(ResolvedWidget))
    {
        return UGV2TextPipeline::Apply(TextBlock, PreparedText);
    }
    if (UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(ResolvedWidget))
    {
        return UGV2TextPipeline::ApplyRichText(RichTextBlock, PreparedText);
    }
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(ResolvedWidget))
    {
        return UGV2TextPipeline::ApplyHint(EditableBox, PreparedText);
    }

    OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a supported text renderer");
    return false;
}

void FGV2TextPropertyConsumer::Reset(UWidget* TargetWidget)
{
    UWidget* ResolvedWidget = TargetWidget;
    if (UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = TW->GetTextBlock();
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock();
    }

    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(ResolvedWidget))
    {
        UGV2TextPipeline::Apply(TextBlock, FGV2TextViewModel());
    }
    else if (UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(ResolvedWidget))
    {
        UGV2TextPipeline::ApplyRichText(RichTextBlock, FGV2TextViewModel());
    }
    else if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(ResolvedWidget))
    {
        UGV2TextPipeline::ApplyHint(EditableBox, FGV2TextViewModel());
    }
}

// --- FGV2ImageResourcePropertyConsumer ---

bool FGV2ImageResourcePropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsStableId() && Value.AsStableId().TargetKind == TEXT("resource");
}

bool FGV2ImageResourcePropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for image capability");
        return false;
    }

    UImage* ImageWidget = Cast<UImage>(TargetWidget);
    if (!ImageWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage");
        return false;
    }

    if (!Value.IsStableId() || Value.AsStableId().TargetKind != TEXT("resource"))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected StableId with target_kind 'resource'");
        return false;
    }

    EGV2PrimitiveScalePolicy Policy = EGV2PrimitiveScalePolicy::Unset;
    TOptional<float> FixedAspect;
    if (UGV2ImageWidgetBase* ImageHost = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        Policy = ImageHost->GetScalePolicy();
        if (ImageHost->GetFixedAspectRatio() > 0.0f)
        {
            FixedAspect = ImageHost->GetFixedAspectRatio();
        }
    }
    else if (UGV2ImageWidgetBase* OuterHost = TargetWidget->GetTypedOuter<UGV2ImageWidgetBase>())
    {
        Policy = OuterHost->GetScalePolicy();
        if (OuterHost->GetFixedAspectRatio() > 0.0f)
        {
            FixedAspect = OuterHost->GetFixedAspectRatio();
        }
    }
    else
    {
        Policy = EGV2PrimitiveScalePolicy::PreserveAspect;
    }

    if (Policy == EGV2PrimitiveScalePolicy::Unset)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.unset_scale_policy: Primitive scale policy is Unset");
        return false;
    }

    PreparedResourceId = Value.AsStableId().Id;
    PreparedScalePolicy = Policy;
    PreparedFixedAspectRatio = FixedAspect;

    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    if (Catalog == nullptr)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_catalog: Configured Image Resource Catalog is unavailable");
        return false;
    }

    FGV2ResolvedImageResource Candidate;
    if (!Catalog->Resolve(PreparedResourceId, Candidate, OutError))
    {
        return false;
    }

    if (!IsScalePolicyCompatible(PreparedScalePolicy, Candidate.RenderMode))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.incompatible_scale_policy: Image resource render mode is incompatible with primitive scaling policy");
        return false;
    }

    if (PreparedScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && PreparedFixedAspectRatio.IsSet()
        && (!FMath::IsFinite(PreparedFixedAspectRatio.GetValue()) || PreparedFixedAspectRatio.GetValue() <= 0.0f
            || !FMath::IsNearlyEqual(Candidate.FixedAspectRatio, PreparedFixedAspectRatio.GetValue(), 0.001f)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.aspect_ratio_mismatch: fixed_aspect resource ratio does not match the target block ratio");
        return false;
    }

    return true;
}

bool FGV2ImageResourcePropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    UImage* ImageWidget = Cast<UImage>(TargetWidget);
    if (!ImageWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage");
        return false;
    }

    FGV2ResolvedImageResource Resolved;
    return FGV2ImagePresentation::ResolveAndApply(
        ImageWidget, PreparedResourceId, PreparedScalePolicy, PreparedFixedAspectRatio, Resolved, OutError);
}

void FGV2ImageResourcePropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        ImageWidget->SetBrush(FSlateBrush());
    }
}

// --- FGV2BooleanPropertyConsumer ---

bool FGV2BooleanPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBoolean();
}

bool FGV2BooleanPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for boolean capability");
        return false;
    }

    if (!Value.IsBoolean())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Boolean value");
        return false;
    }

    bPreparedValue = Value.AsBoolean();
    PropertyName = Capability.PropertyName;
    return true;
}

bool FGV2BooleanPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UCheckBox* CheckBox = Cast<UCheckBox>(TargetWidget))
    {
        if (PropertyName == TEXT("is_read_only"))
        {
            CheckBox->SetIsEnabled(!bPreparedValue);
        }
        else
        {
            CheckBox->SetIsChecked(bPreparedValue);
        }
        return true;
    }
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        if (PropertyName == TEXT("is_read_only"))
        {
            EditableBox->SetIsReadOnly(bPreparedValue);
        }
        else
        {
            EditableBox->SetIsEnabled(bPreparedValue);
        }
        return true;
    }

    TargetWidget->SetIsEnabled(bPreparedValue);
    return true;
}

void FGV2BooleanPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UCheckBox* CheckBox = Cast<UCheckBox>(TargetWidget))
    {
        if (PropertyName == TEXT("is_read_only"))
        {
            CheckBox->SetIsEnabled(true);
        }
        else
        {
            CheckBox->SetIsChecked(false);
        }
    }
    else if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        if (PropertyName == TEXT("is_read_only"))
        {
            EditableBox->SetIsReadOnly(false);
        }
        else
        {
            EditableBox->SetIsEnabled(true);
        }
    }
    else if (TargetWidget)
    {
        TargetWidget->SetIsEnabled(true);
    }
}

// --- FGV2IntegerPropertyConsumer ---

bool FGV2IntegerPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsInteger();
}

bool FGV2IntegerPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for integer capability");
        return false;
    }

    if (!Value.IsInteger())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Integer value");
        return false;
    }

    PreparedValue = Value.AsInteger();
    PropertyName = Capability.PropertyName;
    return true;
}

bool FGV2IntegerPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        if (UGV2InputFieldWidgetBase* InputField = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
        {
            InputField->SetMaxLength(PreparedValue);
        }
        if (PreparedValue > 0)
        {
            const FString Current = EditableBox->GetText().ToString();
            if (Current.Len() > PreparedValue)
            {
                EditableBox->SetText(FText::FromString(Current.Left(static_cast<int32>(PreparedValue))));
            }
        }
    }

    return true;
}

void FGV2IntegerPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        if (UGV2InputFieldWidgetBase* InputField = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
        {
            InputField->SetMaxLength(0);
        }
    }
    PreparedValue = 0;
}

// --- FGV2NumberPropertyConsumer ---

bool FGV2NumberPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsNumber();
}

bool FGV2NumberPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for number capability");
        return false;
    }

    if (!Value.IsNumber())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Number value");
        return false;
    }

    PreparedValue = Value.AsNumber();
    return true;
}

bool FGV2NumberPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        PB->SetPercent(static_cast<float>(PreparedValue));
    }
    return true;
}

void FGV2NumberPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        PB->SetPercent(0.0f);
    }
}

// --- FGV2StringPropertyConsumer ---

bool FGV2StringPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsString();
}

bool FGV2StringPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for string capability");
        return false;
    }

    if (!Value.IsString())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected String value");
        return false;
    }

    PreparedValue = Value.AsString();
    return true;
}

bool FGV2StringPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        FString FinalText = PreparedValue;
        if (const UGV2InputFieldWidgetBase* Host = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
        {
            const int64 MaxLength = Host->GetMaxLength();
            if (MaxLength > 0 && FinalText.Len() > MaxLength)
            {
                FinalText = FinalText.Left(static_cast<int32>(MaxLength));
            }
        }
        EditableBox->SetText(FText::FromString(FinalText));
    }

    return true;
}

void FGV2StringPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(TargetWidget))
    {
        EditableBox->SetText(FText::GetEmpty());
    }
    PreparedValue.Empty();
}

// --- FGV2KeyPropertyConsumer ---

bool FGV2KeyPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsKey();
}

bool FGV2KeyPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for key capability");
        return false;
    }

    if (!Value.IsKey())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Key value");
        return false;
    }

    PreparedValue = Value.AsKey();
    return true;
}

bool FGV2KeyPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        Button->SetKey(FName(*PreparedValue));
    }
    else if (UGV2CheckboxWidgetBase* Checkbox = Cast<UGV2CheckboxWidgetBase>(TargetWidget))
    {
        Checkbox->SetKey(FName(*PreparedValue));
    }
    else if (UGV2InputFieldWidgetBase* InputField = Cast<UGV2InputFieldWidgetBase>(TargetWidget))
    {
        InputField->SetKey(FName(*PreparedValue));
    }
    else if (UGV2ProgressBarWidgetBase* ProgressBar = Cast<UGV2ProgressBarWidgetBase>(TargetWidget))
    {
        ProgressBar->SetKey(FName(*PreparedValue));
    }
    else if (UGV2PortraitWidgetBase* Portrait = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        Portrait->SetKey(FName(*PreparedValue));
    }
    else if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        RichText->SetKey(FName(*PreparedValue));
    }
    else if (UGV2RichTextPopoverWidgetBase* Popover = Cast<UGV2RichTextPopoverWidgetBase>(TargetWidget))
    {
        Popover->SetKey(FName(*PreparedValue));
    }
    return true;
}

void FGV2KeyPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        Button->SetKey(NAME_None);
    }
    else if (UGV2CheckboxWidgetBase* Checkbox = Cast<UGV2CheckboxWidgetBase>(TargetWidget))
    {
        Checkbox->SetKey(NAME_None);
    }
    else if (UGV2InputFieldWidgetBase* InputField = Cast<UGV2InputFieldWidgetBase>(TargetWidget))
    {
        InputField->SetKey(NAME_None);
    }
    else if (UGV2ProgressBarWidgetBase* ProgressBar = Cast<UGV2ProgressBarWidgetBase>(TargetWidget))
    {
        ProgressBar->SetKey(NAME_None);
    }
    else if (UGV2PortraitWidgetBase* Portrait = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        Portrait->SetKey(NAME_None);
    }
    else if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        RichText->SetKey(NAME_None);
    }
    else if (UGV2RichTextPopoverWidgetBase* Popover = Cast<UGV2RichTextPopoverWidgetBase>(TargetWidget))
    {
        Popover->SetKey(NAME_None);
    }
    PreparedValue.Empty();
}

// --- FGV2BindingPropertyConsumer ---

bool FGV2BindingPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBinding();
}

bool FGV2BindingPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for binding capability");
        return false;
    }

    if (!Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget does not implement IGV2UiBindingTarget");
        return false;
    }

    if (!Value.IsBinding())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Binding value");
        return false;
    }

    PreparedBinding = Value.AsBinding();
    return true;
}

bool FGV2BindingPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        BindingTarget->SetBindingHandle(PreparedBinding);
    }
    return true;
}

void FGV2BindingPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        BindingTarget->SetBindingHandle(FGV2UiBindingHandle());
    }
}

// --- FGV2PropertyConsumerFactory ---

TSharedPtr<IGV2PropertyConsumer> FGV2PropertyConsumerFactory::CreateConsumer(
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FString& TargetKind)
{
    switch (Kind)
    {
    case EGV2PreparedUiValueKind::Text:
        return MakeShared<FGV2TextPropertyConsumer>();
    case EGV2PreparedUiValueKind::StableId:
        if (TargetKind == TEXT("resource"))
        {
            return MakeShared<FGV2ImageResourcePropertyConsumer>();
        }
        break;
    case EGV2PreparedUiValueKind::Boolean:
        return MakeShared<FGV2BooleanPropertyConsumer>();
    case EGV2PreparedUiValueKind::Integer:
        return MakeShared<FGV2IntegerPropertyConsumer>();
    case EGV2PreparedUiValueKind::Number:
        return MakeShared<FGV2NumberPropertyConsumer>();
    case EGV2PreparedUiValueKind::String:
        return MakeShared<FGV2StringPropertyConsumer>();
    case EGV2PreparedUiValueKind::Key:
        return MakeShared<FGV2KeyPropertyConsumer>();
    case EGV2PreparedUiValueKind::Binding:
        return MakeShared<FGV2BindingPropertyConsumer>();
    default:
        break;
    }
    return nullptr;
}
