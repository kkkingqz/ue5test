#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2UiCapabilityObservability.h"
#include "UI/GV2UiCapability.h"
#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
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

    // 3. UPP-12: UGV2TextWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2TextWidgetBase* TextWidget = CreateWidget<UGV2TextWidgetBase>(TestWorld, UGV2TextWidgetBase::StaticClass());
        TextWidget->WidgetTree = NewObject<UWidgetTree>(TextWidget);
        UCommonTextBlock* TextBlock = TextWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TextBlock"));
        TextWidget->WidgetTree->RootWidget = TextBlock;

        FGV2UiCapabilityBuilder Builder;
        TextWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree TextCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> TextFailures;
        const bool bTextObservable = RunUiCapabilityObservabilityHarness(TextWidget, TextCaps, TextFailures);
        TestTrue(TEXT("UGV2TextWidgetBase capabilities are observable"), bTextObservable);
        TestEqual(TEXT("No failures for UGV2TextWidgetBase"), TextFailures.Num(), 0);

        // Detached / unbound TextBlock fails observability
        UGV2TextWidgetBase* UnboundTextWidget = CreateWidget<UGV2TextWidgetBase>(TestWorld, UGV2TextWidgetBase::StaticClass());
        UnboundTextWidget->WidgetTree = NewObject<UWidgetTree>(UnboundTextWidget);
        TArray<FGV2UiObservabilityFailure> UnboundFailures;
        const bool bUnboundObservable = RunUiCapabilityObservabilityHarness(UnboundTextWidget, TextCaps, UnboundFailures);
        TestFalse(TEXT("Unbound UGV2TextWidgetBase fails observability"), bUnboundObservable);
        TestEqual(TEXT("1 failure for unbound UGV2TextWidgetBase"), UnboundFailures.Num(), 1);
    }

    // 4. UPP-13: UGV2ImageWidgetBase and UGV2IconWidgetBase implement IGV2UiPropertyHost and are observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2ImageWidgetBase* ImageWidget = CreateWidget<UGV2ImageWidgetBase>(TestWorld, UGV2ImageWidgetBase::StaticClass());
        ImageWidget->WidgetTree = NewObject<UWidgetTree>(ImageWidget);
        UImage* InnerImage = ImageWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
        ImageWidget->WidgetTree->RootWidget = InnerImage;
        ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);

        FGV2UiCapabilityBuilder Builder;
        ImageWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree ImageCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> ImageFailures;
        const bool bImageObservable = RunUiCapabilityObservabilityHarness(ImageWidget, ImageCaps, ImageFailures);
        TestTrue(TEXT("UGV2ImageWidgetBase capabilities are observable"), bImageObservable);
        TestEqual(TEXT("No failures for UGV2ImageWidgetBase with PreserveAspect"), ImageFailures.Num(), 0);

        // Image with Unset scale policy fails observability
        UGV2ImageWidgetBase* UnsetImageWidget = CreateWidget<UGV2ImageWidgetBase>(TestWorld, UGV2ImageWidgetBase::StaticClass());
        UnsetImageWidget->WidgetTree = NewObject<UWidgetTree>(UnsetImageWidget);
        UImage* InnerUnsetImage = UnsetImageWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
        UnsetImageWidget->WidgetTree->RootWidget = InnerUnsetImage;
        // Default scale policy is Unset

        TArray<FGV2UiObservabilityFailure> UnsetFailures;
        const bool bUnsetObservable = RunUiCapabilityObservabilityHarness(UnsetImageWidget, ImageCaps, UnsetFailures);
        TestFalse(TEXT("UGV2ImageWidgetBase with Unset scale policy fails observability"), bUnsetObservable);
        TestEqual(TEXT("1 failure for UGV2ImageWidgetBase with Unset policy"), UnsetFailures.Num(), 1);

        // UGV2IconWidgetBase has PreserveAspect by default and is observable
        UGV2IconWidgetBase* IconWidget = CreateWidget<UGV2IconWidgetBase>(TestWorld, UGV2IconWidgetBase::StaticClass());
        IconWidget->WidgetTree = NewObject<UWidgetTree>(IconWidget);
        UImage* InnerIcon = IconWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
        IconWidget->WidgetTree->RootWidget = InnerIcon;

        TArray<FGV2UiObservabilityFailure> IconFailures;
        const bool bIconObservable = RunUiCapabilityObservabilityHarness(IconWidget, ImageCaps, IconFailures);
        TestTrue(TEXT("UGV2IconWidgetBase capabilities are observable"), bIconObservable);
        TestEqual(TEXT("No failures for UGV2IconWidgetBase"), IconFailures.Num(), 0);
    }

    // 5. UPP-14: UGV2ButtonWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2ButtonWidgetBase* ButtonWidget = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, UGV2ButtonWidgetBase::StaticClass());
        ButtonWidget->WidgetTree = NewObject<UWidgetTree>(ButtonWidget);
        UCommonTextBlock* LabelText = ButtonWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
        ButtonWidget->WidgetTree->RootWidget = LabelText;

        FGV2UiCapabilityBuilder Builder;
        ButtonWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree ButtonCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> ButtonFailures;
        const bool bButtonObservable = RunUiCapabilityObservabilityHarness(ButtonWidget, ButtonCaps, ButtonFailures);
        TestTrue(TEXT("UGV2ButtonWidgetBase capabilities are observable"), bButtonObservable);
        TestEqual(TEXT("No failures for UGV2ButtonWidgetBase"), ButtonFailures.Num(), 0);

        // Detached/unbound LabelText fails observability for the 'text' capability
        UGV2ButtonWidgetBase* UnboundButton = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, UGV2ButtonWidgetBase::StaticClass());
        UnboundButton->WidgetTree = NewObject<UWidgetTree>(UnboundButton);
        TArray<FGV2UiObservabilityFailure> UnboundFailures;
        const bool bUnboundObservable = RunUiCapabilityObservabilityHarness(UnboundButton, ButtonCaps, UnboundFailures);
        TestFalse(TEXT("Unbound UGV2ButtonWidgetBase fails observability"), bUnboundObservable);
        TestEqual(TEXT("1 failure for unbound UGV2ButtonWidgetBase (missing LabelText target)"), UnboundFailures.Num(), 1);
    }

    return true;
}

#endif

