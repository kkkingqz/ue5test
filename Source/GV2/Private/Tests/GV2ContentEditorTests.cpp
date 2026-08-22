#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentEditor/Testing/EditorAdapterConformance.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentEditorTest,
    "GV2.Editor.ContentEditor.AdapterConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentEditorTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentEditor::Testing::RunEditorAdapterConformance();
    TestTrue(
        FString::Printf(TEXT("EditorAdapter conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DefinitionBrowserTreeTest,
    "GV2.Editor.ContentEditor.DefinitionBrowserTree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DefinitionBrowserTreeTest::RunTest(const FString& Parameters)
{
    auto Adapter = MakeShared<GV2ContentEditor::FGV2EditorAdapter>();
    FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));

    std::vector<GV2ContentEditor::FGV2EditorDiagnostic> InitDiags;
    bool bInit = Adapter->Initialize(TCHAR_TO_UTF8(*GameDataDir), InitDiags);
    TestTrue(TEXT("Adapter initialized successfully"), bInit);

    TSharedRef<GV2ContentEditor::SGV2DefinitionBrowser> Browser =
        SNew(GV2ContentEditor::SGV2DefinitionBrowser, Adapter);

    const auto& RootNodes = Browser->GetRootNodes();
    TestTrue(TEXT("Root nodes generated"), RootNodes.Num() > 0);

    // Verify Namespace node
    bool bFoundRhNs = false;
    for (const auto& Root : RootNodes)
    {
        if (Root.IsValid() && Root->NodeType == GV2ContentEditor::EGV2DefinitionTreeNodeType::Namespace && Root->DisplayName == TEXT("rh"))
        {
            bFoundRhNs = true;
            TestTrue(TEXT("RH namespace has kinds"), Root->Children.Num() > 0);
            for (const auto& KindChild : Root->Children)
            {
                if (KindChild.IsValid())
                {
                    TestEqual(TEXT("Kind node type"), KindChild->NodeType, GV2ContentEditor::EGV2DefinitionTreeNodeType::Kind);
                }
            }
            break;
        }
    }
    TestTrue(TEXT("Found rh namespace node"), bFoundRhNs);

    // Test selection and dirty state
    std::vector<GV2ContentEditor::FGV2EditorDiagnostic> LoadDiags;
    auto Loaded = Adapter->LoadDefinition("core:item.iron_sword", LoadDiags);
    if (!Loaded.has_value())
    {
        // Try any available definition
        auto Available = Adapter->ListDefinitions();
        if (Available.size() > 0)
        {
            Loaded = Adapter->LoadDefinition(Available[0].Id, LoadDiags);
        }
    }

    if (Loaded.has_value())
    {
        Adapter->SetCurrentFieldValue("/data/name", GV2ContentCore::FValue::MakeString("Modified Name"));
        TestTrue(TEXT("Adapter is dirty"), Adapter->IsDirty());

        Browser->RefreshList();
        // Check dirty flags propagated to tree
        bool bFoundDirtyNode = false;
        auto CheckDirty = [&](auto& Self, const TSharedPtr<GV2ContentEditor::FGV2DefinitionTreeNode>& Node) -> void {
            if (!Node.IsValid()) return;
            if (Node->bIsDirty || Node->bHasDirtyDescendant)
            {
                bFoundDirtyNode = true;
            }
            for (const auto& C : Node->Children)
            {
                Self(Self, C);
            }
        };

        for (const auto& Root : Browser->GetRootNodes())
        {
            CheckDirty(CheckDirty, Root);
        }
        TestTrue(TEXT("Dirty marker found on tree node or ancestor"), bFoundDirtyNode);

        Adapter->DiscardCurrentChanges();
        Browser->RefreshList();
    }

    return true;
}

#endif

