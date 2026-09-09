#include "UI/GV2DropdownSelectWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiTheme.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2DropdownSelectWidget, Log, All);

void UGV2DropdownSelectWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedDropdownStyle. See UGV2SeparatorWidgetBase.
    if (PopupBorder != nullptr)
    {
        PopupBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UGV2DropdownSelectWidgetBase::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (HeaderButton != nullptr)
    {
        HeaderButton->SetAutomaticInteractionSubmission(false);
        HeaderButton->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleHeaderActivated);
    }
}

void UGV2DropdownSelectWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("placeholder"), FName(TEXT("HeaderButton")));
    OutBuilder.AddKey(TEXT("selected_key"), NAME_None);
    // DCA-03: OptionWidgetClass is read as-is, not resolved through a fallback chain. An
    // unset class still declares the capability; instantiating a NEW entry against it
    // fails downstream (missing_entry_class) instead of silently substituting a default.
    FGV2UiPropertyCapability OptionCap;
    OptionCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    OptionCap.EntryWidgetClass = OptionWidgetClass;
    OutBuilder.AddKeyedCollection(
        TEXT("items"),
        FName(TEXT("OptionsScrollBox")),
        OptionCap,
        TEXT("key"),
        OptionWidgetClass);
    OutBuilder.AddBinding(TEXT("binding"), NAME_None);
    OutBuilder.AddBoolean(TEXT("is_open"), NAME_None);
}

void UGV2DropdownSelectWidgetBase::SetDropdownOpen(const bool bOpen)
{
    bIsOpen = bOpen;
    if (PopupBorder != nullptr)
    {
        PopupBorder->SetVisibility(
            bIsOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UGV2DropdownSelectWidgetBase::SetSelectedKey(const FName InKey)
{
    SelectedKey = InKey;
    UpdateHeaderLabel();
}

bool UGV2DropdownSelectWidgetBase::ApplyPlaceholderText(const FGV2TextViewModel& InText)
{
    CurrentPlaceholder = InText;
    UpdateHeaderLabel();
    return true;
}

void UGV2DropdownSelectWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle)
{
    CurrentBinding = InBindingHandle;
    if (HeaderButton != nullptr)
    {
        HeaderButton->SetBindingHandle(InBindingHandle);
    }
}

void UGV2DropdownSelectWidgetBase::SetInteractionEnabled(const bool bEnabled)
{
    SetIsEnabled(bEnabled);
    if (HeaderButton != nullptr)
    {
        HeaderButton->SetIsInteractionEnabled(bEnabled);
    }
}

void UGV2DropdownSelectWidgetBase::UpdateHeaderLabel()
{
    if (HeaderButton == nullptr) return;

    const FGV2TextViewModel* DisplayText = nullptr;
    if (!SelectedKey.IsNone() && OptionsScrollBox != nullptr)
    {
        for (UWidget* Child : OptionsScrollBox->GetAllChildren())
        {
            if (UGV2ButtonWidgetBase* OptionBtn = Cast<UGV2ButtonWidgetBase>(Child))
            {
                OptionBtn->SetAutomaticInteractionSubmission(false);
                OptionBtn->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleOptionActivated);
                if (OptionBtn->GetKey() == SelectedKey)
                {
                    DisplayText = &OptionBtn->GetTextViewModel();
                }
            }
        }
    }
    else if (OptionsScrollBox != nullptr)
    {
        for (UWidget* Child : OptionsScrollBox->GetAllChildren())
        {
            if (UGV2ButtonWidgetBase* OptionBtn = Cast<UGV2ButtonWidgetBase>(Child))
            {
                OptionBtn->SetAutomaticInteractionSubmission(false);
                OptionBtn->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleOptionActivated);
            }
        }
    }

    if (DisplayText == nullptr)
    {
        DisplayText = &CurrentPlaceholder;
    }

    HeaderButton->SetKey(TEXT("dropdown_header"));
    HeaderButton->SetBindingHandle(CurrentBinding);
    HeaderButton->ApplyText(*DisplayText);
    HeaderButton->SetAutomaticInteractionSubmission(false);
}

void UGV2DropdownSelectWidgetBase::HandleHeaderClicked()
{
    SetDropdownOpen(!bIsOpen);
}

void UGV2DropdownSelectWidgetBase::HandleHeaderActivated(const FName Key)
{
    HandleHeaderClicked();
}

void UGV2DropdownSelectWidgetBase::HandleOptionActivated(const FName Key)
{
    SubmitSelection(Key);
}

EGV2SubmitUiInteractionResult UGV2DropdownSelectWidgetBase::SubmitSelection(
    const FName InSelectedKey)
{
    if (!CurrentBinding.IsValid() || InSelectedKey.IsNone())
    {
        return EGV2SubmitUiInteractionResult::InvalidInputValues;
    }
    FGV2UiControlValue SelectedKeyValue;
    SelectedKeyValue.Name = TEXT("selected_key");
    SelectedKeyValue.Type = EGV2UiControlValueType::String;
    SelectedKeyValue.StringValue = InSelectedKey.ToString();

    const EGV2SubmitUiInteractionResult SubmitResult =
        FGV2UiInteractionEmitter::Submit(
            this, CurrentBinding, {SelectedKeyValue});

    OnSelectionInvoked.Broadcast(CurrentBinding, SubmitResult);
    if (SubmitResult == EGV2SubmitUiInteractionResult::Accepted)
    {
        SetDropdownOpen(false);
    }
    return SubmitResult;
}

void UGV2DropdownSelectWidgetBase::ApplyDropdownStyleValues(
    TSubclassOf<UCommonButtonStyle> InHeaderStyle,
    const FSlateBrush& InPopupBackground,
    const FMargin& InPopupPadding,
    const FMargin& InOptionItemPadding,
    float InMaxPopupHeight,
    const GV2PresentationApply::FPreparedViewportScalePolicy& InPopupScale)
{
    if (HeaderButton != nullptr && InHeaderStyle != nullptr)
    {
        HeaderButton->SetStyle(InHeaderStyle);
    }
    if (PopupBorder != nullptr)
    {
        PopupBorder->SetBrush(InPopupBackground);
        PopupBorder->SetPadding(InPopupPadding);
    }
    if (PopupSizeBox != nullptr)
    {
        // DCA-15 (ADR-0035): the popup's own box follows the same viewport-derived
        // scale as the option text inside it -- a fixed max height (the Theme
        // default, unscaled) shows fewer visible options as the screen grows, the
        // opposite of what a responsive list should do. PSC-10B: the curve and the
        // reference height are now prepared values; only the live viewport is read here.
        const float ViewportScale = GV2PresentationApply::EvaluatePreparedViewportScale(
            InPopupScale,
            GV2PresentationApply::ResolveLiveViewportHeight(this, InPopupScale.ReferenceViewportHeight));
        PopupSizeBox->SetMaxDesiredHeight(InMaxPopupHeight * ViewportScale);
    }
    if (OptionsScrollBox != nullptr)
    {
        for (UPanelSlot* PanelSlot : OptionsScrollBox->GetSlots())
        {
            if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(PanelSlot))
            {
                Slot->SetPadding(InOptionItemPadding);
            }
        }
    }
}

bool UGV2DropdownSelectWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: carried by FPreparedDropdownStyle, written by ApplyDropdownStyleValues.
    return true;
}

