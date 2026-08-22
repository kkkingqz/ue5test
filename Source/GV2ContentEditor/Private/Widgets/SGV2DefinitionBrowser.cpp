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

void SortTreeNodes(TArray<TSharedPtr<FGV2DefinitionTreeNode>>& Nodes)
{
    Nodes.Sort([](const TSharedPtr<FGV2DefinitionTreeNode>& A, const TSharedPtr<FGV2DefinitionTreeNode>& B) {
        if (A->NodeType != B->NodeType)
        {
            return static_cast<uint8>(A->NodeType) < static_cast<uint8>(B->NodeType);
        }
        return A->DisplayName < B->DisplayName;
    });

    for (const auto& Node : Nodes)
    {
        if (Node.IsValid() && Node->Children.Num() > 0)
        {
            SortTreeNodes(Node->Children);
        }
    }
}

TSharedPtr<FGV2DefinitionTreeNode> FilterNodeRecursive(
    const TSharedPtr<FGV2DefinitionTreeNode>& Node,
    const FString& FilterLower)
{
    if (!Node.IsValid()) return nullptr;

    const bool bSelfMatches = Node->DisplayName.ToLower().Contains(FilterLower)
        || Node->StableId.ToLower().Contains(FilterLower)
        || Node->DefinitionType.ToLower().Contains(FilterLower)
        || Node->WinnerPackageId.ToLower().Contains(FilterLower);

    TArray<TSharedPtr<FGV2DefinitionTreeNode>> FilteredChildren;
    for (const auto& Child : Node->Children)
    {
        auto FilteredChild = FilterNodeRecursive(Child, FilterLower);
        if (FilteredChild.IsValid())
        {
            FilteredChildren.Add(FilteredChild);
        }
    }

    if (bSelfMatches || FilteredChildren.Num() > 0)
    {
        auto Copy = MakeShared<FGV2DefinitionTreeNode>();
        Copy->DisplayName = Node->DisplayName;
        Copy->NodeType = Node->NodeType;
        Copy->StableId = Node->StableId;
        Copy->DefinitionType = Node->DefinitionType;
        Copy->PackageId = Node->PackageId;
        Copy->RelativeSource = Node->RelativeSource;
        Copy->LoadIndex = Node->LoadIndex;
        Copy->ProviderCount = Node->ProviderCount;
        Copy->bIsOverridden = Node->bIsOverridden;
        Copy->WinnerPackageId = Node->WinnerPackageId;
        Copy->bIsDirty = Node->bIsDirty;
        Copy->bHasDirtyDescendant = Node->bHasDirtyDescendant;
        Copy->Children = FilteredChildren;
        for (auto& FC : Copy->Children)
        {
            FC->Parent = Copy;
        }
        return Copy;
    }

    return nullptr;
}

} // namespace

void SGV2DefinitionBrowser::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnDefinitionSelected = InArgs._OnDefinitionSelected;
    OnLocatorSelected = InArgs._OnLocatorSelected;
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
            SAssignNew(TreeView, STreeView<TSharedPtr<FGV2DefinitionTreeNode>>)
            .TreeItemsSource(&FilteredRootNodes)
            .OnGenerateRow(this, &SGV2DefinitionBrowser::OnGenerateRow)
            .OnGetChildren(this, &SGV2DefinitionBrowser::OnGetChildren)
            .OnSelectionChanged(this, &SGV2DefinitionBrowser::OnSelectionChanged)
            .OnContextMenuOpening(this, &SGV2DefinitionBrowser::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RefreshList();
}

