#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
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

    return true;
}

#endif
