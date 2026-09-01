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
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "Tests/GV2ForgeryTestWidgets.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "UI/GV2UiTheme.h"
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

    // 2. Factory creation of consumers for all supported kinds and PCC-05 completeness gate
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

        auto CollCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);
        TestNotNull(TEXT("CollectionHost consumer created"), CollCons.Get());

        auto SpansCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CustomControl);
        TestNotNull(TEXT("CustomControl spans consumer created"), SpansCons.Get());

        auto TabsCons = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
        TestNotNull(TEXT("NestedScreen tabs consumer created"), TabsCons.Get());

        // PCC-05: Completeness gate over ALL EGV2PreparedUiValueKind enum values
        TArray<FString> GateDiagnostics;
        const bool bAllHandled = FGV2PropertyConsumerFactory::ValidateAllKindsHandled(GateDiagnostics);
        TestTrue(TEXT("PCC-05: All EGV2PreparedUiValueKind values handled by factory gate"), bAllHandled);
        TestEqual(TEXT("PCC-05: Zero unhandled kind diagnostics"), GateDiagnostics.Num(), 0);

        // PCC-05: Inapplicable kinds registration and architectural justification
        FString NullReason;
        TestTrue(TEXT("PCC-05: Null is registered inapplicable"),
            FGV2PropertyConsumerFactory::IsInapplicableKind(EGV2PreparedUiValueKind::Null, &NullReason));
        TestFalse(TEXT("PCC-05: Null reason is non-empty"), NullReason.IsEmpty());
        TestNull(TEXT("PCC-05: CreateConsumer for Null returns nullptr"),
            FGV2PropertyConsumerFactory::CreateConsumer(EGV2PreparedUiValueKind::Null, EGV2UiCapabilityTargetType::RendererControl).Get());

        FString ObjectReason;
        TestTrue(TEXT("PCC-05: Object is registered inapplicable"),
            FGV2PropertyConsumerFactory::IsInapplicableKind(EGV2PreparedUiValueKind::Object, &ObjectReason));
        TestFalse(TEXT("PCC-05: Object reason is non-empty"), ObjectReason.IsEmpty());
        TestTrue(TEXT("PCC-05: Object reason cites architectural rule"),
            ObjectReason.Contains(TEXT("Direct Object property consumption is forbidden")));
        TestNull(TEXT("PCC-05: CreateConsumer for Object returns nullptr"),
            FGV2PropertyConsumerFactory::CreateConsumer(EGV2PreparedUiValueKind::Object, EGV2UiCapabilityTargetType::RendererControl).Get());

        const TArray<FGV2InapplicableKindInfo> InapplicableKinds = FGV2PropertyConsumerFactory::GetInapplicableKinds();
        TestEqual(TEXT("PCC-05: Exactly 2 inapplicable kinds (Null, Object)"), InapplicableKinds.Num(), 2);

        TestEqual(TEXT("PCC-05: Null status is Inapplicable"),
            FGV2PropertyConsumerFactory::GetKindHandlingStatus(EGV2PreparedUiValueKind::Null),
            EGV2PropertyConsumerKindStatus::Inapplicable);
        TestEqual(TEXT("PCC-05: Object status is Inapplicable"),
            FGV2PropertyConsumerFactory::GetKindHandlingStatus(EGV2PreparedUiValueKind::Object),
            EGV2PropertyConsumerKindStatus::Inapplicable);
        TestEqual(TEXT("PCC-05: Text status is Supported"),
            FGV2PropertyConsumerFactory::GetKindHandlingStatus(EGV2PreparedUiValueKind::Text),
            EGV2PropertyConsumerKindStatus::Supported);
        TestEqual(TEXT("PCC-05: Array status is Supported"),
            FGV2PropertyConsumerFactory::GetKindHandlingStatus(EGV2PreparedUiValueKind::Array),
            EGV2PropertyConsumerKindStatus::Supported);
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

        // 7b. DUC-03: FGV2KeyPropertyConsumer needs no edit for a host declared only in this
        // test -- UGV2NewHostAddedOnlyInTestWidget (GV2ForgeryTestWidgets.h) never appears in
        // GV2PropertyConsumers.cpp. Apply, then reset, both routed purely through the shared
        // IGV2UiPropertyHost::GetKey()/SetKey() -- no `Cast<UGV2NewHostAddedOnlyInTestWidget>`
        // exists anywhere for the consumer to have needed.
        {
            UGV2NewHostAddedOnlyInTestWidget* NewHost = NewObject<UGV2NewHostAddedOnlyInTestWidget>(TestWorld);
            TestNotNull(TEXT("DUC-03: new-host-only-in-test instantiated"), NewHost);

            FGV2KeyPropertyConsumer KeyConsumer;
            FGV2UiPropertyCapability KeyCap;
            KeyCap.PropertyName = TEXT("key");
            KeyCap.SupportedKind = EGV2PreparedUiValueKind::Key;
            KeyCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;

            FString PrepErr, CommitErr;
            TestTrue(TEXT("DUC-03: new host Key Prepare succeeds"), KeyConsumer.Prepare(FGV2PreparedUiValue::MakeKey(TEXT("brand_new")), KeyCap, NewHost, PrepErr));
            TestTrue(TEXT("DUC-03: new host Key Commit succeeds"), KeyConsumer.Commit(NewHost, CommitErr));
            TestEqual(TEXT("DUC-03: new host key matches brand_new"), NewHost->GetKey(), FName(TEXT("brand_new")));

            KeyConsumer.Reset(NewHost);
            TestEqual(TEXT("DUC-03: new host key cleared by Reset"), NewHost->GetKey(), NAME_None);

            // Typed rejection on an unsupported target type is still a defect, not a no-op --
            // a bare UWidget with no IGV2UiPropertyHost at all.
            UPanelWidget* PlainWidget = NewObject<UVerticalBox>(TestWorld);
            FString UnsupportedErr;
            TestFalse(TEXT("DUC-03: Key Commit rejects a target with no IGV2UiPropertyHost"), KeyConsumer.Commit(PlainWidget, UnsupportedErr));
            TestTrue(TEXT("DUC-03: rejection names the unhandled_target diagnostic"), UnsupportedErr.Contains(TEXT("unhandled_target")));
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

            auto BaselineItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
            BaselineItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
            BaselineItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
            BaselineItemSpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });
            static_cast<FGV2KeyedCollectionPropertyConsumer*>(Consumer.Get())->SetCompiledItemSpec(
                BaselineItemSpec,
                TEXT("test:schema.button_item"),
                TEXT("items"));

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

            // 8d. PCC-01: Discrepancy between CompiledItemSpec and entry widget capabilities is observed and recorded without failing Prepare
            {
                FGV2KeyedCollectionPropertyConsumer* KeyedConsumer = static_cast<FGV2KeyedCollectionPropertyConsumer*>(Consumer.Get());
                TestNotNull(TEXT("Consumer is KeyedCollectionConsumer"), KeyedConsumer);

                // Create a compiled spec with an extra property 'foo' (string) not supported by UGV2ButtonWidgetBase
                auto DiscrepantItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                DiscrepantItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                DiscrepantItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                DiscrepantItemSpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });
                DiscrepantItemSpec->Fields.push_back({ "foo", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Scalar) });

                KeyedConsumer->SetCompiledItemSpec(
                    DiscrepantItemSpec,
                    TEXT("test:schema.discrepant_list"),
                    TEXT("items"),
                    TEXT("test:screen.main"),
                    TEXT("items_field"));

                FString DiscrepancyPrepErr;
                const bool bDiscrepancyPrep = KeyedConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BaselineElements)),
                    *CollCap,
                    ListView,
                    DiscrepancyPrepErr);

                // PCC-03: Prepare FAILS when schema has extra property (not silently lost)
                TestFalse(TEXT("PCC-03: Prepare fails when schema has extra property"), bDiscrepancyPrep);

                // Discrepancy IS recorded with Section 32 structured format
                const TArray<FGV2CollectionItemDiscrepancy>& Discrepancies = KeyedConsumer->GetDiscrepancies();
                TestTrue(TEXT("PCC-03: At least 1 discrepancy recorded"), Discrepancies.Num() >= 1);
                if (Discrepancies.Num() >= 1)
                {
                    TestEqual(TEXT("Discrepancy 0 path"), Discrepancies[0].PropertyPath, TEXT("items[item_a].foo"));
                    TestEqual(TEXT("Discrepancy 0 code"), Discrepancies[0].Code, TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
                    TestEqual(TEXT("Discrepancy 0 schema"), Discrepancies[0].SchemaId, TEXT("test:schema.discrepant_list"));
                    TestEqual(TEXT("Discrepancy 0 screen"), Discrepancies[0].ScreenId, TEXT("test:screen.main"));
                    TestEqual(TEXT("Discrepancy 0 field"), Discrepancies[0].FieldId, TEXT("items_field"));
                }

                // Matching schema yields zero discrepancies
                auto MatchingItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                MatchingItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                MatchingItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                MatchingItemSpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });

                KeyedConsumer->SetCompiledItemSpec(
                    MatchingItemSpec,
                    TEXT("test:schema.matching_list"),
                    TEXT("items"));

                const bool bMatchingPrep = KeyedConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BaselineElements)),
                    *CollCap,
                    ListView,
                    DiscrepancyPrepErr);
                TestTrue(TEXT("PCC-03: Matching prepare succeeds"), bMatchingPrep);
                TestEqual(TEXT("PCC-03: Zero discrepancies for matching schema"), KeyedConsumer->GetDiscrepancies().Num(), 0);
            }

            // 8e. PCC-03: Element property declared in schema and absent from entry-widget capability is rejected, not lost
            {
                UGV2ListViewWidgetBase* LuaListView = CreateWidget<UGV2ListViewWidgetBase>(TestWorld, UGV2ListViewWidgetBase::StaticClass());
                UVerticalBox* LuaContainerBox = NewObject<UVerticalBox>(LuaListView);
                LuaListView->SetContainerPanel(LuaContainerBox);

                FGV2UiPropertyCapability ButtonCap;
                ButtonCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
                ButtonCap.EntryWidgetClass = UGV2ButtonWidgetBase::StaticClass();

                FGV2UiCapabilityBuilder LuaBuilder;
                LuaBuilder.AddKeyedCollection(TEXT("items"), FName(TEXT("ContainerPanel")), ButtonCap, TEXT("key"), UGV2ButtonWidgetBase::StaticClass());
                const FGV2UiCapabilityTree LuaTree = LuaBuilder.Build();
                const FGV2UiPropertyCapability* LuaCollCap = LuaTree.FindProperty(TEXT("items"));
                TestNotNull(TEXT("Lua collection capability found"), LuaCollCap);

                TSharedPtr<IGV2PropertyConsumer> LuaConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                    EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);
                FGV2KeyedCollectionPropertyConsumer* LuaKeyedConsumer = static_cast<FGV2KeyedCollectionPropertyConsumer*>(LuaConsumer.Get());

                // Schema declares an extra property 'icon_resource' which UGV2ButtonWidgetBase does not support
                auto SchemaWithExtra = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                SchemaWithExtra->Kind = GV2ContentCore::EUiFieldKind::Object;
                SchemaWithExtra->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                SchemaWithExtra->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });
                SchemaWithExtra->Fields.push_back({ "icon_resource", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Ref) });

                LuaKeyedConsumer->SetCompiledItemSpec(
                    SchemaWithExtra,
                    TEXT("core:schema.ui_field.button_list.v2"),
                    TEXT("items"),
                    TEXT("core:screen.test"),
                    TEXT("commands"));

                // Value constructed representing presenter output carrying icon_resource
                TMap<FString, FGV2PreparedUiValue> LuaItemMap;
                LuaItemMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("cmd_explore")));
                LuaItemMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("core:command.test.explore@1:1"))));
                LuaItemMap.Add(TEXT("icon_resource"), FGV2PreparedUiValue::MakeStableId(TEXT("core:resource.ui.compass"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> LuaElements;
                LuaElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(LuaItemMap)));

                FString LuaPrepErr;
                const bool bLuaPrep = LuaKeyedConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(LuaElements)),
                    *LuaCollCap,
                    LuaListView,
                    LuaPrepErr);

                // PCC-03: Property declared in schema but unsupported by entry widget is REJECTED before Ready
                TestFalse(TEXT("PCC-03: Prepare fails when element property is absent from entry widget capabilities"), bLuaPrep);
                TestTrue(TEXT("PCC-03: Error mentions prepare failed or unknown property"), LuaPrepErr.Contains(TEXT("ui_capability.unknown_schema_property")) || LuaPrepErr.Contains(TEXT("prepare_failed")) || LuaPrepErr.Contains(TEXT("not supported")));
                const TArray<FGV2CollectionItemDiscrepancy>& LuaDiscrepancies = LuaKeyedConsumer->GetDiscrepancies();
                TestTrue(TEXT("PCC-03: Discrepancy recorded for icon_resource"), LuaDiscrepancies.Num() >= 1);
                if (LuaDiscrepancies.Num() >= 1)
                {
                    TestEqual(TEXT("Discrepancy path is items[cmd_explore].icon_resource"), LuaDiscrepancies[0].PropertyPath, TEXT("items[cmd_explore].icon_resource"));
                    TestEqual(TEXT("Discrepancy code is unknown_schema_property"), LuaDiscrepancies[0].Code, TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
                }

                // 8f. PCC-03 end-to-end: same rejection, but starting from a RAW Lua-shaped
                // GV2RuntimeCore::FValue (not a hand-built FGV2PreparedUiValue like 8e) and
                // driven through the real production chain ValidateUiFieldValue() ->
                // GV2ScreenFieldMaterializer::ProjectMaterializedValue() -> consumer Prepare().
                // 8e alone proves the consumer rejects a mismatched item once it has one;
                // this proves the mismatch actually survives real materialization intact,
                // rather than being silently dropped or altered before it ever reaches
                // the consumer -- the concrete "value crossed the boundary and vanished"
                // failure mode PCC-03 exists to close.
                {
                using FRuntimeObject = GV2RuntimeCore::FValue::FObject;

                FRuntimeObject ItemObj;
                ItemObj["key"] = GV2RuntimeCore::FValue(std::string("cmd_explore"));
                FRuntimeObject BindingObj;
                BindingObj["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.test.explore"));
                ItemObj["binding"] = GV2RuntimeCore::FValue(BindingObj);
                ItemObj["icon_resource"] = GV2RuntimeCore::FValue(std::string("core:resource.ui.compass"));

                const GV2ContentCore::FValue ContentItemValue =
                    GV2RuntimeCore::RuntimeValueToContentValue(GV2RuntimeCore::FValue(ItemObj));

                GV2ContentCore::FValue MaterializedItem;
                std::vector<GV2ContentCore::FDiagnostic> ValidateDiags;
                GV2ContentCore::FValidationDiagnosticContext ValidateCtx;
                ValidateCtx.SchemaId = "test:schema.ui_value.button_list_item_with_icon.v1";
                // SchemaWithExtra's icon_resource field (shared with 8e) has no RefTargetKind set;
                // 8e never runs it through ValidateUiFieldValue so that never mattered there, but
                // this test does, and FStableId::IsOfKind() requires an exact target-kind match.
                auto SchemaWithExtraValidated = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(*SchemaWithExtra);
                for (auto& FieldEntry : SchemaWithExtraValidated->Fields)
                {
                    if (FieldEntry.Name == "icon_resource")
                    {
                        auto RefSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Ref);
                        RefSpec->RefTargetKind = "resource";
                        FieldEntry.Spec = RefSpec;
                    }
                }

                const bool bValidated = GV2ContentCore::ValidateUiFieldValue(
                    ContentItemValue, *SchemaWithExtraValidated, MaterializedItem, nullptr, "", ValidateCtx, ValidateDiags);
                TestTrue(
                    TEXT("PCC-03: raw Lua-shaped item validates against its own (schema-legitimate) item schema"),
                    bValidated);

                TArray<FGV2UiBindingHandle> Handles;
                Handles.Add(FGV2UiBindingHandle::Create(TEXT("core:command.test.explore@1:1")));
                int32 HandleCursor = 0;
                GV2ScreenFieldMaterializer::FMaterializeContext MatCtx;
                MatCtx.Handles = &Handles;
                MatCtx.HandleCursor = &HandleCursor;

                FGV2PreparedUiValue ProjectedItem;
                const bool bProjected = bValidated && GV2ScreenFieldMaterializer::ProjectMaterializedValue(
                    MatCtx, *SchemaWithExtraValidated, MaterializedItem, ProjectedItem);
                TestTrue(TEXT("PCC-03: real projection (BuildFields' own function) succeeds for the schema-legitimate item"), bProjected);

                TArray<FGV2PreparedUiValue> RealElements;
                RealElements.Add(ProjectedItem);
                FString RealPrepErr;
                const bool bRealPrep = bProjected && LuaKeyedConsumer->Prepare(
                    FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(RealElements)),
                    *LuaCollCap,
                    LuaListView,
                    RealPrepErr);

                TestFalse(
                    TEXT("PCC-03: a value that survived real ValidateUiFieldValue+ProjectMaterializedValue is still rejected by the entry-widget capability check"),
                    bRealPrep);
                const TArray<FGV2CollectionItemDiscrepancy>& RealDiscrepancies = LuaKeyedConsumer->GetDiscrepancies();
                TestTrue(TEXT("PCC-03: discrepancy recorded for the real-pipeline icon_resource value"), RealDiscrepancies.Num() >= 1);
                if (RealDiscrepancies.Num() >= 1)
                {
                    TestEqual(TEXT("PCC-03: real-pipeline discrepancy code is unknown_schema_property"),
                        RealDiscrepancies[0].Code, TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
                }
            }
        }
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

                auto BtnItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                BtnItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                BtnItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                BtnItemSpec->Fields.push_back({ "text", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                BtnItemSpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });
                static_cast<FGV2KeyedCollectionPropertyConsumer*>(BtnCollConsumer.Get())->SetCompiledItemSpec(
                    BtnItemSpec, TEXT("core:schema.ui_field.button_list.v2"), TEXT("items"));

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

            // 9a-bis. UPP-30: KeyedCollection Commit must restyle the ButtonList wrapper itself
            // (mirrors production resolution, e.g. UGV2ModalWidgetBase's "buttons" capability,
            // where TargetWidget resolves to the UGV2ButtonListWidgetBase instance, not its
            // inner VerticalBox), so newly created slots pick up Theme->ButtonListItemPadding.
            {
                UGV2ButtonListWidgetBase* StyledButtonList = CreateWidget<UGV2ButtonListWidgetBase>(TestWorld, UGV2ButtonListWidgetBase::StaticClass());
                UVerticalBox* StyledBtnBox = NewObject<UVerticalBox>(StyledButtonList);
                StyledButtonList->SetButtonContainer(StyledBtnBox);

                FGV2UiCapabilityBuilder StyledBtnBuilder;
                StyledButtonList->DescribeUiCapabilities(StyledBtnBuilder);
                const FGV2UiCapabilityTree StyledBtnTree = StyledBtnBuilder.Build();
                const FGV2UiPropertyCapability* StyledItemsCap = StyledBtnTree.FindProperty(TEXT("items"));
                TestNotNull(TEXT("Styled ButtonList items capability found"), StyledItemsCap);

                if (StyledItemsCap != nullptr)
                {
                    TSharedPtr<IGV2PropertyConsumer> StyledCollConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                        EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost);

                    auto StyledBtnItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                    StyledBtnItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                    StyledBtnItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                    StyledBtnItemSpec->Fields.push_back({ "text", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                    StyledBtnItemSpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });
                    static_cast<FGV2KeyedCollectionPropertyConsumer*>(StyledCollConsumer.Get())->SetCompiledItemSpec(
                        StyledBtnItemSpec, TEXT("core:schema.ui_field.button_list.v2"), TEXT("items"));

                    FGV2TextViewModel StyledBtnText;
                    StyledBtnText.Text = FText::FromString(TEXT("Styled Button"));
                    StyledBtnText.StyleToken = TEXT("default");

                    TMap<FString, FGV2PreparedUiValue> StyledItemMap;
                    StyledItemMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("styled_btn")));
                    StyledItemMap.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(StyledBtnText));
                    StyledItemMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd_styled@1:1"))));

                    TArray<FGV2PreparedUiValue> StyledElements;
                    StyledElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(StyledItemMap)));

                    FString StyledPrepErr, StyledCommitErr;
                    const bool bStyledPrep = StyledCollConsumer->Prepare(
                        FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(StyledElements)),
                        *StyledItemsCap,
                        StyledButtonList,
                        StyledPrepErr);
                    TestTrue(TEXT("Styled ButtonList Prepare succeeds"), bStyledPrep);

                    const bool bStyledCommit = StyledCollConsumer->Commit(StyledButtonList, StyledCommitErr);
                    TestTrue(TEXT("Styled ButtonList Commit succeeds"), bStyledCommit);
                    TestEqual(TEXT("Styled ButtonList container has 1 child"), StyledBtnBox->GetChildrenCount(), 1);

                    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
                    TestNotNull(TEXT("Theme resolved for padding assertion"), Theme);

                    if (Theme != nullptr && StyledBtnBox->GetChildrenCount() == 1)
                    {
                        UVerticalBoxSlot* NewSlot = Cast<UVerticalBoxSlot>(StyledBtnBox->GetSlots()[0]);
                        TestNotNull(TEXT("New button slot is a UVerticalBoxSlot"), NewSlot);
                        if (NewSlot != nullptr)
                        {
                            const FMargin ActualPadding = NewSlot->GetPadding();
                            TestEqual(TEXT("New button slot padding.Left matches theme"), ActualPadding.Left, Theme->ButtonListItemPadding.Left);
                            TestEqual(TEXT("New button slot padding.Top matches theme"), ActualPadding.Top, Theme->ButtonListItemPadding.Top);
                            TestEqual(TEXT("New button slot padding.Right matches theme"), ActualPadding.Right, Theme->ButtonListItemPadding.Right);
                            TestEqual(TEXT("New button slot padding.Bottom matches theme"), ActualPadding.Bottom, Theme->ButtonListItemPadding.Bottom);
                        }
                    }
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

                auto DdItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                DdItemSpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                DdItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
                DdItemSpec->Fields.push_back({ "text", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                static_cast<FGV2KeyedCollectionPropertyConsumer*>(DdCollConsumer.Get())->SetCompiledItemSpec(
                    DdItemSpec, TEXT("core:schema.ui_field.dropdown_select.v1"), TEXT("items"));

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
        // REV3-04 closure: every declared Modal capability (title/content/buttons/backdrop_close_action)
        // has a real consumer wired through DescribeUiCapabilities — none is silently dropped/partial.
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
        // REV3-06 closure: tabs go through FGV2TabContainerTabsPropertyConsumer (not a raw ScreenId
        // assignment) — screen_id kind/registration is validated per tab (12b below), and this is the
        // same nested-screen consumer path the Screen Registry resolves through, not an ad hoc bypass.
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

        // 13. UPP-24: TopBar (DUC-08: generic declared composite) and UGV2LocationPlayerStatusWidgetBase as IGV2UiPropertyHost
        {
            auto MakeScalarSpec = [](const GV2ContentCore::EScalarFieldKind Kind,
                                     const TOptional<double> Min = {},
                                     const TOptional<double> Max = {}) -> GV2ContentCore::FCompiledUiFieldSpecPtr
            {
                auto Spec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                Spec->Kind = GV2ContentCore::EUiFieldKind::Scalar;
                GV2ContentCore::FScalarFieldSpec Scalar;
                Scalar.Kind = Kind;
                if (Min.IsSet()) { Scalar.MinimumNumber = Min.GetValue(); }
                if (Max.IsSet()) { Scalar.MaximumNumber = Max.GetValue(); }
                Spec->Scalar = MoveTemp(Scalar);
                return Spec;
            };

            auto MakeKeySpec = []() -> GV2ContentCore::FCompiledUiFieldSpecPtr
            {
                auto Spec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                Spec->Kind = GV2ContentCore::EUiFieldKind::Key;
                return Spec;
            };

            auto MakeRefSpec = [](const std::string& TargetKind) -> GV2ContentCore::FCompiledUiFieldSpecPtr
            {
                auto Spec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                Spec->Kind = GV2ContentCore::EUiFieldKind::Ref;
                Spec->RefTargetKind = TargetKind;
                return Spec;
            };

            // Helper to prepare and commit properties on a host widget
            auto ApplyHostProps = [&](
                UUserWidget* Host,
                const TArray<TPair<FString, FGV2PreparedUiValue>>& Props,
                const GV2ContentCore::FCompiledUiFieldSpec& Schema,
                const FString& SchemaId,
                FString& OutError) -> bool
            {
                if (Host == nullptr) return false;
                IGV2UiPropertyHost* PropHost = Cast<IGV2UiPropertyHost>(Host);
                if (!PropHost) return false;

                FGV2UiCapabilityBuilder Builder;
                PropHost->DescribeUiCapabilities(Builder);
                const FGV2UiCapabilityTree Caps = Builder.Build();

                TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(CopyTemp(Props));
                FGV2UiHostMutationPlan Plan;
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
                
                const bool bPrepared = PrepareUiHostProperties(
                    Host, Caps, *Candidate, Schema, SchemaId,
                    SchemaId, PropHost->GetPropertyHostState().GetLastCommittedProperties(), Plan, Diagnostics);

                if (!bPrepared)
                {
                    OutError = Diagnostics.Num() > 0 ? Diagnostics[0].Message : TEXT("Prepare failed");
                    UE_LOG(LogTemp, Warning, TEXT("ApplyHostProps prepare failed on %s: %s"), *Host->GetName(), *OutError);
                    return false;
                }

                FString FailedPath;
                const bool bCommitted = CommitUiHostProperties(Host, Plan, FailedPath, OutError);
                if (!bCommitted)
                {
                    UE_LOG(LogTemp, Warning, TEXT("ApplyHostProps commit failed on %s at %s: %s"), *Host->GetName(), *FailedPath, *OutError);
                }
                else
                {
                    PropHost->GetPropertyHostState().SetLastCommittedProperties(*Candidate);
                }
                return bCommitted;
            };

            auto ResetHostProps = [&](
                UUserWidget* Host,
                const GV2ContentCore::FCompiledUiFieldSpec& Schema,
                const FString& SchemaId)
            {
                if (Host == nullptr) return;
                IGV2UiPropertyHost* PropHost = Cast<IGV2UiPropertyHost>(Host);
                if (!PropHost) return;

                FGV2UiCapabilityBuilder Builder;
                PropHost->DescribeUiCapabilities(Builder);
                const FGV2UiCapabilityTree Caps = Builder.Build();

                TArray<TPair<FString, FGV2PreparedUiValue>> EmptyProps;
                TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(EmptyProps));
                FGV2UiHostMutationPlan Plan;
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;

                PrepareUiHostProperties(
                    Host, Caps, *Candidate, Schema, SchemaId,
                    SchemaId, PropHost->GetPropertyHostState().GetLastCommittedProperties(), Plan, Diagnostics);

                FString FailedPath, Error;
                CommitUiHostProperties(Host, Plan, FailedPath, Error);
                PropHost->GetPropertyHostState().SetLastCommittedProperties(FGV2PreparedUiObject());
            };

            // 13a. TopBar (DUC-08: generic declared composite) Property Host Reconciliation
            {
                UGV2DeclaredCompositeWidgetBase* TopBar = CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, UGV2DeclaredCompositeWidgetBase::StaticClass());
                TestNotNull(TEXT("TopBar instantiated"), TopBar);

                TopBar->WidgetTree = NewObject<UWidgetTree>(TopBar);
                UVerticalBox* TopBarRoot = TopBar->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
                TopBar->WidgetTree->RootWidget = TopBarRoot;
                UGV2TextWidgetBase* DayBlock = TopBar->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
                UGV2TextWidgetBase* LocBlock = TopBar->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("LocationText"));
                UGV2TextWidgetBase* ResBlock = TopBar->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("PrimaryResourceText"));
                TopBarRoot->AddChildToVerticalBox(DayBlock);
                TopBarRoot->AddChildToVerticalBox(LocBlock);
                TopBarRoot->AddChildToVerticalBox(ResBlock);

                TopBar->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });
                TopBar->DeclaredCapabilities.Add({ FName(TEXT("location")), FName(TEXT("LocationText")), EGV2DeclaredUiCapabilityKind::Text });
                TopBar->DeclaredCapabilities.Add({ FName(TEXT("primary_resource")), FName(TEXT("PrimaryResourceText")), EGV2DeclaredUiCapabilityKind::Text });
                TopBar->DeclaredCapabilities.Add({ FName(TEXT("key")), NAME_None, EGV2DeclaredUiCapabilityKind::Key });

                FGV2UiCapabilityBuilder TopBuilder;
                TopBar->DescribeUiCapabilities(TopBuilder);
                FGV2UiCapabilityTree TopTree = TopBuilder.Build();
                TestNotNull(TEXT("TopBar capability 'day' exists"), TopTree.FindProperty(TEXT("day")));
                TestNotNull(TEXT("TopBar capability 'location' exists"), TopTree.FindProperty(TEXT("location")));
                TestNotNull(TEXT("TopBar capability 'primary_resource' exists"), TopTree.FindProperty(TEXT("primary_resource")));
                TestNotNull(TEXT("TopBar capability 'key' exists"), TopTree.FindProperty(TEXT("key")));

                GV2ContentCore::FCompiledUiFieldSpec TopBarSchema;
                TopBarSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
                TopBarSchema.Fields.push_back({ "day", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                TopBarSchema.Fields.push_back({ "location", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                TopBarSchema.Fields.push_back({ "primary_resource", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                TopBarSchema.Fields.push_back({ "key", false, MakeKeySpec() });

                FGV2TextViewModel DayVM; DayVM.Text = FText::FromString(TEXT("Day 42"));
                FGV2TextViewModel LocVM; LocVM.Text = FText::FromString(TEXT("Tavern"));
                FGV2TextViewModel ResVM; ResVM.Text = FText::FromString(TEXT("Gold: 1000"));

                TArray<TPair<FString, FGV2PreparedUiValue>> TopBarValues;
                TopBarValues.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeText(DayVM));
                TopBarValues.Emplace(TEXT("location"), FGV2PreparedUiValue::MakeText(LocVM));
                TopBarValues.Emplace(TEXT("primary_resource"), FGV2PreparedUiValue::MakeText(ResVM));
                TopBarValues.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("top_bar_inst")));

                FString ApplyErr;
                TestTrue(TEXT("TopBar ApplyHostProps succeeds"), ApplyHostProps(TopBar, TopBarValues, TopBarSchema, TEXT("textsystem:schema.ui_field.location_top_bar.v1"), ApplyErr));

                TestEqual(TEXT("TopBar Day text committed"), DayBlock->GetTextContent().ToString(), TEXT("Day 42"));
                TestEqual(TEXT("TopBar Location text committed"), LocBlock->GetTextContent().ToString(), TEXT("Tavern"));
                TestEqual(TEXT("TopBar PrimaryResource text committed"), ResBlock->GetTextContent().ToString(), TEXT("Gold: 1000"));
                TestEqual(TEXT("TopBar Key committed"), TopBar->GetKey(), FName(TEXT("top_bar_inst")));

                // Reset
                ResetHostProps(TopBar, TopBarSchema, TEXT("textsystem:schema.ui_field.location_top_bar.v1"));
                TestTrue(TEXT("TopBar Day text cleared on reset"), DayBlock->GetTextContent().IsEmpty());
                TestTrue(TEXT("TopBar Location text cleared on reset"), LocBlock->GetTextContent().IsEmpty());
                TestTrue(TEXT("TopBar PrimaryResource text cleared on reset"), ResBlock->GetTextContent().IsEmpty());
                TestEqual(TEXT("TopBar Key cleared on reset"), TopBar->GetKey(), NAME_None);

                // Negative: wrong kind for text
                TArray<TPair<FString, FGV2PreparedUiValue>> BadTopBarValues;
                BadTopBarValues.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeNumber(123.0));
                TestFalse(TEXT("TopBar Prepare rejects number for text"), ApplyHostProps(TopBar, BadTopBarValues, TopBarSchema, TEXT("textsystem:schema.ui_field.location_top_bar.v1"), ApplyErr));
            }

            // 13b. PlayerStatus Property Host Reconciliation
            {
                UGV2LocationPlayerStatusWidgetBase* PlayerStatus = CreateWidget<UGV2LocationPlayerStatusWidgetBase>(TestWorld, UGV2LocationPlayerStatusWidgetBase::StaticClass());
                TestNotNull(TEXT("PlayerStatus instantiated"), PlayerStatus);

                UGV2TextWidgetBase* NameBlock = NewObject<UGV2TextWidgetBase>(PlayerStatus);
                UGV2PortraitWidgetBase* PortraitWidget = NewObject<UGV2PortraitWidgetBase>(PlayerStatus);
                UImage* InnerPortrait = NewObject<UImage>(PortraitWidget);
                if (FProperty* Prop = UGV2PortraitWidgetBase::StaticClass()->FindPropertyByName(TEXT("PortraitImage")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UImage>>(PortraitWidget) = InnerPortrait;
                }
                UGV2ListViewWidgetBase* MeterRep = NewObject<UGV2ListViewWidgetBase>(PlayerStatus);
                UVerticalBox* MeterBox = NewObject<UVerticalBox>(PlayerStatus);
                MeterRep->SetContainerPanel(MeterBox);

                UGV2ListViewWidgetBase* ItemRep = NewObject<UGV2ListViewWidgetBase>(PlayerStatus);
                UVerticalBox* ItemBox = NewObject<UVerticalBox>(PlayerStatus);
                ItemRep->SetContainerPanel(ItemBox);

                UGV2ListViewWidgetBase* EffectRep = NewObject<UGV2ListViewWidgetBase>(PlayerStatus);
                UVerticalBox* EffectBox = NewObject<UVerticalBox>(PlayerStatus);
                EffectRep->SetContainerPanel(EffectBox);

                if (FProperty* Prop = UGV2LocationPlayerStatusWidgetBase::StaticClass()->FindPropertyByName(TEXT("PlayerNameText")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2TextWidgetBase>>(PlayerStatus) = NameBlock;
                }
                if (FProperty* Prop = UGV2LocationPlayerStatusWidgetBase::StaticClass()->FindPropertyByName(TEXT("Portrait")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2PortraitWidgetBase>>(PlayerStatus) = PortraitWidget;
                }
                if (FProperty* Prop = UGV2LocationPlayerStatusWidgetBase::StaticClass()->FindPropertyByName(TEXT("MeterRepeater")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(PlayerStatus) = MeterRep;
                }
                if (FProperty* Prop = UGV2LocationPlayerStatusWidgetBase::StaticClass()->FindPropertyByName(TEXT("ItemRepeater")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(PlayerStatus) = ItemRep;
                }
                if (FProperty* Prop = UGV2LocationPlayerStatusWidgetBase::StaticClass()->FindPropertyByName(TEXT("EffectRepeater")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(PlayerStatus) = EffectRep;
                }

                FGV2UiCapabilityBuilder StatusBuilder;
                PlayerStatus->DescribeUiCapabilities(StatusBuilder);
                FGV2UiCapabilityTree StatusTree = StatusBuilder.Build();
                TestNotNull(TEXT("PlayerStatus capability 'name' exists"), StatusTree.FindProperty(TEXT("name")));
                TestNotNull(TEXT("PlayerStatus capability 'portrait_resource_id' exists"), StatusTree.FindProperty(TEXT("portrait_resource_id")));
                TestNotNull(TEXT("PlayerStatus capability 'meters' exists"), StatusTree.FindProperty(TEXT("meters")));
                TestNotNull(TEXT("PlayerStatus capability 'items' exists"), StatusTree.FindProperty(TEXT("items")));
                TestNotNull(TEXT("PlayerStatus capability 'effects' exists"), StatusTree.FindProperty(TEXT("effects")));
                TestNotNull(TEXT("PlayerStatus capability 'key' exists"), StatusTree.FindProperty(TEXT("key")));

                // Schema for PlayerStatus
                GV2ContentCore::FCompiledUiFieldSpec PlayerStatusSchema;
                PlayerStatusSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
                PlayerStatusSchema.Fields.push_back({ "name", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                PlayerStatusSchema.Fields.push_back({ "portrait_resource_id", false, MakeRefSpec("resource") });

                auto MeterEntrySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                MeterEntrySpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                MeterEntrySpec->Fields.push_back({ "key", true, MakeKeySpec() });
                MeterEntrySpec->Fields.push_back({ "percent", true, MakeScalarSpec(GV2ContentCore::EScalarFieldKind::Number, 0.0, 1.0) });
                MeterEntrySpec->Fields.push_back({ "label", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });

                auto MetersArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                MetersArraySpec->Kind = GV2ContentCore::EUiFieldKind::Array;
                MetersArraySpec->Items = MeterEntrySpec;
                MetersArraySpec->KeyedBy = "key";
                PlayerStatusSchema.Fields.push_back({ "meters", false, MetersArraySpec });

                auto ItemEntrySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                ItemEntrySpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                ItemEntrySpec->Fields.push_back({ "key", true, MakeKeySpec() });
                ItemEntrySpec->Fields.push_back({ "resource_id", true, MakeRefSpec("resource") });

                auto ItemsArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                ItemsArraySpec->Kind = GV2ContentCore::EUiFieldKind::Array;
                ItemsArraySpec->Items = ItemEntrySpec;
                ItemsArraySpec->KeyedBy = "key";
                PlayerStatusSchema.Fields.push_back({ "items", false, ItemsArraySpec });
                PlayerStatusSchema.Fields.push_back({ "effects", false, ItemsArraySpec });
                PlayerStatusSchema.Fields.push_back({ "key", false, MakeKeySpec() });

                // Prepare initial valid values
                FGV2TextViewModel NameVM; NameVM.Text = FText::FromString(TEXT("Hero"));
                FGV2TextViewModel HpLabel; HpLabel.Text = FText::FromString(TEXT("80/100"));
                FGV2TextViewModel StamLabel; StamLabel.Text = FText::FromString(TEXT("50/100"));

                // Meters
                TArray<TPair<FString, FGV2PreparedUiValue>> MeterHpMap;
                MeterHpMap.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("hp")));
                MeterHpMap.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.8));
                MeterHpMap.Emplace(TEXT("label"), FGV2PreparedUiValue::MakeText(HpLabel));

                TArray<TPair<FString, FGV2PreparedUiValue>> MeterStamMap;
                MeterStamMap.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("stamina")));
                MeterStamMap.Emplace(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.5));
                MeterStamMap.Emplace(TEXT("label"), FGV2PreparedUiValue::MakeText(StamLabel));

                TArray<FGV2PreparedUiValue> MeterElements;
                MeterElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(MeterHpMap))));
                MeterElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(MeterStamMap))));

                // Items
                TArray<TPair<FString, FGV2PreparedUiValue>> Item1Map;
                Item1Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@1")));
                Item1Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Item2Map;
                Item2Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@2")));
                Item2Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> ItemElements;
                ItemElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Item1Map))));
                ItemElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Item2Map))));

                // Effects
                TArray<TPair<FString, FGV2PreparedUiValue>> Effect1Map;
                Effect1Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("effect@1")));
                Effect1Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> EffectElements;
                EffectElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Effect1Map))));

                TArray<TPair<FString, FGV2PreparedUiValue>> StatusValues;
                StatusValues.Emplace(TEXT("name"), FGV2PreparedUiValue::MakeText(NameVM));
                StatusValues.Emplace(TEXT("portrait_resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));
                StatusValues.Emplace(TEXT("meters"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(MeterElements))));
                StatusValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(ItemElements))));
                StatusValues.Emplace(TEXT("effects"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(EffectElements))));
                StatusValues.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("player_status_inst")));

                FString StatusErr;
                TestTrue(TEXT("PlayerStatus ApplyHostProps succeeds"), ApplyHostProps(PlayerStatus, StatusValues, PlayerStatusSchema, TEXT("textsystem:schema.ui_field.location_player_status.v1"), StatusErr));

                TestEqual(TEXT("PlayerStatus Name committed"), NameBlock->GetTextContent().ToString(), TEXT("Hero"));
                TestEqual(TEXT("PlayerStatus Key committed"), PlayerStatus->GetKey(), FName(TEXT("player_status_inst")));
                TestEqual(TEXT("MeterRepeater count is 2"), MeterRep->GetEntryCount(), 2);
                TestEqual(TEXT("ItemRepeater count is 2"), ItemRep->GetEntryCount(), 2);
                TestEqual(TEXT("EffectRepeater count is 1"), EffectRep->GetEntryCount(), 1);

                UWidget* Item1Widget = ItemRep->GetEntryWidget(FName(TEXT("item@1")));
                UWidget* Item2Widget = ItemRep->GetEntryWidget(FName(TEXT("item@2")));
                TestNotNull(TEXT("Item 1 widget exists"), Item1Widget);
                TestNotNull(TEXT("Item 2 widget exists"), Item2Widget);

                // Reorder items: item@2, item@3, item@1
                TArray<TPair<FString, FGV2PreparedUiValue>> Item3Map;
                Item3Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@3")));
                Item3Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Item1MapCopy;
                Item1MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@1")));
                Item1MapCopy.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Item2MapCopy;
                Item2MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@2")));
                Item2MapCopy.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> ReorderedItems;
                ReorderedItems.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Item2MapCopy))));
                ReorderedItems.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Item3Map))));
                ReorderedItems.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Item1MapCopy))));

                TArray<TPair<FString, FGV2PreparedUiValue>> ReorderValues;
                ReorderValues.Emplace(TEXT("name"), FGV2PreparedUiValue::MakeText(NameVM));
                ReorderValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(ReorderedItems))));

                TestTrue(TEXT("PlayerStatus Reorder Apply succeeds"), ApplyHostProps(PlayerStatus, ReorderValues, PlayerStatusSchema, TEXT("textsystem:schema.ui_field.location_player_status.v1"), StatusErr));
                TestEqual(TEXT("ItemRepeater count is 3 after reorder"), ItemRep->GetEntryCount(), 3);
                TestEqual(TEXT("Item 1 widget reused across reorder"), ItemRep->GetEntryWidget(FName(TEXT("item@1"))), Item1Widget);
                TestEqual(TEXT("Item 2 widget reused across reorder"), ItemRep->GetEntryWidget(FName(TEXT("item@2"))), Item2Widget);

                // Rollback verification on invalid candidate
                TArray<TPair<FString, FGV2PreparedUiValue>> BadItemMap;
                BadItemMap.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("item@bad")));
                BadItemMap.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeNumber(999.0)); // invalid kind

                TArray<FGV2PreparedUiValue> FailingItems;
                FailingItems.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(BadItemMap))));

                TArray<TPair<FString, FGV2PreparedUiValue>> FailingValues;
                FailingValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(FailingItems))));

                TestFalse(TEXT("PlayerStatus Prepare fails on invalid item resource_id kind"), ApplyHostProps(PlayerStatus, FailingValues, PlayerStatusSchema, TEXT("textsystem:schema.ui_field.location_player_status.v1"), StatusErr));
                TestEqual(TEXT("ItemRepeater count remains 3 after failed prepare (rollback)"), ItemRep->GetEntryCount(), 3);
                TestEqual(TEXT("Item 1 widget preserved without mutation"), ItemRep->GetEntryWidget(FName(TEXT("item@1"))), Item1Widget);

                // Reset
                ResetHostProps(PlayerStatus, PlayerStatusSchema, TEXT("textsystem:schema.ui_field.location_player_status.v1"));
                TestTrue(TEXT("PlayerStatus Name cleared on reset"), NameBlock->GetTextContent().IsEmpty());
                TestEqual(TEXT("PlayerStatus MeterRepeater cleared on reset"), MeterRep->GetEntryCount(), 0);
                TestEqual(TEXT("PlayerStatus ItemRepeater cleared on reset"), ItemRep->GetEntryCount(), 0);
                TestEqual(TEXT("PlayerStatus EffectRepeater cleared on reset"), EffectRep->GetEntryCount(), 0);
            }

            // 13c. LocationScene Property Host Reconciliation
            {
                UGV2LocationSceneWidgetBase* SceneWidget = CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, UGV2LocationSceneWidgetBase::StaticClass());
                TestNotNull(TEXT("SceneWidget instantiated"), SceneWidget);

                UGV2ImageWidgetBase* BgTile = NewObject<UGV2ImageWidgetBase>(SceneWidget);
                BgTile->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
                UImage* InnerBgTileImage = NewObject<UImage>(BgTile);
                if (FProperty* Prop = UGV2ImageWidgetBase::StaticClass()->FindPropertyByName(TEXT("Image")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UImage>>(BgTile) = InnerBgTileImage;
                }

                UGV2ImageWidgetBase* Bg = NewObject<UGV2ImageWidgetBase>(SceneWidget);
                Bg->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
                UImage* InnerBgImage = NewObject<UImage>(Bg);
                if (FProperty* Prop = UGV2ImageWidgetBase::StaticClass()->FindPropertyByName(TEXT("Image")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UImage>>(Bg) = InnerBgImage;
                }

                UGV2TextWidgetBase* ContextText = NewObject<UGV2TextWidgetBase>(SceneWidget);
                UGV2ListViewWidgetBase* CharRep = NewObject<UGV2ListViewWidgetBase>(SceneWidget);
                UVerticalBox* CharBox = NewObject<UVerticalBox>(SceneWidget);
                CharRep->SetContainerPanel(CharBox);

                if (FProperty* Prop = UGV2LocationSceneWidgetBase::StaticClass()->FindPropertyByName(TEXT("BackgroundTile")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ImageWidgetBase>>(SceneWidget) = BgTile;
                }
                if (FProperty* Prop = UGV2LocationSceneWidgetBase::StaticClass()->FindPropertyByName(TEXT("Background")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ImageWidgetBase>>(SceneWidget) = Bg;
                }
                if (FProperty* Prop = UGV2LocationSceneWidgetBase::StaticClass()->FindPropertyByName(TEXT("SceneContextText")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2TextWidgetBase>>(SceneWidget) = ContextText;
                }
                if (FProperty* Prop = UGV2LocationSceneWidgetBase::StaticClass()->FindPropertyByName(TEXT("CharacterRepeater")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(SceneWidget) = CharRep;
                }

                FGV2UiCapabilityBuilder SceneBuilder;
                SceneWidget->DescribeUiCapabilities(SceneBuilder);
                FGV2UiCapabilityTree SceneTree = SceneBuilder.Build();
                TestNotNull(TEXT("Scene capability 'background_tile_resource_id' exists"), SceneTree.FindProperty(TEXT("background_tile_resource_id")));
                TestNotNull(TEXT("Scene capability 'background_resource_id' exists"), SceneTree.FindProperty(TEXT("background_resource_id")));
                TestNotNull(TEXT("Scene capability 'context_text' exists"), SceneTree.FindProperty(TEXT("context_text")));
                TestNotNull(TEXT("Scene capability 'characters' exists"), SceneTree.FindProperty(TEXT("characters")));
                TestNotNull(TEXT("Scene capability 'key' exists"), SceneTree.FindProperty(TEXT("key")));

                // Schema for LocationScene
                GV2ContentCore::FCompiledUiFieldSpec SceneSchema;
                SceneSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
                SceneSchema.Fields.push_back({ "background_tile_resource_id", false, MakeRefSpec("resource") });
                SceneSchema.Fields.push_back({ "background_resource_id", false, MakeRefSpec("resource") });
                SceneSchema.Fields.push_back({ "context_text", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });

                auto CharEntrySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                CharEntrySpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                CharEntrySpec->Fields.push_back({ "key", true, MakeKeySpec() });
                CharEntrySpec->Fields.push_back({ "resource_id", true, MakeRefSpec("resource") });

                auto CharsArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                CharsArraySpec->Kind = GV2ContentCore::EUiFieldKind::Array;
                CharsArraySpec->Items = CharEntrySpec;
                CharsArraySpec->KeyedBy = "key";
                SceneSchema.Fields.push_back({ "characters", false, CharsArraySpec });
                SceneSchema.Fields.push_back({ "key", false, MakeKeySpec() });

                // Values
                FGV2TextViewModel ContextVM;
                ContextVM.Text = FText::FromString(TEXT("Market Square"));

                TArray<TPair<FString, FGV2PreparedUiValue>> Char1Map;
                Char1Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("c1")));
                Char1Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Char2Map;
                Char2Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("c2")));
                Char2Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> CharElements;
                CharElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Char1Map))));
                CharElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Char2Map))));

                TArray<TPair<FString, FGV2PreparedUiValue>> SceneValues;
                SceneValues.Emplace(TEXT("background_tile_resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:resource.ui.old_paper_tile_256"), TEXT("resource")));
                SceneValues.Emplace(TEXT("background_resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));
                SceneValues.Emplace(TEXT("context_text"), FGV2PreparedUiValue::MakeText(ContextVM));
                SceneValues.Emplace(TEXT("characters"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(CharElements))));
                SceneValues.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("scene_inst")));

                FString SceneErr;
                TestTrue(TEXT("Scene ApplyHostProps succeeds"), ApplyHostProps(SceneWidget, SceneValues, SceneSchema, TEXT("textsystem:schema.ui_field.location_scene.v1"), SceneErr));
                TestEqual(TEXT("Scene Context text committed"), ContextText->GetTextContent().ToString(), TEXT("Market Square"));
                TestEqual(TEXT("Scene Key committed"), SceneWidget->GetKey(), FName(TEXT("scene_inst")));
                TestEqual(TEXT("Scene Character count is 2"), CharRep->GetEntryCount(), 2);

                UWidget* C1Widget = CharRep->GetEntryWidget(FName(TEXT("c1")));
                UWidget* C2Widget = CharRep->GetEntryWidget(FName(TEXT("c2")));
                TestNotNull(TEXT("C1 widget exists"), C1Widget);
                TestNotNull(TEXT("C2 widget exists"), C2Widget);

                // Reorder characters
                TArray<TPair<FString, FGV2PreparedUiValue>> Char3Map;
                Char3Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("c3")));
                Char3Map.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Char1MapCopy;
                Char1MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("c1")));
                Char1MapCopy.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));

                TArray<TPair<FString, FGV2PreparedUiValue>> Char2MapCopy;
                Char2MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("c2")));
                Char2MapCopy.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));

                TArray<FGV2PreparedUiValue> ReorderedChars;
                ReorderedChars.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Char2MapCopy))));
                ReorderedChars.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Char3Map))));
                ReorderedChars.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Char1MapCopy))));

                TArray<TPair<FString, FGV2PreparedUiValue>> ReorderSceneValues;
                ReorderSceneValues.Emplace(TEXT("context_text"), FGV2PreparedUiValue::MakeText(ContextVM));
                ReorderSceneValues.Emplace(TEXT("characters"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(ReorderedChars))));

                TestTrue(TEXT("Scene Reorder Apply succeeds"), ApplyHostProps(SceneWidget, ReorderSceneValues, SceneSchema, TEXT("textsystem:schema.ui_field.location_scene.v1"), SceneErr));
                TestEqual(TEXT("Scene Character count is 3 after reorder"), CharRep->GetEntryCount(), 3);
                TestEqual(TEXT("C1 widget preserved across reorder"), CharRep->GetEntryWidget(FName(TEXT("c1"))), C1Widget);
                TestEqual(TEXT("C2 widget preserved across reorder"), CharRep->GetEntryWidget(FName(TEXT("c2"))), C2Widget);

                // Rollback on failure
                TArray<TPair<FString, FGV2PreparedUiValue>> BadCharMap;
                BadCharMap.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("bad_c")));
                BadCharMap.Emplace(TEXT("resource_id"), FGV2PreparedUiValue::MakeNumber(42.0));

                TArray<FGV2PreparedUiValue> FailingChars;
                FailingChars.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(BadCharMap))));

                TArray<TPair<FString, FGV2PreparedUiValue>> FailingSceneValues;
                FailingSceneValues.Emplace(TEXT("characters"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(FailingChars))));

                TestFalse(TEXT("Scene Prepare fails on invalid char resource_id"), ApplyHostProps(SceneWidget, FailingSceneValues, SceneSchema, TEXT("textsystem:schema.ui_field.location_scene.v1"), SceneErr));
                TestEqual(TEXT("Scene Character count remains 3 after failed prepare"), CharRep->GetEntryCount(), 3);

                // Reset
                ResetHostProps(SceneWidget, SceneSchema, TEXT("textsystem:schema.ui_field.location_scene.v1"));
                TestTrue(TEXT("Scene Context text cleared on reset"), ContextText->GetTextContent().IsEmpty());
                TestEqual(TEXT("Scene Characters cleared on reset"), CharRep->GetEntryCount(), 0);
                TestEqual(TEXT("Scene Key cleared on reset"), SceneWidget->GetKey(), NAME_None);
            }

            // 13d. LocationCommandPanel Property Host Reconciliation
            {
                UGV2LocationCommandPanelWidgetBase* CmdPanel = CreateWidget<UGV2LocationCommandPanelWidgetBase>(TestWorld, UGV2LocationCommandPanelWidgetBase::StaticClass());
                TestNotNull(TEXT("CmdPanel instantiated"), CmdPanel);

                UGV2ListViewWidgetBase* BtnRep = NewObject<UGV2ListViewWidgetBase>(CmdPanel);
                UWrapBox* WrapBox = NewObject<UWrapBox>(CmdPanel);
                BtnRep->SetContainerPanel(WrapBox);

                if (FProperty* Prop = UGV2LocationCommandPanelWidgetBase::StaticClass()->FindPropertyByName(TEXT("ButtonRepeater")))
                {
                    *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(CmdPanel) = BtnRep;
                }

                FGV2UiCapabilityBuilder CmdBuilder;
                CmdPanel->DescribeUiCapabilities(CmdBuilder);
                FGV2UiCapabilityTree CmdTree = CmdBuilder.Build();
                TestNotNull(TEXT("CmdPanel capability 'items' exists"), CmdTree.FindProperty(TEXT("items")));
                TestNotNull(TEXT("CmdPanel capability 'key' exists"), CmdTree.FindProperty(TEXT("key")));

                // Schema for LocationCommands
                GV2ContentCore::FCompiledUiFieldSpec CmdSchema;
                CmdSchema.Kind = GV2ContentCore::EUiFieldKind::Object;

                auto BtnEntrySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                BtnEntrySpec->Kind = GV2ContentCore::EUiFieldKind::Object;
                BtnEntrySpec->Fields.push_back({ "key", true, MakeKeySpec() });
                BtnEntrySpec->Fields.push_back({ "text", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                BtnEntrySpec->Fields.push_back({ "binding", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Binding) });

                auto BtnsArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                BtnsArraySpec->Kind = GV2ContentCore::EUiFieldKind::Array;
                BtnsArraySpec->Items = BtnEntrySpec;
                BtnsArraySpec->KeyedBy = "key";
                CmdSchema.Fields.push_back({ "items", true, BtnsArraySpec });
                CmdSchema.Fields.push_back({ "key", false, MakeKeySpec() });

                // Values
                FGV2TextViewModel TalkVM; TalkVM.Text = FText::FromString(TEXT("Talk"));
                FGV2TextViewModel LeaveVM; LeaveVM.Text = FText::FromString(TEXT("Leave"));

                TArray<TPair<FString, FGV2PreparedUiValue>> Btn1Map;
                Btn1Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("b1")));
                Btn1Map.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TalkVM));
                Btn1Map.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:1"))));

                TArray<TPair<FString, FGV2PreparedUiValue>> Btn2Map;
                Btn2Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("b2")));
                Btn2Map.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(LeaveVM));
                Btn2Map.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:2"))));

                TArray<FGV2PreparedUiValue> BtnElements;
                BtnElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Btn1Map))));
                BtnElements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Btn2Map))));

                TArray<TPair<FString, FGV2PreparedUiValue>> CmdValues;
                CmdValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(BtnElements))));
                CmdValues.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("cmd_panel_inst")));

                FString CmdErr;
                TestTrue(TEXT("CmdPanel ApplyHostProps succeeds"), ApplyHostProps(CmdPanel, CmdValues, CmdSchema, TEXT("textsystem:schema.ui_field.location_commands.v1"), CmdErr));
                TestEqual(TEXT("CmdPanel Key committed"), CmdPanel->GetKey(), FName(TEXT("cmd_panel_inst")));
                TestEqual(TEXT("CmdPanel Button count is 2"), BtnRep->GetEntryCount(), 2);

                UGV2ButtonWidgetBase* B1Widget = Cast<UGV2ButtonWidgetBase>(BtnRep->GetEntryWidget(FName(TEXT("b1"))));
                UGV2ButtonWidgetBase* B2Widget = Cast<UGV2ButtonWidgetBase>(BtnRep->GetEntryWidget(FName(TEXT("b2"))));
                TestNotNull(TEXT("B1 widget exists"), B1Widget);
                TestNotNull(TEXT("B2 widget exists"), B2Widget);
                if (B1Widget != nullptr)
                {
                    TestEqual(TEXT("B1 text committed"), B1Widget->GetTextViewModel().Text.ToString(), TEXT("Talk"));
                    TestTrue(TEXT("B1 binding handle valid"), B1Widget->GetBindingHandle().IsValid());
                }

                // Reorder buttons
                FGV2TextViewModel AttackVM; AttackVM.Text = FText::FromString(TEXT("Attack"));

                TArray<TPair<FString, FGV2PreparedUiValue>> Btn3Map;
                Btn3Map.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("b3")));
                Btn3Map.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(AttackVM));
                Btn3Map.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:3"))));

                TArray<TPair<FString, FGV2PreparedUiValue>> Btn1MapCopy;
                Btn1MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("b1")));
                Btn1MapCopy.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TalkVM));
                Btn1MapCopy.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:1"))));

                TArray<TPair<FString, FGV2PreparedUiValue>> Btn2MapCopy;
                Btn2MapCopy.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("b2")));
                Btn2MapCopy.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(LeaveVM));
                Btn2MapCopy.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("cmd@1:2"))));

                TArray<FGV2PreparedUiValue> ReorderedBtns;
                ReorderedBtns.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Btn2MapCopy))));
                ReorderedBtns.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Btn3Map))));
                ReorderedBtns.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Btn1MapCopy))));

                TArray<TPair<FString, FGV2PreparedUiValue>> ReorderCmdValues;
                ReorderCmdValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(ReorderedBtns))));

                TestTrue(TEXT("CmdPanel Reorder Apply succeeds"), ApplyHostProps(CmdPanel, ReorderCmdValues, CmdSchema, TEXT("textsystem:schema.ui_field.location_commands.v1"), CmdErr));
                TestEqual(TEXT("CmdPanel Button count is 3 after reorder"), BtnRep->GetEntryCount(), 3);
                TestTrue(TEXT("B1 widget preserved across reorder"), BtnRep->GetEntryWidget(FName(TEXT("b1"))) == B1Widget);
                TestTrue(TEXT("B2 widget preserved across reorder"), BtnRep->GetEntryWidget(FName(TEXT("b2"))) == B2Widget);

                // Rollback on failure
                TArray<TPair<FString, FGV2PreparedUiValue>> BadBtnMap;
                BadBtnMap.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("bad_btn")));
                BadBtnMap.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeNumber(123.0));

                TArray<FGV2PreparedUiValue> FailingBtns;
                FailingBtns.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(BadBtnMap))));

                TArray<TPair<FString, FGV2PreparedUiValue>> FailingCmdValues;
                FailingCmdValues.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(FailingBtns))));

                TestFalse(TEXT("CmdPanel Prepare fails on invalid text kind"), ApplyHostProps(CmdPanel, FailingCmdValues, CmdSchema, TEXT("textsystem:schema.ui_field.location_commands.v1"), CmdErr));
                TestEqual(TEXT("CmdPanel Button count remains 3 after failed prepare"), BtnRep->GetEntryCount(), 3);

                // Reset
                ResetHostProps(CmdPanel, CmdSchema, TEXT("textsystem:schema.ui_field.location_commands.v1"));
                TestEqual(TEXT("CmdPanel Buttons cleared on reset"), BtnRep->GetEntryCount(), 0);
                TestEqual(TEXT("CmdPanel Key cleared on reset"), CmdPanel->GetKey(), NAME_None);
            }
        }
    }

    return true;
}

#endif