void SGV2DefinitionBrowser::BuildTree()
{
    RootNodes.Empty();
    if (!Adapter.IsValid()) return;

    const auto& AuthIndex = Adapter->GetAuthoringIndex();
    auto EffectiveDefs = AuthIndex.GetEffectiveDefinitions();

    TMap<FString, TSharedPtr<FGV2DefinitionTreeNode>> NamespaceMap;

    for (const auto& Loc : EffectiveDefs)
    {
        const FString FullId = UTF8_TO_TCHAR(Loc.DefinitionId.c_str());
        FString NsPart, Remainder;
        if (!FullId.Split(TEXT(":"), &NsPart, &Remainder))
        {
            NsPart = TEXT("core");
            Remainder = FullId;
        }

        FString KindPart, PathPart;
        if (!Remainder.Split(TEXT("."), &KindPart, &PathPart))
        {
            KindPart = UTF8_TO_TCHAR(Loc.DefinitionType.c_str());
            PathPart = Remainder;
        }

        // 1. Namespace root node
        TSharedPtr<FGV2DefinitionTreeNode> NsNode;
        if (auto* FoundNs = NamespaceMap.Find(NsPart))
        {
            NsNode = *FoundNs;
        }
        else
        {
            NsNode = MakeShared<FGV2DefinitionTreeNode>();
            NsNode->DisplayName = NsPart;
            NsNode->NodeType = EGV2DefinitionTreeNodeType::Namespace;
            NsNode->StableId = NsPart;
            RootNodes.Add(NsNode);
            NamespaceMap.Add(NsPart, NsNode);
        }

        // 2. Kind node under Namespace
        TSharedPtr<FGV2DefinitionTreeNode> KindNode;
        for (const auto& Child : NsNode->Children)
        {
            if (Child->DisplayName == KindPart && Child->NodeType == EGV2DefinitionTreeNodeType::Kind)
            {
                KindNode = Child;
                break;
            }
        }
        if (!KindNode.IsValid())
        {
            KindNode = MakeShared<FGV2DefinitionTreeNode>();
            KindNode->DisplayName = KindPart;
            KindNode->NodeType = EGV2DefinitionTreeNodeType::Kind;
            KindNode->StableId = FString::Printf(TEXT("%s:%s"), *NsPart, *KindPart);
            KindNode->DefinitionType = KindPart;
            KindNode->Parent = NsNode;
            NsNode->Children.Add(KindNode);
        }

        // 3. Path segments down to leaf definition node
        TArray<FString> Segments;
        PathPart.ParseIntoArray(Segments, TEXT("."), true);

        TSharedPtr<FGV2DefinitionTreeNode> CurrentParent = KindNode;
        FString AccumulatedPath = FString::Printf(TEXT("%s:%s"), *NsPart, *KindPart);

        for (int32 SegIdx = 0; SegIdx < Segments.Num(); ++SegIdx)
        {
            const FString& Seg = Segments[SegIdx];
            AccumulatedPath += TEXT(".") + Seg;
            const bool bIsLeaf = (SegIdx == Segments.Num() - 1);

            TSharedPtr<FGV2DefinitionTreeNode> ExistingChild;
            for (const auto& Child : CurrentParent->Children)
            {
                if (Child->DisplayName == Seg)
                {
                    ExistingChild = Child;
                    break;
                }
            }

            if (!ExistingChild.IsValid())
            {
                ExistingChild = MakeShared<FGV2DefinitionTreeNode>();
                ExistingChild->DisplayName = Seg;
                ExistingChild->NodeType = bIsLeaf ? EGV2DefinitionTreeNodeType::Definition : EGV2DefinitionTreeNodeType::PathSegment;
                ExistingChild->StableId = AccumulatedPath;
                ExistingChild->Parent = CurrentParent;
                CurrentParent->Children.Add(ExistingChild);
            }

            if (bIsLeaf)
            {
                ExistingChild->NodeType = EGV2DefinitionTreeNodeType::Definition;
                ExistingChild->StableId = FullId;
                ExistingChild->DefinitionType = UTF8_TO_TCHAR(Loc.DefinitionType.c_str());
                ExistingChild->PackageId = UTF8_TO_TCHAR(Loc.PackageId.c_str());
                ExistingChild->RelativeSource = UTF8_TO_TCHAR(Loc.RelativeSource.c_str());
                ExistingChild->LoadIndex = static_cast<int32>(Loc.LoadIndex);
                ExistingChild->WinnerPackageId = UTF8_TO_TCHAR(Loc.PackageId.c_str());

                auto Providers = Adapter->GetProvidersForDefinition(Loc.DefinitionId);
                ExistingChild->ProviderCount = static_cast<int32>(Providers.size());
                ExistingChild->bIsOverridden = (ExistingChild->ProviderCount > 1);
            }

            CurrentParent = ExistingChild;
        }
    }

    SortTreeNodes(RootNodes);
    UpdateDirtyState();
}

