#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DropdownSelectWidgetContractTests,
    "GV2.Runtime.UIKit.DropdownSelectWidgetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DropdownSelectWidgetContractTests::RunTest(const FString& Parameters)
{
    // 1. Setup a dummy Dropdown widget
    UGV2DropdownSelectWidgetBase* Dropdown = NewObject<UGV2DropdownSelectWidgetBase>();
    TestNotNull(TEXT("Dropdown widget created"), Dropdown);

    FGV2DropdownSelectViewModel ValidModel;
    ValidModel.Binding = FGV2UiBindingHandle::Create(TEXT("core:command.test"));
    ValidModel.Placeholder.Text = FText::FromString(TEXT("Select item..."));
    ValidModel.Placeholder.StyleToken = TEXT("text_regular");

    FGV2DropdownOptionViewModel Opt1;
    Opt1.Key = TEXT("option_a");
    Opt1.Text.Text = FText::FromString(TEXT("Option A"));
    Opt1.Text.StyleToken = TEXT("text_regular");

    FGV2DropdownOptionViewModel Opt2;
    Opt2.Key = TEXT("option_b");
    Opt2.Text.Text = FText::FromString(TEXT("Option B"));
    Opt2.Text.StyleToken = TEXT("text_regular");

    ValidModel.Options.Add(Opt1);
    ValidModel.Options.Add(Opt2);

    // 2. Negative Test: Option named "dropdown_header" must be rejected by CanApplyDropdownModel
    {
        FGV2DropdownSelectViewModel ModelWithReservedKey = ValidModel;
        FGV2DropdownOptionViewModel BadOpt;
        BadOpt.Key = TEXT("dropdown_header");
        BadOpt.Text.Text = FText::FromString(TEXT("Reserved"));
        BadOpt.Text.StyleToken = TEXT("text_regular");
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
        DupOpt.Text.StyleToken = TEXT("text_regular");
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
        ExtraOpt.Text.StyleToken = TEXT("text_regular");
        ChangedOptionsModel.Options.Add(ExtraOpt);
        TestFalse(TEXT("Model with extra option must not compare equal"), ChangedOptionsModel == ValidModel);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
