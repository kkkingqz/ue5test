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
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiTheme.h"
#include "Bridge/GV2StableIdUE.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/UserWidget.h"

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
        ResolvedWidget = TW->GetTextBlock() ? Cast<UWidget>(TW->GetTextBlock()) : Cast<UWidget>(TW);
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock() ? Cast<UWidget>(RTW->GetRichTextBlock()) : Cast<UWidget>(RTW);
    }
    else if (UGV2ButtonWidgetBase* BW = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        ResolvedWidget = BW->GetLabelText() ? Cast<UWidget>(BW->GetLabelText()) : Cast<UWidget>(BW);
    }
    else if (UGV2DropdownSelectWidgetBase* DW = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        ResolvedWidget = DW;
    }

    if (!ResolvedWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for text capability");
        return false;
    }

    if (!Cast<UCommonTextBlock>(ResolvedWidget) && !Cast<UCommonRichTextBlock>(ResolvedWidget)
        && !Cast<UEditableTextBox>(ResolvedWidget) && !Cast<UGV2ButtonWidgetBase>(ResolvedWidget)
        && !Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget) && !Cast<UGV2TextWidgetBase>(ResolvedWidget)
        && !Cast<UGV2RichTextWidgetBase>(ResolvedWidget))
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
    if (Cast<UEditableTextBox>(ResolvedWidget) || Cast<UCommonTextBlock>(ResolvedWidget)
        || Cast<UGV2ButtonWidgetBase>(ResolvedWidget) || Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget)
        || Cast<UGV2TextWidgetBase>(ResolvedWidget))
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
        ResolvedWidget = TW->GetTextBlock() ? Cast<UWidget>(TW->GetTextBlock()) : Cast<UWidget>(TW);
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock() ? Cast<UWidget>(RTW->GetRichTextBlock()) : Cast<UWidget>(RTW);
    }
    else if (UGV2ButtonWidgetBase* BW = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        ResolvedWidget = BW;
    }
    else if (UGV2DropdownSelectWidgetBase* DW = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        ResolvedWidget = DW;
    }

    if (!ResolvedWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UGV2TextWidgetBase* TextWidget = Cast<UGV2TextWidgetBase>(ResolvedWidget))
    {
        return TextWidget->ApplyText(PreparedText);
    }
    if (UGV2ButtonWidgetBase* ButtonWidget = Cast<UGV2ButtonWidgetBase>(ResolvedWidget))
    {
        return ButtonWidget->ApplyText(PreparedText);
    }
    if (UGV2DropdownSelectWidgetBase* DropdownWidget = Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget))
    {
        return DropdownWidget->ApplyPlaceholderText(PreparedText);
    }
    if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(ResolvedWidget))
    {
        return RichText->ApplyText(PreparedText);
    }
    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(ResolvedWidget))
    {
        if (UGV2ButtonWidgetBase* ParentButton = TextBlock->GetTypedOuter<UGV2ButtonWidgetBase>())
        {
            ParentButton->ApplyText(PreparedText);
        }
        else if (UGV2TextWidgetBase* ParentTW = TextBlock->GetTypedOuter<UGV2TextWidgetBase>())
        {
            ParentTW->ApplyText(PreparedText);
        }
        return UGV2TextPipeline::Apply(TextBlock, PreparedText);
    }
    if (UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(ResolvedWidget))
    {
        if (UGV2RichTextWidgetBase* ParentRT = RichTextBlock->GetTypedOuter<UGV2RichTextWidgetBase>())
        {
            ParentRT->ApplyText(PreparedText);
        }
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
        ResolvedWidget = TW->GetTextBlock() ? Cast<UWidget>(TW->GetTextBlock()) : Cast<UWidget>(TW);
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW;
    }
    else if (UGV2ButtonWidgetBase* BW = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        ResolvedWidget = BW;
    }
    else if (UGV2DropdownSelectWidgetBase* DW = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        ResolvedWidget = DW;
    }

    if (UGV2TextWidgetBase* TextWidget = Cast<UGV2TextWidgetBase>(ResolvedWidget))
    {
        TextWidget->ApplyText({});
    }
    else if (UGV2ButtonWidgetBase* ButtonWidget = Cast<UGV2ButtonWidgetBase>(ResolvedWidget))
    {
        ButtonWidget->ApplyText({});
    }
    else if (UGV2DropdownSelectWidgetBase* DropdownWidget = Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget))
    {
        DropdownWidget->ApplyPlaceholderText({});
    }
    else if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(ResolvedWidget))
    {
        RichText->ApplyText({});
    }
    else if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(ResolvedWidget))
    {
        if (UGV2ButtonWidgetBase* ParentButton = TextBlock->GetTypedOuter<UGV2ButtonWidgetBase>())
        {
            ParentButton->ApplyText({});
        }
        else if (UGV2TextWidgetBase* ParentTW = TextBlock->GetTypedOuter<UGV2TextWidgetBase>())
        {
            ParentTW->ApplyText({});
        }
        UGV2TextPipeline::Apply(TextBlock, FGV2TextViewModel());
    }
    else if (UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(ResolvedWidget))
    {
        if (UGV2RichTextWidgetBase* ParentRT = RichTextBlock->GetTypedOuter<UGV2RichTextWidgetBase>())
        {
            ParentRT->ApplyText({});
        }
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
        if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(TargetWidget))
        {
            ImageWidget = ImageBase->GetImageWidget();
        }
        else if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(TargetWidget))
        {
            ImageWidget = PortraitWidget->GetPortraitImage();
        }
    }

    if (!ImageWidget && !Cast<UGV2ImageWidgetBase>(TargetWidget) && !Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage or image host");
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
    else if (UGV2PortraitWidgetBase* PortraitHost = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        Policy = EGV2PrimitiveScalePolicy::PreserveAspect;
        if (PortraitHost->GetPortraitAspectRatio() > 0.0f)
        {
            FixedAspect = PortraitHost->GetPortraitAspectRatio();
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

    // Routes through the host's own Apply method rather than reaching past it to its
    // inner UImage: GV2ImageWidgetBase/GV2PortraitWidgetBase track what they last applied
    // (AppliedResourceId/ResolvedAspectRatio, GetPortraitResourceId/GetFrameResourceId) as
    // part of their own bookkeeping, and bypassing it here left that bookkeeping frozen at
    // whatever NativePreConstruct set (or unset) even though the brush itself did update.
    if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        return ImageBase->ApplyImageResource(PreparedResourceId, OutError);
    }
    if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        PortraitWidget->SetVisibility(ESlateVisibility::Visible);
        return PortraitWidget->ApplyPortrait(PreparedResourceId, FString(), OutError);
    }
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        FGV2ResolvedImageResource Resolved;
        return FGV2ImagePresentation::ResolveAndApply(
            ImageWidget, PreparedResourceId, PreparedScalePolicy, PreparedFixedAspectRatio, Resolved, OutError);
    }

    OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage or image host");
    return false;
}