void SGV2DefinitionBrowser::UpdateDirtyState()
{
    if (!Adapter.IsValid()) return;

    auto ClearDirty = [](auto& Self, const TSharedPtr<FGV2DefinitionTreeNode>& Node) -> void {
        if (!Node.IsValid()) return;
        Node->bIsDirty = false;
        Node->bHasDirtyDescendant = false;
        for (const auto& Child : Node->Children)
        {
            Self(Self, Child);
        }
    };

    for (const auto& Root : RootNodes)
    {
        ClearDirty(ClearDirty, Root);
    }

    if (!Adapter->IsDirty()) return;

    auto CurrentLoc = Adapter->GetCurrentLocator();
    if (!CurrentLoc.has_value()) return;

    const FString ActiveId = UTF8_TO_TCHAR(CurrentLoc->DefinitionId.c_str());

    auto MarkDirty = [&](auto& Self, const TSharedPtr<FGV2DefinitionTreeNode>& Node) -> bool {
        if (!Node.IsValid()) return false;
        bool bHasDirtyChild = false;
        for (const auto& Child : Node->Children)
        {
            if (Self(Self, Child))
            {
                bHasDirtyChild = true;
            }
        }

        if (Node->NodeType == EGV2DefinitionTreeNodeType::Definition && Node->StableId == ActiveId)
        {
            Node->bIsDirty = true;
            return true;
        }

        Node->bHasDirtyDescendant = bHasDirtyChild;
        return bHasDirtyChild || Node->bIsDirty;
    };

    for (const auto& Root : RootNodes)
    {
        MarkDirty(MarkDirty, Root);
    }
}

void SGV2DefinitionBrowser::RefreshList()
{
    BuildTree();
    FilterItems();
}

void SGV2DefinitionBrowser::SetSelectedDefinition(const FString& DefinitionId)
{
    std::string TargetId = TCHAR_TO_UTF8(*DefinitionId);
    auto FindDefNode = [&](auto& Self, const TSharedPtr<FGV2DefinitionTreeNode>& Node) -> TSharedPtr<FGV2DefinitionTreeNode> {
        if (!Node.IsValid()) return nullptr;
        if (Node->NodeType == EGV2DefinitionTreeNodeType::Definition && Node->StableId == DefinitionId)
        {
            return Node;
        }
        for (const auto& Child : Node->Children)
        {
            auto Found = Self(Self, Child);
            if (Found.IsValid()) return Found;
        }
        return nullptr;
    };

    for (const auto& Root : FilteredRootNodes)
    {
        auto Found = FindDefNode(FindDefNode, Root);
        if (Found.IsValid())
        {
            if (TreeView.IsValid())
            {
                // Expand ancestors
                TSharedPtr<FGV2DefinitionTreeNode> Current = Found->Parent.Pin();
                while (Current.IsValid())
                {
                    TreeView->SetItemExpansion(Current, true);
                    Current = Current->Parent.Pin();
                }
                TreeView->SetSelection(Found);
                TreeView->RequestScrollIntoView(Found);
            }
            break;
        }
    }
}

void SGV2DefinitionBrowser::SetSelectedLocator(const GV2ContentAuthoring::FAuthoringLocator& Locator)
{
    SetSelectedDefinition(UTF8_TO_TCHAR(Locator.DefinitionId.c_str()));
}

void SGV2DefinitionBrowser::OnSearchTextChanged(const FText& InSearchText)
{
    CurrentFilterText = InSearchText.ToString();
    FilterItems();
}

