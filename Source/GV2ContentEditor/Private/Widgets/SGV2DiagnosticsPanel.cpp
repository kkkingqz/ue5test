#include "GV2ContentEditor/Widgets/SGV2DiagnosticsPanel.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GV2ContentEditor
{

void SGV2DiagnosticsPanel::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnNavigateToDiagnostic = InArgs._OnNavigateToDiagnostic;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(6.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Problems / Diagnostics")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SSpacer)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Validate All")))
                .OnClicked_Lambda([this]() {
                    RefreshDiagnostics();
                    return FReply::Handled();
                })
            ]
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(2.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FGV2EditorDiagnostic>>)
            .ListItemsSource(&DiagnosticItems)
            .OnGenerateRow(this, &SGV2DiagnosticsPanel::OnGenerateRow)
            .OnSelectionChanged(this, &SGV2DiagnosticsPanel::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RefreshDiagnostics();
}

void SGV2DiagnosticsPanel::SetDiagnostics(const std::vector<FGV2EditorDiagnostic>& Diagnostics)
{
    DiagnosticItems.Empty();
    for (const auto& Diag : Diagnostics)
    {
        DiagnosticItems.Add(MakeShared<FGV2EditorDiagnostic>(Diag));
    }
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void SGV2DiagnosticsPanel::RefreshDiagnostics()
{
    if (Adapter.IsValid())
    {
        auto Diags = Adapter->ValidateRepository();
        SetDiagnostics(Diags);
    }
}

TSharedRef<ITableRow> SGV2DiagnosticsPanel::OnGenerateRow(
    TSharedPtr<FGV2EditorDiagnostic> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    FLinearColor SeverityColor = FLinearColor(0.8f, 0.8f, 0.8f);
    FString SeverityPrefix = TEXT("[INFO]");

    if (Item->Severity == GV2ContentCore::EDiagnosticSeverity::Error)
    {
        SeverityColor = FLinearColor(1.0f, 0.3f, 0.3f);
        SeverityPrefix = TEXT("[ERROR]");
    }
    else if (Item->Severity == GV2ContentCore::EDiagnosticSeverity::Warning)
    {
        SeverityColor = FLinearColor(1.0f, 0.7f, 0.2f);
        SeverityPrefix = TEXT("[WARN]");
    }

    FString HeaderText = FString::Printf(TEXT("%s %s: %s"),
        *SeverityPrefix,
        UTF8_TO_TCHAR(Item->Code.c_str()),
        UTF8_TO_TCHAR(Item->Message.c_str()));

    FString LocText = FString::Printf(TEXT("Def: %s | Ptr: %s | %s:%d:%d"),
        UTF8_TO_TCHAR(Item->StableId.c_str()),
        UTF8_TO_TCHAR(Item->JsonPointer.c_str()),
        UTF8_TO_TCHAR(Item->RelativeSource.c_str()),
        static_cast<int32>(Item->Line),
        static_cast<int32>(Item->Column));

    return SNew(STableRow<TSharedPtr<FGV2EditorDiagnostic>>, OwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(HeaderText))
            .ColorAndOpacity(FSlateColor(SeverityColor))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 0.0f, 4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(LocText))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ]
    ];
}

void SGV2DiagnosticsPanel::OnSelectionChanged(
    TSharedPtr<FGV2EditorDiagnostic> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && OnNavigateToDiagnostic.IsBound())
    {
        OnNavigateToDiagnostic.Execute(
            UTF8_TO_TCHAR(SelectedItem->StableId.c_str()),
            UTF8_TO_TCHAR(SelectedItem->JsonPointer.c_str()));
    }
}

} // namespace GV2ContentEditor
#endif
