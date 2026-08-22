#include "GV2ContentEditor/Widgets/SGV2DefinitionProperties.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GV2ContentEditor
{

void SGV2DefinitionProperties::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnFieldValueChanged = InArgs._OnFieldValueChanged;
    OnSaveCompleted = InArgs._OnSaveCompleted;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildToolbar()
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(ContentScrollBox, SScrollBox)
        ]
    ];

    RefreshProperties();
}

TSharedRef<SWidget> SGV2DefinitionProperties::BuildToolbar()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Save (Atomic)")))
                .IsEnabled_Lambda([this]() {
                    return Adapter.IsValid() && Adapter->IsDirty();
                })
                .OnClicked(this, &SGV2DefinitionProperties::HandleSaveClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Reload / Discard")))
                .IsEnabled_Lambda([this]() {
                    return Adapter.IsValid() && Adapter->GetCurrentDefinition() != nullptr;
                })
                .OnClicked(this, &SGV2DefinitionProperties::HandleRevertClicked)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SSpacer)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() {
                    if (Adapter.IsValid() && Adapter->IsDirty())
                    {
                        return FText::FromString(FString::Printf(TEXT("%d unsaved change(s)"), static_cast<int32>(Adapter->GetDirtyFields().size())));
                    }
                    return FText::FromString(TEXT("Clean"));
                })
                .ColorAndOpacity_Lambda([this]() {
                    if (Adapter.IsValid() && Adapter->IsDirty())
                    {
                        return FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f));
                    }
                    return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
                })
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
            ]
        ];
}

FReply SGV2DefinitionProperties::HandleSaveClicked()
{
    if (Adapter.IsValid() && Adapter->GetCurrentDefinition() != nullptr)
    {
        auto Result = Adapter->SaveCurrentDefinition();
        RefreshProperties();
        if (OnSaveCompleted.IsBound())
        {
            OnSaveCompleted.Execute(Result);
        }
    }
    return FReply::Handled();
}

FReply SGV2DefinitionProperties::HandleRevertClicked()
{
    if (Adapter.IsValid() && Adapter->GetCurrentDefinition() != nullptr)
    {
        const auto* Current = Adapter->GetCurrentDefinition();
        const std::string DefinitionId = Current ? Current->Id : std::string();
        std::vector<FGV2EditorDiagnostic> Diagnostics;
        if (!DefinitionId.empty()) Adapter->LoadDefinition(DefinitionId, Diagnostics);
        RefreshProperties();
        if (OnFieldValueChanged.IsBound())
        {
            OnFieldValueChanged.Execute();
        }
        if (!Diagnostics.empty() && OnSaveCompleted.IsBound())
        {
            FGV2EditorAuthoringResult Result;
            Result.Outcome = EEditorAuthoringOutcome::ValidationFailed;
            Result.Diagnostics = std::move(Diagnostics);
            OnSaveCompleted.Execute(Result);
        }
    }
    return FReply::Handled();
}

void SGV2DefinitionProperties::FocusField(const FString& JsonPointer)
{
    HighlightedPointer = JsonPointer;
    RefreshProperties();
}

void SGV2DefinitionProperties::RefreshProperties()
{
    if (!ContentScrollBox.IsValid()) return;
    ContentScrollBox->ClearChildren();
    FieldWidgets.Empty();
    OwnedOptionLists.Empty();

    if (!Adapter.IsValid()) return;
    const auto* CurrentDef = Adapter->GetCurrentDefinition();
    if (CurrentDef == nullptr)
    {
        ContentScrollBox->AddSlot()
        .Padding(16.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No definition selected.")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
        ];
        return;
    }

    // Header displaying ID and Type
    ContentScrollBox->AddSlot()
    .Padding(8.0f, 4.0f)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("%s (%s)"), UTF8_TO_TCHAR(CurrentDef->Id.c_str()), UTF8_TO_TCHAR(CurrentDef->Type.c_str()))))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Package: %s | File: %s"), UTF8_TO_TCHAR(CurrentDef->PackageId.c_str()), UTF8_TO_TCHAR(CurrentDef->RelativeSource.c_str()))))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ]
    ];

    auto FormModelOpt = Adapter->GetFormModelForDefinitionType(CurrentDef->Type, CurrentDef->PackageId);
    if (!FormModelOpt.has_value())
    {
        ContentScrollBox->AddSlot()
        .Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No schema found for this definition type.")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.4f, 0.4f)))
        ];
        return;
    }

    for (const auto& Category : FormModelOpt->Categories)
    {
        ContentScrollBox->AddSlot()
        .Padding(4.0f, 4.0f)
        [
            BuildCategorySection(Category)
        ];
    }

    if (!HighlightedPointer.IsEmpty())
    {
        if (const TSharedPtr<SWidget>* Widget = FieldWidgets.Find(HighlightedPointer); Widget && Widget->IsValid())
        {
            ContentScrollBox->ScrollDescendantIntoView(*Widget);
        }
    }
}