void SGV2DefinitionBrowser::FilterItems()
{
    FilteredRootNodes.Empty();
    const FString FilterLower = CurrentFilterText.ToLower();

    if (FilterLower.IsEmpty())
    {
        FilteredRootNodes = RootNodes;
        if (TreeView.IsValid())
        {
            TreeView->RequestTreeRefresh();
            // Default expand namespaces
            for (const auto& Root : FilteredRootNodes)
            {
                TreeView->SetItemExpansion(Root, true);
            }
        }
        return;
    }

    for (const auto& Root : RootNodes)
    {
        auto FilteredRoot = FilterNodeRecursive(Root, FilterLower);
        if (FilteredRoot.IsValid())
        {
            FilteredRootNodes.Add(FilteredRoot);
        }
    }

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();

        // Auto-expand all filtered ancestors during search
        auto ExpandAll = [&](auto& Self, const TSharedPtr<FGV2DefinitionTreeNode>& Node) -> void {
            if (!Node.IsValid()) return;
            TreeView->SetItemExpansion(Node, true);
            for (const auto& Child : Node->Children)
            {
                Self(Self, Child);
            }
        };

        for (const auto& Root : FilteredRootNodes)
        {
            ExpandAll(ExpandAll, Root);
        }
    }
}

void SGV2DefinitionBrowser::OnGetChildren(
    TSharedPtr<FGV2DefinitionTreeNode> Item,
    TArray<TSharedPtr<FGV2DefinitionTreeNode>>& OutChildren)
{
    if (Item.IsValid())
    {
        OutChildren = Item->Children;
    }
}

TSharedRef<ITableRow> SGV2DefinitionBrowser::OnGenerateRow(
    TSharedPtr<FGV2DefinitionTreeNode> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    FString DisplayText = Item->DisplayName;
    FString SubText;
    FLinearColor TextColor = FLinearColor::White;

    if (Item->NodeType == EGV2DefinitionTreeNodeType::Namespace)
    {
        DisplayText = FString::Printf(TEXT("📦 %s"), *Item->DisplayName);
        TextColor = FLinearColor(0.6f, 0.8f, 1.0f);
    }
    else if (Item->NodeType == EGV2DefinitionTreeNodeType::Kind)
    {
        DisplayText = FString::Printf(TEXT("📁 %s"), *Item->DisplayName);
        TextColor = FLinearColor(0.9f, 0.9f, 0.6f);
    }
    else if (Item->NodeType == EGV2DefinitionTreeNodeType::PathSegment)
    {
        DisplayText = FString::Printf(TEXT("📂 %s"), *Item->DisplayName);
        TextColor = FLinearColor(0.8f, 0.8f, 0.8f);
    }
    else if (Item->NodeType == EGV2DefinitionTreeNodeType::Definition)
    {
        DisplayText = FString::Printf(TEXT("📄 %s"), *Item->DisplayName);
        if (Item->bIsOverridden)
        {
            SubText = FString::Printf(TEXT(" [Override: %s (%d)]"), *Item->WinnerPackageId, Item->ProviderCount);
        }
        else
        {
            SubText = FString::Printf(TEXT(" [%s]"), *Item->WinnerPackageId);
        }
    }

    FString DirtyIndicator;
    if (Item->bIsDirty)
    {
        DirtyIndicator = TEXT(" * [Unsaved]");
    }
    else if (Item->bHasDirtyDescendant)
    {
        DirtyIndicator = TEXT(" * [Unsaved inside]");
    }

    return SNew(STableRow<TSharedPtr<FGV2DefinitionTreeNode>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(DisplayText))
            .ColorAndOpacity(FSlateColor(TextColor))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(SubText))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(DirtyIndicator))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.1f)))
        ]
    ];
}

void SGV2DefinitionBrowser::OnSelectionChanged(
    TSharedPtr<FGV2DefinitionTreeNode> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && SelectedItem->NodeType == EGV2DefinitionTreeNodeType::Definition)
    {
        if (Adapter.IsValid() && !SelectedItem->PackageId.IsEmpty() && OnLocatorSelected.IsBound())
        {
            auto Loc = Adapter->GetAuthoringIndex().FindLocator(
                TCHAR_TO_UTF8(*SelectedItem->PackageId),
                TCHAR_TO_UTF8(*SelectedItem->RelativeSource),
                TCHAR_TO_UTF8(*SelectedItem->StableId));
            if (Loc.has_value())
            {
                OnLocatorSelected.Execute(*Loc);
            }
        }
        if (OnDefinitionSelected.IsBound())
        {
            OnDefinitionSelected.Execute(SelectedItem->StableId);
        }
    }
}

