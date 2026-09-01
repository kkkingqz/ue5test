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
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ScreenFieldHost.h"
#include "Tests/GV2ForgeryTestWidgets.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

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

/**
 * The sweep's source set is the native implementation boundary, not a hand-maintained
 * list in this test. A new production UUserWidget that implements IGV2UiPropertyHost is
 * therefore either represented by a real WBP instance below or makes this automation fail.
 * Test-only forgeries opt out through UCLASS(meta=(GV2TestOnly)); they deliberately violate
 * the harness and are covered by their own negative tests.
 */
TArray<UClass*> CollectProductionUiPropertyHostImplementations()
{
    TArray<UClass*> Result;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* const WidgetClass = *It;
        if (WidgetClass == nullptr
            || !WidgetClass->HasAnyClassFlags(CLASS_Native)
            || WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
            || WidgetClass->GetOutermost()->GetName() != TEXT("/Script/GV2")
            || WidgetClass->HasMetaData(TEXT("GV2TestOnly"))
            || !WidgetClass->IsChildOf(UUserWidget::StaticClass())
            || !WidgetClass->ImplementsInterface(UGV2UiPropertyHost::StaticClass()))
        {
            continue;
        }

        // A derived native class inherits its parent's capability implementation; the
        // parent is the implementation boundary which must have a real sweep fixture.
        UClass* const SuperClass = WidgetClass->GetSuperClass();
        if (SuperClass != nullptr && SuperClass->ImplementsInterface(UGV2UiPropertyHost::StaticClass()))
        {
            continue;
        }
        Result.Add(WidgetClass);
    }

    Result.Sort([](const UClass& A, const UClass& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    return Result;
}

void VerifyHostIdentitySweep(
    FAutomationTestBase& Test,
    UUserWidget* Widget,
    const FString& Label)
{
    IGV2UiPropertyHost* const PropertyHost = Cast<IGV2UiPropertyHost>(Widget);
    if (PropertyHost == nullptr)
    {
        Test.AddError(FString::Printf(TEXT("%s: cannot sweep HostIdentity on a non-property host"), *Label));
        return;
    }

    const FName OriginalIdentity = PropertyHost->GetHostIdentity();
    const FName FirstIdentity(TEXT("duc04_identity_a"));
    const FName SecondIdentity(TEXT("duc04_identity_b"));
    PropertyHost->SetHostIdentity(FirstIdentity);
    const FName ObservedFirst = PropertyHost->GetHostIdentity();
    PropertyHost->SetHostIdentity(SecondIdentity);
    const FName ObservedSecond = PropertyHost->GetHostIdentity();
    PropertyHost->SetHostIdentity(OriginalIdentity);

    Test.TestEqual(*FString::Printf(TEXT("%s: HostIdentity commits the first distinct value"), *Label), ObservedFirst, FirstIdentity);
    Test.TestEqual(*FString::Printf(TEXT("%s: HostIdentity commits the second distinct value"), *Label), ObservedSecond, SecondIdentity);
    Test.TestNotEqual(*FString::Printf(TEXT("%s: HostIdentity distinguishes two values"), *Label), ObservedFirst, ObservedSecond);

    if (IGV2ScreenFieldHost* const ScreenFieldHost = Cast<IGV2ScreenFieldHost>(Widget))
    {
        PropertyHost->SetHostIdentity(FirstIdentity);
        const FName ScreenFieldId = ScreenFieldHost->GetScreenFieldId();
        PropertyHost->SetHostIdentity(OriginalIdentity);
        Test.TestEqual(
            *FString::Printf(TEXT("%s: screen field identity delegates to HostIdentity"), *Label),
            ScreenFieldId,
            FirstIdentity);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCapabilityObservabilityCompositeSweepTest,
    "GV2.UI.CapabilityObservabilityCompositeSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCapabilityObservabilityCompositeSweepTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeSweepWorld();
    const TArray<UClass*> ProductionHostImplementations = CollectProductionUiPropertyHostImplementations();
    TestTrue(TEXT("DUC-04: reflection discovers production IGV2UiPropertyHost implementations"), ProductionHostImplementations.Num() > 0);

    auto SweepClass = [this, World](UClass* WidgetClass, const TCHAR* Label)
    {
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

        VerifyHostIdentitySweep(*this, Host, Label);

        FGV2UiCapabilityBuilder Builder;
        PropertyHost->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree Caps = Builder.Build();

        // A generic repeater primitive swept only because it happens to implement the
        // interface (e.g. UGV2ListViewWidgetBase) may honestly declare zero capabilities of
        // any kind -- that is not the same defect as a real composite whose wiring was
        // simply forgotten. The gate that matters equally for both is below: whatever a host
        // *does* declare must be genuinely observable, never silently unproven.
        if (Caps.Properties.Num() == 0)
        {
            AddInfo(FString::Printf(TEXT("%s: declares no capabilities of its own (nothing to sweep)"), Label));
        }

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            AddError(FString::Printf(TEXT("%s: capability '%s' is not observable -- %s"),
                Label, *Failure.PropertyName, *Failure.Reason));
        }
        TestTrue(*FString::Printf(TEXT("%s: every declared capability is observable"), Label), bObservable);
    };

    // PCC-10: the set of widgets to sweep is derived from reflection -- every WBP_ asset
    // under the project's UI package roots whose generated class implements
    // IGV2UiPropertyHost -- instead of a hardcoded path list that silently stops covering a
    // composite the day someone adds a new one. A future WBP_* implementing the interface is
    // picked up here with no test edit required; one that stops implementing it drops out
    // the same way, rather than lingering as a stale, no-longer-true entry.
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter UiAssetFilter;
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/TextSystem/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/RH/UI"));
    UiAssetFilter.bRecursivePaths = true;
    TArray<FAssetData> UiAssets;
    AssetRegistryModule.Get().GetAssets(UiAssetFilter, UiAssets);

    int32 DiscoveredCount = 0;
    TArray<UClass*> SweptWidgetClasses;
    for (const FAssetData& Asset : UiAssets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.StartsWith(TEXT("WBP_")))
        {
            continue;
        }
        const FString GeneratedClassPath =
            FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *AssetName);
        UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        if (WidgetClass == nullptr || !WidgetClass->ImplementsInterface(UGV2UiPropertyHost::StaticClass()))
        {
            continue;
        }
        ++DiscoveredCount;
        SweptWidgetClasses.Add(WidgetClass);
        SweepClass(WidgetClass, *AssetName);
    }
    TestTrue(
        TEXT("at least one WBP_* asset implementing IGV2UiPropertyHost was discovered to sweep"),
        DiscoveredCount > 0);

    for (UClass* const ImplementationClass : ProductionHostImplementations)
    {
        const bool bCoveredByRealWidgetBlueprint = SweptWidgetClasses.ContainsByPredicate(
            [ImplementationClass](const UClass* SweptClass)
            {
                return SweptClass != nullptr && SweptClass->IsChildOf(ImplementationClass);
            });
        TestTrue(
            *FString::Printf(
                TEXT("DUC-04: %s has a real WBP instance in the capability sweep"),
                *ImplementationClass->GetName()),
            bCoveredByRealWidgetBlueprint);

        const FStructProperty* const PropertyHostState = FindFProperty<FStructProperty>(ImplementationClass, TEXT("PropertyHostState"));
        TestNotNull(
            *FString::Printf(TEXT("DUC-04: %s stores HostIdentity in shared PropertyHostState"), *ImplementationClass->GetName()),
            PropertyHostState);
        if (PropertyHostState != nullptr)
        {
            TestEqual(
                *FString::Printf(TEXT("DUC-04: %s uses FGV2UiPropertyHostState"), *ImplementationClass->GetName()),
                PropertyHostState->Struct.Get(),
                FGV2UiPropertyHostState::StaticStruct());
            TestTrue(
                *FString::Printf(TEXT("DUC-04: %s exposes HostIdentity in Designer"), *ImplementationClass->GetName()),
                PropertyHostState->HasAnyPropertyFlags(CPF_Edit)
                    && PropertyHostState->HasMetaData(TEXT("ShowOnlyInnerProperties")));
        }

        // This exercises the common state through the direct native implementation too,
        // including a host currently represented by no distinct top-level screen field.
        VerifyHostIdentitySweep(
            *this,
            ImplementationClass->GetDefaultObject<UUserWidget>(),
            ImplementationClass->GetName());
    }

    // This remains an asset-independent fixture for Modal's nested collection route. The
    // reflection-driven sweep above now covers the real WBP_Modal instance as well; this
    // block does not contribute to the production class list or act as a coverage exception.
    {
        UGV2ModalWidgetBase* Modal = CreateWidget<UGV2ModalWidgetBase>(World, UGV2ModalWidgetBase::StaticClass());
        Modal->WidgetTree = NewObject<UWidgetTree>(Modal);
        UVerticalBox* ModalRoot = Modal->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
        Modal->WidgetTree->RootWidget = ModalRoot;
        ModalRoot->AddChildToVerticalBox(
            Modal->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleText")));
        ModalRoot->AddChildToVerticalBox(
            Modal->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("ContentText")));

        // PCC-10: without a real ButtonList child, Modal's "buttons" collection had no
        // ResolveButtonWidgetClass() to delegate to and fell back to a bare, unwired
        // UGV2ButtonWidgetBase -- exactly the gap the collection sweep below now catches.
        UGV2ButtonListWidgetBase* ModalButtonList = Modal->WidgetTree->ConstructWidget<UGV2ButtonListWidgetBase>(
            UGV2ButtonListWidgetBase::StaticClass(), TEXT("ButtonList"));
        UVerticalBox* ModalButtonContainer = Modal->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ModalButtonContainer"));
        ModalButtonList->SetButtonContainer(ModalButtonContainer);
        ModalRoot->AddChildToVerticalBox(ModalButtonList);
        // ButtonList is a protected BindWidgetOptional pointer with no public setter --
        // GetWidgetFromName("ButtonList") would already resolve it for target-resolution
        // purposes, but DescribeUiCapabilities' EntryWidgetClass fix reads this member
        // directly, so it must be bound via reflection here too.
        if (FObjectProperty* ButtonListProp = FindFProperty<FObjectProperty>(UGV2ModalWidgetBase::StaticClass(), TEXT("ButtonList")))
        {
            ButtonListProp->SetObjectPropertyValue_InContainer(Modal, ModalButtonList);
        }

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
    return true;
}

// PCC-10's Done text requires the harness be shown red on three deliberately forged
// collection entries, not just on the two real bugs the recursion happened to find in
// production (Modal's bare button class, PlayerStatus's mis-pointed icon) -- those are
// already fixed, so nothing exercises the recursion's failure paths without a widget
// built specifically to be broken in each of these three ways.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCollectionForgeryTest,
    "GV2.UI.CapabilityObservabilityCollectionForgery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCollectionForgeryTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeSweepWorld();

    auto RunForgeryScenario = [this, World](EGV2ForgeryMode Mode, const TCHAR* ExpectedCode, const TCHAR* Label)
    {
        // The recursion instantiates a *fresh* entry instance via CreateWidget, with no seam
        // for the test to configure that specific instance -- ModeForNextInstance is a
        // static the widget's DescribeUiCapabilities reads at call time instead.
        UGV2ForgeryEntryTestWidget::ModeForNextInstance() = Mode;

        UUserWidget* Host = CreateWidget<UGV2PanelWidgetBase>(World, UGV2PanelWidgetBase::StaticClass());
        Host->WidgetTree = NewObject<UWidgetTree>(Host);
        Host->WidgetTree->RootWidget =
            Host->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));

        FGV2UiPropertyCapability EntryCap;
        EntryCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
        EntryCap.EntryWidgetClass = UGV2ForgeryEntryTestWidget::StaticClass();

        FGV2UiCapabilityBuilder Builder;
        Builder.AddKeyedCollection(
            TEXT("forgery_entries"),
            NAME_None,
            EntryCap,
            TEXT("key"),
            UGV2ForgeryEntryTestWidget::StaticClass());
        const FGV2UiCapabilityTree Caps = Builder.Build();

        TArray<FGV2UiObservabilityFailure> Failures;
        const bool bObservable = RunUiCapabilityObservabilityHarness(Host, Caps, Failures);

        TestFalse(*FString::Printf(TEXT("%s: harness must reject this forged collection entry"), Label), bObservable);

        bool bFoundExpected = false;
        for (const FGV2UiObservabilityFailure& Failure : Failures)
        {
            if (Failure.PropertyName.StartsWith(TEXT("forgery_entries[].")) && Failure.Reason.Contains(ExpectedCode))
            {
                bFoundExpected = true;
            }
            AddInfo(FString::Printf(TEXT("%s: saw failure '%s' -- %s"), Label, *Failure.PropertyName, *Failure.Reason));
        }
        TestTrue(
            *FString::Printf(TEXT("%s: harness reports '%s' at the collection entry path, not just at the collection itself"), Label, ExpectedCode),
            bFoundExpected);
    };

    // 1. Consumer replaced with a no-op: the entry's SetBindingHandle never stores what it
    // is given, so Capture(Commit(A)) == Capture(Commit(B)) at the entry's own target.
    RunForgeryScenario(
        EGV2ForgeryMode::NoOpConsumer,
        TEXT("core:diagnostic.ui_observability.not_distinguishable"),
        TEXT("NoOpConsumer"));

    // 2. Renderer detached: the entry is a bare native widget with no WidgetTree at all, so
    // its capability's TargetName can never resolve to any child.
    RunForgeryScenario(
        EGV2ForgeryMode::DetachedRenderer,
        TEXT("prepare_or_commit_failed"),
        TEXT("DetachedRenderer"));

    // 3. Capability declared without implementation: StableId with a non-"resource"
    // TargetKind has no probe pair the harness can even attempt to synthesize.
    RunForgeryScenario(
        EGV2ForgeryMode::UnimplementableKind,
        TEXT("core:diagnostic.ui_observability.no_distinct_pair"),
        TEXT("UnimplementableKind"));

    return true;
}

#endif
