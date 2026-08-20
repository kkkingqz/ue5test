#include "GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

namespace GV2ContentEditor
{

namespace
{
TOptional<FString> PromptForText(const FText& Title, const FText& Label, const FString& InitialValue)
{
    TOptional<FString> Result;
    TSharedPtr<SEditableTextBox> Input;
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(Title)
        .ClientSize(FVector2D(520.0f, 120.0f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);
    Window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f)
        [ SNew(STextBlock).Text(Label) ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
        [ SAssignNew(Input, SEditableTextBox).Text(FText::FromString(InitialValue)).SelectAllTextWhenFocused(true) ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [
                SNew(SButton).Text(FText::FromString(TEXT("Cancel")))
                .OnClicked_Lambda([Window]() { Window->RequestDestroyWindow(); return FReply::Handled(); })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [
                SNew(SButton).Text(FText::FromString(TEXT("OK")))
                .OnClicked_Lambda([Window, Input, &Result]() {
                    if (Input.IsValid() && !Input->GetText().IsEmpty()) Result = Input->GetText().ToString();
                    Window->RequestDestroyWindow();
                    return FReply::Handled();
                })
            ]
        ]);
    FSlateApplication::Get().AddModalWindow(
        Window, FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr), false);
    return Result;
}
}

void SGV2DefinitionBrowser::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnDefinitionSelected = InArgs._OnDefinitionSelected;
    OnDefinitionsChanged = InArgs._OnDefinitionsChanged;
    OnOperationCompleted = InArgs._OnOperationCompleted;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 4.0f, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("New Definition")))
            .OnClicked(this, &SGV2DefinitionBrowser::HandleCreate)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SSearchBox)
            .HintText(FText::FromString(TEXT("Search definitions by ID or kind...")))
            .OnTextChanged(this, &SGV2DefinitionBrowser::OnSearchTextChanged)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(2.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FGV2DefinitionSummary>>)
            .ListItemsSource(&FilteredItems)
            .OnGenerateRow(this, &SGV2DefinitionBrowser::OnGenerateRow)
            .OnSelectionChanged(this, &SGV2DefinitionBrowser::OnSelectionChanged)
            .OnContextMenuOpening(this, &SGV2DefinitionBrowser::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RefreshList();
}

void SGV2DefinitionBrowser::RefreshList()
{
    AllItems.Empty();
    if (Adapter.IsValid())
    {
        auto Summaries = Adapter->ListDefinitions();
        for (const auto& Summary : Summaries)
        {
            AllItems.Add(MakeShared<FGV2DefinitionSummary>(Summary));
        }
    }
    FilterItems();
}

void SGV2DefinitionBrowser::SetSelectedDefinition(const FString& DefinitionId)
{
    std::string TargetId = TCHAR_TO_UTF8(*DefinitionId);
    for (const auto& Item : FilteredItems)
    {
        if (Item.IsValid() && Item->Id == TargetId)
        {
            if (ListView.IsValid())
            {
                ListView->SetSelection(Item);
                ListView->RequestScrollIntoView(Item);
            }
            break;
        }
    }
}

void SGV2DefinitionBrowser::OnSearchTextChanged(const FText& InSearchText)
{
    CurrentFilterText = InSearchText.ToString();
    FilterItems();
}

