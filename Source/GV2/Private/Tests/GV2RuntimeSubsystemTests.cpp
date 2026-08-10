#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/GV2RuntimeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2TestRuntimeRoundTrip,
    "GV2.Runtime.TestBackend.ButtonRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2TestRuntimeRoundTrip::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGV2RuntimeSubsystem* Runtime = NewObject<UGV2RuntimeSubsystem>(GameInstance);
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Runtime->Initialize(Collection);

    Runtime->StartTestSession();

    FGV2TestButtonSpec ButtonSpec;
    ButtonSpec.Text = FText::FromString(TEXT("First option"));
    ButtonSpec.TestAction = TEXT("core:command.test.first_option");

    const FGV2TestScreenViewModel ScreenModel = Runtime->CreateTestScreenModel(
        FText::FromString(TEXT("Test description")),
        {ButtonSpec});

    TestEqual(TEXT("One button is projected"), ScreenModel.Buttons.Num(), 1);
    if (ScreenModel.Buttons.Num() != 1)
    {
        return false;
    }

    const FGV2UiBindingHandle FirstBinding = ScreenModel.Buttons[0].Binding;
    TestTrue(TEXT("Projected binding is opaque and valid"), FirstBinding.IsValid());
    TestEqual(
        TEXT("Current binding is accepted"),
        Runtime->SubmitUiInteraction(FirstBinding, {}),
        EGV2SubmitUiInteractionResult::Accepted);

    FGV2UiControlValue DuplicateA;
    DuplicateA.Name = TEXT("value");
    FGV2UiControlValue DuplicateB;
    DuplicateB.Name = TEXT("value");
    TestEqual(
        TEXT("Duplicate control fields are rejected"),
        Runtime->SubmitUiInteraction(FirstBinding, {DuplicateA, DuplicateB}),
        EGV2SubmitUiInteractionResult::InvalidInputValues);

    Runtime->StartTestSession();
    TestEqual(
        TEXT("Previous generation binding is stale"),
        Runtime->SubmitUiInteraction(FirstBinding, {}),
        EGV2SubmitUiInteractionResult::StaleBindingHandle);

    Runtime->EndTestSession();
    TestEqual(
        TEXT("Input is gated outside Ready"),
        Runtime->SubmitUiInteraction(FGV2UiBindingHandle(), {}),
        EGV2SubmitUiInteractionResult::RuntimeNotReady);

    Runtime->Deinitialize();

    return true;
}

#endif
