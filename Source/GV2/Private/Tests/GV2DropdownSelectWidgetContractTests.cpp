#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
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

    FGV2DropdownSelectViewModel ValidModel;
    ValidModel.Binding = FGV2UiBindingHandle::Create(TEXT("core:command.test"));
    ValidModel.Placeholder.Text = FText::FromString(TEXT("Select item..."));
    ValidModel.Placeholder.StyleToken = TEXT("default");

    FGV2DropdownOptionViewModel Opt1;
    Opt1.Key = TEXT("option_a");
    Opt1.Text.Text = FText::FromString(TEXT("Option A"));
    Opt1.Text.StyleToken = TEXT("default");

    FGV2DropdownOptionViewModel Opt2;
    Opt2.Key = TEXT("option_b");
    Opt2.Text.Text = FText::FromString(TEXT("Option B"));
    Opt2.Text.StyleToken = TEXT("default");

    ValidModel.Options.Add(Opt1);
    ValidModel.Options.Add(Opt2);

    if (Dropdown != nullptr)
    {
        TestTrue(TEXT("CanApplyDropdownModel accepts valid model"), Dropdown->CanApplyDropdownModel(ValidModel));
        TestTrue(TEXT("ApplyDropdownModel succeeds on valid model"), Dropdown->ApplyDropdownModel(ValidModel));

        // 2. Negative Test: Option named "dropdown_header" must be rejected by CanApplyDropdownModel
        {
            FGV2DropdownSelectViewModel ModelWithReservedKey = ValidModel;
            FGV2DropdownOptionViewModel BadOpt;
            BadOpt.Key = TEXT("dropdown_header");
            BadOpt.Text.Text = FText::FromString(TEXT("Reserved"));
            BadOpt.Text.StyleToken = TEXT("default");
            ModelWithReservedKey.Options.Add(BadOpt);

            const bool bCanApplyReserved = Dropdown->CanApplyDropdownModel(ModelWithReservedKey);
            TestFalse(TEXT("CanApplyDropdownModel must reject option with reserved key 'dropdown_header'"), bCanApplyReserved);
        }

        // 3. Negative Test: Option with duplicate key must be rejected
        {
            FGV2DropdownSelectViewModel ModelWithDupKey = ValidModel;
            FGV2DropdownOptionViewModel DupOpt;
            DupOpt.Key = TEXT("option_a");
            DupOpt.Text.Text = FText::FromString(TEXT("Option A Dup"));
            DupOpt.Text.StyleToken = TEXT("default");
            ModelWithDupKey.Options.Add(DupOpt);

            const bool bCanApplyDup = Dropdown->CanApplyDropdownModel(ModelWithDupKey);
            TestFalse(TEXT("CanApplyDropdownModel must reject duplicate option keys"), bCanApplyDup);
        }

        // 4. Negative Test: Multiple selected options must be rejected
        {
            FGV2DropdownSelectViewModel ModelMultiSelect = ValidModel;
            ModelMultiSelect.Options[0].bSelected = true;
            ModelMultiSelect.Options[1].bSelected = true;

            const bool bCanApplyMulti = Dropdown->CanApplyDropdownModel(ModelMultiSelect);
            TestFalse(TEXT("CanApplyDropdownModel must reject multiple selected options"), bCanApplyMulti);
        }
    }

    // 5. Model Equality Tests (TWH-07)
    {
        FGV2DropdownSelectViewModel ModelCopy = ValidModel;
        TestTrue(TEXT("Identical dropdown models must compare equal"), ModelCopy == ValidModel);

        FGV2DropdownSelectViewModel ChangedSelectionModel = ValidModel;
        ChangedSelectionModel.Options[0].bSelected = true;
        TestFalse(TEXT("Model with changed selection must not compare equal"), ChangedSelectionModel == ValidModel);

        FGV2DropdownSelectViewModel ChangedOptionsModel = ValidModel;
        FGV2DropdownOptionViewModel ExtraOpt;
        ExtraOpt.Key = TEXT("option_c");
        ExtraOpt.Text.Text = FText::FromString(TEXT("Option C"));
        ExtraOpt.Text.StyleToken = TEXT("default");
        ChangedOptionsModel.Options.Add(ExtraOpt);
        TestFalse(TEXT("Model with extra option must not compare equal"), ChangedOptionsModel == ValidModel);
    }

    GEngine->DestroyWorldContext(TestWorld);
    TestWorld->DestroyWorld(false);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
