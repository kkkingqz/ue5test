#include "UI/GV2DropdownSelectWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2KeyedCollection.h"
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
        HeaderButton->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleOptionActivated);
    }
}

bool UGV2DropdownSelectWidgetBase::ApplyDropdownModel(
    const FGV2DropdownSelectViewModel& InModel)
{
    if (!CanApplyDropdownModel(InModel))
    {
        return false;
    }

    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedOptionClass = ResolveOptionWidgetClass();

    TArray<FGV2ButtonViewModel> ButtonModels;
    ButtonModels.Reserve(InModel.Options.Num());
    for (const FGV2DropdownOptionViewModel& Option : InModel.Options)
    {
        FGV2ButtonViewModel& BM = ButtonModels.AddDefaulted_GetRef();
        BM.Key = Option.Key;
        BM.Text = Option.Text;
        BM.Binding = InModel.Binding;
    }

    TArray<UGV2ButtonWidgetBase*> OrderedWidgets;
    if (!FGV2KeyedCollection::Reconcile<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(
        OptionsScrollBox,
        ButtonModels,
        OptionsByKey,
        [](const FGV2ButtonViewModel& Model) { return Model.Key; },
        [this, ResolvedOptionClass]() -> UGV2ButtonWidgetBase*
        {
            if (GetOwningPlayer() != nullptr)
            {
                return CreateWidget<UGV2ButtonWidgetBase>(GetOwningPlayer(), ResolvedOptionClass);
            }
            return GetWorld() != nullptr
                ? CreateWidget<UGV2ButtonWidgetBase>(GetWorld(), ResolvedOptionClass)
                : nullptr;
        },
        [this](UGV2ButtonWidgetBase& Button, const FGV2ButtonViewModel& Model) -> bool
        {
            Button.ApplyButtonModel(Model);
            Button.SetAutomaticInteractionSubmission(false);
            Button.OnActivated.AddUniqueDynamic(this, &ThisClass::HandleOptionActivated);
            return true;
        },
        OrderedWidgets))
    {
        return false;
    }

    // Apply option item padding from theme.
    if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        for (int32 Index = 0; Index < OrderedWidgets.Num(); ++Index)
        {
            if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(OptionsScrollBox->GetSlots()[Index]))
            {
                Slot->SetPadding(Theme->DropdownOptionItemPadding);
            }
        }
    }

    AppliedModel = InModel;
    UpdateHeaderLabel();

    if (bIsOpen)
    {
        SetDropdownOpen(false);
    }

    return true;
}

bool UGV2DropdownSelectWidgetBase::CanApplyDropdownModel(
    const FGV2DropdownSelectViewModel& InModel) const
{
    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedOptionClass = ResolveOptionWidgetClass();
    if (OptionsScrollBox == nullptr || HeaderButton == nullptr
        || PopupBorder == nullptr || PopupSizeBox == nullptr
        || ResolvedOptionClass == nullptr)
    {
        UE_LOG(LogGV2DropdownSelectWidget, Error,
            TEXT("CanApplyDropdownModel rejected: missing bound widgets or OptionWidgetClass"));
        return false;
    }

    if (!InModel.Binding.IsValid()
        || InModel.Placeholder.NormalizedMarkup.Contains(TEXT("<gv2"))
        || UGV2TextPipeline::ResolveStyleClass(InModel.Placeholder.StyleToken) == nullptr)
    {
        return false;
    }

    TSet<FName> Keys;
    int32 SelectedCount = 0;
    for (const FGV2DropdownOptionViewModel& Option : InModel.Options)
    {
        if (Option.Key.IsNone() || Keys.Contains(Option.Key))
        {
            UE_LOG(LogGV2DropdownSelectWidget, Error,
                TEXT("CanApplyDropdownModel rejected: invalid or duplicate key"));
            return false;
        }
        Keys.Add(Option.Key);
        SelectedCount += Option.bSelected ? 1 : 0;
        if (Option.Text.NormalizedMarkup.Contains(TEXT("<gv2"))
            || UGV2TextPipeline::ResolveStyleClass(Option.Text.StyleToken) == nullptr)
        {
            return false;
        }
    }

    return SelectedCount <= 1;
}

bool UGV2DropdownSelectWidgetBase::IsDropdownOpen() const
{
    return bIsOpen;
}

TSubclassOf<UGV2ButtonWidgetBase> UGV2DropdownSelectWidgetBase::ResolveOptionWidgetClass() const
{
    if (OptionWidgetClass != nullptr)
    {
        return OptionWidgetClass;
    }
    const UGV2DropdownSelectWidgetBase* ClassDefault =
        GetClass()->GetDefaultObject<UGV2DropdownSelectWidgetBase>();
    return ClassDefault != nullptr && ClassDefault != this
        ? ClassDefault->OptionWidgetClass
        : nullptr;
}