TSharedRef<SWidget> SGV2DefinitionProperties::BuildCategorySection(const FGV2FormCategorySection& Category)
{
    TSharedRef<SVerticalBox> FieldsBox = SNew(SVerticalBox);

    int32 VisibleFieldsCount = 0;
    for (const auto& Field : Category.Fields)
    {
        EPropertyPresence Presence = Adapter.IsValid()
            ? Adapter->GetPropertyPresence(Field.JsonPointer, Field.Spec.get(), Field.bRequired)
            : EPropertyPresence::Explicit;

        if (Presence == EPropertyPresence::Absent)
        {
            continue;
        }

        VisibleFieldsCount++;
        FieldsBox->AddSlot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            BuildFieldRow(Field, Presence)
        ];
    }

    if (Adapter.IsValid())
    {
        auto AbsentFields = Adapter->GetAbsentOptionalFieldsForCategory(Category.CategoryName);
        if (!AbsentFields.empty())
        {
            auto AddPropOptions = MakeShared<TArray<TSharedPtr<FString>>>();
            AddPropOptions->Add(MakeShared<FString>(TEXT("+ Add Property...")));
            for (const auto& AF : AbsentFields)
            {
                AddPropOptions->Add(MakeShared<FString>(UTF8_TO_TCHAR(AF.DisplayLabel.c_str())));
            }
            OwnedOptionLists.Add(AddPropOptions);

            FieldsBox->AddSlot()
            .AutoHeight()
            .Padding(4.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&AddPropOptions.Get())
                    .InitiallySelectedItem((*AddPropOptions)[0])
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
                    })
                    .OnSelectionChanged_Lambda([this, CategoryName = Category.CategoryName](TSharedPtr<FString> SelectedItem, ESelectInfo::Type) {
                        if (!SelectedItem.IsValid() || *SelectedItem == TEXT("+ Add Property...")) return;
                        if (Adapter.IsValid())
                        {
                            auto Absent = Adapter->GetAbsentOptionalFieldsForCategory(CategoryName);
                            for (const auto& AF : Absent)
                            {
                                if (UTF8_TO_TCHAR(AF.DisplayLabel.c_str()) == *SelectedItem)
                                {
                                    Adapter->AddCurrentOptionalProperty(AF.JsonPointer);
                                    RefreshProperties();
                                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                                    break;
                                }
                            }
                        }
                    })
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT("+ Add Property...")))
                        .ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 0.4f)))
                    ]
                ]
            ];
        }
    }

    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .HeaderContent()
        [
            SNew(STextBlock)
            .Text(FText::FromString(UTF8_TO_TCHAR(Category.CategoryName.c_str())))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
        ]
        .BodyContent()
        [
            FieldsBox
        ];
}