void FGV2ImageResourcePropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        ImageWidget->SetBrush(FSlateBrush());
    }
    else if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        PortraitWidget->SetVisibility(ESlateVisibility::Collapsed);
        if (UImage* Img = PortraitWidget->GetPortraitImage())
        {
            Img->SetBrush(FSlateBrush());
        }
    }
    else if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        if (UImage* Img = ImageBase->GetImageWidget())
        {
            Img->SetBrush(FSlateBrush());
        }
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
    if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        if (PropertyName == TEXT("is_open"))
        {
            Dropdown->SetDropdownOpen(bPreparedValue);
            return true;
        }
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
    else if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        if (PropertyName == TEXT("is_open"))
        {
            Dropdown->SetDropdownOpen(false);
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
    else if (UGV2ProgressBarWidgetBase* ProgressHost = Cast<UGV2ProgressBarWidgetBase>(TargetWidget))
    {
        ProgressHost->ApplyProgress(static_cast<float>(PreparedValue));
    }
    return true;
}

void FGV2NumberPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        PB->SetPercent(0.0f);
    }
    else if (UGV2ProgressBarWidgetBase* ProgressHost = Cast<UGV2ProgressBarWidgetBase>(TargetWidget))
    {
        ProgressHost->ApplyProgress(0.0f);
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
    else if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        Dropdown->SetSelectedKey(FName(*PreparedValue));
    }
    else if (UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget))
    {
        TabContainer->ApplyDefaultTabKey(FName(*PreparedValue));
    }
    else if (UGV2LocationTopBarWidgetBase* TopBar = Cast<UGV2LocationTopBarWidgetBase>(TargetWidget))
    {
        TopBar->SetKey(FName(*PreparedValue));
    }
    else if (UGV2LocationPlayerStatusWidgetBase* PlayerStatus = Cast<UGV2LocationPlayerStatusWidgetBase>(TargetWidget))
    {
        PlayerStatus->SetKey(FName(*PreparedValue));
    }
    else if (UGV2LocationSceneWidgetBase* SceneWidget = Cast<UGV2LocationSceneWidgetBase>(TargetWidget))
    {
        SceneWidget->SetKey(FName(*PreparedValue));
    }
    else if (UGV2LocationCommandPanelWidgetBase* CmdPanel = Cast<UGV2LocationCommandPanelWidgetBase>(TargetWidget))
    {
        CmdPanel->SetKey(FName(*PreparedValue));
    }
    else if (UGV2ImageWidgetBase* ImageWidget = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        ImageWidget->SetKey(FName(*PreparedValue));
    }
    else if (UGV2ModalWidgetBase* Modal = Cast<UGV2ModalWidgetBase>(TargetWidget))
    {
        Modal->SetKey(FName(*PreparedValue));
    }
    else
    {
        // A host that declares a `key` capability but has no branch here would otherwise
        // report a successful commit while storing nothing -- the exact shape this pipeline
        // exists to make impossible. Unhandled target type is a defect, not a no-op.
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.unhandled_target: key capability declared for '%s' has no commit branch"),
            *TargetWidget->GetClass()->GetName());
        return false;
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
    else if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        Dropdown->SetSelectedKey(NAME_None);
    }
    else if (UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget))
    {
        TabContainer->ApplyDefaultTabKey(NAME_None);
    }
    else if (UGV2LocationTopBarWidgetBase* TopBar = Cast<UGV2LocationTopBarWidgetBase>(TargetWidget))
    {
        TopBar->SetKey(NAME_None);
    }
    else if (UGV2LocationPlayerStatusWidgetBase* PlayerStatus = Cast<UGV2LocationPlayerStatusWidgetBase>(TargetWidget))
    {
        PlayerStatus->SetKey(NAME_None);
    }
    else if (UGV2LocationSceneWidgetBase* SceneWidget = Cast<UGV2LocationSceneWidgetBase>(TargetWidget))
    {
        SceneWidget->SetKey(NAME_None);
    }
    else if (UGV2LocationCommandPanelWidgetBase* CmdPanel = Cast<UGV2LocationCommandPanelWidgetBase>(TargetWidget))
    {
        CmdPanel->SetKey(NAME_None);
    }
    else if (UGV2ImageWidgetBase* ImageWidget = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        ImageWidget->SetKey(NAME_None);
    }
    else if (UGV2ModalWidgetBase* Modal = Cast<UGV2ModalWidgetBase>(TargetWidget))
    {
        Modal->SetKey(NAME_None);
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

// --- FGV2KeyedCollectionPropertyConsumer ---

bool FGV2KeyedCollectionPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

static GV2ContentCore::FCompiledUiFieldSpecPtr MakeCollectionSpecFromCapability(const FGV2UiPropertyCapability& Cap)
{
    using namespace GV2ContentCore;
    switch (Cap.SupportedKind)
    {
    case EGV2PreparedUiValueKind::Boolean:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::Boolean;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Integer:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Integer;
        if (Cap.IntMin.IsSet()) { Scalar.MinimumInteger = Cap.IntMin.GetValue(); }
        if (Cap.IntMax.IsSet()) { Scalar.MaximumInteger = Cap.IntMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::Number:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Number;
        if (Cap.NumberMin.IsSet()) { Scalar.MinimumNumber = Cap.NumberMin.GetValue(); }
        if (Cap.NumberMax.IsSet()) { Scalar.MaximumNumber = Cap.NumberMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::String:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::String;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Key:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key);
    case EGV2PreparedUiValueKind::Text:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
    case EGV2PreparedUiValueKind::Binding:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding);
    case EGV2PreparedUiValueKind::StableId:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Ref);
        Spec->RefTargetKind = Cap.TargetKind.IsEmpty() ? "resource" : TCHAR_TO_UTF8(*Cap.TargetKind);
        return Spec;
    }
    case EGV2PreparedUiValueKind::Object:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Object);
        if (Cap.ChildTree)
        {
            for (const auto& Entry : Cap.ChildTree->Properties)
            {
                Spec->Fields.push_back({ TCHAR_TO_UTF8(*Entry.Key), false, MakeCollectionSpecFromCapability(Entry.Value) });
            }
        }
        return Spec;
    }
    default:
        return nullptr;
    }
}

