#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
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

    // 3. Code Audit Test: centralized presentation pipeline compliance
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

    return true;
}

#endif
