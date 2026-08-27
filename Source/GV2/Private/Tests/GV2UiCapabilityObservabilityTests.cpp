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
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "CommonRichTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Engine/GameInstance.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2LocationCompositeWidgetBases.h"

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
                *FString::Printf(TEXT("Failure for '%s' names a defect code"), *Failure.PropertyName),
                Failure.Reason.Contains(TEXT("core:diagnostic.ui_observability.not_distinguishable"))
                    || Failure.Reason.Contains(TEXT("core:diagnostic.ui_observability.prepare_or_commit_failed")));
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

    // 6. UPP-16: UGV2CheckboxWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2CheckboxWidgetBase* CheckboxWidget = CreateWidget<UGV2CheckboxWidgetBase>(TestWorld, UGV2CheckboxWidgetBase::StaticClass());
        CheckboxWidget->WidgetTree = NewObject<UWidgetTree>(CheckboxWidget);
        UVerticalBox* Root = CheckboxWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        CheckboxWidget->WidgetTree->RootWidget = Root;
        UCheckBox* Checkbox = CheckboxWidget->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("Checkbox"));
        Root->AddChildToVerticalBox(Checkbox);
        UCommonTextBlock* LabelText = CheckboxWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
        Root->AddChildToVerticalBox(LabelText);

        FGV2UiCapabilityBuilder Builder;
        CheckboxWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree CheckboxCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> CheckboxFailures;
        const bool bCheckboxObservable = RunUiCapabilityObservabilityHarness(CheckboxWidget, CheckboxCaps, CheckboxFailures);
        TestTrue(TEXT("UGV2CheckboxWidgetBase capabilities are observable"), bCheckboxObservable);
        TestEqual(TEXT("No failures for UGV2CheckboxWidgetBase"), CheckboxFailures.Num(), 0);
    }

    // 7. UPP-16: UGV2InputFieldWidgetBase implements IGV2UiPropertyHost and is observable
    // REV3-08 closure: label/placeholder_text are Text-kind capabilities (TextPropertyConsumer),
    // is_read_only/max_length are primitive capabilities — all four are swept by the generic
    // observability harness below, proving none of them is a raw setter bypassing Prepare/Commit.
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2InputFieldWidgetBase* InputFieldWidget = CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, UGV2InputFieldWidgetBase::StaticClass());
        InputFieldWidget->WidgetTree = NewObject<UWidgetTree>(InputFieldWidget);
        UVerticalBox* Root = InputFieldWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        InputFieldWidget->WidgetTree->RootWidget = Root;
        UEditableTextBox* EditableTextBox = InputFieldWidget->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox"));
        Root->AddChildToVerticalBox(EditableTextBox);
        UCommonTextBlock* LabelText = InputFieldWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
        Root->AddChildToVerticalBox(LabelText);

        FGV2UiCapabilityBuilder Builder;
        InputFieldWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree InputCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> InputFailures;
        const bool bInputObservable = RunUiCapabilityObservabilityHarness(InputFieldWidget, InputCaps, InputFailures);
        TestTrue(TEXT("UGV2InputFieldWidgetBase capabilities are observable"), bInputObservable);
        TestEqual(TEXT("No failures for UGV2InputFieldWidgetBase"), InputFailures.Num(), 0);
    }

    // 8. UPP-17: UGV2ProgressBarWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2ProgressBarWidgetBase* ProgressBarWidget = CreateWidget<UGV2ProgressBarWidgetBase>(TestWorld, UGV2ProgressBarWidgetBase::StaticClass());
        ProgressBarWidget->WidgetTree = NewObject<UWidgetTree>(ProgressBarWidget);
        UVerticalBox* Root = ProgressBarWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        ProgressBarWidget->WidgetTree->RootWidget = Root;
        UProgressBar* ProgressBar = ProgressBarWidget->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
        Root->AddChildToVerticalBox(ProgressBar);
        UCommonTextBlock* LabelText = ProgressBarWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
        Root->AddChildToVerticalBox(LabelText);

        FGV2UiCapabilityBuilder Builder;
        ProgressBarWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree ProgressCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> ProgressFailures;
        const bool bProgressObservable = RunUiCapabilityObservabilityHarness(ProgressBarWidget, ProgressCaps, ProgressFailures);
        TestTrue(TEXT("UGV2ProgressBarWidgetBase capabilities are observable"), bProgressObservable);
        TestEqual(TEXT("No failures for UGV2ProgressBarWidgetBase"), ProgressFailures.Num(), 0);
    }

    // 9. UPP-17: UGV2PortraitWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2PortraitWidgetBase* PortraitWidget = CreateWidget<UGV2PortraitWidgetBase>(TestWorld, UGV2PortraitWidgetBase::StaticClass());
        PortraitWidget->WidgetTree = NewObject<UWidgetTree>(PortraitWidget);
        UVerticalBox* Root = PortraitWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        PortraitWidget->WidgetTree->RootWidget = Root;
        UImage* PortraitImage = PortraitWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
        Root->AddChildToVerticalBox(PortraitImage);
        UImage* FrameImage = PortraitWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FrameImage"));
        Root->AddChildToVerticalBox(FrameImage);

        FGV2UiCapabilityBuilder Builder;
        PortraitWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree PortraitCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> PortraitFailures;
        const bool bPortraitObservable = RunUiCapabilityObservabilityHarness(PortraitWidget, PortraitCaps, PortraitFailures);
        TestTrue(TEXT("UGV2PortraitWidgetBase capabilities are observable"), bPortraitObservable);
        TestEqual(TEXT("No failures for UGV2PortraitWidgetBase"), PortraitFailures.Num(), 0);
    }

    // 10. UPP-18: UGV2RichTextWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2RichTextWidgetBase* RichTextWidget = CreateWidget<UGV2RichTextWidgetBase>(TestWorld, UGV2RichTextWidgetBase::StaticClass());
        RichTextWidget->WidgetTree = NewObject<UWidgetTree>(RichTextWidget);
        UVerticalBox* Root = RichTextWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        RichTextWidget->WidgetTree->RootWidget = Root;
        UCommonRichTextBlock* RichTextBlock = RichTextWidget->WidgetTree->ConstructWidget<UCommonRichTextBlock>(UCommonRichTextBlock::StaticClass(), TEXT("RichTextBlock"));
        Root->AddChildToVerticalBox(RichTextBlock);

        FGV2UiCapabilityBuilder Builder;
        RichTextWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree RichTextCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> RichTextFailures;
        const bool bRichTextObservable = RunUiCapabilityObservabilityHarness(RichTextWidget, RichTextCaps, RichTextFailures);
        TestTrue(TEXT("UGV2RichTextWidgetBase capabilities are observable"), bRichTextObservable);
        TestEqual(TEXT("No failures for UGV2RichTextWidgetBase"), RichTextFailures.Num(), 0);
    }

    // 11. UPP-18: UGV2RichTextPopoverWidgetBase implements IGV2UiPropertyHost and is observable
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2RichTextPopoverWidgetBase* PopoverWidget = CreateWidget<UGV2RichTextPopoverWidgetBase>(TestWorld, UGV2RichTextPopoverWidgetBase::StaticClass());
        PopoverWidget->WidgetTree = NewObject<UWidgetTree>(PopoverWidget);
        UVerticalBox* Root = PopoverWidget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        PopoverWidget->WidgetTree->RootWidget = Root;
        UCommonTextBlock* TitleText = PopoverWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleText"));
        Root->AddChildToVerticalBox(TitleText);
        UGV2RichTextWidgetBase* DescriptionText = PopoverWidget->WidgetTree->ConstructWidget<UGV2RichTextWidgetBase>(UGV2RichTextWidgetBase::StaticClass(), TEXT("DescriptionText"));
        DescriptionText->WidgetTree = NewObject<UWidgetTree>(DescriptionText);
        UCommonRichTextBlock* InnerRichText = DescriptionText->WidgetTree->ConstructWidget<UCommonRichTextBlock>(UCommonRichTextBlock::StaticClass(), TEXT("RichTextBlock"));
        DescriptionText->WidgetTree->RootWidget = InnerRichText;
        Root->AddChildToVerticalBox(DescriptionText);
        UImage* Icon = PopoverWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
        Root->AddChildToVerticalBox(Icon);

        FGV2UiCapabilityBuilder Builder;
        PopoverWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree PopoverCaps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> PopoverFailures;
        const bool bPopoverObservable = RunUiCapabilityObservabilityHarness(PopoverWidget, PopoverCaps, PopoverFailures);
        for (const FGV2UiObservabilityFailure& Failure : PopoverFailures)
        {
            UE_LOG(LogTemp, Error, TEXT("PopoverObservabilityFailure: property '%s': %s"), *Failure.PropertyName, *Failure.Reason);
        }
        TestTrue(TEXT("UGV2RichTextPopoverWidgetBase capabilities are observable"), bPopoverObservable);
        TestEqual(TEXT("No failures for UGV2RichTextPopoverWidgetBase"), PopoverFailures.Num(), 0);
    }

    return true;
}