bool FGV2KeyedCollectionPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for keyed collection capability");
        return false;
    }

    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Array value for keyed collection");
        return false;
    }

    KeyPropertyName = Capability.KeyPropertyName.IsEmpty() ? TEXT("key") : Capability.KeyPropertyName;
    PreparedItems.Reset();
    CandidateWidgetsByKey.Reset();

    const FGV2PreparedUiArray& Array = Value.AsArray();
    UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget);
    UPanelWidget* Panel = ListView ? ListView->GetContainerPanel() : Cast<UPanelWidget>(TargetWidget);

    TMap<FName, TObjectPtr<UWidget>> ExistingWidgets;
    if (ListView)
    {
        ExistingWidgets = ListView->GetActiveWidgetsMap();
    }
    else
    {
        ExistingWidgets = ActiveWidgetsByKey;
    }

    // Determine fallback widget class if needed
    UClass* EntryClass = Capability.EntryWidgetClass != nullptr ? Capability.EntryWidgetClass.Get() : nullptr;
    if (!EntryClass)
    {
        if (ExistingWidgets.Num() > 0)
        {
            for (const auto& Pair : ExistingWidgets)
            {
                if (Pair.Value != nullptr)
                {
                    EntryClass = Pair.Value->GetClass();
                    break;
                }
            }
        }
        else if (Panel && Panel->GetChildrenCount() > 0)
        {
            EntryClass = Panel->GetChildAt(0)->GetClass();
        }
    }

    for (const FGV2PreparedUiValue& ItemVal : Array)
    {
        if (!ItemVal.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.invalid_item: Keyed collection item must be an Object");
            return false;
        }

        const FGV2PreparedUiObject& ItemObj = ItemVal.AsObject();
        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(KeyPropertyName);
        if (!KeyVal || !KeyVal->IsKey())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_item_key: Keyed collection item missing required Key field '%s'"), *KeyPropertyName);
            return false;
        }

        const FName ItemKey = FName(*KeyVal->AsKey());
        if (ItemKey.IsNone())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.invalid_item_key: Keyed collection item key is empty");
            return false;
        }

        if (CandidateWidgetsByKey.Contains(ItemKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_item_key: Duplicate key '%s' in keyed collection"), *ItemKey.ToString());
            return false;
        }

        UWidget* ItemWidget = ExistingWidgets.FindRef(ItemKey);
        if (!ItemWidget)
        {
            if (!EntryClass)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_entry_class: No entry class available to instantiate item for key '%s'"), *ItemKey.ToString());
                return false;
            }

            UWorld* World = TargetWidget->GetWorld();
            if (EntryClass->IsChildOf(UUserWidget::StaticClass()))
            {
                ItemWidget = CreateWidget<UUserWidget>(World ? World : TargetWidget->GetTypedOuter<UWorld>(), EntryClass);
            }
            else
            {
                ItemWidget = NewObject<UWidget>(TargetWidget, EntryClass);
            }

            if (!ItemWidget)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.item_widget_creation_failed: Failed to create widget for key '%s'"), *ItemKey.ToString());
                return false;
            }
        }

        CandidateWidgetsByKey.Add(ItemKey, ItemWidget);

        if (IGV2UiPropertyHost* ItemHost = Cast<IGV2UiPropertyHost>(ItemWidget))
        {
            FGV2UiCapabilityBuilder ItemCapBuilder;
            ItemHost->DescribeUiCapabilities(ItemCapBuilder);
            const FGV2UiCapabilityTree ItemCaps = ItemCapBuilder.Build();

            GV2ContentCore::FCompiledUiFieldSpec ItemSchema;
            ItemSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
            for (const auto& CapEntry : ItemCaps.Properties)
            {
                if (auto Spec = MakeCollectionSpecFromCapability(CapEntry.Value))
                {
                    ItemSchema.Fields.push_back({ TCHAR_TO_UTF8(*CapEntry.Key), false, MoveTemp(Spec) });
                }
            }

            TSharedPtr<FGV2UiHostMutationPlan> ItemPlan = MakeShared<FGV2UiHostMutationPlan>();
            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const FGV2PreparedUiObject EmptyPrev;
            if (!PrepareUiHostProperties(
                    Cast<UUserWidget>(ItemWidget),
                    ItemCaps,
                    ItemObj,
                    ItemSchema,
                    TEXT("core:schema.ui_value.collection_item.v1"),
                    KeyVal->AsKey(),
                    EmptyPrev,
                    *ItemPlan,
                    Diagnostics))
            {
                OutError = Diagnostics.Num() > 0 ? Diagnostics[0].Message : TEXT("core:diagnostic.ui_mutation.prepare_failed: Prepare failed on collection item");
                return false;
            }

            FPreparedCollectionItem PreparedItem;
            PreparedItem.Key = ItemKey;
            PreparedItem.Widget = ItemWidget;
            PreparedItem.Plan = ItemPlan;
            PreparedItem.bIsHost = true;
            PreparedItems.Add(MoveTemp(PreparedItem));
        }
        else
        {
            FPreparedCollectionItem PreparedItem;
            PreparedItem.Key = ItemKey;
            PreparedItem.Widget = ItemWidget;
            PreparedItem.bIsHost = false;
            PreparedItems.Add(MoveTemp(PreparedItem));
        }
    }

    return true;
}

