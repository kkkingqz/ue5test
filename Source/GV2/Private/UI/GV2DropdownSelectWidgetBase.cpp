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
    ApplyCentralStyle_Implementation();
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
    FGV2UiPropertyCapability OptionCap;
    OptionCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    OptionCap.EntryWidgetClass = ResolveOptionWidgetClass();
    OutBuilder.AddKeyedCollection(
        TEXT("items"),
        FName(TEXT("OptionsScrollBox")),
        OptionCap,
        TEXT("key"),
        ResolveOptionWidgetClass());
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

TSubclassOf<UGV2ButtonWidgetBase> UGV2DropdownSelectWidgetBase::ResolveOptionWidgetClass() const
{
    if (OptionWidgetClass != nullptr)
    {
        return OptionWidgetClass;
    }
    const UGV2DropdownSelectWidgetBase* ClassDefault =
        GetClass()->GetDefaultObject<UGV2DropdownSelectWidgetBase>();
    if (ClassDefault != nullptr && ClassDefault != this && ClassDefault->OptionWidgetClass != nullptr)
    {
        return ClassDefault->OptionWidgetClass;
    }
    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return Found;
    }
    if (UClass* Loaded = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return Loaded;
    }
    return UGV2ButtonWidgetBase::StaticClass();
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

bool UGV2DropdownSelectWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || HeaderButton == nullptr || PopupBorder == nullptr
        || PopupSizeBox == nullptr || OptionsScrollBox == nullptr
        || Theme->DropdownHeaderStyle == nullptr
        || Theme->DropdownPopupBackground.DrawAs == ESlateBrushDrawType::NoDrawType
        || Theme->DropdownMaxPopupHeight < 32.0f)
    {
        return false;
    }

    HeaderButton->SetStyle(Theme->DropdownHeaderStyle);
    PopupBorder->SetBrush(Theme->DropdownPopupBackground);
    PopupBorder->SetPadding(Theme->DropdownPopupPadding);
    PopupSizeBox->SetMaxDesiredHeight(Theme->DropdownMaxPopupHeight);

    for (UPanelSlot* PanelSlot : OptionsScrollBox->GetSlots())
    {
        if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(PanelSlot))
        {
            Slot->SetPadding(Theme->DropdownOptionItemPadding);
        }
    }

    return true;
}