namespace
{
// STATUS-005 closure: sweep every remaining IGV2UiPropertyHost, using the real WBP assets
// rather than a synthetic widget tree. The composites are exactly where every historical
// "accepted and silently dropped" defect lived, so leaving them outside the sweep left the
// plan's central guarantee unverified precisely where it has failed before. Loading the
// production asset also proves the capability/target binding of §17.2, not just the code.
UWorld* MakeSweepWorld()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    return GameInstance->GetWorld();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCapabilityObservabilityCompositeSweepTest,
    "GV2.UI.CapabilityObservabilityCompositeSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCapabilityObservabilityCompositeSweepTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeSweepWorld();

    auto SweepAsset = [this, World](const TCHAR* AssetPath, const TCHAR* Label)
    {
        UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, AssetPath);
        if (WidgetClass == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: widget blueprint '%s' could not be loaded"), Label, AssetPath));
            return;
        }
        UUserWidget* Host = CreateWidget<UUserWidget>(World, WidgetClass);
        if (Host == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: widget could not be instantiated"), Label));
            return;
        }
        IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(Host);
        if (PropertyHost == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: widget does not implement IGV2UiPropertyHost"), Label));
            return;
        }

        FGV2UiCapabilityBuilder Builder;
        PropertyHost->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree Caps = Builder.Build();

        int32 OwnLeafCaps = 0;
        for (const auto& Entry : Caps.Properties)
        {
            if (Entry.Value.TargetType == EGV2UiCapabilityTargetType::RendererControl)
            {
                ++OwnLeafCaps;
            }
        }
        TestTrue(
            *FString::Printf(TEXT("%s declares at least one own capability to sweep"), Label),
            OwnLeafCaps > 0);

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            AddError(FString::Printf(TEXT("%s: capability '%s' is not observable -- %s"),
                Label, *Failure.PropertyName, *Failure.Reason));
        }
        TestTrue(*FString::Printf(TEXT("%s: every declared capability is observable"), Label), bObservable);
    };

    SweepAsset(TEXT("/Game/TextSystem/UI/Widgets/WBP_LocationTopBar.WBP_LocationTopBar_C"), TEXT("UGV2LocationTopBarWidgetBase"));
    SweepAsset(TEXT("/Game/TextSystem/UI/Widgets/WBP_PlayerStatusPanel.WBP_PlayerStatusPanel_C"), TEXT("UGV2LocationPlayerStatusWidgetBase"));
    SweepAsset(TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"), TEXT("UGV2LocationSceneWidgetBase"));
    SweepAsset(TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"), TEXT("UGV2LocationCommandPanelWidgetBase"));
    // WBP_Modal is not based on UGV2ModalWidgetBase, so the modal host is swept as a
    // synthetic instance with its declared renderer targets present by name.
    {
        UGV2ModalWidgetBase* Modal = CreateWidget<UGV2ModalWidgetBase>(World, UGV2ModalWidgetBase::StaticClass());
        Modal->WidgetTree = NewObject<UWidgetTree>(Modal);
        UVerticalBox* ModalRoot = Modal->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        Modal->WidgetTree->RootWidget = ModalRoot;
        ModalRoot->AddChildToVerticalBox(
            Modal->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleText")));
        ModalRoot->AddChildToVerticalBox(
            Modal->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("ContentText")));

        FGV2UiCapabilityBuilder Builder;
        Modal->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree Caps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Modal, Caps, Failures);
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            AddError(FString::Printf(TEXT("UGV2ModalWidgetBase: capability '%s' is not observable -- %s"),
                *Failure.PropertyName, *Failure.Reason));
        }
        TestTrue(TEXT("UGV2ModalWidgetBase: every declared capability is observable"), bObservable);
    }
    SweepAsset(TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"), TEXT("UGV2DropdownSelectWidgetBase"));
    SweepAsset(TEXT("/Game/UI/Widgets/WBP_TabContainer.WBP_TabContainer_C"), TEXT("UGV2TabContainerWidgetBase"));

    return true;
}

#endif
