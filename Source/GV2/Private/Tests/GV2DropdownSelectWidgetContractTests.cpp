#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PropertyConsumers.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DropdownSelectWidgetContractTests,
    "GV2.Runtime.UIKit.DropdownSelectWidgetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DropdownSelectWidgetContractTests::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("TestWorld created"), TestWorld);
    if (TestWorld == nullptr)
    {
        return false;
    }

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(TestWorld);

    UClass* DropdownClass = LoadClass<UGV2DropdownSelectWidgetBase>(
        nullptr, TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"));
    TestNotNull(TEXT("WBP_DropdownSelect class loaded"), DropdownClass);

    UGV2DropdownSelectWidgetBase* Dropdown = DropdownClass != nullptr
        ? CreateWidget<UGV2DropdownSelectWidgetBase>(TestWorld, DropdownClass)
        : NewObject<UGV2DropdownSelectWidgetBase>(TestWorld);
    TestNotNull(TEXT("Dropdown widget created"), Dropdown);

    if (Dropdown != nullptr)
    {
        // 1. Capability check
        FGV2UiCapabilityBuilder Builder;
        Dropdown->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree Tree = Builder.Build();

        TestNotNull(TEXT("Exposes placeholder text capability"), Tree.FindProperty(TEXT("placeholder")));
        TestNotNull(TEXT("Exposes selected_key key capability"), Tree.FindProperty(TEXT("selected_key")));
        TestNotNull(TEXT("Exposes items keyed collection capability"), Tree.FindProperty(TEXT("items")));
        TestNotNull(TEXT("Exposes binding capability"), Tree.FindProperty(TEXT("binding")));
        TestNotNull(TEXT("Exposes is_open boolean capability"), Tree.FindProperty(TEXT("is_open")));

        // 2. Direct property host interface check
        FGV2TextViewModel PlaceholderText;
        PlaceholderText.Text = FText::FromString(TEXT("Select item..."));
        PlaceholderText.StyleToken = TEXT("default");
        Dropdown->ApplyPlaceholderText(PlaceholderText);

        const FGV2UiBindingHandle TestBinding = FGV2UiBindingHandle::Create(TEXT("core:command.test"));
        Dropdown->SetBindingHandle(TestBinding);
        TestEqual(TEXT("Binding handle matches"), Dropdown->GetBindingHandle(), TestBinding);

        // 3. Populate options via KeyedCollection consumer
        const FGV2UiPropertyCapability* ItemsCap = Tree.FindProperty(TEXT("items"));
        TestNotNull(TEXT("ItemsCap found"), ItemsCap);

        if (ItemsCap != nullptr && Dropdown->GetOptionsScrollBox() != nullptr)
        {
            TSharedPtr<IGV2PropertyConsumer> CollConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);
            TestNotNull(TEXT("Collection consumer created"), CollConsumer.Get());

            FGV2TextViewModel Opt1Text;
            Opt1Text.Text = FText::FromString(TEXT("Option A"));
            Opt1Text.StyleToken = TEXT("default");

            FGV2TextViewModel Opt2Text;
            Opt2Text.Text = FText::FromString(TEXT("Option B"));
            Opt2Text.StyleToken = TEXT("default");

            TMap<FString, FGV2PreparedUiValue> Item1Map;
            Item1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("option_a")));
            Item1Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Opt1Text));

            TMap<FString, FGV2PreparedUiValue> Item2Map;
            Item2Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("option_b")));
            Item2Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Opt2Text));

            TArray<FGV2PreparedUiValue> Elements;
            Elements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Item1Map)));
            Elements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Item2Map)));

            FString PrepErr, CommitErr;
            bool bPrep = CollConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(Elements)),
                *ItemsCap,
                Dropdown->GetOptionsScrollBox(),
                PrepErr);
            TestTrue(TEXT("Collection consumer Prepare succeeds"), bPrep);

            bool bCommit = CollConsumer->Commit(Dropdown->GetOptionsScrollBox(), CommitErr);
            TestTrue(TEXT("Collection consumer Commit succeeds"), bCommit);

            // Set selection to option_a
            Dropdown->SetSelectedKey(TEXT("option_a"));
            TestEqual(TEXT("Selected key is option_a"), Dropdown->GetSelectedKey(), FName(TEXT("option_a")));
            if (Dropdown->GetHeaderButton() != nullptr)
            {
                TestEqual(
                    TEXT("Header button displays selected option label"),
                    Dropdown->GetHeaderButton()->GetTextViewModel().Text.ToString(),
                    TEXT("Option A"));
            }

            // Change selection to option_b
            Dropdown->SetSelectedKey(TEXT("option_b"));
            if (Dropdown->GetHeaderButton() != nullptr)
            {
                TestEqual(
                    TEXT("Header button updates to Option B"),
                    Dropdown->GetHeaderButton()->GetTextViewModel().Text.ToString(),
                    TEXT("Option B"));
            }

            // Test popup open / close
            Dropdown->SetDropdownOpen(true);
            TestTrue(TEXT("Dropdown is open"), Dropdown->IsDropdownOpen());
            Dropdown->SetDropdownOpen(false);
            TestFalse(TEXT("Dropdown is closed"), Dropdown->IsDropdownOpen());
        }
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
