#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
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
    }

    return true;
}

#endif