TSharedRef<SWidget> SGV2DefinitionProperties::BuildFieldRow(const FGV2FormFieldDescriptor& Field, EPropertyPresence Presence)
{
    FString TooltipText = UTF8_TO_TCHAR(Field.Description.c_str());
    FString LabelText = UTF8_TO_TCHAR(Field.DisplayLabel.c_str());

    if (Presence == EPropertyPresence::RequiredMissing)
    {
        LabelText += TEXT(" * [Required - Missing]");
    }
    else if (Presence == EPropertyPresence::ImplicitDefault)
    {
        LabelText += TEXT(" (default)");
    }
    else if (Field.bRequired)
    {
        LabelText += TEXT(" *");
    }

    TSharedRef<SWidget> Control = CreateControlForField(Field, Presence);
    const FString Pointer = UTF8_TO_TCHAR(Field.JsonPointer.c_str());
    FieldWidgets.Add(Pointer, Control);

    TSharedRef<SHorizontalBox> ActionsBox = SNew(SHorizontalBox);

    if (Presence == EPropertyPresence::ImplicitDefault)
    {
        ActionsBox->AddSlot()
        .AutoWidth()
        .Padding(2.0f, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Override")))
            .ToolTipText(FText::FromString(TEXT("Set explicit value for this default property")))
            .OnClicked_Lambda([this, Ptr = Field.JsonPointer, DefVal = Field.DefaultValue]() {
                if (Adapter.IsValid() && DefVal.has_value())
                {
                    Adapter->SetCurrentFieldValue(Ptr, *DefVal);
                    RefreshProperties();
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
                return FReply::Handled();
            })
        ];
    }
    else if (Presence == EPropertyPresence::Explicit && !Field.bRequired)
    {
        if (Field.Spec && Field.Spec->DefaultValue.has_value())
        {
            ActionsBox->AddSlot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Reset")))
                .ToolTipText(FText::FromString(TEXT("Reset to schema default")))
                .OnClicked_Lambda([this, Ptr = Field.JsonPointer]() {
                    if (Adapter.IsValid())
                    {
                        Adapter->ResetCurrentFieldToDefault(Ptr);
                        RefreshProperties();
                        if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                    }
                    return FReply::Handled();
                })
            ];
        }
        else
        {
            ActionsBox->AddSlot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Remove")))
                .ToolTipText(FText::FromString(TEXT("Remove optional property from definition")))
                .OnClicked_Lambda([this, Ptr = Field.JsonPointer]() {
                    if (Adapter.IsValid())
                    {
                        Adapter->RemoveCurrentProperty(Ptr);
                        RefreshProperties();
                        if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                    }
                    return FReply::Handled();
                })
            ];
        }
    }

    FSlateColor LabelColor = FSlateColor(FLinearColor::White);
    if (Presence == EPropertyPresence::RequiredMissing)
    {
        LabelColor = FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f));
    }
    else if (Presence == EPropertyPresence::ImplicitDefault)
    {
        LabelColor = FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f));
    }

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(Pointer == HighlightedPointer
            ? FLinearColor(0.35f, 0.25f, 0.05f, 1.0f)
            : FLinearColor::Transparent)
        .Padding(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(0.35f)
            .VAlign(VAlign_Center)
            .Padding(4.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(LabelText))
                .ToolTipText(FText::FromString(TooltipText))
                .ColorAndOpacity(LabelColor)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(0.55f)
            .VAlign(VAlign_Center)
            .Padding(4.0f, 2.0f)
            [
                Control
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.0f, 0.0f)
            [
                ActionsBox
            ]
        ];
}