bool FGV2KeyedCollectionPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    for (FPreparedCollectionItem& Item : PreparedItems)
    {
        if (Item.bIsHost && Item.Plan)
        {
            FString FailedPath, CommitError;
            if (!CommitUiHostProperties(Cast<UUserWidget>(Item.Widget), *Item.Plan, FailedPath, CommitError))
            {
                OutError = CommitError;
                return false;
            }
        }
    }

    UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget);
    UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(TargetWidget);
    UPanelWidget* Panel = ListView ? ListView->GetContainerPanel() : (ButtonList ? Cast<UPanelWidget>(ButtonList->GetButtonContainer()) : Cast<UPanelWidget>(TargetWidget));

    if (Panel != nullptr)
    {
        Panel->ClearChildren();
        for (const FPreparedCollectionItem& Item : PreparedItems)
        {
            Panel->AddChild(Item.Widget);
        }
    }

    // Newly added slots have no styling of their own; let the container reapply its
    // central style (e.g. per-item slot padding) now that the collection has settled.
    if (TargetWidget->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()))
    {
        IGV2UiStyleConsumer::Execute_ApplyCentralStyle(TargetWidget);
    }

    ActiveWidgetsByKey = MoveTemp(CandidateWidgetsByKey);
    if (ListView != nullptr)
    {
        ListView->SetActiveWidgetsMap(ActiveWidgetsByKey);
    }
    if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget->GetOuter()))
    {
        Dropdown->UpdateHeaderLabel();
    }
    else if (UGV2DropdownSelectWidgetBase* DropdownOuter = TargetWidget->GetTypedOuter<UGV2DropdownSelectWidgetBase>())
    {
        DropdownOuter->UpdateHeaderLabel();
    }

    return true;
}

