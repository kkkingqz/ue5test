#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2UiTheme.h"

// FGV2KeyedCollection carries a ConfigureSlot callback whose own comment states the rule:
// "rebuild must reapply caller-owned layout policy to that new slot in the same physical
// commit; otherwise an Overlay silently falls back to Left/Top even when the widget was Fill
// before ClearChildren." That callback has no caller anywhere in the repository, and the
// production collection commit (FPreparedKeyedCollectionOperation in PresentationApplyFacade)
// does a bare ClearChildren()/AddChild() loop that does not go through FGV2KeyedCollection at
// all.
//
// This test measures the consequence on the one collection that has a slot policy today: the
// Theme's ButtonListItemPadding, applied by UGV2ButtonListWidgetBase::ApplyItemPaddingStyleValue
// to the container's EXISTING slots. ClearChildren destroys those slots.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CollectionItemSlotPolicyTests,
    "GV2.Runtime.UIKit.CollectionItemSlotPolicySurvivesRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CollectionItemSlotPolicyTests::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = GV2PresentationTestFixtures::LoadTheme();
    TestNotNull(TEXT("Configured theme resolves"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("TestWorld created"), TestWorld);
    if (TestWorld == nullptr)
    {
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(TestWorld);

    // Fully synthetic host: the class under test carries the policy, and nothing here needs
    // a game package's Widget Blueprint to show whether a slot keeps its padding. ADR-0046
    // forbids a contract test naming game content, and SetButtonContainer exists precisely
    // so the host can be assembled without one.
    UGV2ButtonListWidgetBase* List = NewObject<UGV2ButtonListWidgetBase>(TestWorld);
    TestNotNull(TEXT("Button list created"), List);
    UVerticalBox* SyntheticContainer = NewObject<UVerticalBox>(TestWorld);
    TestNotNull(TEXT("Synthetic container created"), SyntheticContainer);
    if (List == nullptr || SyntheticContainer == nullptr)
    {
        return false;
    }
    List->SetButtonContainer(SyntheticContainer);

    UPanelWidget* const Container = List->GetPreparedCollectionPanel();
    TestEqual(TEXT("Button list exposes the container it was given"), Container, static_cast<UPanelWidget*>(SyntheticContainer));
    if (Container == nullptr)
    {
        return false;
    }

    const auto MakeEntry = [&](const FName Key) -> GV2PresentationApply::FPreparedKeyedCollectionEntry
    {
        // The item only has to be a widget the panel can hold: this test measures the SLOT,
        // not what sits in it.
        GV2PresentationApply::FPreparedKeyedCollectionEntry Entry;
        Entry.Key = Key;
        Entry.Widget = NewObject<UVerticalBox>(TestWorld);
        return Entry;
    };

    // Production commit shape: one FPreparedKeyedCollectionOperation through the real apply
    // facade, not a hand-rolled imitation of it.
    const auto CommitEntries = [&](const TArray<FName>& Keys)
    {
        GV2PresentationApply::FPreparedKeyedCollectionOperation Operation;
        Operation.TargetWidget = List;
        for (const FName& Key : Keys)
        {
            Operation.OrderedEntries.Add(MakeEntry(Key));
        }
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddKeyedCollectionOperation(MoveTemp(Operation));
        FGV2PresentationApplyResult Result;
        TestTrue(
            *FString::Printf(TEXT("Keyed collection commit applies (%d entries)"), Keys.Num()),
            FGV2PresentationApply::Apply(Transaction, Result));
    };

    const auto SlotPaddings = [&]() -> TArray<FMargin>
    {
        TArray<FMargin> Paddings;
        for (UPanelSlot* PanelSlot : Container->GetSlots())
        {
            if (const UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(PanelSlot))
            {
                Paddings.Add(Slot->GetPadding());
            }
        }
        return Paddings;
    };

    // 1. First commit, then the central style that owns item padding -- the order the
    //    preparer and the consumer produce for a freshly materialized collection.
    CommitEntries({FName(TEXT("alpha")), FName(TEXT("beta"))});
    List->ApplyItemPaddingStyleValue(Theme->ButtonListItemPadding);

    const TArray<FMargin> AfterStyle = SlotPaddings();
    TestEqual(TEXT("Both item slots exist after the first commit"), AfterStyle.Num(), 2);
    for (int32 Index = 0; Index < AfterStyle.Num(); ++Index)
    {
        TestEqual(
            *FString::Printf(TEXT("Item slot %d carries the theme's item padding"), Index),
            AfterStyle[Index],
            Theme->ButtonListItemPadding);
    }

    // 2. A later revision rebuilds the same collection. Nothing about the theme changed, so
    //    the item padding must still be what the theme says -- the slot policy is a property
    //    of the host, not of one particular set of child widgets.
    CommitEntries({FName(TEXT("alpha")), FName(TEXT("beta")), FName(TEXT("gamma"))});

    const TArray<FMargin> AfterRebuild = SlotPaddings();
    TestEqual(TEXT("All three item slots exist after the rebuild"), AfterRebuild.Num(), 3);

    AddInfo(FString::Printf(
        TEXT("Theme item padding (%g,%g,%g,%g); first slot after rebuild (%g,%g,%g,%g)"),
        Theme->ButtonListItemPadding.Left, Theme->ButtonListItemPadding.Top,
        Theme->ButtonListItemPadding.Right, Theme->ButtonListItemPadding.Bottom,
        AfterRebuild.IsEmpty() ? 0.0f : AfterRebuild[0].Left,
        AfterRebuild.IsEmpty() ? 0.0f : AfterRebuild[0].Top,
        AfterRebuild.IsEmpty() ? 0.0f : AfterRebuild[0].Right,
        AfterRebuild.IsEmpty() ? 0.0f : AfterRebuild[0].Bottom));

    for (int32 Index = 0; Index < AfterRebuild.Num(); ++Index)
    {
        TestEqual(
            *FString::Printf(
                TEXT("Item slot %d still carries the theme's item padding after a rebuild"), Index),
            AfterRebuild[Index],
            Theme->ButtonListItemPadding);
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif
