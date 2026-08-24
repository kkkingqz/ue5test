#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PropertyConsumers.h"
#include "GV2ContentCore/UiSchema.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiPrepareCommitTest,
    "GV2.UI.PrepareCommitAndFailureInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiPrepareCommitTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // Build capabilities
    const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
        .AddText(TEXT("text"), FName(TEXT("Label")))
        .AddNumber(TEXT("percent"), FName(TEXT("Bar")), 0.0, 1.0)
        .AddBoolean(TEXT("enabled"), FName(TEXT("Root")))
        .Build();

    // 1. Prepare Purity Check: Prepare does NOT modify live state
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.ObjectFields.emplace_back("text", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text));
        Schema.ObjectFields.emplace_back("percent", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Number));

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("PreparedTitle"));
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        Fields.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.75));

        const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));

        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const FGV2PreparedUiObject EmptyPrev;

        // Run Prepare on valid input
        const bool bPrepared = PrepareUiHostProperties(
            nullptr, Caps, *Candidate, Schema, TEXT("core:schema.ui_field.test.v1"),
            TEXT("screen.test"), EmptyPrev, Plan, Diagnostics);

        TestTrue(TEXT("Prepare succeeds on valid input"), bPrepared);
        TestTrue(TEXT("Plan contains 3 mutations (2 present, 1 reset for enabled)"), Plan.Num() == 3);

        // Run Prepare on invalid input (e.g. unknown schema property)
        FCompiledUiFieldSpec BadSchema;
        BadSchema.Kind = EUiFieldKind::Object;
        BadSchema.ObjectFields.emplace_back("bad_prop", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::String));

        FGV2UiHostMutationPlan BadPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> BadDiagnostics;
        const bool bBadPrepared = PrepareUiHostProperties(
            nullptr, Caps, *Candidate, BadSchema, TEXT("core:schema.ui_field.test.v1"),
            TEXT("screen.test"), EmptyPrev, BadPlan, BadDiagnostics);

        TestTrue(TEXT("Prepare fails on invalid schema"), !bBadPrepared);
        TestTrue(TEXT("BadPlan is empty"), BadPlan.IsEmpty());
    }

    // 2. Commit Failure Injection: verifies failure reporting with property_path
    {
        FGV2UiHostMutationPlan Plan;
        FGV2UiPropertyMutation Mut1;
        Mut1.PropertyName = TEXT("text");
        Mut1.PropertyPath = TEXT("screen.widget.text");
        Mut1.Kind = EGV2PreparedUiValueKind::Text;
        Mut1.bIsReset = false;
        Plan.AddMutation(Mut1);

        FGV2UiPropertyMutation Mut2;
        Mut2.PropertyName = TEXT("percent");
        Mut2.PropertyPath = TEXT("screen.widget.percent");
        Mut2.Kind = EGV2PreparedUiValueKind::Number;
        Mut2.bIsReset = false;
        Plan.AddMutation(Mut2);

        FString FailedPath;
        FString Error;

        // Inject failure on percent property
        auto Injector = [](const FString& PropertyPath) -> bool
        {
            return PropertyPath == TEXT("screen.widget.percent");
        };

        const bool bCommitResult = CommitUiHostProperties(
            nullptr, Plan, FailedPath, Error, Injector);

        TestTrue(TEXT("Commit fails when failure is injected"), !bCommitResult);
        TestEqual(TEXT("Failed property path matches injected property"),
            FailedPath, TEXT("screen.widget.percent"));
        TestTrue(TEXT("Error contains diagnostic code"),
            Error.Contains(TEXT("core:diagnostic.ui_mutation.commit_failed_injected")));
    }

    // 3. Full-field semantics and reset on reused instances
    {
        // Reused instance previously committed { text, percent, enabled }
        TArray<TPair<FString, FGV2PreparedUiValue>> PrevFields;
        PrevFields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeString(TEXT("Old")));
        PrevFields.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.5));
        PrevFields.Emplace(TEXT("enabled"), FGV2PreparedUiValue::MakeBoolean(true));
        const TSharedRef<const FGV2PreparedUiObject> PrevCommitted = FGV2PreparedUiObject::Create(MoveTemp(PrevFields));

        // New schema only has { text }
        FCompiledUiFieldSpec NewSchema;
        NewSchema.Kind = EUiFieldKind::Object;
        NewSchema.ObjectFields.emplace_back("text", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text));

        FGV2TextViewModel NewText;
        NewText.Text = FText::FromString(TEXT("NewText"));
        TArray<TPair<FString, FGV2PreparedUiValue>> NewFields;
        NewFields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(NewText));
        const TSharedRef<const FGV2PreparedUiObject> NewCandidate = FGV2PreparedUiObject::Create(MoveTemp(NewFields));

        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            nullptr, Caps, *NewCandidate, NewSchema, TEXT("core:schema.ui_field.new.v1"),
            TEXT("screen.item"), *PrevCommitted, Plan, Diagnostics);

        TestTrue(TEXT("Prepare succeeds for reused instance"), bPrepared);

        // Plan must contain:
        // 1. Apply 'text'
        // 2. Reset 'percent' (was in PrevCommitted, not in NewSchema)
        // 3. Reset 'enabled' (was in PrevCommitted, not in NewSchema)
        int32 ApplyCount = 0;
        int32 ResetCount = 0;
        for (const auto& Mut : Plan.GetMutations())
        {
            if (Mut.bIsReset)
            {
                ResetCount++;
            }
            else
            {
                ApplyCount++;
            }
        }

        TestEqual(TEXT("1 applied mutation"), ApplyCount, 1);
        TestEqual(TEXT("2 reset mutations for disowned properties"), ResetCount, 2);
    }

    return true;
}

#endif