void FGV2KeyedCollectionPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget))
    {
        ListView->ClearEntries();
    }
    else if (UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(TargetWidget))
    {
        if (ButtonList->GetButtonContainer())
        {
            ButtonList->GetButtonContainer()->ClearChildren();
        }
    }
    else if (UPanelWidget* Panel = Cast<UPanelWidget>(TargetWidget))
    {
        Panel->ClearChildren();
    }
    ActiveWidgetsByKey.Reset();
    CandidateWidgetsByKey.Reset();
    PreparedItems.Reset();
}

// --- FGV2RichTextSpansPropertyConsumer ---

bool FGV2RichTextSpansPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

bool FGV2RichTextSpansPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    PreparedSpans.Reset();
    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.kind_mismatch: Expected Array kind for RichText spans");
        return false;
    }

    UGV2RichTextWidgetBase* RichTextWidget = Cast<UGV2RichTextWidgetBase>(TargetWidget);
    if (!RichTextWidget && TargetWidget)
    {
        RichTextWidget = TargetWidget->GetTypedOuter<UGV2RichTextWidgetBase>();
    }

    bool bHasHover = false;
    TSet<FName> SeenKeys;

    for (const FGV2PreparedUiValue& Item : Value.AsArray())
    {
        if (!Item.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Expected Object for RichText span item");
            return false;
        }

        const FGV2PreparedUiObject& ItemObj = Item.AsObject();
        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(TEXT("key"));
        if (!KeyVal || !KeyVal->IsKey() || KeyVal->AsKey().IsEmpty())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.missing_span_key: RichText span requires a non-empty key");
            return false;
        }

        const FName ItemKey = FName(*KeyVal->AsKey());
        if (SeenKeys.Contains(ItemKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_span_key: Duplicate span key '%s'"), *ItemKey.ToString());
            return false;
        }
        SeenKeys.Add(ItemKey);

        FGV2RichTextSpanViewModel Span;
        Span.Key = ItemKey;
        Span.SpanId = ItemKey;

        if (const FGV2PreparedUiValue* SpanIdVal = ItemObj.FindField(TEXT("span_id")))
        {
            if (SpanIdVal->IsKey() && !SpanIdVal->AsKey().IsEmpty())
            {
                Span.SpanId = FName(*SpanIdVal->AsKey());
            }
            else if (SpanIdVal->IsString() && !SpanIdVal->AsString().IsEmpty())
            {
                Span.SpanId = FName(*SpanIdVal->AsString());
            }
        }

        if (!GV2StableIdUE::IsValidSegment(Span.SpanId.ToString()))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.invalid_span_id: Invalid span ID '%s'"), *Span.SpanId.ToString());
            return false;
        }

        if (const FGV2PreparedUiValue* BindingVal = ItemObj.FindField(TEXT("binding")))
        {
            if (BindingVal->IsBinding())
            {
                Span.Binding = BindingVal->AsBinding();
            }
        }

        if (const FGV2PreparedUiValue* HoverVal = ItemObj.FindField(TEXT("hover")))
        {
            if (HoverVal->IsObject())
            {
                const FGV2PreparedUiObject& HoverObj = HoverVal->AsObject();
                if (const FGV2PreparedUiValue* TitleVal = HoverObj.FindField(TEXT("title")))
                {
                    if (TitleVal->IsText())
                    {
                        Span.Hover.Title = TitleVal->AsText();
                    }
                }
                if (const FGV2PreparedUiValue* DescVal = HoverObj.FindField(TEXT("description")))
                {
                    if (DescVal->IsText())
                    {
                        Span.Hover.Description = DescVal->AsText();
                    }
                }
                if (const FGV2PreparedUiValue* ImageVal = HoverObj.FindField(TEXT("image_resource_id")))
                {
                    if (ImageVal->IsStableId())
                    {
                        Span.Hover.ImageResourceId = ImageVal->AsStableId().Id;
                    }
                    else if (ImageVal->IsString())
                    {
                        Span.Hover.ImageResourceId = ImageVal->AsString();
                    }
                }
                if (!Span.Hover.IsEmpty())
                {
                    bHasHover = true;
                }
            }
        }

        if (Span.Hover.IsEmpty() && !Span.Binding.IsValid())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.empty_span: Span '%s' must have hover or binding"), *ItemKey.ToString());
            return false;
        }

        PreparedSpans.Add(MoveTemp(Span));
    }

    if (bHasHover)
    {
        const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
        if (Theme == nullptr || Theme->RichTextPopoverClass.IsNull() || Theme->RichTextPopoverClass.LoadSynchronous() == nullptr)
        {
            OutError = TEXT("core:diagnostic.ui_consumer.missing_popover_renderer: RichText popover renderer unavailable in theme");
            return false;
        }
    }

    return true;
}