void SGV2DefinitionBrowser::FilterItems()
{
    FilteredItems.Empty();
    FString FilterLower = CurrentFilterText.ToLower();

    for (const auto& Item : AllItems)
    {
        if (!Item.IsValid()) continue;

        if (FilterLower.IsEmpty() ||
            FString(Item->Id.c_str()).ToLower().Contains(FilterLower) ||
            FString(Item->Type.c_str()).ToLower().Contains(FilterLower) ||
            FString(Item->PackageId.c_str()).ToLower().Contains(FilterLower))
        {
            FilteredItems.Add(Item);
        }
    }
    FilteredItems.Sort([](const TSharedPtr<FGV2DefinitionSummary>& A, const TSharedPtr<FGV2DefinitionSummary>& B) {
        if (A->Type != B->Type) return A->Type < B->Type;
        return A->Id < B->Id;
    });

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SGV2DefinitionBrowser::OnGenerateRow(
    TSharedPtr<FGV2DefinitionSummary> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    bool bIsDirty = false;
    if (Adapter.IsValid())
    {
        const auto* CurrentDef = Adapter->GetCurrentDefinition();
        if (CurrentDef != nullptr && CurrentDef->Id == Item->Id && Adapter->IsDirty())
        {
            bIsDirty = true;
        }
    }

    FString DisplayText = FString::Printf(TEXT("%s%s"),
        UTF8_TO_TCHAR(Item->Id.c_str()),
        bIsDirty ? TEXT(" *") : TEXT(""));

    FString SubText = FString::Printf(TEXT("[%s] %s"),
        UTF8_TO_TCHAR(Item->Type.c_str()),
        UTF8_TO_TCHAR(Item->PackageId.c_str()));

    const int32 ItemIndex = FilteredItems.IndexOfByKey(Item);
    const bool bStartsKind = ItemIndex == 0
        || !FilteredItems.IsValidIndex(ItemIndex - 1)
        || FilteredItems[ItemIndex - 1]->Type != Item->Type;

    return SNew(STableRow<TSharedPtr<FGV2DefinitionSummary>>, OwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Visibility(bStartsKind ? EVisibility::Visible : EVisibility::Collapsed)
            .Text(FText::FromString(UTF8_TO_TCHAR(Item->Type.c_str())))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.7f, 1.0f)))
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(4.0f, 2.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(DisplayText))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(SubText))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
            ]
        ]
        ]
    ];
}

void SGV2DefinitionBrowser::OnSelectionChanged(
    TSharedPtr<FGV2DefinitionSummary> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && OnDefinitionSelected.IsBound())
    {
        OnDefinitionSelected.Execute(UTF8_TO_TCHAR(SelectedItem->Id.c_str()));
    }
}

TSharedPtr<SWidget> SGV2DefinitionBrowser::OnContextMenuOpening()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid())
    {
        return nullptr;
    }

    FMenuBuilder MenuBuilder(true, nullptr);

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Copy Stable ID")),
        FText::FromString(TEXT("Copy definition ID to clipboard")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleCopyId)));

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Duplicate")),
        FText::FromString(TEXT("Duplicate this definition")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDuplicate)));

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Rename")),
        FText::FromString(TEXT("Rename this definition and rewrite references in its package")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleRename)));

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Delete")),
        FText::FromString(TEXT("Delete this definition")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDelete)));

    return MenuBuilder.MakeWidget();
}

FReply SGV2DefinitionBrowser::HandleCreate()
{
    if (!Adapter.IsValid()) return FReply::Handled();
    FString DefaultPackage = AllItems.IsEmpty() ? TEXT("core") : UTF8_TO_TCHAR(AllItems[0]->PackageId.c_str());
    FString DefaultType = AllItems.IsEmpty() ? TEXT("item") : UTF8_TO_TCHAR(AllItems[0]->Type.c_str());
    auto Package = PromptForText(FText::FromString(TEXT("New Definition")), FText::FromString(TEXT("Package ID")), DefaultPackage);
    if (!Package.IsSet()) return FReply::Handled();
    auto Type = PromptForText(FText::FromString(TEXT("New Definition")), FText::FromString(TEXT("Definition type")), DefaultType);
    if (!Type.IsSet()) return FReply::Handled();
    auto Id = PromptForText(FText::FromString(TEXT("New Definition")), FText::FromString(TEXT("Stable ID")), *Package + TEXT(":") + *Type + TEXT(".new"));
    if (!Id.IsSet()) return FReply::Handled();

    auto Result = Adapter->CreateDefinition(
        TCHAR_TO_UTF8(**Package), TCHAR_TO_UTF8(**Id), TCHAR_TO_UTF8(**Type));
    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        SetSelectedDefinition(*Id);
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
    return FReply::Handled();
}