TSharedRef<SWidget> SGV2DefinitionProperties::CreateControlForField(const FGV2FormFieldDescriptor& Field, EPropertyPresence Presence)
{
    auto CurrentValOpt = Adapter ? Adapter->GetCurrentFieldValue(Field.JsonPointer) : std::nullopt;
    if (!CurrentValOpt.has_value() && Presence == EPropertyPresence::ImplicitDefault && Field.DefaultValue.has_value())
    {
        CurrentValOpt = Field.DefaultValue;
    }

    std::string Ptr = Field.JsonPointer;

    auto BuildPicker = [this, Ptr](
        const std::vector<std::string>& Choices,
        const FString& EmptyHint) -> TSharedRef<SWidget>
    {
        auto Options = MakeShared<TArray<TSharedPtr<FString>>>();
        TSharedPtr<FString> Selected;
        const auto Current = Adapter.IsValid() ? Adapter->GetCurrentFieldValue(Ptr) : std::nullopt;
        const FString CurrentText = Current.has_value() && Current->IsString()
            ? UTF8_TO_TCHAR(Current->AsString().c_str()) : FString();
        for (const auto& Choice : Choices)
        {
            auto Item = MakeShared<FString>(UTF8_TO_TCHAR(Choice.c_str()));
            Options->Add(Item);
            if (*Item == CurrentText) Selected = Item;
        }
        OwnedOptionLists.Add(Options);

        return SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&Options.Get())
            .InitiallySelectedItem(Selected)
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
            })
            .OnSelectionChanged_Lambda([this, Ptr](TSharedPtr<FString> Item, ESelectInfo::Type) {
                if (Adapter.IsValid() && Item.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(TCHAR_TO_UTF8(**Item)));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this, Ptr, EmptyHint]() {
                    const auto Value = Adapter.IsValid() ? Adapter->GetCurrentFieldValue(Ptr) : std::nullopt;
                    return Value.has_value() && Value->IsString() && !Value->AsString().empty()
                        ? FText::FromString(UTF8_TO_TCHAR(Value->AsString().c_str()))
                        : FText::FromString(EmptyHint);
                })
            ];
    };

    switch (Field.AdapterDescriptor.ControlType)
    {
    case EFieldControlType::Checkbox:
    {
        bool bChecked = CurrentValOpt.has_value() && CurrentValOpt->IsBoolean() && CurrentValOpt->AsBoolean();
        return SNew(SCheckBox)
            .IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged_Lambda([this, Ptr](ECheckBoxState NewState) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(NewState == ECheckBoxState::Checked));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
    }
    case EFieldControlType::IntegerNumeric:
    {
        int64 CurrentInt = (CurrentValOpt.has_value() && CurrentValOpt->IsInteger()) ? CurrentValOpt->AsInteger() : 0;
        int64 MinVal = Field.AdapterDescriptor.MinValue.has_value() ? static_cast<int64>(*Field.AdapterDescriptor.MinValue) : TNumericLimits<int64>::Lowest();
        int64 MaxVal = Field.AdapterDescriptor.MaxValue.has_value() ? static_cast<int64>(*Field.AdapterDescriptor.MaxValue) : TNumericLimits<int64>::Max();

        return SNew(SSpinBox<int64>)
            .Value(CurrentInt)
            .MinValue(MinVal)
            .MaxValue(MaxVal)
            .OnValueChanged_Lambda([this, Ptr](int64 NewVal) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(static_cast<std::int64_t>(NewVal)));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
    }
    case EFieldControlType::DoubleNumeric:
    {
        double CurrentNum = (CurrentValOpt.has_value() && CurrentValOpt->IsNumber()) ? CurrentValOpt->AsNumber() : 0.0;
        double MinVal = Field.AdapterDescriptor.MinValue.value_or(-1000000.0);
        double MaxVal = Field.AdapterDescriptor.MaxValue.value_or(1000000.0);

        return SNew(SSpinBox<double>)
            .Value(CurrentNum)
            .MinValue(MinVal)
            .MaxValue(MaxVal)
            .OnValueChanged_Lambda([this, Ptr](double NewVal) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(NewVal));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
    }
    case EFieldControlType::Slider:
    {
        if (Field.AdapterDescriptor.ScalarKind == GV2ContentCore::EScalarFieldKind::Integer)
        {
            int64 CurrentInt = (CurrentValOpt.has_value() && CurrentValOpt->IsInteger()) ? CurrentValOpt->AsInteger() : 0;
            int64 MinVal = Field.AdapterDescriptor.MinValue.has_value() ? static_cast<int64>(*Field.AdapterDescriptor.MinValue) : TNumericLimits<int64>::Lowest();
            int64 MaxVal = Field.AdapterDescriptor.MaxValue.has_value() ? static_cast<int64>(*Field.AdapterDescriptor.MaxValue) : TNumericLimits<int64>::Max();

            return SNew(SSpinBox<int64>)
                .Value(CurrentInt)
                .MinValue(MinVal)
                .MaxValue(MaxVal)
                .OnValueChanged_Lambda([this, Ptr](int64 NewVal) {
                    if (Adapter.IsValid())
                    {
                        Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(static_cast<std::int64_t>(NewVal)));
                        if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                    }
                });
        }
        else
        {
            double CurrentNum = (CurrentValOpt.has_value() && CurrentValOpt->IsNumber()) ? CurrentValOpt->AsNumber() : 0.0;
            double MinVal = Field.AdapterDescriptor.MinValue.value_or(-1000000.0);
            double MaxVal = Field.AdapterDescriptor.MaxValue.value_or(1000000.0);

            return SNew(SSpinBox<double>)
                .Value(CurrentNum)
                .MinValue(MinVal)
                .MaxValue(MaxVal)
                .OnValueChanged_Lambda([this, Ptr](double NewVal) {
                    if (Adapter.IsValid())
                    {
                        Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(NewVal));
                        if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                    }
                });
        }
    }
    case EFieldControlType::EnumDropdown:
        return BuildPicker(Field.AdapterDescriptor.EnumChoices, TEXT("Select value..."));
    case EFieldControlType::MultilineText:
    {
        FString CurrentStr = (CurrentValOpt.has_value() && CurrentValOpt->IsString()) ? UTF8_TO_TCHAR(CurrentValOpt->AsString().c_str()) : TEXT("");
        return SNew(SMultiLineEditableTextBox)
            .Text(FText::FromString(CurrentStr))
            .OnTextChanged_Lambda([this, Ptr](const FText& NewText) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(TCHAR_TO_UTF8(*NewText.ToString())));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
    }
    case EFieldControlType::ReferencePicker:
    {
        const auto Choices = Adapter.IsValid()
            ? Adapter->GetCompatibleReferenceTargets(Field.AdapterDescriptor.TargetReferenceKind)
            : std::vector<std::string>{};
        return BuildPicker(Choices, FString::Printf(
            TEXT("Select ref<%s>..."), UTF8_TO_TCHAR(Field.AdapterDescriptor.TargetReferenceKind.c_str())));
    }
    case EFieldControlType::ResourcePicker:
        return BuildPicker(
            Adapter.IsValid() ? Adapter->GetCompatibleResourceTargets(Field.AdapterDescriptor.TargetResourceClass) : std::vector<std::string>{},
            FString::Printf(TEXT("Select resource<%s>..."), UTF8_TO_TCHAR(Field.AdapterDescriptor.TargetResourceClass.c_str())));
    case EFieldControlType::TextIdPicker:
        return BuildPicker(
            Adapter.IsValid() ? Adapter->GetCompatibleReferenceTargets("text") : std::vector<std::string>{},
            TEXT("Select text ID..."));
    case EFieldControlType::ArrayEditor:
    {
        TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
        GV2ContentCore::FValue::FArray Values;
        if (CurrentValOpt.has_value() && CurrentValOpt->IsArray()) Values = CurrentValOpt->AsArray();

        for (int32 Index = 0; Index < static_cast<int32>(Values.size()); ++Index)
        {
            const GV2ContentCore::FValue ItemValue = Values[static_cast<std::size_t>(Index)];
            TSharedRef<SWidget> ItemControl = SNew(SEditableTextBox)
                .Text(FText::FromString(ItemValue.IsString()
                    ? UTF8_TO_TCHAR(ItemValue.AsString().c_str()) : (ItemValue.IsInteger() ? FString::Printf(TEXT("%lld"), ItemValue.AsInteger()) : (ItemValue.IsNumber() ? FString::Printf(TEXT("%g"), ItemValue.AsNumber()) : TEXT("<value>")))))
                .OnTextCommitted_Lambda([this, Ptr, Index](const FText& Text, ETextCommit::Type) {
                    if (Adapter.IsValid())
                    {
                        auto Value = Adapter->GetCurrentFieldValue(Ptr);
                        if (!Value.has_value() || !Value->IsArray()) return;
                        auto Array = Value->AsArray();
                        if (Index < static_cast<int32>(Array.size()))
                        {
                            Array[static_cast<std::size_t>(Index)] = GV2ContentCore::FValue(TCHAR_TO_UTF8(*Text.ToString()));
                            Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(std::move(Array)));
                            if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                        }
                    }
                });

            if (Field.Spec && Field.Spec->Items
                && Field.Spec->Items->Kind == GV2ContentCore::EFieldKind::Reference)
            {
                auto Options = MakeShared<TArray<TSharedPtr<FString>>>();
                TSharedPtr<FString> Selected;
                const auto Choices = Adapter.IsValid()
                    ? Adapter->GetCompatibleReferenceTargets(Field.Spec->Items->ExpectedStableIdKind)
                    : std::vector<std::string>{};
                for (const auto& Choice : Choices)
                {
                    auto Option = MakeShared<FString>(UTF8_TO_TCHAR(Choice.c_str()));
                    Options->Add(Option);
                    if (ItemValue.IsString() && *Option == UTF8_TO_TCHAR(ItemValue.AsString().c_str())) Selected = Option;
                }
                OwnedOptionLists.Add(Options);
                ItemControl = SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&Options.Get())
                    .InitiallySelectedItem(Selected)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
                    })
                    .OnSelectionChanged_Lambda([this, Ptr, Index](TSharedPtr<FString> Item, ESelectInfo::Type) {
                        if (!Item.IsValid() || !Adapter.IsValid()) return;
                        auto Value = Adapter->GetCurrentFieldValue(Ptr);
                        if (!Value.has_value() || !Value->IsArray()) return;
                        auto Array = Value->AsArray();
                        if (Index < static_cast<int32>(Array.size()))
                        {
                            Array[static_cast<std::size_t>(Index)] = GV2ContentCore::FValue(TCHAR_TO_UTF8(**Item));
                            Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(std::move(Array)));
                            if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                        }
                    })
                    [ SNew(STextBlock).Text(FText::FromString(ItemValue.IsString() ? UTF8_TO_TCHAR(ItemValue.AsString().c_str()) : TEXT("Select..."))) ];
            }

            Rows->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    ItemControl
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(1.0f, 0.0f)
                [
                    SNew(SButton).Text(FText::FromString(TEXT("▲")))
                    .IsEnabled(Index > 0)
                    .ToolTipText(FText::FromString(TEXT("Move item up")))
                    .OnClicked_Lambda([this, Ptr, Index]() {
                        if (Adapter.IsValid() && Index > 0)
                        {
                            Adapter->MoveCurrentArrayElement(Ptr, static_cast<std::size_t>(Index), static_cast<std::size_t>(Index - 1));
                            RefreshProperties();
                            if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                        }
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(1.0f, 0.0f)
                [
                    SNew(SButton).Text(FText::FromString(TEXT("▼")))
                    .IsEnabled(Index + 1 < static_cast<int32>(Values.size()))
                    .ToolTipText(FText::FromString(TEXT("Move item down")))
                    .OnClicked_Lambda([this, Ptr, Index]() {
                        if (Adapter.IsValid())
                        {
                            Adapter->MoveCurrentArrayElement(Ptr, static_cast<std::size_t>(Index), static_cast<std::size_t>(Index + 1));
                            RefreshProperties();
                            if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                        }
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(1.0f, 0.0f)
                [
                    SNew(SButton).Text(FText::FromString(TEXT("✕")))
                    .ToolTipText(FText::FromString(TEXT("Remove item")))
                    .OnClicked_Lambda([this, Ptr, Index]() {
                        if (Adapter.IsValid())
                        {
                            Adapter->RemoveCurrentArrayElement(Ptr, static_cast<std::size_t>(Index));
                            RefreshProperties();
                            if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                        }
                        return FReply::Handled();
                    })
                ]
            ];
        }

        Rows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
        [
            SNew(SButton).Text(FText::FromString(TEXT("+ Add Item")))
            .OnClicked_Lambda([this, Ptr, Spec = Field.Spec, ItemCount = Values.size()]() {
                if (Adapter.IsValid())
                {
                    GV2ContentCore::FValue NewItem("");
                    if (Spec && Spec->Items)
                    {
                        if (Spec->Items->DefaultValue.has_value()) NewItem = *Spec->Items->DefaultValue;
                        else if (Spec->Items->Kind == GV2ContentCore::EFieldKind::Scalar && Spec->Items->Scalar && Spec->Items->Scalar->Kind == GV2ContentCore::EScalarFieldKind::Integer) NewItem = GV2ContentCore::FValue(static_cast<std::int64_t>(0));
                        else if (Spec->Items->Kind == GV2ContentCore::EFieldKind::Scalar && Spec->Items->Scalar && Spec->Items->Scalar->Kind == GV2ContentCore::EScalarFieldKind::Number) NewItem = GV2ContentCore::FValue(0.0);
                        else if (Spec->Items->Kind == GV2ContentCore::EFieldKind::Object) NewItem = GV2ContentCore::FValue::MakeObject({});
                    }
                    Adapter->InsertCurrentArrayElement(Ptr, ItemCount, NewItem);
                    RefreshProperties();
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
                return FReply::Handled();
            })
        ];
        return Rows;
    }
    case EFieldControlType::ObjectEditor:
    {
        if (Presence == EPropertyPresence::Absent)
        {
            return SNew(SButton)
                .Text(FText::FromString(TEXT("+ Add Object")))
                .OnClicked_Lambda([this, Ptr]() {
                    if (Adapter.IsValid())
                    {
                        Adapter->AddCurrentOptionalProperty(Ptr);
                        RefreshProperties();
                        if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                    }
                    return FReply::Handled();
                });
        }
        return SNew(STextBlock)
            .Text(FText::FromString(TEXT("Object materialized")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 0.55f)));
    }
    case EFieldControlType::Text:
    default:
    {
        FString CurrentStr = (CurrentValOpt.has_value() && CurrentValOpt->IsString()) ? UTF8_TO_TCHAR(CurrentValOpt->AsString().c_str()) : TEXT("");
        return SNew(SEditableTextBox)
            .Text(FText::FromString(CurrentStr))
            .OnTextChanged_Lambda([this, Ptr](const FText& NewText) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(TCHAR_TO_UTF8(*NewText.ToString())));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
    }
    }
}

} // namespace GV2ContentEditor
#endif
