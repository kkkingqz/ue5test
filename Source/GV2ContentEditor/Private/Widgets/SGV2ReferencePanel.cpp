#include "GV2ContentEditor/Widgets/SGV2ReferencePanel.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GV2ContentEditor
{

void SGV2ReferencePanel::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnNavigateToDefinition = InArgs._OnNavigateToDefinition;

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Vertical)
        + SSplitter::Slot()
        .Value(0.5f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Uses (Outgoing)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(OutgoingListView, SListView<TSharedPtr<FGV2ReferenceItem>>)
                .ListItemsSource(&OutgoingItems)
                .OnGenerateRow(this, &SGV2ReferencePanel::OnGenerateOutgoingRow)
                .OnSelectionChanged(this, &SGV2ReferencePanel::OnOutgoingSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
            ]
        ]
        + SSplitter::Slot()
        .Value(0.5f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Used by (Incoming)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(IncomingListView, SListView<TSharedPtr<FGV2ReferenceItem>>)
                .ListItemsSource(&IncomingItems)
                .OnGenerateRow(this, &SGV2ReferencePanel::OnGenerateIncomingRow)
                .OnSelectionChanged(this, &SGV2ReferencePanel::OnIncomingSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
            ]
        ]
    ];

    RefreshReferences();
}

void SGV2ReferencePanel::RefreshReferences()
{
    OutgoingItems.Empty();
    IncomingItems.Empty();

    if (Adapter.IsValid())
    {
        const auto* CurrentDef = Adapter->GetCurrentDefinition();
        if (CurrentDef != nullptr)
        {
            auto OutRefs = Adapter->GetOutgoingReferences();
            for (const auto& Ref : OutRefs)
            {
                OutgoingItems.Add(MakeShared<FGV2ReferenceItem>(Ref));
            }

            auto InRefs = Adapter->GetIncomingReferences(CurrentDef->Id);
            for (const auto& Ref : InRefs)
            {
                IncomingItems.Add(MakeShared<FGV2ReferenceItem>(Ref));
            }
        }
    }

    if (OutgoingListView.IsValid()) OutgoingListView->RequestListRefresh();
    if (IncomingListView.IsValid()) IncomingListView->RequestListRefresh();
}

TSharedRef<ITableRow> SGV2ReferencePanel::OnGenerateOutgoingRow(
    TSharedPtr<FGV2ReferenceItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    FString TargetText = UTF8_TO_TCHAR(Item->TargetDefinitionId.c_str());
    FString SubText = FString::Printf(TEXT("%s (%s)"), UTF8_TO_TCHAR(Item->JsonPointer.c_str()), UTF8_TO_TCHAR(Item->TargetKind.c_str()));

    return SNew(STableRow<TSharedPtr<FGV2ReferenceItem>>, OwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TargetText))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f)))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 0.0f, 4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(SubText))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ]
    ];
}

TSharedRef<ITableRow> SGV2ReferencePanel::OnGenerateIncomingRow(
    TSharedPtr<FGV2ReferenceItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    FString SourceText = Item->SourceDefinitionId.empty()
        ? UTF8_TO_TCHAR(Item->RelativeSource.c_str())
        : UTF8_TO_TCHAR(Item->SourceDefinitionId.c_str());

    FString SubText = FString::Printf(TEXT("%s:%d (%s)"),
        UTF8_TO_TCHAR(Item->RelativeSource.c_str()),
        static_cast<int32>(Item->Line),
        UTF8_TO_TCHAR(Item->JsonPointer.c_str()));

    return SNew(STableRow<TSharedPtr<FGV2ReferenceItem>>, OwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(SourceText))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f)))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 0.0f, 4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(SubText))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ]
    ];
}

void SGV2ReferencePanel::OnOutgoingSelectionChanged(
    TSharedPtr<FGV2ReferenceItem> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && OnNavigateToDefinition.IsBound())
    {
        OnNavigateToDefinition.Execute(UTF8_TO_TCHAR(SelectedItem->TargetDefinitionId.c_str()));
    }
}

void SGV2ReferencePanel::OnIncomingSelectionChanged(
    TSharedPtr<FGV2ReferenceItem> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && !SelectedItem->SourceDefinitionId.empty() && OnNavigateToDefinition.IsBound())
    {
        OnNavigateToDefinition.Execute(UTF8_TO_TCHAR(SelectedItem->SourceDefinitionId.c_str()));
    }
}

} // namespace GV2ContentEditor
#endif
