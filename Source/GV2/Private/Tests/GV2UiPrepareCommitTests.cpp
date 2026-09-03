#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiRollbackBoundary.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PropertyConsumers.h"
#include "GV2ContentCore/UiSchema.h"
#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "CommonTextBlock.h"
#include "UI/GV2PanelWidgetBase.h"
#include "Engine/GameInstance.h"

namespace
{
using namespace GV2ContentCore;

FCompiledUiFieldSpecPtr MakeScalarSpec(
    const EScalarFieldKind Kind,
    const TOptional<double> MinNumber = {},
    const TOptional<double> MaxNumber = {})
{
    auto Spec = std::make_shared<FCompiledUiFieldSpec>();
    Spec->Kind = EUiFieldKind::Scalar;
    FScalarFieldSpec Scalar;
    Scalar.Kind = Kind;
    if (MinNumber.IsSet()) { Scalar.MinimumNumber = MinNumber.GetValue(); }
    if (MaxNumber.IsSet()) { Scalar.MaximumNumber = MaxNumber.GetValue(); }
    Spec->Scalar = MoveTemp(Scalar);
    return Spec;
}

// Prepare validates target presence/type on real renderer controls (proposal §13), so
// a nullptr host cannot exercise a successful Prepare -- this builds a minimal
// UUserWidget (a concrete, non-abstract subclass; UUserWidget itself is Abstract)
// whose WidgetTree carries named children matching the test capability tree
// (Label: text, Bar: percent, Root: enabled).
UUserWidget* MakeTestHostWidget()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UUserWidget* Host = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
    Host->WidgetTree = NewObject<UWidgetTree>(Host);
    UVerticalBox* Root = Host->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    Host->WidgetTree->RootWidget = Root;

    UCommonTextBlock* Label = Host->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Label"));
    Root->AddChildToVerticalBox(Label);
    UProgressBar* Bar = Host->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
    Root->AddChildToVerticalBox(Bar);

    return Host;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiPrepareCommitTest,
    "GV2.UI.PrepareCommitAndFailureInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiPrepareCommitTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // GBF-07: the inventory's expected side is a real C++ mapping, not only
    // source text parsed by the portable gate. Count drives this test so adding
    // an enum boundary requires an executable recovery decision.
    for (uint8 BoundaryIndex = 0;
         BoundaryIndex < static_cast<uint8>(EGV2UiRollbackBoundary::Count);
         ++BoundaryIndex)
    {
        const EGV2UiRollbackBoundary Boundary = static_cast<EGV2UiRollbackBoundary>(BoundaryIndex);
        const EGV2UiRollbackRecovery ExpectedRecovery =
            Boundary == EGV2UiRollbackBoundary::ShellAttach
                ? EGV2UiRollbackRecovery::RestoreStructure
                : EGV2UiRollbackRecovery::ReplayInverse;
        TestEqual(
            FString::Printf(TEXT("Rollback boundary %d has its required recovery"), BoundaryIndex),
            GetUiRollbackBoundaryRecovery(Boundary),
            ExpectedRecovery);
    }

    // Build capabilities
    const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
        .AddText(TEXT("text"), FName(TEXT("Label")))
        .AddNumber(TEXT("percent"), FName(TEXT("Bar")), 0.0, 1.0)
        .AddBoolean(TEXT("enabled"), FName(TEXT("Root")))
        .Build();