TSharedPtr<SWidget> SGV2DefinitionBrowser::OnContextMenuOpening()
{
    auto SelectedItems = TreeView->GetSelectedItems();
    if (SelectedItems.Num() == 0 || !SelectedItems[0].IsValid())
    {
        return nullptr;
    }

    const auto SelectedNode = SelectedItems[0];
    FMenuBuilder MenuBuilder(true, nullptr);

    if (SelectedNode->NodeType == EGV2DefinitionTreeNodeType::Definition)
    {
        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Copy Stable ID")),
            FText::FromString(TEXT("Copy definition Stable ID to clipboard")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleCopyId)));

        MenuBuilder.AddMenuSeparator();

        // CEH-05: Provider inspection submenu if overridden
        if (SelectedNode->bIsOverridden && Adapter.IsValid())
        {
            MenuBuilder.AddSubMenu(
                FText::FromString(FString::Printf(TEXT("Providers (%d)"), SelectedNode->ProviderCount)),
                FText::FromString(TEXT("Inspect all physical providers for this Stable ID")),
                FNewMenuDelegate::CreateLambda([this, SelectedNode](FMenuBuilder& SubMenu) {
                    auto Providers = Adapter->GetProvidersForDefinition(TCHAR_TO_UTF8(*SelectedNode->StableId));
                    for (const auto& Prov : Providers)
                    {
                        FString Label = FString::Printf(TEXT("[%s] %s (%s)"),
                            Prov.bIsWinner ? TEXT("Winner") : TEXT("Shadowed"),
                            UTF8_TO_TCHAR(Prov.PackageId.c_str()),
                            UTF8_TO_TCHAR(Prov.RelativeSource.c_str()));
                        SubMenu.AddMenuEntry(
                            FText::FromString(Label),
                            FText::FromString(TEXT("Open this physical definition provider")),
                            FSlateIcon(),
                            FUIAction(FExecuteAction::CreateLambda([this, Prov]() { HandleSelectProvider(Prov); })));
                    }
                }));
            MenuBuilder.AddMenuSeparator();
        }

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Duplicate...")),
            FText::FromString(TEXT("Duplicate this definition to a new ID")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDuplicate)));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Rename...")),
            FText::FromString(TEXT("Rename definition and update references")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleRename)));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Delete")),
            FText::FromString(TEXT("Delete this definition")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDelete)));
    }
    else
    {
        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Create Definition Here...")),
            FText::FromString(TEXT("Create a new definition under this namespace/kind")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleCreateAction)));
    }

    return MenuBuilder.MakeWidget();
}

void SGV2DefinitionBrowser::HandleCreateAction()
{
    HandleCreate();
}

void SGV2DefinitionBrowser::HandleSelectProvider(const GV2ContentAuthoring::FAuthoringLocator& Locator)
{
    if (OnLocatorSelected.IsBound())
    {
        OnLocatorSelected.Execute(Locator);
    }
    else if (Adapter.IsValid())
    {
        std::vector<FGV2EditorDiagnostic> LoadDiags;
        Adapter->LoadDefinition(Locator, LoadDiags);
        if (OnDefinitionsChanged.IsBound())
        {
            OnDefinitionsChanged.Execute();
        }
    }
}

void SGV2DefinitionBrowser::HandleCopyId()
{
    auto SelectedItems = TreeView->GetSelectedItems();
    if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(*SelectedItems[0]->StableId);
    }
}

