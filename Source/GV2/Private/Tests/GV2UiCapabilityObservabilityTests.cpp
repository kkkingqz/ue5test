#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2UiCapabilityObservability.h"
#include "UI/GV2UiCapability.h"
#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"
#include "UI/GV2PanelWidgetBase.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCapabilityObservabilityTest,
    "GV2.UI.CapabilityObservabilityHarness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UUserWidget* MakeBoundHost()
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

UUserWidget* MakeUnboundHost()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    // Asset drift simulation: a host whose WidgetTree never bound Label/Bar/Root, as if a
    // Blueprint edit renamed or removed the renderer control a capability still points to.
    UUserWidget* Host = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
    Host->WidgetTree = NewObject<UWidgetTree>(Host);
    return Host;
}
}

bool FGV2UiCapabilityObservabilityTest::RunTest(const FString& Parameters)
{
    // 1. Positive: Boolean/Number/Text all have real UPP-09 consumers with a genuine
    // physical target, so every capability must be provably observable.
    {
        UUserWidget* Host = MakeBoundHost();
        const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
            .AddBoolean(TEXT("enabled_ok"), FName(TEXT("Root")))
            .AddNumber(TEXT("percent_ok"), FName(TEXT("Bar")), 0.0, 1.0)
            .AddText(TEXT("text_ok"), FName(TEXT("Label")))
            .Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);

        TestTrue(TEXT("Boolean/Number/Text capabilities are observable"), bObservable);
        TestEqual(TEXT("No failures reported for genuinely observable capabilities"), Failures.Num(), 0);
    }

    // 2a. Red scenario: consumer has no physical target to write to at all (Integer/String/
    // Key). Prepare/Commit both report success, yet nothing distinguishable happens --
    // exactly the class of lie ADR-0040 Decision 4 exists to catch.
    {
        UUserWidget* Host = MakeBoundHost();
        const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
            .AddInteger(TEXT("int_gap"), FName(TEXT("Label")))
            .AddString(TEXT("str_gap"), FName(TEXT("Label")))
            .AddKey(TEXT("key_gap"), FName(TEXT("Label")))
            .Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);

        TestFalse(TEXT("Consumer-without-physical-effect capabilities are rejected"), bObservable);
        TestEqual(TEXT("All three no-op capabilities are individually reported"), Failures.Num(), 3);
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            TestTrue(
                *FString::Printf(TEXT("Failure for '%s' names not_distinguishable"), *Failure.PropertyName),
                Failure.Reason.Contains(TEXT("core:diagnostic.ui_observability.not_distinguishable")));
        }
    }

    // 2b. Red scenario: renderer target disconnected in the asset (Blueprint drift) --
    // GetWidgetFromName resolves to nullptr for every capability's TargetName.
    {
        UUserWidget* Host = MakeUnboundHost();
        const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
            .AddBoolean(TEXT("enabled_missing"), FName(TEXT("Root")))
            .AddNumber(TEXT("percent_missing"), FName(TEXT("Bar")), 0.0, 1.0)
            .Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);

        TestFalse(TEXT("Disconnected renderer target capabilities are rejected"), bObservable);
        TestEqual(TEXT("Both capabilities with a missing target are reported"), Failures.Num(), 2);
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            TestTrue(
                *FString::Printf(TEXT("Failure for '%s' names prepare_or_commit_failed"), *Failure.PropertyName),
                Failure.Reason.Contains(TEXT("core:diagnostic.ui_observability.prepare_or_commit_failed")));
        }
    }

    // 2c. Red scenario: capability declared without any working implementation --
    // StableId(resource) has no synthesizable probe pair without a live content
    // repository, and Binding's consumer is a genuine no-op today because no widget yet
    // implements IGV2UiBindingTarget (UPP-12+ migrates the first one).
    {
        UUserWidget* Host = MakeBoundHost();
        const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
            .AddImage(TEXT("icon_gap"), FName(TEXT("Label")), TEXT("resource"))
            .AddBinding(TEXT("binding_gap"), FName(TEXT("Root")))
            .Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);

        TestFalse(TEXT("Capabilities declared without a working implementation are rejected"), bObservable);
        TestEqual(TEXT("Both unimplemented capabilities are reported, not skipped"), Failures.Num(), 2);
    }

    return true;
}

#endif
