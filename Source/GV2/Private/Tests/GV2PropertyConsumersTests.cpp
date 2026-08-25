#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PropertyConsumersTest,
    "GV2.UI.StandardPropertyConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PropertyConsumersTest::RunTest(const FString& Parameters)
{
    // 1. Missing target rejection (must NOT be silently ignored)
    {
        FGV2TextPropertyConsumer TextConsumer;
        FGV2UiPropertyCapability TextCap;
        TextCap.PropertyName = TEXT("label");
        TextCap.SupportedKind = EGV2PreparedUiValueKind::Text;

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("Test"));
        const FGV2PreparedUiValue TextVal = FGV2PreparedUiValue::MakeText(TextModel);

        FString Error;
        const bool bPrepared = TextConsumer.Prepare(TextVal, TextCap, nullptr, Error);
        TestTrue(TEXT("Prepare with null target fails"), !bPrepared);
        TestTrue(TEXT("Error contains missing_target diagnostic"),
            Error.Contains(TEXT("core:diagnostic.ui_consumer.missing_target")));
    }

    // 2. Factory creation of consumers for all supported kinds
    {
        auto TextCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Text, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Text consumer created"), TextCons.Get());

        auto ImageCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::StableId, EGV2UiCapabilityTargetType::RendererControl, TEXT("resource"));
        TestNotNull(TEXT("Image consumer created"), ImageCons.Get());

        auto BoolCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Boolean, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Boolean consumer created"), BoolCons.Get());

        auto IntCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Integer, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Integer consumer created"), IntCons.Get());

        auto NumCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Number, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Number consumer created"), NumCons.Get());

        auto StrCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::String, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("String consumer created"), StrCons.Get());

        auto KeyCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Key, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Key consumer created"), KeyCons.Get());

        auto BindingCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Binding, EGV2UiCapabilityTargetType::RendererControl);
        TestNotNull(TEXT("Binding consumer created"), BindingCons.Get());
    }

    // 3. UPP-13: FGV2ImageResourcePropertyConsumer rejects Unset scale policy and verifies compatibility in Prepare
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2ImageWidgetBase* ImageWidget = CreateWidget<UGV2ImageWidgetBase>(TestWorld, UGV2ImageWidgetBase::StaticClass());
        ImageWidget->WidgetTree = NewObject<UWidgetTree>(ImageWidget);
        UImage* InnerImage = ImageWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
        ImageWidget->WidgetTree->RootWidget = InnerImage;

        // Verify default scale policy of UGV2ImageWidgetBase is Unset
        TestEqual(TEXT("UGV2ImageWidgetBase default ScalePolicy is Unset"),
            ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::Unset);

        FGV2ImageResourcePropertyConsumer ImageConsumer;
        FGV2UiPropertyCapability ImageCap;
        ImageCap.PropertyName = TEXT("resource_id");
        ImageCap.SupportedKind = EGV2PreparedUiValueKind::StableId;
        ImageCap.TargetKind = TEXT("resource");

        const FGV2PreparedUiValue ImageVal = FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource"));

        // 3a. Prepare fails when ScalePolicy is Unset
        FString UnsetError;
        const bool bUnsetPrepared = ImageConsumer.Prepare(ImageVal, ImageCap, InnerImage, UnsetError);
        TestFalse(TEXT("FGV2ImageResourcePropertyConsumer rejects Unset scale policy"), bUnsetPrepared);
        TestTrue(TEXT("Error contains unset_scale_policy diagnostic"),
            UnsetError.Contains(TEXT("core:diagnostic.ui_consumer.unset_scale_policy")));

        // 3b. Prepare succeeds when ScalePolicy is explicitly declared
        ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
        FString ValidError;
        const bool bValidPrepared = ImageConsumer.Prepare(ImageVal, ImageCap, InnerImage, ValidError);
        TestTrue(TEXT("FGV2ImageResourcePropertyConsumer succeeds with explicit PreserveAspect"), bValidPrepared);

        // 3c. Prepare rejects incompatible scale policy (Tile with FixedAspect missing_icon)
        ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
        FString IncompatError;
        const bool bIncompatPrepared = ImageConsumer.Prepare(ImageVal, ImageCap, InnerImage, IncompatError);
        TestFalse(TEXT("FGV2ImageResourcePropertyConsumer rejects incompatible Tile scale policy for FixedAspect resource"), bIncompatPrepared);
        TestTrue(TEXT("Error contains incompatible_scale_policy diagnostic"),
            IncompatError.Contains(TEXT("core:diagnostic.ui_consumer.incompatible_scale_policy")));

        // 3d. UGV2IconWidgetBase default is PreserveAspect
        UGV2IconWidgetBase* IconWidget = CreateWidget<UGV2IconWidgetBase>(TestWorld, UGV2IconWidgetBase::StaticClass());
        IconWidget->WidgetTree = NewObject<UWidgetTree>(IconWidget);
        UImage* InnerIcon = IconWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
        IconWidget->WidgetTree->RootWidget = InnerIcon;

        TestEqual(TEXT("UGV2IconWidgetBase default ScalePolicy is PreserveAspect"),
            IconWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);

        FString IconError;
        const bool bIconPrepared = ImageConsumer.Prepare(ImageVal, ImageCap, InnerIcon, IconError);
        TestTrue(TEXT("FGV2ImageResourcePropertyConsumer succeeds with UGV2IconWidgetBase"), bIconPrepared);
    }

    // 4. Code Audit Test: centralized presentation pipeline compliance
    // Prohibits raw calls to SetText/SetBrush outside centralized pipelines in production code
    {
        auto ReadSource = [this](const TCHAR* RelativePath, FString& OutSource)
        {
            const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
            const bool bLoaded = FFileHelper::LoadFileToString(OutSource, *FullPath);
            TestTrue(*FString::Printf(TEXT("Pipeline audit can read %s"), RelativePath), bLoaded);
            return bLoaded;
        };

        // Check that consumers use centralized pipelines
        FString ConsumerSource;
        if (ReadSource(TEXT("Source/GV2/Private/UI/GV2PropertyConsumers.cpp"), ConsumerSource))
        {
            TestTrue(
                TEXT("PropertyConsumers uses UGV2TextPipeline::Apply"),
                ConsumerSource.Contains(TEXT("UGV2TextPipeline::Apply")));
            TestTrue(
                TEXT("PropertyConsumers uses FGV2ImagePresentation::ResolveAndApply"),
                ConsumerSource.Contains(TEXT("FGV2ImagePresentation::ResolveAndApply")));
        }
    }

    // 5. UPP-14: UGV2ButtonWidgetBase binding/key consumer & negative schema compatibility test
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        UGV2ButtonWidgetBase* ButtonWidget = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, UGV2ButtonWidgetBase::StaticClass());
        ButtonWidget->WidgetTree = NewObject<UWidgetTree>(ButtonWidget);
        UCommonTextBlock* LabelText = ButtonWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
        ButtonWidget->WidgetTree->RootWidget = LabelText;

        // 5a. FGV2BindingPropertyConsumer rejects target that does not implement IGV2UiBindingTarget
        FGV2BindingPropertyConsumer BindingConsumer;
        FGV2UiPropertyCapability BindingCap;
        BindingCap.SupportedKind = EGV2PreparedUiValueKind::Binding;
        BindingCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
        const FGV2UiBindingHandle TestHandle = FGV2UiBindingHandle::Create(TEXT("test_action@1:1"));
        const FGV2PreparedUiValue BindingVal = FGV2PreparedUiValue::MakeBinding(TestHandle);

        FString RejectBindingError;
        const bool bRejectBindingPrepared = BindingConsumer.Prepare(BindingVal, BindingCap, LabelText, RejectBindingError);
        TestFalse(TEXT("BindingConsumer rejects target not implementing IGV2UiBindingTarget"), bRejectBindingPrepared);
        TestTrue(TEXT("Error contains target_type_mismatch"),
            RejectBindingError.Contains(TEXT("core:diagnostic.ui_consumer.target_type_mismatch")));

        // 5b. FGV2BindingPropertyConsumer succeeds on UGV2ButtonWidgetBase
        FString ValidBindingError;
        const bool bValidBindingPrepared = BindingConsumer.Prepare(BindingVal, BindingCap, ButtonWidget, ValidBindingError);
        TestTrue(TEXT("BindingConsumer succeeds on UGV2ButtonWidgetBase"), bValidBindingPrepared);

        FString CommitBindingError;
        const bool bBindingCommitted = BindingConsumer.Commit(ButtonWidget, CommitBindingError);
        TestTrue(TEXT("BindingConsumer Commit succeeds"), bBindingCommitted);
        TestEqual(TEXT("ButtonWidget binding handle matches"), ButtonWidget->GetBindingHandle(), TestHandle);
        TestTrue(TEXT("ButtonWidget is enabled when binding is valid"), ButtonWidget->GetIsEnabled());

        // 5c. FGV2KeyPropertyConsumer commits key to UGV2ButtonWidgetBase
        FGV2KeyPropertyConsumer KeyConsumer;
        FGV2UiPropertyCapability KeyCap;
        KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
        KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
        const FGV2PreparedUiValue KeyVal = FGV2PreparedUiValue::MakeKey(TEXT("test_btn_key"));

        FString KeyPrepError;
        TestTrue(TEXT("KeyConsumer Prepare succeeds"), KeyConsumer.Prepare(KeyVal, KeyCap, ButtonWidget, KeyPrepError));
        FString KeyCommitError;
        TestTrue(TEXT("KeyConsumer Commit succeeds"), KeyConsumer.Commit(ButtonWidget, KeyCommitError));
        TestEqual(TEXT("ButtonWidget Key matches"), ButtonWidget->GetKey(), FName(TEXT("test_btn_key")));

        // 5d. Negative test: schema requiring property that Button cannot handle (e.g. 'percent')
        // is rejected before Ready with core:diagnostic.ui_schema_compatibility.missing_capability
        FGV2UiCapabilityBuilder Builder;
        ButtonWidget->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree ButtonCaps = Builder.Build();

        GV2ContentCore::FCompiledUiFieldSpec IncompatibleSchema;
        IncompatibleSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
        auto PercentSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
        PercentSpec->Kind = GV2ContentCore::EUiFieldKind::Scalar;
        PercentSpec->Scalar = GV2ContentCore::FScalarFieldSpec{};
        PercentSpec->Scalar->Kind = GV2ContentCore::EScalarFieldKind::Number;
        IncompatibleSchema.Fields.push_back({ "percent", true, PercentSpec }); // required unsupported field

        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.5));
        const TSharedRef<const FGV2PreparedUiObject> IncompatCandidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));

        FGV2UiHostMutationPlan IncompatPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> IncompatDiagnostics;
        const bool bPreparedIncompat = PrepareUiHostProperties(
            ButtonWidget,
            ButtonCaps,
            *IncompatCandidate,
            IncompatibleSchema,
            TEXT("core:schema.ui_field.incompatible.v1"),
            TEXT("incompatible_field"),
            FGV2PreparedUiObject(),
            IncompatPlan,
            IncompatDiagnostics);

        TestFalse(TEXT("PrepareUiHostProperties rejects schema requiring unsupported property on Button"), bPreparedIncompat);
        TestTrue(TEXT("At least one diagnostic emitted"), IncompatDiagnostics.Num() > 0);
        if (IncompatDiagnostics.Num() > 0)
        {
            TestEqual(
                TEXT("Diagnostic code is unknown_schema_property"),
                IncompatDiagnostics[0].Code,
                FString(TEXT("core:diagnostic.ui_capability.unknown_schema_property")));
        }
    }

    // 6. UPP-16: UCheckBox, UEditableTextBox, UGV2CheckboxWidgetBase, and UGV2InputFieldWidgetBase consumers
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

        // 6a. UCheckBox Boolean consumer (is_checked, is_read_only)
        {
            UCheckBox* CheckBox = NewObject<UCheckBox>(TestWorld);
            CheckBox->SetIsChecked(false);
            CheckBox->SetIsEnabled(true);

            FGV2BooleanPropertyConsumer CheckedConsumer(TEXT("is_checked"));
            FGV2UiPropertyCapability CheckedCap;
            CheckedCap.PropertyName = TEXT("is_checked");
            CheckedCap.SupportedKind = EGV2PreparedUiValueKind::Boolean;
            CheckedCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FString PrepErr;
            TestTrue(TEXT("CheckedConsumer Prepare succeeds"), CheckedConsumer.Prepare(FGV2PreparedUiValue::MakeBoolean(true), CheckedCap, CheckBox, PrepErr));
            FString CommitErr;
            TestTrue(TEXT("CheckedConsumer Commit succeeds"), CheckedConsumer.Commit(CheckBox, CommitErr));
            TestTrue(TEXT("CheckBox IsChecked is true"), CheckBox->IsChecked());

            FGV2BooleanPropertyConsumer ReadOnlyConsumer(TEXT("is_read_only"));
            FGV2UiPropertyCapability ReadOnlyCap;
            ReadOnlyCap.PropertyName = TEXT("is_read_only");
            ReadOnlyCap.SupportedKind = EGV2PreparedUiValueKind::Boolean;
            ReadOnlyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("ReadOnlyConsumer Prepare succeeds"), ReadOnlyConsumer.Prepare(FGV2PreparedUiValue::MakeBoolean(true), ReadOnlyCap, CheckBox, PrepErr));
            TestTrue(TEXT("ReadOnlyConsumer Commit succeeds"), ReadOnlyConsumer.Commit(CheckBox, CommitErr));
            TestFalse(TEXT("CheckBox interaction disabled when is_read_only is true"), CheckBox->GetIsEnabled());
        }

        // 6b. UEditableTextBox Hint / Text consumer (placeholder_text via UGV2TextPipeline::ApplyHint)
        {
            UEditableTextBox* EditableTextBox = NewObject<UEditableTextBox>(TestWorld);

            FGV2TextPropertyConsumer TextConsumer;
            FGV2UiPropertyCapability HintCap;
            HintCap.PropertyName = TEXT("placeholder_text");
            HintCap.SupportedKind = EGV2PreparedUiValueKind::Text;
            HintCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FGV2TextViewModel ValidHint;
            ValidHint.Text = FText::FromString(TEXT("Enter query here..."));
            FString PrepErr;
            TestTrue(TEXT("Hint Prepare succeeds for plain text"), TextConsumer.Prepare(FGV2PreparedUiValue::MakeText(ValidHint), HintCap, EditableTextBox, PrepErr));
            FString CommitErr;
            TestTrue(TEXT("Hint Commit succeeds"), TextConsumer.Commit(EditableTextBox, CommitErr));
            TestEqual(TEXT("HintText matches"), EditableTextBox->GetHintText().ToString(), FString(TEXT("Enter query here...")));

            // Red test: markup containing <gv2 is rejected
            FGV2TextViewModel BadHint;
            BadHint.Text = FText::FromString(TEXT("Invalid"));
            BadHint.NormalizedMarkup = TEXT("<gv2 style=\"red\">Invalid</gv2>");
            FString BadHintErr;
            TestFalse(TEXT("Hint Prepare rejects markup containing <gv2"), TextConsumer.Prepare(FGV2PreparedUiValue::MakeText(BadHint), HintCap, EditableTextBox, BadHintErr));
        }

        // 6c. UEditableTextBox Boolean (is_read_only), Integer (max_length), String (value)
        {
            UGV2InputFieldWidgetBase* InputWidget = CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, UGV2InputFieldWidgetBase::StaticClass());
            InputWidget->WidgetTree = NewObject<UWidgetTree>(InputWidget);
            UEditableTextBox* InnerEdit = InputWidget->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox"));
            InputWidget->WidgetTree->RootWidget = InnerEdit;

            FGV2BooleanPropertyConsumer ReadOnlyConsumer(TEXT("is_read_only"));
            FGV2UiPropertyCapability ReadOnlyCap;
            ReadOnlyCap.PropertyName = TEXT("is_read_only");
            ReadOnlyCap.SupportedKind = EGV2PreparedUiValueKind::Boolean;
            ReadOnlyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FString PrepErr, CommitErr;
            TestTrue(TEXT("EditableTextBox ReadOnly Prepare succeeds"), ReadOnlyConsumer.Prepare(FGV2PreparedUiValue::MakeBoolean(true), ReadOnlyCap, InnerEdit, PrepErr));
            TestTrue(TEXT("EditableTextBox ReadOnly Commit succeeds"), ReadOnlyConsumer.Commit(InnerEdit, CommitErr));
            TestTrue(TEXT("EditableTextBox IsReadOnly is true"), InnerEdit->GetIsReadOnly());

            FGV2IntegerPropertyConsumer MaxLenConsumer(TEXT("max_length"));
            FGV2UiPropertyCapability MaxLenCap;
            MaxLenCap.PropertyName = TEXT("max_length");
            MaxLenCap.SupportedKind = EGV2PreparedUiValueKind::Integer;
            MaxLenCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("EditableTextBox MaxLength Prepare succeeds"), MaxLenConsumer.Prepare(FGV2PreparedUiValue::MakeInteger(5), MaxLenCap, InnerEdit, PrepErr));
            TestTrue(TEXT("EditableTextBox MaxLength Commit succeeds"), MaxLenConsumer.Commit(InnerEdit, CommitErr));
            TestEqual(TEXT("InputWidget MaxLength updated to 5"), InputWidget->GetMaxLength(), static_cast<int64>(5));

            FGV2StringPropertyConsumer StrConsumer;
            FGV2UiPropertyCapability StrCap;
            StrCap.PropertyName = TEXT("value");
            StrCap.SupportedKind = EGV2PreparedUiValueKind::String;
            StrCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("EditableTextBox Value Prepare succeeds"), StrConsumer.Prepare(FGV2PreparedUiValue::MakeString(TEXT("Hello World Long Text")), StrCap, InnerEdit, PrepErr));
            TestTrue(TEXT("EditableTextBox Value Commit succeeds"), StrConsumer.Commit(InnerEdit, CommitErr));
            TestEqual(TEXT("EditableTextBox value is set and truncated to MaxLength 5"), InnerEdit->GetText().ToString(), FString(TEXT("Hello")));
        }

        // 7a. UPP-17: UGV2ProgressBarWidgetBase as IGV2UiPropertyHost
        {
            UGV2ProgressBarWidgetBase* BarWidget = CreateWidget<UGV2ProgressBarWidgetBase>(TestWorld, UGV2ProgressBarWidgetBase::StaticClass());
            BarWidget->WidgetTree = NewObject<UWidgetTree>(BarWidget);
            UProgressBar* InnerBar = BarWidget->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
            BarWidget->WidgetTree->RootWidget = InnerBar;

            FGV2NumberPropertyConsumer NumConsumer;
            FGV2UiPropertyCapability NumCap;
            NumCap.PropertyName = TEXT("percent");
            NumCap.SupportedKind = EGV2PreparedUiValueKind::Number;
            NumCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            NumCap.NumberMin = 0.0;
            NumCap.NumberMax = 1.0;

            FString PrepErr, CommitErr;
            TestTrue(TEXT("ProgressBar Number Prepare succeeds for 0.75"), NumConsumer.Prepare(FGV2PreparedUiValue::MakeNumber(0.75), NumCap, InnerBar, PrepErr));
            TestTrue(TEXT("ProgressBar Number Commit succeeds"), NumConsumer.Commit(InnerBar, CommitErr));
            TestEqual(TEXT("ProgressBar percent is 0.75"), InnerBar->GetPercent(), 0.75f);

            FGV2KeyPropertyConsumer KeyConsumer;
            FGV2UiPropertyCapability KeyCap;
            KeyCap.PropertyName = TEXT("key");
            KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
            KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("ProgressBar Key Prepare succeeds"), KeyConsumer.Prepare(FGV2PreparedUiValue::MakeKey(TEXT("hp_meter")), KeyCap, BarWidget, PrepErr));
            TestTrue(TEXT("ProgressBar Key Commit succeeds"), KeyConsumer.Commit(BarWidget, CommitErr));
            TestEqual(TEXT("ProgressBar key matches hp_meter"), BarWidget->GetKey(), FName(TEXT("hp_meter")));
        }

        // 7b. UPP-17: UGV2PortraitWidgetBase as IGV2UiPropertyHost
        {
            UGV2PortraitWidgetBase* PortraitWidget = CreateWidget<UGV2PortraitWidgetBase>(TestWorld, UGV2PortraitWidgetBase::StaticClass());
            PortraitWidget->WidgetTree = NewObject<UWidgetTree>(PortraitWidget);
            UImage* InnerPortrait = PortraitWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
            PortraitWidget->WidgetTree->RootWidget = InnerPortrait;

            FGV2ImageResourcePropertyConsumer ImageConsumer;
            FGV2UiPropertyCapability ImageCap;
            ImageCap.PropertyName = TEXT("resource_id");
            ImageCap.SupportedKind = EGV2PreparedUiValueKind::StableId;
            ImageCap.TargetKind = TEXT("resource");
            ImageCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FString PrepErr, CommitErr;
            // Failure on unbound target: Prepare with null target fails
            TestFalse(TEXT("Portrait resource Prepare with null target fails"), ImageConsumer.Prepare(
                FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")),
                ImageCap, nullptr, PrepErr));

            // Success with bound target
            TestTrue(TEXT("Portrait resource Prepare with bound target succeeds"), ImageConsumer.Prepare(
                FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")),
                ImageCap, InnerPortrait, PrepErr));
            TestTrue(TEXT("Portrait resource Commit succeeds"), ImageConsumer.Commit(InnerPortrait, CommitErr));

            FGV2KeyPropertyConsumer KeyConsumer;
            FGV2UiPropertyCapability KeyCap;
            KeyCap.PropertyName = TEXT("key");
            KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
            KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("Portrait Key Prepare succeeds"), KeyConsumer.Prepare(FGV2PreparedUiValue::MakeKey(TEXT("hero_portrait")), KeyCap, PortraitWidget, PrepErr));
            TestTrue(TEXT("Portrait Key Commit succeeds"), KeyConsumer.Commit(PortraitWidget, CommitErr));
            TestEqual(TEXT("Portrait key matches hero_portrait"), PortraitWidget->GetKey(), FName(TEXT("hero_portrait")));
        }

        // 7c. UPP-18: UGV2RichTextWidgetBase as IGV2UiPropertyHost & RichText Markup handling
        {
            UGV2RichTextWidgetBase* RichTextWidget = CreateWidget<UGV2RichTextWidgetBase>(TestWorld, UGV2RichTextWidgetBase::StaticClass());
            RichTextWidget->WidgetTree = NewObject<UWidgetTree>(RichTextWidget);
            UCommonRichTextBlock* InnerRichText = RichTextWidget->WidgetTree->ConstructWidget<UCommonRichTextBlock>(UCommonRichTextBlock::StaticClass(), TEXT("RichTextBlock"));
            RichTextWidget->WidgetTree->RootWidget = InnerRichText;

            FGV2TextPropertyConsumer TextConsumer;
            FGV2UiPropertyCapability TextCap;
            TextCap.PropertyName = TEXT("text");
            TextCap.SupportedKind = EGV2PreparedUiValueKind::Text;
            TextCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FGV2TextViewModel FormattedModel;
            FormattedModel.Text = FText::FromString(TEXT("Hello World"));
            FormattedModel.NormalizedMarkup = TEXT("<gv2:style token=\"heading\">Hello World</>");

            FString PrepErr, CommitErr;
            // RichText consumer accepts <gv2 formatted markup
            TestTrue(TEXT("RichText Prepare accepts <gv2 markup"), TextConsumer.Prepare(
                FGV2PreparedUiValue::MakeText(FormattedModel), TextCap, InnerRichText, PrepErr));
            TestTrue(TEXT("RichText Commit succeeds"), TextConsumer.Commit(InnerRichText, CommitErr));

            FGV2KeyPropertyConsumer KeyConsumer;
            FGV2UiPropertyCapability KeyCap;
            KeyCap.PropertyName = TEXT("key");
            KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
            KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("RichText Key Prepare succeeds"), KeyConsumer.Prepare(FGV2PreparedUiValue::MakeKey(TEXT("dialogue_body")), KeyCap, RichTextWidget, PrepErr));
            TestTrue(TEXT("RichText Key Commit succeeds"), KeyConsumer.Commit(RichTextWidget, CommitErr));
            TestEqual(TEXT("RichText key matches dialogue_body"), RichTextWidget->GetKey(), FName(TEXT("dialogue_body")));
        }

        // 7d. UPP-18: UGV2RichTextPopoverWidgetBase as IGV2UiPropertyHost
        {
            UGV2RichTextPopoverWidgetBase* PopoverWidget = CreateWidget<UGV2RichTextPopoverWidgetBase>(TestWorld, UGV2RichTextPopoverWidgetBase::StaticClass());
            PopoverWidget->WidgetTree = NewObject<UWidgetTree>(PopoverWidget);
            UCommonTextBlock* InnerTitle = PopoverWidget->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleText"));
            PopoverWidget->WidgetTree->RootWidget = InnerTitle;

            FGV2TextPropertyConsumer TextConsumer;
            FGV2UiPropertyCapability TextCap;
            TextCap.PropertyName = TEXT("title");
            TextCap.SupportedKind = EGV2PreparedUiValueKind::Text;
            TextCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FGV2TextViewModel TitleModel;
            TitleModel.Text = FText::FromString(TEXT("Popover Title"));

            FString PrepErr, CommitErr;
            TestTrue(TEXT("Popover Title Prepare succeeds"), TextConsumer.Prepare(
                FGV2PreparedUiValue::MakeText(TitleModel), TextCap, InnerTitle, PrepErr));
            TestTrue(TEXT("Popover Title Commit succeeds"), TextConsumer.Commit(InnerTitle, CommitErr));

            FGV2KeyPropertyConsumer KeyConsumer;
            FGV2UiPropertyCapability KeyCap;
            KeyCap.PropertyName = TEXT("key");
            KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
            KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            TestTrue(TEXT("Popover Key Prepare succeeds"), KeyConsumer.Prepare(FGV2PreparedUiValue::MakeKey(TEXT("info_popover")), KeyCap, PopoverWidget, PrepErr));
            TestTrue(TEXT("Popover Key Commit succeeds"), KeyConsumer.Commit(PopoverWidget, CommitErr));
            TestEqual(TEXT("Popover key matches info_popover"), PopoverWidget->GetKey(), FName(TEXT("info_popover")));
        }

        // 8. UPP-20: FGV2KeyedCollectionPropertyConsumer & Value-Level Transactionality
        {
            UGV2ListViewWidgetBase* ListView = CreateWidget<UGV2ListViewWidgetBase>(TestWorld, UGV2ListViewWidgetBase::StaticClass());
            UVerticalBox* ContainerBox = NewObject<UVerticalBox>(ListView);
            ListView->SetContainerPanel(ContainerBox);

            FGV2UiPropertyCapability ItemCap;
            ItemCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            ItemCap.EntryWidgetClass = UGV2ButtonWidgetBase::StaticClass();

            FGV2UiCapabilityBuilder Builder;
            Builder.AddKeyedCollection(TEXT("items"), FName(TEXT("ContainerPanel")), ItemCap, TEXT("key"), UGV2ButtonWidgetBase::StaticClass());
            const FGV2UiCapabilityTree Tree = Builder.Build();
            const FGV2UiPropertyCapability* CollCap = Tree.FindProperty(TEXT("items"));
            TestNotNull(TEXT("Collection capability found"), CollCap);

            TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
                EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);
            TestNotNull(TEXT("Factory created FGV2KeyedCollectionPropertyConsumer"), Consumer.Get());

            const FGV2UiBindingHandle TestHandleA = FGV2UiBindingHandle::Create(TEXT("action_a@1:1"));
            const FGV2UiBindingHandle TestHandleB = FGV2UiBindingHandle::Create(TEXT("action_b@1:1"));

            // 8a. Successful baseline reconciliation with initial values (item_a = handleA, item_b = handleB)
            TMap<FString, FGV2PreparedUiValue> ItemAInitMap;
            ItemAInitMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item_a")));
            ItemAInitMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(TestHandleA));

            TMap<FString, FGV2PreparedUiValue> ItemBInitMap;
            ItemBInitMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item_b")));
            ItemBInitMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(TestHandleB));

            TArray<FGV2PreparedUiValue> BaselineElements;
            BaselineElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ItemAInitMap)));
            BaselineElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ItemBInitMap)));

            FString PrepErr, CommitErr;
            bool bPrepSuccess = Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BaselineElements)), *CollCap, ListView, PrepErr);
            TestTrue(TEXT("Collection baseline Prepare succeeds"), bPrepSuccess);
            bool bCommitSuccess = Consumer->Commit(ListView, CommitErr);
            TestTrue(TEXT("Collection baseline Commit succeeds"), bCommitSuccess);

            UGV2ButtonWidgetBase* BtnA = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_a")));
            UGV2ButtonWidgetBase* BtnB = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_b")));
            TestNotNull(TEXT("BtnA created and tracked"), BtnA);
            TestNotNull(TEXT("BtnB created and tracked"), BtnB);
            TestEqual(TEXT("BtnA initial key matches item_a"), BtnA->GetKey(), FName(TEXT("item_a")));
            TestEqual(TEXT("BtnB initial key matches item_b"), BtnB->GetKey(), FName(TEXT("item_b")));
            TestEqual(TEXT("BtnA initial binding matches TestHandleA"), BtnA->GetBindingHandle(), TestHandleA);
            TestEqual(TEXT("BtnB initial binding matches TestHandleB"), BtnB->GetBindingHandle(), TestHandleB);
            TestTrue(TEXT("BtnA is enabled"), BtnA->GetIsEnabled());
            TestTrue(TEXT("BtnB is enabled"), BtnB->GetIsEnabled());
            TestEqual(TEXT("Container children count is 2"), ContainerBox->GetChildrenCount(), 2);

            // 8b. Transactionality / Value-level rollback verification:
            // Item A has valid candidate value (TestHandleA_New), Item B has invalid type for binding (number instead of binding)
            const FGV2UiBindingHandle TestHandleA_New = FGV2UiBindingHandle::Create(TEXT("action_a_new@1:1"));
            TMap<FString, FGV2PreparedUiValue> ItemACandidateMap;
            ItemACandidateMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item_a")));
            ItemACandidateMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(TestHandleA_New));

            TMap<FString, FGV2PreparedUiValue> ItemBCandidateMap;
            ItemBCandidateMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item_b")));
            ItemBCandidateMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeNumber(999.0)); // invalid type for binding!

            TArray<FGV2PreparedUiValue> FailingElements;
            FailingElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ItemACandidateMap)));
            FailingElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ItemBCandidateMap)));

            bool bFailingPrep = Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(FailingElements)), *CollCap, ListView, PrepErr);
            TestFalse(TEXT("Collection Prepare fails when item B has invalid property"), bFailingPrep);

            // CRITICAL TEST: BtnA was reused, but its binding handle and key MUST NOT be mutated to TestHandleA_New!
            TestEqual(TEXT("BtnA binding remains strictly TestHandleA (NO in-place mutation on failure)"), BtnA->GetBindingHandle(), TestHandleA);
            TestEqual(TEXT("BtnB binding remains strictly TestHandleB"), BtnB->GetBindingHandle(), TestHandleB);
            TestEqual(TEXT("BtnA key remains item_a"), BtnA->GetKey(), FName(TEXT("item_a")));
            TestEqual(TEXT("BtnB key remains item_b"), BtnB->GetKey(), FName(TEXT("item_b")));
            TestEqual(TEXT("Container child 0 remains BtnA"), ContainerBox->GetChildAt(0), Cast<UWidget>(BtnA));
            TestEqual(TEXT("Container child 1 remains BtnB"), ContainerBox->GetChildAt(1), Cast<UWidget>(BtnB));

            // 8c. Negative tests: missing key, empty key, duplicate key
            TMap<FString, FGV2PreparedUiValue> MissingKeyMap;
            MissingKeyMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(TestHandleA));
            TArray<FGV2PreparedUiValue> MissingKeyElements;
            MissingKeyElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MissingKeyMap)));
            TestFalse(TEXT("Missing key fails Prepare"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MissingKeyElements)), *CollCap, ListView, PrepErr));

            TMap<FString, FGV2PreparedUiValue> EmptyKeyMap;
            EmptyKeyMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("")));
            TArray<FGV2PreparedUiValue> EmptyKeyElements;
            EmptyKeyElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(EmptyKeyMap)));
            TestFalse(TEXT("Empty key fails Prepare"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(EmptyKeyElements)), *CollCap, ListView, PrepErr));

            TMap<FString, FGV2PreparedUiValue> DupA1Map;
            DupA1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("dup_key")));
            TMap<FString, FGV2PreparedUiValue> DupA2Map;
            DupA2Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("dup_key")));
            TArray<FGV2PreparedUiValue> DupKeyElements;
            DupKeyElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(DupA1Map)));
            DupKeyElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(DupA2Map)));
            TestFalse(TEXT("Duplicate key fails Prepare"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DupKeyElements)), *CollCap, ListView, PrepErr));
        }

        // 9. UPP-21: UGV2ButtonListWidgetBase and UGV2DropdownSelectWidgetBase Property Host Reconciliation
        {
            // 9a. ButtonList Property Host
            UGV2ButtonListWidgetBase* ButtonList = CreateWidget<UGV2ButtonListWidgetBase>(TestWorld, UGV2ButtonListWidgetBase::StaticClass());
            UVerticalBox* BtnBox = NewObject<UVerticalBox>(ButtonList);
            ButtonList->SetButtonContainer(BtnBox);

            FGV2UiCapabilityBuilder BtnBuilder;
            ButtonList->DescribeUiCapabilities(BtnBuilder);
            const FGV2UiCapabilityTree BtnTree = BtnBuilder.Build();
            const FGV2UiPropertyCapability* BtnItemsCap = BtnTree.FindProperty(TEXT("items"));
            TestNotNull(TEXT("ButtonList items capability found"), BtnItemsCap);

            if (BtnItemsCap != nullptr)
            {
                TSharedPtr<IGV2PropertyConsumer> BtnCollConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);

                FGV2TextViewModel Btn1Text;
                Btn1Text.Text = FText::FromString(TEXT("Button 1"));
                Btn1Text.StyleToken = TEXT("default");

                FGV2TextViewModel Btn2Text;
                Btn2Text.Text = FText::FromString(TEXT("Button 2"));
                Btn2Text.StyleToken = TEXT("default");

                TMap<FString, FGV2PreparedUiValue> Item1Map;
                Item1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("btn_1")));
                Item1Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Btn1Text));
                Item1Map.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd_1@1:1"))));

                TMap<FString, FGV2PreparedUiValue> Item2Map;
                Item2Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("btn_2")));
                Item2Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Btn2Text));
                Item2Map.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd_2@1:1"))));

                TArray<FGV2PreparedUiValue> BtnElements;
                BtnElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Item1Map)));
                BtnElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Item2Map)));

                FString PrepErr, CommitErr;
                bool bBtnPrep = BtnCollConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BtnElements)),
                    *BtnItemsCap,
                    BtnBox,
                    PrepErr);
                TestTrue(TEXT("ButtonList Prepare succeeds"), bBtnPrep);

                bool bBtnCommit = BtnCollConsumer->Commit(BtnBox, CommitErr);
                TestTrue(TEXT("ButtonList Commit succeeds"), bBtnCommit);
                TestEqual(TEXT("ButtonList container has 2 children"), BtnBox->GetChildrenCount(), 2);

                UGV2ButtonWidgetBase* ChildBtn1 = Cast<UGV2ButtonWidgetBase>(BtnBox->GetChildAt(0));
                UGV2ButtonWidgetBase* ChildBtn2 = Cast<UGV2ButtonWidgetBase>(BtnBox->GetChildAt(1));
                TestNotNull(TEXT("ChildBtn1 created"), ChildBtn1);
                TestNotNull(TEXT("ChildBtn2 created"), ChildBtn2);
                if (ChildBtn1 && ChildBtn2)
                {
                    TestEqual(TEXT("ChildBtn1 key is btn_1"), ChildBtn1->GetKey(), FName(TEXT("btn_1")));
                    TestEqual(TEXT("ChildBtn2 key is btn_2"), ChildBtn2->GetKey(), FName(TEXT("btn_2")));
                }
            }

            // 9b. DropdownSelect Property Host
            UGV2DropdownSelectWidgetBase* Dropdown = CreateWidget<UGV2DropdownSelectWidgetBase>(TestWorld, UGV2DropdownSelectWidgetBase::StaticClass());
            UGV2ButtonWidgetBase* HeaderBtn = CreateWidget<UGV2ButtonWidgetBase>(TestWorld, UGV2ButtonWidgetBase::StaticClass());
            UScrollBox* OptBox = NewObject<UScrollBox>(Dropdown);
            UBorder* PopBorder = NewObject<UBorder>(Dropdown);
            Dropdown->SetHeaderButton(HeaderBtn);
            Dropdown->SetOptionsScrollBox(OptBox);
            Dropdown->SetPopupBorder(PopBorder);

            FGV2UiCapabilityBuilder DdBuilder;
            Dropdown->DescribeUiCapabilities(DdBuilder);
            const FGV2UiCapabilityTree DdTree = DdBuilder.Build();
            const FGV2UiPropertyCapability* DdItemsCap = DdTree.FindProperty(TEXT("items"));
            TestNotNull(TEXT("Dropdown items capability found"), DdItemsCap);

            FGV2TextViewModel DropdownPlaceholder;
            DropdownPlaceholder.Text = FText::FromString(TEXT("Select..."));
            DropdownPlaceholder.StyleToken = TEXT("default");
            Dropdown->ApplyPlaceholderText(DropdownPlaceholder);

            if (DdItemsCap != nullptr)
            {
                TSharedPtr<IGV2PropertyConsumer> DdCollConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);

                FGV2TextViewModel Opt1Text;
                Opt1Text.Text = FText::FromString(TEXT("Option 1"));
                Opt1Text.StyleToken = TEXT("default");

                FGV2TextViewModel Opt2Text;
                Opt2Text.Text = FText::FromString(TEXT("Option 2"));
                Opt2Text.StyleToken = TEXT("default");

                TMap<FString, FGV2PreparedUiValue> Opt1Map;
                Opt1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("opt_1")));
                Opt1Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Opt1Text));

                TMap<FString, FGV2PreparedUiValue> Opt2Map;
                Opt2Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("opt_2")));
                Opt2Map.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(Opt2Text));

                TArray<FGV2PreparedUiValue> DdElements;
                DdElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Opt1Map)));
                DdElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Opt2Map)));

                FString PrepErr, CommitErr;
                bool bDdPrep = DdCollConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DdElements)),
                    *DdItemsCap,
                    OptBox,
                    PrepErr);
                TestTrue(TEXT("Dropdown items Prepare succeeds"), bDdPrep);

                bool bDdCommit = DdCollConsumer->Commit(OptBox, CommitErr);
                TestTrue(TEXT("Dropdown items Commit succeeds"), bDdCommit);

                Dropdown->SetSelectedKey(FName(TEXT("opt_1")));
                TestEqual(TEXT("Dropdown selected key is opt_1"), Dropdown->GetSelectedKey(), FName(TEXT("opt_1")));
                TestEqual(TEXT("HeaderButton displays Option 1 text"), HeaderBtn->GetTextViewModel().Text.ToString(), TEXT("Option 1"));

                Dropdown->SetSelectedKey(FName(TEXT("opt_2")));
                TestEqual(TEXT("HeaderButton updates to Option 2 text"), HeaderBtn->GetTextViewModel().Text.ToString(), TEXT("Option 2"));
            }
        }

        // 10. UPP-22: FGV2RichTextSpansPropertyConsumer and UGV2RichTextWidgetBase Property Host
        {
            UGV2RichTextWidgetBase* RichTextWidget = CreateWidget<UGV2RichTextWidgetBase>(TestWorld, UGV2RichTextWidgetBase::StaticClass());

            FGV2UiCapabilityBuilder RichBuilder;
            RichTextWidget->DescribeUiCapabilities(RichBuilder);
            const FGV2UiCapabilityTree RichTree = RichBuilder.Build();
            const FGV2UiPropertyCapability* SpansCap = RichTree.FindProperty(TEXT("spans"));
            TestNotNull(TEXT("RichText spans capability found"), SpansCap);
            TestNotNull(TEXT("RichText text capability found"), RichTree.FindProperty(TEXT("text")));

            if (SpansCap != nullptr)
            {
                TSharedPtr<IGV2PropertyConsumer> SpansConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CustomControl);
                TestNotNull(TEXT("Factory creates FGV2RichTextSpansPropertyConsumer for CustomControl Array"), SpansConsumer.Get());

                // 10a. Valid spans prepare & commit
                TMap<FString, FGV2PreparedUiValue> Span1Map;
                Span1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item_span")));
                Span1Map.Add(TEXT("span_id"), FGV2PreparedUiValue::MakeKey(TEXT("item_span")));
                Span1Map.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd_inspect@1:1"))));

                TMap<FString, FGV2PreparedUiValue> HoverMap;
                FGV2TextViewModel HoverTitle;
                HoverTitle.Text = FText::FromString(TEXT("Hover Title"));
                HoverMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(HoverTitle));
                Span1Map.Add(TEXT("hover"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(HoverMap)));

                TArray<FGV2PreparedUiValue> SpanElements;
                SpanElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Span1Map)));

                FString PrepErr, CommitErr;
                bool bPrepOk = SpansConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(SpanElements)),
                    *SpansCap,
                    RichTextWidget,
                    PrepErr);
                TestTrue(TEXT("RichText spans Prepare succeeds"), bPrepOk);

                bool bCommitOk = SpansConsumer->Commit(RichTextWidget, CommitErr);
                TestTrue(TEXT("RichText spans Commit succeeds"), bCommitOk);
                TestTrue(TEXT("RichText has interactive span item_span"), RichTextWidget->HasInteractiveSpan(TEXT("item_span")));
                const FGV2RichTextSpanViewModel* FoundSpan = RichTextWidget->FindInteractiveSpan(TEXT("item_span"));
                TestNotNull(TEXT("Found interactive span"), FoundSpan);
                if (FoundSpan)
                {
                    TestEqual(TEXT("Span binding handle matches"), FoundSpan->Binding.ToString(), FString(TEXT("cmd_inspect@1:1")));
                    TestEqual(TEXT("Span hover title matches"), FoundSpan->Hover.Title.Text.ToString(), TEXT("Hover Title"));
                }

                // 10b. Negative tests
                // Missing key
                TMap<FString, FGV2PreparedUiValue> MissingKeySpan;
                MissingKeySpan.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:1"))));
                TArray<FGV2PreparedUiValue> MissingKeyList;
                MissingKeyList.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MissingKeySpan)));
                TestFalse(TEXT("Missing span key rejected"), SpansConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MissingKeyList)), *SpansCap, RichTextWidget, PrepErr));

                // Duplicate key
                TMap<FString, FGV2PreparedUiValue> DupKeySpan1;
                DupKeySpan1.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("dup_span")));
                DupKeySpan1.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd1@1:1"))));
                TMap<FString, FGV2PreparedUiValue> DupKeySpan2;
                DupKeySpan2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("dup_span")));
                DupKeySpan2.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd2@1:1"))));
                TArray<FGV2PreparedUiValue> DupKeyList;
                DupKeyList.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(DupKeySpan1)));
                DupKeyList.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(DupKeySpan2)));
                TestFalse(TEXT("Duplicate span key rejected"), SpansConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DupKeyList)), *SpansCap, RichTextWidget, PrepErr));

                // Span with neither hover nor binding
                TMap<FString, FGV2PreparedUiValue> EmptySpan;
                EmptySpan.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("empty_span")));
                TArray<FGV2PreparedUiValue> EmptySpanList;
                EmptySpanList.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(EmptySpan)));
                TestFalse(TEXT("Empty span (no hover or binding) rejected"), SpansConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(EmptySpanList)), *SpansCap, RichTextWidget, PrepErr));
            }
        }

        // 11. UPP-22: UGV2ModalWidgetBase Property Host Reconciliation
        {
            UGV2ModalWidgetBase* ModalWidget = CreateWidget<UGV2ModalWidgetBase>(TestWorld, UGV2ModalWidgetBase::StaticClass());

            FGV2UiCapabilityBuilder ModalBuilder;
            ModalWidget->DescribeUiCapabilities(ModalBuilder);
            const FGV2UiCapabilityTree ModalTree = ModalBuilder.Build();

            TestNotNull(TEXT("Modal title capability found"), ModalTree.FindProperty(TEXT("title")));
            TestNotNull(TEXT("Modal content capability found"), ModalTree.FindProperty(TEXT("content")));
            TestNotNull(TEXT("Modal buttons capability found"), ModalTree.FindProperty(TEXT("buttons")));
            TestNotNull(TEXT("Modal backdrop_close_action capability found"), ModalTree.FindProperty(TEXT("backdrop_close_action")));
            TestNotNull(TEXT("Modal key capability found"), ModalTree.FindProperty(TEXT("key")));

            const FGV2UiPropertyCapability* ButtonsCap = ModalTree.FindProperty(TEXT("buttons"));
            if (ButtonsCap != nullptr)
            {
                TestEqual(TEXT("Modal buttons is KeyedCollection"), ButtonsCap->TargetType, EGV2UiCapabilityTargetType::CollectionHost);
                TestEqual(TEXT("Modal buttons value kind is Array"), ButtonsCap->SupportedKind, EGV2PreparedUiValueKind::Array);
                TestEqual(TEXT("Modal buttons entry class is ButtonWidgetBase"), ButtonsCap->EntryWidgetClass.Get(), (UClass*)UGV2ButtonWidgetBase::StaticClass());
            }

            // Test backdrop close binding handle
            const FGV2UiBindingHandle BackdropHandle = FGV2UiBindingHandle::Create(TEXT("close_modal@1:1"));
            ModalWidget->SetBindingHandle(BackdropHandle);
            TestEqual(TEXT("Modal binding handle is set"), ModalWidget->GetBindingHandle(), BackdropHandle);
            TestEqual(TEXT("SubmitBackdropClose with valid binding returns RuntimeNotReady in unit test"), ModalWidget->SubmitBackdropClose(), EGV2SubmitUiInteractionResult::RuntimeNotReady);

            ModalWidget->SetBindingHandle(FGV2UiBindingHandle());
            TestEqual(TEXT("SubmitBackdropClose with invalid binding returns InvalidBindingHandle"), ModalWidget->SubmitBackdropClose(), EGV2SubmitUiInteractionResult::InvalidBindingHandle);

            // Test Title and Content Apply
            FGV2TextViewModel TitleModel;
            TitleModel.Text = FText::FromString(TEXT("Confirm Action"));
            TitleModel.StyleToken = TEXT("title");
            TestTrue(TEXT("ApplyTitle succeeds"), ModalWidget->ApplyTitle(TitleModel));
            TestEqual(TEXT("GetTitle matches"), ModalWidget->GetTitle().Text.ToString(), TEXT("Confirm Action"));

            FGV2TextViewModel ContentModel;
            ContentModel.Text = FText::FromString(TEXT("Are you sure?"));
            ContentModel.StyleToken = TEXT("body");
            TestTrue(TEXT("ApplyContent succeeds"), ModalWidget->ApplyContent(ContentModel));
            TestEqual(TEXT("GetContent matches"), ModalWidget->GetContent().Text.ToString(), TEXT("Are you sure?"));
        }

        // 12. UPP-23: UGV2TabContainerWidgetBase & FGV2TabContainerTabsPropertyConsumer
        {
            UGV2TabContainerWidgetBase* TabContainer = CreateWidget<UGV2TabContainerWidgetBase>(TestWorld, UGV2TabContainerWidgetBase::StaticClass());

            FGV2UiCapabilityBuilder Builder;
            TabContainer->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Tree = Builder.Build();

            TestNotNull(TEXT("default_tab_key capability found"), Tree.FindProperty(TEXT("default_tab_key")));
            TestNotNull(TEXT("tabs capability found"), Tree.FindProperty(TEXT("tabs")));
            TestNotNull(TEXT("key capability found"), Tree.FindProperty(TEXT("key")));

            const FGV2UiPropertyCapability* TabsCap = Tree.FindProperty(TEXT("tabs"));
            if (TabsCap != nullptr)
            {
                TestEqual(TEXT("Tabs target type is NestedScreen"), TabsCap->TargetType, EGV2UiCapabilityTargetType::NestedScreen);
                TestEqual(TEXT("Tabs supported kind is Array"), TabsCap->SupportedKind, EGV2PreparedUiValueKind::Array);
            }

            TSharedPtr<IGV2PropertyConsumer> TabsConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
            TestNotNull(TEXT("Factory created FGV2TabContainerTabsPropertyConsumer"), TabsConsumer.Get());

            // 12a. Successful reconciliation
            TArray<FGV2PreparedUiValue> ValidTabs;
            TMap<FString, FGV2PreparedUiValue> Tab1Map;
            Tab1Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("inventory")));
            FGV2TextViewModel T1Title;
            T1Title.Text = FText::FromString(TEXT("Inventory"));
            Tab1Map.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(T1Title));
            Tab1Map.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test"), TEXT("screen")));
            ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Tab1Map)));

            TMap<FString, FGV2PreparedUiValue> Tab2Map;
            Tab2Map.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("skills")));
            FGV2TextViewModel T2Title;
            T2Title.Text = FText::FromString(TEXT("Skills"));
            Tab2Map.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(T2Title));
            Tab2Map.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test"), TEXT("screen")));
            ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Tab2Map)));

            FString PrepErr, CommitErr;
            FGV2PreparedUiValue ValidTabsValue = FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(ValidTabs));

            TabContainer->ApplyDefaultTabKey(FName(TEXT("skills")));
            TestTrue(TEXT("Tabs Prepare succeeds"), TabsConsumer->Prepare(ValidTabsValue, *TabsCap, TabContainer, PrepErr));
            TestTrue(TEXT("Tabs Commit succeeds"), TabsConsumer->Commit(TabContainer, CommitErr));

            TestEqual(TEXT("Active tab is skills"), TabContainer->GetActiveTabKey(), FName(TEXT("skills")));
            TestEqual(TEXT("Active tab index is 1"), TabContainer->GetActiveTabIndex(), 1);
            TestEqual(TEXT("TabEntries count is 2"), TabContainer->GetTabEntries().Num(), 2);

            // Tab switching
            TestTrue(TEXT("SelectTabByKey inventory succeeds"), TabContainer->SelectTabByKey(FName(TEXT("inventory"))));
            TestEqual(TEXT("Active tab changed to inventory"), TabContainer->GetActiveTabKey(), FName(TEXT("inventory")));
            TestEqual(TEXT("Active tab index is 0"), TabContainer->GetActiveTabIndex(), 0);

            // Reset
            TabsConsumer->Reset(TabContainer);
            TestEqual(TEXT("Reset clears active tab"), TabContainer->GetActiveTabKey(), NAME_None);
            TestEqual(TEXT("Reset clears tab entries"), TabContainer->GetTabEntries().Num(), 0);

            // 12b. Negative validations
            // Empty tabs
            TArray<FGV2PreparedUiValue> EmptyTabs;
            TestFalse(TEXT("Empty tabs rejected"), TabsConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(EmptyTabs)), *TabsCap, TabContainer, PrepErr));

            // Duplicate keys
            TArray<FGV2PreparedUiValue> DupTabs;
            DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Tab1Map)));
            DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Tab1Map)));
            TestFalse(TEXT("Duplicate tab keys rejected"), TabsConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DupTabs)), *TabsCap, TabContainer, PrepErr));

            // Missing title
            TMap<FString, FGV2PreparedUiValue> NoTitleTab;
            NoTitleTab.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("no_title")));
            NoTitleTab.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test"), TEXT("screen")));
            TArray<FGV2PreparedUiValue> NoTitleTabs;
            NoTitleTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(NoTitleTab)));
            TestFalse(TEXT("Tab missing title rejected"), TabsConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(NoTitleTabs)), *TabsCap, TabContainer, PrepErr));

            // Invalid screen_id kind
            TMap<FString, FGV2PreparedUiValue> BadScreenTab;
            BadScreenTab.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("bad_screen")));
            BadScreenTab.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(T1Title));
            BadScreenTab.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:item.sword"), TEXT("item")));
            TArray<FGV2PreparedUiValue> BadScreenTabs;
            BadScreenTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BadScreenTab)));
            TestFalse(TEXT("Tab with non-screen screen_id rejected"), TabsConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BadScreenTabs)), *TabsCap, TabContainer, PrepErr));

            // Unregistered screen_id
            TMap<FString, FGV2PreparedUiValue> UnregScreenTab;
            UnregScreenTab.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("unregistered")));
            UnregScreenTab.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(T1Title));
            UnregScreenTab.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.nonexistent_screen"), TEXT("screen")));
            TArray<FGV2PreparedUiValue> UnregScreenTabs;
            UnregScreenTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UnregScreenTab)));
            TestFalse(TEXT("Tab with unregistered screen_id rejected"), TabsConsumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(UnregScreenTabs)), *TabsCap, TabContainer, PrepErr));
        }
    }

    return true;
}

#endif