FReply SGV2DefinitionBrowser::HandleCreate()
{
    if (!Adapter.IsValid()) return FReply::Handled();

    auto SelectedItems = TreeView->GetSelectedItems();
    FString DefaultNs = TEXT("core");
    FString DefaultKind = TEXT("item");
    if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
    {
        if (SelectedItems[0]->NodeType == EGV2DefinitionTreeNodeType::Namespace)
        {
            DefaultNs = SelectedItems[0]->DisplayName;
        }
        else if (SelectedItems[0]->NodeType == EGV2DefinitionTreeNodeType::Kind)
        {
            DefaultKind = SelectedItems[0]->DisplayName;
            if (SelectedItems[0]->Parent.IsValid())
            {
                DefaultNs = SelectedItems[0]->Parent.Pin()->DisplayName;
            }
        }
    }

    FString InitialId = FString::Printf(TEXT("%s:%s.new_%s"), *DefaultNs, *DefaultKind, *DefaultKind);
    auto OptId = PromptForText(
        FText::FromString(TEXT("New Definition")),
        FText::FromString(TEXT("Enter unique Stable ID (e.g. core:item.potion):")),
        InitialId);

    if (!OptId.IsSet() || OptId.GetValue().IsEmpty()) return FReply::Handled();

    FString NewId = OptId.GetValue();
    FString NsPart, RemPart;
    if (!NewId.Split(TEXT(":"), &NsPart, &RemPart))
    {
        NsPart = DefaultNs;
        RemPart = NewId;
    }
    FString KindPart, PathPart;
    if (!RemPart.Split(TEXT("."), &KindPart, &PathPart))
    {
        KindPart = DefaultKind;
    }

    auto Result = Adapter->CreateDefinition(
        TCHAR_TO_UTF8(*NsPart),
        TCHAR_TO_UTF8(*NewId),
        TCHAR_TO_UTF8(*KindPart));

    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        SetSelectedDefinition(NewId);
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
    return FReply::Handled();
}

void SGV2DefinitionBrowser::HandleDuplicate()
{
    auto SelectedItems = TreeView->GetSelectedItems();
    if (SelectedItems.Num() == 0 || !SelectedItems[0].IsValid() || !Adapter.IsValid()) return;

    FString SourceId = SelectedItems[0]->StableId;
    auto OptTargetId = PromptForText(
        FText::FromString(TEXT("Duplicate Definition")),
        FText::FromString(TEXT("Enter target Stable ID:")),
        SourceId + TEXT("_copy"));

    if (!OptTargetId.IsSet() || OptTargetId.GetValue().IsEmpty()) return;

    auto Result = Adapter->DuplicateDefinition(
        TCHAR_TO_UTF8(*SourceId),
        TCHAR_TO_UTF8(*OptTargetId.GetValue()));

    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        SetSelectedDefinition(OptTargetId.GetValue());
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
}

void SGV2DefinitionBrowser::HandleRename()
{
    auto SelectedItems = TreeView->GetSelectedItems();
    if (SelectedItems.Num() == 0 || !SelectedItems[0].IsValid() || !Adapter.IsValid()) return;

    FString OldId = SelectedItems[0]->StableId;
    auto OptNewId = PromptForText(
        FText::FromString(TEXT("Rename Definition")),
        FText::FromString(TEXT("Enter new Stable ID:")),
        OldId);

    if (!OptNewId.IsSet() || OptNewId.GetValue().IsEmpty() || OptNewId.GetValue() == OldId) return;

    auto Result = Adapter->RenameDefinition(
        TCHAR_TO_UTF8(*OldId),
        TCHAR_TO_UTF8(*OptNewId.GetValue()));

    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        SetSelectedDefinition(OptNewId.GetValue());
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
}

void SGV2DefinitionBrowser::HandleDelete()
{
    auto SelectedItems = TreeView->GetSelectedItems();
    if (SelectedItems.Num() == 0 || !SelectedItems[0].IsValid() || !Adapter.IsValid()) return;

    FString TargetId = SelectedItems[0]->StableId;
    EAppReturnType::Type Choice = FMessageDialog::Open(
        EAppMsgType::OkCancel,
        FText::FromString(FString::Printf(TEXT("Delete definition '%s'? This operation cannot be undone."), *TargetId)));

    if (Choice != EAppReturnType::Ok) return;

    auto Result = Adapter->DeleteDefinition(TCHAR_TO_UTF8(*TargetId));
    ReportOperation(Result);
    if (Result.IsSuccess())
    {
        RefreshList();
        if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
    }
}

void SGV2DefinitionBrowser::ReportOperation(const FGV2EditorAuthoringResult& Result)
{
    if (OnOperationCompleted.IsBound())
    {
        OnOperationCompleted.Execute(Result);
    }
}

} // namespace GV2ContentEditor
#endif