bool FGV2RichTextSpansPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    UGV2RichTextWidgetBase* RichTextWidget = Cast<UGV2RichTextWidgetBase>(TargetWidget);
    if (!RichTextWidget && TargetWidget)
    {
        RichTextWidget = TargetWidget->GetTypedOuter<UGV2RichTextWidgetBase>();
    }

    if (RichTextWidget != nullptr)
    {
        RichTextWidget->ApplySpans(PreparedSpans);
    }
    return true;
}

void FGV2RichTextSpansPropertyConsumer::Reset(UWidget* TargetWidget)
{
    UGV2RichTextWidgetBase* RichTextWidget = Cast<UGV2RichTextWidgetBase>(TargetWidget);
    if (!RichTextWidget && TargetWidget)
    {
        RichTextWidget = TargetWidget->GetTypedOuter<UGV2RichTextWidgetBase>();
    }

    if (RichTextWidget != nullptr)
    {
        RichTextWidget->ApplySpans({});
    }
    PreparedSpans.Reset();
}

// --- FGV2TabContainerTabsPropertyConsumer ---

bool FGV2TabContainerTabsPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

bool FGV2TabContainerTabsPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    PreparedTabs.Reset();
    CandidateWidgetsByKey.Reset();

    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Array for tab container tabs");
        return false;
    }

    const TArray<FGV2PreparedUiValue>& TabsArray = Value.AsArray().GetElements();
    if (TabsArray.Num() == 0)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.empty_tabs: Tab container requires a non-empty tabs array");
        return false;
    }

    UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget);
    if (!TabContainer && TargetWidget)
    {
        TabContainer = TargetWidget->GetTypedOuter<UGV2TabContainerWidgetBase>();
    }

    const UGV2ScreenRegistry* ScreenRegistry = UGV2ScreenRegistrySettings::GetConfiguredRegistry();

    TSet<FName> SeenKeys;

    for (const FGV2PreparedUiValue& TabVal : TabsArray)
    {
        if (!TabVal.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Each tab item must be an object");
            return false;
        }

        const FGV2PreparedUiObject& TabObj = TabVal.AsObject();

        // 1. Key
        const FGV2PreparedUiValue* KeyVal = TabObj.FindField(TEXT("key"));
        if (!KeyVal || !KeyVal->IsKey() || KeyVal->AsKey().IsEmpty())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.missing_tab_key: Tab requires a non-empty key");
            return false;
        }

        const FName TabKey = FName(*KeyVal->AsKey());
        if (SeenKeys.Contains(TabKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_tab_key: Duplicate tab key '%s'"), *TabKey.ToString());
            return false;
        }
        SeenKeys.Add(TabKey);

        // 2. Title
        const FGV2PreparedUiValue* TitleVal = TabObj.FindField(TEXT("title"));
        if (!TitleVal || !TitleVal->IsText())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_tab_title: Tab '%s' requires a text title"), *TabKey.ToString());
            return false;
        }

        // 3. ScreenId
        const FGV2PreparedUiValue* ScreenIdVal = TabObj.FindField(TEXT("screen_id"));
        if (!ScreenIdVal || (!ScreenIdVal->IsStableId() && !ScreenIdVal->IsString()))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_tab_screen_id: Tab '%s' requires a screen_id"), *TabKey.ToString());
            return false;
        }

        const FString TabScreenId = ScreenIdVal->IsStableId() ? ScreenIdVal->AsStableId().Id : ScreenIdVal->AsString();
        if (!GV2StableIdUE::IsOfKind(TabScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.invalid_tab_screen_id: Tab '%s' has invalid screen_id '%s'"), *TabKey.ToString(), *TabScreenId);
            return false;
        }

        // 4. Resolve ScreenId in UGV2ScreenRegistry
        TSubclassOf<UGV2ScreenWidgetBase> TargetWidgetClass = UGV2ScreenWidgetBase::StaticClass();
        if (ScreenRegistry != nullptr)
        {
            const FGV2ScreenRegistryEntry* Entry = ScreenRegistry->FindEntry(TabScreenId);
            if (Entry == nullptr)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.unregistered_screen_id: Screen '%s' for tab '%s' not found in Screen Registry"), *TabScreenId, *TabKey.ToString());
                return false;
            }

            if (Entry->WidgetClass.IsNull())
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.null_screen_widget_class: Null widget class for screen '%s'"), *TabScreenId);
                return false;
            }

            TargetWidgetClass = Entry->WidgetClass.LoadSynchronous();
            if (TargetWidgetClass == nullptr)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.screen_widget_load_failed: Failed to load widget class for screen '%s'"), *TabScreenId);
                return false;
            }
        }

        // 5. Reconcile / instantiate screen widget off-tree
        TObjectPtr<UGV2ScreenWidgetBase> ChildWidget = nullptr;
        if (TabContainer != nullptr)
        {
            ChildWidget = TabContainer->GetScreenWidgetForTab(TabKey);
        }

        if (!ChildWidget && TargetWidget != nullptr)
        {
            UWorld* World = TargetWidget->GetWorld();
            if (World != nullptr)
            {
                ChildWidget = CreateWidget<UGV2ScreenWidgetBase>(World, TargetWidgetClass);
            }
        }

        FPreparedTabItem PreparedItem;
        PreparedItem.Key = TabKey;
        PreparedItem.Title = TitleVal->AsText();
        PreparedItem.ScreenId = TabScreenId;
        PreparedItem.ScreenWidgetClass = TargetWidgetClass;
        PreparedItem.ScreenWidget = ChildWidget;

        // 6. Child fields preparation off-tree
        const FGV2PreparedUiValue* FieldsVal = TabObj.FindField(TEXT("fields"));
        if (FieldsVal != nullptr && !FieldsVal->IsNull())
        {
            if (!FieldsVal->IsObject())
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.kind_mismatch: Tab '%s' fields must be an object"), *TabKey.ToString());
                return false;
            }

            if (ChildWidget != nullptr && ChildWidget->GetClass()->ImplementsInterface(UGV2UiPropertyHost::StaticClass()))
            {
                FGV2UiCapabilityTree ChildCaps;
                if (IGV2UiPropertyHost* Host = Cast<IGV2UiPropertyHost>(ChildWidget))
                {
                    FGV2UiCapabilityBuilder ChildCapBuilder;
                    Host->DescribeUiCapabilities(ChildCapBuilder);
                    ChildCaps = ChildCapBuilder.Build();
                }

                GV2ContentCore::FCompiledUiFieldSpec ChildSchema;
                ChildSchema.Kind = GV2ContentCore::EUiFieldKind::Object;

                for (const auto& CapPair : ChildCaps.Properties)
                {
                    if (auto Spec = MakeCollectionSpecFromCapability(CapPair.Value))
                    {
                        ChildSchema.Fields.push_back({ TCHAR_TO_UTF8(*CapPair.Key), false, MoveTemp(Spec) });
                    }
                }

                PreparedItem.ChildMutationPlan = MakeShared<FGV2UiHostMutationPlan>();
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
                const FGV2PreparedUiObject EmptyPrev;

                const bool bPrepared = PrepareUiHostProperties(
                    ChildWidget,
                    ChildCaps,
                    FieldsVal->AsObject(),
                    ChildSchema,
                    TEXT("core:schema.screen_fields.v1"),
                    TabKey.ToString(),
                    EmptyPrev,
                    *PreparedItem.ChildMutationPlan,
                    Diagnostics);

                if (!bPrepared)
                {
                    OutError = Diagnostics.Num() > 0 ? Diagnostics[0].Message : FString::Printf(TEXT("core:diagnostic.ui_mutation.prepare_failed: Child fields prepare failed for tab '%s'"), *TabKey.ToString());
                    return false;
                }
                PreparedItem.bHasChildPlan = true;
            }
        }

        if (ChildWidget != nullptr)
        {
            CandidateWidgetsByKey.Add(TabKey, ChildWidget);
        }

        PreparedTabs.Add(MoveTemp(PreparedItem));
    }

    return true;
}