    // 1. Prepare Purity Check: Prepare does NOT modify live state
    {
        UUserWidget* Host = MakeTestHostWidget();

        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "text", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });
        Schema.Fields.push_back({ "percent", false, MakeScalarSpec(EScalarFieldKind::Number, 0.0, 1.0) });

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("PreparedTitle"));
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        Fields.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.75));

        const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));

        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const FGV2PreparedUiObject EmptyPrev;

        // Capture physical state before Prepare to prove Prepare does not mutate live state.
        const FText TextBeforePrepare = Cast<UCommonTextBlock>(Host->GetWidgetFromName(TEXT("Label")))->GetText();
        const float PercentBeforePrepare = Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")))->GetPercent();

        // Run Prepare on valid input
        const bool bPrepared = PrepareUiHostProperties(
            Host, Caps, *Candidate, Schema, TEXT("core:schema.ui_field.test.v1"),
            TEXT("screen.test"), EmptyPrev, Plan, Diagnostics);

        TestTrue(TEXT("Prepare succeeds on valid input"), bPrepared);
        // 'enabled' has a capability but no schema field and no prior committed value on this
        // fresh instance, so it is neither applied nor reset (full-field reset only fires for a
        // property the instance previously owned -- see scenario 3 below).
        TestTrue(TEXT("Plan contains 2 mutations (text, percent both present)"), Plan.Num() == 2);
        TestEqual(TEXT("Prepare purity: text unchanged on valid input"),
            Cast<UCommonTextBlock>(Host->GetWidgetFromName(TEXT("Label")))->GetText().ToString(), TextBeforePrepare.ToString());
        TestEqual(TEXT("Prepare purity: percent unchanged on valid input"),
            Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")))->GetPercent(), PercentBeforePrepare);

        // Run Prepare on invalid input (e.g. unknown schema property)
        FCompiledUiFieldSpec BadSchema;
        BadSchema.Kind = EUiFieldKind::Object;
        BadSchema.Fields.push_back({ "bad_prop", false, MakeScalarSpec(EScalarFieldKind::String) });

        FGV2UiHostMutationPlan BadPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> BadDiagnostics;
        const bool bBadPrepared = PrepareUiHostProperties(
            Host, Caps, *Candidate, BadSchema, TEXT("core:schema.ui_field.test.v1"),
            TEXT("screen.test"), EmptyPrev, BadPlan, BadDiagnostics);

        TestTrue(TEXT("Prepare fails on invalid schema"), !bBadPrepared);
        TestTrue(TEXT("BadPlan is empty"), BadPlan.IsEmpty());
        TestEqual(TEXT("Prepare purity: text unchanged on invalid input"),
            Cast<UCommonTextBlock>(Host->GetWidgetFromName(TEXT("Label")))->GetText().ToString(), TextBeforePrepare.ToString());
        TestEqual(TEXT("Prepare purity: percent unchanged on invalid input"),
            Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")))->GetPercent(), PercentBeforePrepare);
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
        NewSchema.Fields.push_back({ "text", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

        FGV2TextViewModel NewText;
        NewText.Text = FText::FromString(TEXT("NewText"));
        TArray<TPair<FString, FGV2PreparedUiValue>> NewFields;
        NewFields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(NewText));
        const TSharedRef<const FGV2PreparedUiObject> NewCandidate = FGV2PreparedUiObject::Create(MoveTemp(NewFields));

        UUserWidget* Host2 = MakeTestHostWidget();
        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Host2, Caps, *NewCandidate, NewSchema, TEXT("core:schema.ui_field.new.v1"),
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

    // 4. Child Prepare failure propagates to parent Prepare failure. This is a distinct
    // code path from schema/capability incompatibility (scenario 1's BadSchema case fails
    // at CheckUiSchemaCapabilityCompatibility, before any consumer ever runs): here the
    // schema is fully compatible with the capability tree, but the target bound to 'text'
    // is a UProgressBar instead of a UCommonTextBlock, so FGV2TextPropertyConsumer::Prepare
    // itself rejects it. UPP-15 gate condition 2 requires this path to be provably load-
    // bearing, not just present -- verified by rollback: silencing the child failure check
    // in PrepareUiHostProperties (GV2UiMutationPlan.cpp) leaves this scenario green.
    {
        UUserWidget* Host = MakeTestHostWidget();
        const FGV2UiCapabilityTree MisboundCaps = FGV2UiCapabilityBuilder()
            .AddText(TEXT("text"), FName(TEXT("Bar"))) // wrong target type on purpose
            .Build();

        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "text", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("ChildFailure"));
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));
        const FGV2PreparedUiObject EmptyPrev;

        const float PercentBeforePrepare = Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")))->GetPercent();

        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Host, MisboundCaps, *Candidate, Schema, TEXT("core:schema.ui_field.child_failure.v1"),
            TEXT("screen.child_failure"), EmptyPrev, Plan, Diagnostics);

        TestFalse(TEXT("Parent Prepare fails when child consumer Prepare fails"), bPrepared);
        TestTrue(TEXT("Plan is empty: partial mutations are not staged past the failure"), Plan.IsEmpty());
        TestTrue(TEXT("Diagnostics name the child Prepare failure, not swallowed"),
            Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_mutation.prepare_failed"));
        TestEqual(TEXT("Prepare purity holds even for a failed child: Bar percent unchanged"),
            Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")))->GetPercent(), PercentBeforePrepare);
    }

    // 5. GBH-10 (ADR-0041): Commit rollback restores an already-committed property when
    // a LATER property in the same host's plan fails, so a reused host is never left
    // with a partially-new revision (REM-02). Mutation order is explicit here (text
    // first, then percent) rather than left to capability-tree iteration order, so this
    // test does not depend on the same ordering the production forward/rollback plan
    // pair relies on structurally (both built from the same FGV2UiCapabilityTree).
    {
        UUserWidget* Host = MakeTestHostWidget();
        UCommonTextBlock* LabelWidget = Cast<UCommonTextBlock>(Host->GetWidgetFromName(TEXT("Label")));
        UProgressBar* BarWidget = Cast<UProgressBar>(Host->GetWidgetFromName(TEXT("Bar")));

        FGV2TextViewModel OldText;
        OldText.Text = FText::FromString(TEXT("OldRollbackText"));

        FGV2UiHostMutationPlan ForwardPlan;
        FGV2UiHostMutationPlan RollbackPlan;
        FString PrepError;

        // Mutation 0: text, new value (will already be committed when percent fails).
        {
            FGV2TextViewModel NewText;
            NewText.Text = FText::FromString(TEXT("NewRollbackText"));
            const FGV2UiPropertyCapability TextCap;

            TSharedPtr<IGV2PropertyConsumer> ForwardConsumer = MakeShared<FGV2TextPropertyConsumer>();
            TestTrue(TEXT("GBH-10: forward text consumer prepares"),
                ForwardConsumer->Prepare(FGV2PreparedUiValue::MakeText(NewText), TextCap, LabelWidget, PrepError));
            FGV2UiPropertyMutation ForwardMut;
            ForwardMut.PropertyName = TEXT("text");
            ForwardMut.PropertyPath = TEXT("screen.item.text");
            ForwardMut.Kind = EGV2PreparedUiValueKind::Text;
            ForwardMut.Consumer = ForwardConsumer;
            ForwardMut.TargetWidget = LabelWidget;
            ForwardPlan.AddMutation(ForwardMut);

            TSharedPtr<IGV2PropertyConsumer> RollbackConsumer = MakeShared<FGV2TextPropertyConsumer>();
            TestTrue(TEXT("GBH-10: rollback text consumer prepares old value"),
                RollbackConsumer->Prepare(FGV2PreparedUiValue::MakeText(OldText), TextCap, LabelWidget, PrepError));
            FGV2UiPropertyMutation RollbackMut = ForwardMut;
            RollbackMut.Consumer = RollbackConsumer;
            RollbackPlan.AddMutation(RollbackMut);
        }

        // Mutation 1: percent, new value (this is where the injected failure hits).
        {
            FGV2UiPropertyCapability PercentCap;
            PercentCap.NumberMin = 0.0;
            PercentCap.NumberMax = 1.0;

            TSharedPtr<IGV2PropertyConsumer> ForwardConsumer = MakeShared<FGV2NumberPropertyConsumer>();
            TestTrue(TEXT("GBH-10: forward percent consumer prepares"),
                ForwardConsumer->Prepare(FGV2PreparedUiValue::MakeNumber(0.75), PercentCap, BarWidget, PrepError));
            FGV2UiPropertyMutation ForwardMut;
            ForwardMut.PropertyName = TEXT("percent");
            ForwardMut.PropertyPath = TEXT("screen.item.percent");
            ForwardMut.Kind = EGV2PreparedUiValueKind::Number;
            ForwardMut.Consumer = ForwardConsumer;
            ForwardMut.TargetWidget = BarWidget;
            ForwardPlan.AddMutation(ForwardMut);

            TSharedPtr<IGV2PropertyConsumer> RollbackConsumer = MakeShared<FGV2NumberPropertyConsumer>();
            TestTrue(TEXT("GBH-10: rollback percent consumer prepares old value"),
                RollbackConsumer->Prepare(FGV2PreparedUiValue::MakeNumber(0.25), PercentCap, BarWidget, PrepError));
            FGV2UiPropertyMutation RollbackMut = ForwardMut;
            RollbackMut.Consumer = RollbackConsumer;
            RollbackPlan.AddMutation(RollbackMut);
        }

        // Seed the host's pre-transaction physical state by committing RollbackPlan once,
        // up front -- the same consumers get reused below to actually perform the
        // rollback, which is simply committing them again with the same cached value.
        FString SeedFailedPath, SeedError;
        TestTrue(TEXT("GBH-10: seeding pre-transaction state via rollback plan succeeds"),
            CommitUiHostProperties(Cast<UUserWidget>(Host), RollbackPlan, SeedFailedPath, SeedError));
        TestEqual(TEXT("GBH-10: seeded text reads back as OLD value"),
            LabelWidget->GetText().ToString(), OldText.Text.ToString());
        TestEqual(TEXT("GBH-10: seeded percent reads back as 0.25"), BarWidget->GetPercent(), 0.25f);

        // Inject failure on percent (the SECOND mutation) -- text has already committed
        // to its NEW value by the time percent's Commit is attempted.
        auto Injector = [](const FString& InPropertyPath) -> bool
        {
            return InPropertyPath == TEXT("screen.item.percent");
        };

        FString FailedPath, Error;
        const bool bCommitResult = CommitUiHostProperties(
            Cast<UUserWidget>(Host), ForwardPlan, FailedPath, Error, Injector, &RollbackPlan);

        TestFalse(TEXT("GBH-10: Commit fails when injected on the second property"), bCommitResult);
        TestEqual(TEXT("GBH-10: failed property path names percent"), FailedPath, TEXT("screen.item.percent"));
        TestEqual(TEXT("GBH-10: text widget restored to OLD value after rollback, not left on NEW value"),
            LabelWidget->GetText().ToString(), OldText.Text.ToString());
        TestEqual(TEXT("GBH-10: percent widget stays at its pre-transaction value (never committed)"),
            BarWidget->GetPercent(), 0.25f);
    }

    // 6. GBF-04: the named inverse-required kind set is the enumerator for the
    // rollback-pair gate. For every kind that can directly mutate a widget, deleting
    // its inverse makes validation fail before Commit can touch physical state.
    {
        const TConstArrayView<EGV2PreparedUiValueKind> InverseKinds = GetUiMutationKindsRequiringInverse();
        TestTrue(TEXT("GBF-04: inverse-required mutation kind set is not empty"), !InverseKinds.IsEmpty());
        for (const EGV2PreparedUiValueKind Kind : InverseKinds)
        {
            FGV2UiPropertyMutation Mutation;
            Mutation.PropertyName = FString::Printf(TEXT("kind_%d"), static_cast<int32>(Kind));
            Mutation.PropertyPath = FString::Printf(TEXT("screen.rollback.%s"), *Mutation.PropertyName);
            Mutation.Kind = Kind;

            FGV2UiHostMutationPlan ForwardPlan;
            ForwardPlan.AddMutation(Mutation);
            FGV2UiHostMutationPlan CompleteInverse;
            CompleteInverse.AddMutation(Mutation);
            FString CompleteError;
            TestTrue(*FString::Printf(TEXT("GBF-04: kind %d accepts its paired inverse"), static_cast<int32>(Kind)),
                ValidateUiRollbackPlan(ForwardPlan, CompleteInverse, CompleteError));

            FGV2UiHostMutationPlan MissingInverse;
            FString MissingError;
            TestFalse(*FString::Printf(TEXT("GBF-04: kind %d rejects a removed inverse mutation"), static_cast<int32>(Kind)),
                ValidateUiRollbackPlan(ForwardPlan, MissingInverse, MissingError));
            TestTrue(*FString::Printf(TEXT("GBF-04: kind %d emits typed inverse diagnostic"), static_cast<int32>(Kind)),
                MissingError.Contains(TEXT("core:diagnostic.ui_rollback.plan_mismatch")));
        }
    }

    return true;
}

#endif
