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
                .Text(FText::FromString(TEXT("Revert")))
                .IsEnabled_Lambda([this]() {
                    return Adapter.IsValid() && Adapter->IsDirty();
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
    if (Adapter.IsValid() && Adapter->IsDirty())
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
    if (Adapter.IsValid() && Adapter->IsDirty())
    {
        Adapter->DiscardCurrentChanges();
        RefreshProperties();
        if (OnFieldValueChanged.IsBound())
        {
            OnFieldValueChanged.Execute();
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

    auto FormModelOpt = Adapter->GetFormModelForDefinitionType(CurrentDef->Type);
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
}

TSharedRef<SWidget> SGV2DefinitionProperties::BuildCategorySection(const FGV2FormCategorySection& Category)
{
    TSharedRef<SVerticalBox> FieldsBox = SNew(SVerticalBox);

    for (const auto& Field : Category.Fields)
    {
        FieldsBox->AddSlot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            BuildFieldRow(Field)
        ];
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

TSharedRef<SWidget> SGV2DefinitionProperties::BuildFieldRow(const FGV2FormFieldDescriptor& Field)
{
    FString TooltipText = UTF8_TO_TCHAR(Field.Description.c_str());
    FString LabelText = FString::Printf(TEXT("%s%s"),
        UTF8_TO_TCHAR(Field.DisplayLabel.c_str()),
        Field.bRequired ? TEXT(" *") : TEXT(""));

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(0.35f)
        .VAlign(VAlign_Center)
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(LabelText))
            .ToolTipText(FText::FromString(TooltipText))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ]
        + SHorizontalBox::Slot()
        .FillWidth(0.65f)
        .VAlign(VAlign_Center)
        .Padding(4.0f, 2.0f)
        [
            CreateControlForField(Field)
        ];
}

TSharedRef<SWidget> SGV2DefinitionProperties::CreateControlForField(const FGV2FormFieldDescriptor& Field)
{
    auto CurrentValOpt = Adapter ? Adapter->GetCurrentFieldValue(Field.JsonPointer) : std::nullopt;

    std::string Ptr = Field.JsonPointer;

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
        FString CurrentRef = (CurrentValOpt.has_value() && CurrentValOpt->IsString()) ? UTF8_TO_TCHAR(CurrentValOpt->AsString().c_str()) : TEXT("");
        return SNew(SEditableTextBox)
            .Text(FText::FromString(CurrentRef))
            .HintText(FText::FromString(FString::Printf(TEXT("ref<%s>"), UTF8_TO_TCHAR(Field.AdapterDescriptor.TargetReferenceKind.c_str()))))
            .OnTextChanged_Lambda([this, Ptr](const FText& NewText) {
                if (Adapter.IsValid())
                {
                    Adapter->SetCurrentFieldValue(Ptr, GV2ContentCore::FValue(TCHAR_TO_UTF8(*NewText.ToString())));
                    if (OnFieldValueChanged.IsBound()) OnFieldValueChanged.Execute();
                }
            });
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