void SGV2DefinitionBrowser::HandleCopyId()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(UTF8_TO_TCHAR(SelectedItems[0]->Id.c_str()));
    }
}

void SGV2DefinitionBrowser::HandleDuplicate()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid() && Adapter.IsValid())
    {
        std::string SourceId = SelectedItems[0]->Id;
        auto RequestedId = PromptForText(
            FText::FromString(TEXT("Duplicate Definition")), FText::FromString(TEXT("Target Stable ID")),
            UTF8_TO_TCHAR((SourceId + "_copy").c_str()));
        if (!RequestedId.IsSet()) return;
        std::string TargetId = TCHAR_TO_UTF8(**RequestedId);
        auto Result = Adapter->DuplicateDefinition(SourceId, TargetId);
        ReportOperation(Result);
        if (Result.IsSuccess())
        {
            RefreshList();
            SetSelectedDefinition(UTF8_TO_TCHAR(TargetId.c_str()));
            if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
        }
    }
}

void SGV2DefinitionBrowser::HandleRename()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid() || !Adapter.IsValid()) return;
    const std::string OldId = SelectedItems[0]->Id;
    auto RequestedId = PromptForText(
        FText::FromString(TEXT("Rename Definition")), FText::FromString(TEXT("New Stable ID")),
        UTF8_TO_TCHAR(OldId.c_str()));
    if (!RequestedId.IsSet()) return;
    const std::string NewId = TCHAR_TO_UTF8(**RequestedId);
    auto Result = Adapter->RenameDefinition(OldId, NewId);
    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        SetSelectedDefinition(*RequestedId);
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
}

void SGV2DefinitionBrowser::HandleDelete()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid() && Adapter.IsValid())
    {
        std::string TargetId = SelectedItems[0]->Id;
        auto InRefs = Adapter->GetIncomingReferences(TargetId);
        if (!InRefs.empty())
        {
            FGV2EditorAuthoringResult Blocked;
            Blocked.Outcome = EEditorAuthoringOutcome::ValidationFailed;
            Blocked.ErrorCode = "definition_has_incoming_references";
            Blocked.ErrorMessage = "Definition is referenced by " + std::to_string(InRefs.size()) + " field(s)";
            for (const auto& Ref : InRefs)
            {
                FGV2EditorDiagnostic Diagnostic;
                Diagnostic.Code = "core:diagnostic.reference.incoming";
                Diagnostic.Message = "Incoming reference from " + Ref.SourceDefinitionId;
                Diagnostic.StableId = Ref.SourceDefinitionId;
                Diagnostic.JsonPointer = Ref.JsonPointer;
                Blocked.Diagnostics.push_back(std::move(Diagnostic));
            }
            ReportOperation(Blocked);
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(UTF8_TO_TCHAR(Blocked.ErrorMessage.c_str())));
            return;
        }

        if (FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::FromString(FString::Printf(TEXT("Delete %s?"), UTF8_TO_TCHAR(TargetId.c_str())))) != EAppReturnType::Yes)
        {
            return;
        }

        auto Result = Adapter->DeleteDefinition(TargetId);
        ReportOperation(Result);
        if (Result.IsSuccess())
        {
            RefreshList();
            if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
        }
    }
}

void SGV2DefinitionBrowser::ReportOperation(const FGV2EditorAuthoringResult& Result)
{
    if (OnOperationCompleted.IsBound()) OnOperationCompleted.Execute(Result);
    if (!Result.IsSuccess() && Result.Diagnostics.empty())
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(UTF8_TO_TCHAR(Result.ErrorMessage.c_str())));
    }
}

} // namespace GV2ContentEditor
#endif