void UGV2DropdownSelectWidgetBase::HandleHeaderClicked()
{
    SetDropdownOpen(!bIsOpen);
}

void UGV2DropdownSelectWidgetBase::HandleOptionActivated(const FName Key)
{
    if (Key == TEXT("dropdown_header"))
    {
        HandleHeaderClicked();
        return;
    }
    SubmitSelection(Key);
}

EGV2SubmitUiInteractionResult UGV2DropdownSelectWidgetBase::SubmitSelection(
    const FName SelectedKey)
{
    if (!AppliedModel.Binding.IsValid() || SelectedKey.IsNone()
        || !OptionsByKey.Contains(SelectedKey))
    {
        return EGV2SubmitUiInteractionResult::InvalidInputValues;
    }
    FGV2UiControlValue SelectedKeyValue;
    SelectedKeyValue.Name = TEXT("selected_key");
    SelectedKeyValue.Type = EGV2UiControlValueType::String;
    SelectedKeyValue.StringValue = SelectedKey.ToString();

    const EGV2SubmitUiInteractionResult SubmitResult =
        FGV2UiInteractionEmitter::Submit(
            this, AppliedModel.Binding, {SelectedKeyValue});

    OnSelectionInvoked.Broadcast(AppliedModel.Binding, SubmitResult);
    if (SubmitResult == EGV2SubmitUiInteractionResult::Accepted)
    {
        SetDropdownOpen(false);
    }
    return SubmitResult;
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

void UGV2DropdownSelectWidgetBase::UpdateHeaderLabel()
{
    if (HeaderButton == nullptr) return;

    // Find the currently selected option.
    const FGV2DropdownOptionViewModel* SelectedOption = nullptr;
    for (const FGV2DropdownOptionViewModel& Option : AppliedModel.Options)
    {
        if (Option.bSelected)
        {
            SelectedOption = &Option;
            break;
        }
    }

    // Show selected option label, or placeholder if nothing selected.
    const FGV2TextViewModel& DisplayText = SelectedOption != nullptr
        ? SelectedOption->Text
        : AppliedModel.Placeholder;

    FGV2ButtonViewModel HeaderModel;
    HeaderModel.Key = TEXT("dropdown_header");
    HeaderModel.Text = DisplayText;
    HeaderModel.Binding = AppliedModel.Binding;
    HeaderButton->ApplyButtonModel(HeaderModel);
    HeaderButton->SetAutomaticInteractionSubmission(false);
}

// --- IGV2DynamicScreenElement ---

FGV2ScreenFieldDescriptor UGV2DropdownSelectWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Descriptor;
    Descriptor.FieldId = ScreenFieldId;
    Descriptor.SchemaId = TEXT("core:schema.ui_field.dropdown_select.v1");
    Descriptor.bRequired = bScreenFieldRequired;
    return Descriptor;
}

bool UGV2DropdownSelectWidgetBase::CanApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue) const
{
    return FieldValue.FieldId == ScreenFieldId
        && FieldValue.SchemaId == TEXT("core:schema.ui_field.dropdown_select.v1")
        && CanApplyDropdownModel(FieldValue.DropdownSelectValue);
}

bool UGV2DropdownSelectWidgetBase::ApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue)
{
    return CanApplyScreenField_Implementation(FieldValue)
        && ApplyDropdownModel(FieldValue.DropdownSelectValue);
}

bool UGV2DropdownSelectWidgetBase::CaptureScreenField_Implementation(
    FGV2ScreenFieldValue& OutFieldValue) const
{
    if (OptionsScrollBox == nullptr || ScreenFieldId.IsNone())
    {
        return false;
    }
    OutFieldValue = FGV2ScreenFieldValue::MakeDropdownSelect(
        ScreenFieldId, AppliedModel);
    return true;
}

bool UGV2DropdownSelectWidgetBase::ResetScreenField_Implementation()
{
    AppliedModel = FGV2DropdownSelectViewModel();
    OptionsByKey.Reset();
    if (OptionsScrollBox != nullptr)
    {
        OptionsScrollBox->ClearChildren();
    }
    if (HeaderButton != nullptr)
    {
        FGV2ButtonViewModel EmptyHeader;
        EmptyHeader.Key = TEXT("dropdown_header");
        HeaderButton->ApplyButtonModel(EmptyHeader);
        HeaderButton->SetAutomaticInteractionSubmission(false);
    }
    SetDropdownOpen(false);
    SetIsEnabled(false);
    return OptionsScrollBox != nullptr && HeaderButton != nullptr
        && PopupBorder != nullptr && PopupSizeBox != nullptr;
}

// --- IGV2UiStyleConsumer ---

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