bool FGV2TabContainerTabsPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget);
    if (!TabContainer && TargetWidget)
    {
        TabContainer = TargetWidget->GetTypedOuter<UGV2TabContainerWidgetBase>();
    }

    // Commit child mutation plans
    for (FPreparedTabItem& Item : PreparedTabs)
    {
        if (Item.bHasChildPlan && Item.ChildMutationPlan.IsValid() && Item.ScreenWidget != nullptr)
        {
            FString FailedPath;
            FString CommitError;
            if (!CommitUiHostProperties(Item.ScreenWidget, *Item.ChildMutationPlan, FailedPath, CommitError))
            {
                OutError = CommitError;
                return false;
            }
        }
    }

    if (TabContainer != nullptr)
    {
        TArray<FGV2TabItemEntry> Entries;
        TMap<FName, UGV2ScreenWidgetBase*> Widgets;

        for (const FPreparedTabItem& Item : PreparedTabs)
        {
            FGV2TabItemEntry Entry;
            Entry.Key = Item.Key;
            Entry.Title = Item.Title;
            Entry.ScreenId = Item.ScreenId;
            Entries.Add(MoveTemp(Entry));

            if (Item.ScreenWidget != nullptr)
            {
                Widgets.Add(Item.Key, Item.ScreenWidget.Get());
            }
        }

        TabContainer->ApplyTabEntries(Entries, Widgets);
    }

    return true;
}

void FGV2TabContainerTabsPropertyConsumer::Reset(UWidget* TargetWidget)
{
    UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget);
    if (!TabContainer && TargetWidget)
    {
        TabContainer = TargetWidget->GetTypedOuter<UGV2TabContainerWidgetBase>();
    }

    if (TabContainer != nullptr)
    {
        TabContainer->ResetTabContainerModel();
    }
    PreparedTabs.Reset();
    CandidateWidgetsByKey.Reset();
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
    case EGV2PreparedUiValueKind::Array:
        if (TargetType == EGV2UiCapabilityTargetType::CollectionHost)
        {
            return MakeShared<FGV2KeyedCollectionPropertyConsumer>();
        }
        else if (TargetType == EGV2UiCapabilityTargetType::CustomControl)
        {
            return MakeShared<FGV2RichTextSpansPropertyConsumer>();
        }
        else if (TargetType == EGV2UiCapabilityTargetType::NestedScreen)
        {
            return MakeShared<FGV2TabContainerTabsPropertyConsumer>();
        }
        break;
    default:
        break;
    }
    return nullptr;
}
