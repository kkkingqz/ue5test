#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SVirtualWindow.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Application/GV2PackageClosure.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2RuntimeCore/Testing/GV2StableIdConformance.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImagePresentation.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "Tests/GV2ForgeryTestWidgets.h"
#include "UI/GV2CentralStylePreparer.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2TextPipelineHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2LayoutConstants.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2ScrollAreaWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2PresentationAuthorityProbe.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiMutationPlan.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "GV2ContentCore/UiSchema.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/WrapBox.h"
#include "Components/Border.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Engine/World.h"
#include "Slate/SceneViewport.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/UObjectIterator.h"

namespace
{
// TSR-03: shared presentation and UI test helpers moved to GV2PresentationTestFixtures.h.
using GV2PresentationTestFixtures::LoadConfiguredThemeForTest;
using GV2PresentationTestFixtures::MakeResolvedLiteralTextForTest;
using GV2PresentationTestFixtures::MakePreparedResolvedImageForTest;
using GV2PresentationTestFixtures::FGV2ScopedSamplePackageOverride;
using GV2PresentationTestFixtures::GV2TickWidgetSubtreeRecursively;
using GV2PresentationTestFixtures::GV2SimulateResponsiveFrame;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2StableIdConformanceTest,
    "GV2.Runtime.StableId.Conformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2StableIdConformanceTest::RunTest(const FString& Parameters)
{
    const std::string Failure = GV2RuntimeCore::Testing::RunStableIdConformance();
    TestTrue(
        *FString::Printf(
            TEXT("Shared Stable ID conformance passes%s%s"),
            Failure.empty() ? TEXT("") : TEXT(": "),
            Failure.empty() ? TEXT("") : UTF8_TO_TCHAR(Failure.c_str())),
        Failure.empty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageResourceLookupScaling,
    "GV2.Runtime.Resources.ImageLookupScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ImageResourceLookupScaling::RunTest(const FString& Parameters)
{
    constexpr int32 LookupIterations = 200000;
    auto MeasureLookup = [this](const int32 EntryCount)
    {
        UGV2ImageResourceCatalog* Catalog = NewObject<UGV2ImageResourceCatalog>();
        Catalog->ResolvedById.Reserve(EntryCount);
        TArray<FString> ResourceIds;
        ResourceIds.Reserve(EntryCount);
        for (int32 Index = 0; Index < EntryCount; ++Index)
        {
            FString ResourceId = FString::Printf(
                TEXT("core:resource.benchmark.entry_%05d"),
                Index);
            FGV2ResolvedImageResource Resolved;
            Resolved.ResourceId = ResourceId;
            Resolved.RenderMode = EGV2ImageRenderMode::FixedAspect;
            Resolved.FixedAspectRatio = 1.0f;
            Catalog->ResolvedById.Add(ResourceId, MoveTemp(Resolved));
            ResourceIds.Add(MoveTemp(ResourceId));
        }

        FGV2ResolvedImageResource Resolved;
        FString ResolveError;
        uint64 Checksum = 0;
        for (int32 Index = 0; Index < EntryCount; ++Index)
        {
            Catalog->Resolve(ResourceIds[Index], Resolved, ResolveError);
        }

        const double StartedAt = FPlatformTime::Seconds();
        for (int32 Iteration = 0; Iteration < LookupIterations; ++Iteration)
        {
            const bool bResolved = Catalog->Resolve(
                ResourceIds[Iteration % EntryCount],
                Resolved,
                ResolveError);
            Checksum += bResolved ? static_cast<uint64>(Resolved.ResourceId.Len()) : 0;
        }
        const double Elapsed = FPlatformTime::Seconds() - StartedAt;
        TestTrue(
            *FString::Printf(TEXT("Synthetic catalog with %d entries resolves all lookups"), EntryCount),
            Checksum > 0);
        return Elapsed;
    };

    const double SmallCatalogSeconds = MeasureLookup(10);
    const double MediumCatalogSeconds = MeasureLookup(1000);
    const double LargeCatalogSeconds = MeasureLookup(10000);
    AddInfo(FString::Printf(
        TEXT("Image lookup scaling: 10=%.6fs, 1000=%.6fs, 10000=%.6fs"),
        SmallCatalogSeconds,
        MediumCatalogSeconds,
        LargeCatalogSeconds));

    const double BaselineSeconds = FMath::Max(SmallCatalogSeconds, 0.000001);
    TestTrue(
        TEXT("Lookup at 1,000 entries does not scale linearly with catalog size"),
        MediumCatalogSeconds < BaselineSeconds * 20.0);
    TestTrue(
        TEXT("Lookup at 10,000 entries does not scale linearly with catalog size"),
        LargeCatalogSeconds < BaselineSeconds * 20.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CentralPresentationPathSourceAudit,
    "GV2.Runtime.UIKit.CentralPresentationPathSourceAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CentralPresentationPathSourceAudit::RunTest(const FString& Parameters)
{
    auto ReadSource = [this](const TCHAR* RelativePath, FString& OutSource)
    {
        const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
        const bool bLoaded = FFileHelper::LoadFileToString(OutSource, *FullPath);
        TestTrue(*FString::Printf(TEXT("Source audit can read %s"), RelativePath), bLoaded);
        return bLoaded;
    };

    TArray<FString> InputComponents;
    IFileManager::Get().FindFilesRecursive(
        InputComponents,
        *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GV2PresentationApply/Private/UI")),
        TEXT("*.cpp"),
        true,
        false);
    TestTrue(TEXT("Source audit enumerates the physical UI implementation set"), !InputComponents.IsEmpty());
    for (FString FullPath : InputComponents)
    {
        FString RelativePath = FullPath;
        FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());
        FString Source;
        if (ReadSource(*RelativePath, Source))
        {
            TestFalse(
                *FString::Printf(TEXT("Component delegates runtime lookup to the narrow interaction sink: %s"), *RelativePath),
                Source.Contains(TEXT("GetSubsystem<UGV2RuntimeSubsystem>")));
            TestFalse(
                *FString::Printf(TEXT("Component does not call Runtime SubmitUiInteraction directly: %s"), *RelativePath),
                Source.Contains(TEXT("Runtime->SubmitUiInteraction")));
        }
    }

    FString RuntimeSource;
    if (ReadSource(TEXT("Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp"), RuntimeSource))
    {
        TestFalse(TEXT("Generic runtime does not assemble description field"), RuntimeSource.Contains(TEXT("MakeInteractiveRichText")));
        TestFalse(TEXT("Generic runtime does not assemble button field"), RuntimeSource.Contains(TEXT("MakeButtonList")));
        TestFalse(TEXT("Generic runtime does not contain description field literal"), RuntimeSource.Contains(TEXT("TEXT(\"description\")")));
        TestFalse(TEXT("Generic runtime does not contain buttons field literal"), RuntimeSource.Contains(TEXT("TEXT(\"buttons\")")));
        TestFalse(TEXT("Generic runtime does not know the test screen ID"), RuntimeSource.Contains(TEXT("core:screen.test")));
        TestFalse(TEXT("Generic runtime does not know the test Widget class"), RuntimeSource.Contains(TEXT("WBP_Testscreen")));
    }

    const UGV2RuntimeSettings* RuntimeSettings = GetDefault<UGV2RuntimeSettings>();
    TestNotNull(TEXT("Runtime development settings are available"), RuntimeSettings);
    if (RuntimeSettings != nullptr)
    {
        TestTrue(
            TEXT("Editor development profile connects the RH gameplay package"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/rh")));
        TestFalse(
            TEXT("Editor development profile does not replace gameplay with the sample fixture"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/sample")));
    }

    FString CoordinatorSource;
    if (ReadSource(
            TEXT("Source/GV2/Private/Application/GV2SessionCoordinator.cpp"),
            CoordinatorSource))
    {
        TestTrue(
            TEXT("Coordinator delegates Screen Field conversion to the materializer"),
            CoordinatorSource.Contains(TEXT("GV2ScreenFieldMaterializer::PrepareBindingDefinitions")));
        TestFalse(
            TEXT("Coordinator contains no concrete Screen Field schema IDs"),
            CoordinatorSource.Contains(TEXT("core:schema.ui_field.")));
    }

    FString MaterializerSource;
    if (ReadSource(
            TEXT("Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp"),
            MaterializerSource))
    {
        // UPP-27/UPP-30: the materializer is fully schema-driven (FGV2UiSchemaCache
        // resolves any schema_id by scanning *.schema.json5 content) and is free
        // functions, not a per-schema adapter class/registry -- it no longer
        // hardcodes even the LocationScreen schema ids the way per-field adapters
        // used to, and FGV2ScreenFieldAdapterRegistry itself no longer exists.
        TestFalse(
            TEXT("Materializer contains no concrete Screen Field schema IDs"),
            MaterializerSource.Contains(TEXT("schema.ui_field.")));
    }

    FString ScreenTemplatesContract;
    if (ReadSource(
            TEXT("Docs/UI/ScreenTemplates.md"),
            ScreenTemplatesContract))
    {
        const TCHAR* PublishedFieldSchemas[] = {
            TEXT("core:schema.ui_field.button_list.v2"),
            TEXT("core:schema.ui_field.rich_text.v3"),
            TEXT("core:schema.ui_field.checkbox.v1"),
            TEXT("core:schema.ui_field.input_field.v1"),
            TEXT("core:schema.ui_field.dropdown_select.v1")
        };
        for (const TCHAR* SchemaId : PublishedFieldSchemas)
        {
            TestTrue(
                *FString::Printf(TEXT("Screen Templates contract names supported schema %s"), SchemaId),
                ScreenTemplatesContract.Contains(SchemaId));
        }
    }

    FString ImageCatalogSource;
    if (ReadSource(
            TEXT("Source/GV2/Private/UI/GV2ImageResourceCatalog.cpp"),
            ImageCatalogSource))
    {
        const int32 ResolveStart = ImageCatalogSource.Find(
            TEXT("bool UGV2ImageResourceCatalog::Resolve("));
        const int32 ResolveEnd = ImageCatalogSource.Find(
            TEXT("bool UGV2ImageResourceCatalog::BuildFromPackageClosure"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            ResolveStart);
        TestTrue(
            TEXT("Image Catalog source audit locates the runtime Resolve body"),
            ResolveStart != INDEX_NONE && ResolveEnd > ResolveStart);
        if (ResolveStart != INDEX_NONE && ResolveEnd > ResolveStart)
        {
            const FString ResolveSource = ImageCatalogSource.Mid(
                ResolveStart,
                ResolveEnd - ResolveStart);
            TestTrue(
                TEXT("Image Catalog Resolve uses the immutable prepared lookup"),
                ResolveSource.Contains(TEXT("ResolvedById.Find(ResourceId)")));
            TestFalse(
                TEXT("Image Catalog Resolve does not validate the whole catalog"),
                ResolveSource.Contains(TEXT("Validate(OutError)")));
            TestFalse(
                TEXT("Image Catalog Resolve does not linearly scan definitions"),
                ResolveSource.Contains(TEXT("FindByPredicate")));
            TestFalse(
                TEXT("Image Catalog Resolve does not rebuild a brush"),
                ResolveSource.Contains(TEXT("ResolveDefinition")));
        }
    }

    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    GV2RuntimeCore::FScreenRequest UnknownSchemaRequest;
    UnknownSchemaRequest.ScreenId = "core:screen.unknown_schema_fixture";
    GV2RuntimeCore::FScreenField UnknownField;
    UnknownField.FieldId = "unknown";
    UnknownField.SchemaId = "core:schema.ui_field.unknown.v1";
    UnknownField.Value = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FObject{});
    UnknownSchemaRequest.Fields.push_back(MoveTemp(UnknownField));
    TArray<FGV2UiBindingDefinition> UnknownDefinitions;
    TestFalse(
        TEXT("Adapter registry rejects an unknown Screen Field schema"),
        GV2ScreenFieldMaterializer::PrepareBindingDefinitions(
            *PrepareContext,
            UnknownSchemaRequest,
            UnknownDefinitions));
    TestTrue(
        TEXT("Unknown Screen Field schema leaves no partial binding definitions"),
        UnknownDefinitions.IsEmpty());

    // Helper lambda to construct a button item
    auto MakeButtonItem = [](const std::string* Key, const std::string& CommandId) -> GV2RuntimeCore::FValue
    {
        GV2RuntimeCore::FValue::FObject Item;
        if (Key != nullptr)
        {
            Item["key"] = GV2RuntimeCore::FValue(*Key);
        }
        GV2RuntimeCore::FValue::FObject TextObj;
        TextObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.button.ok"));
        Item["text"] = GV2RuntimeCore::FValue(MoveTemp(TextObj));
        GV2RuntimeCore::FValue::FObject BindingObj;
        BindingObj["command_id"] = GV2RuntimeCore::FValue(CommandId);
        Item["binding"] = GV2RuntimeCore::FValue(MoveTemp(BindingObj));
        return GV2RuntimeCore::FValue(MoveTemp(Item));
    };

    // 1. Valid location commands with distinct keys
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.synthetic_mechanical";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "core:schema.ui_field.synthetic_commands.v1";
        const std::string KeyA = "btn_a";
        const std::string KeyB = "btn_b";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(&KeyA, "core:command.screen.action_a"),
            MakeButtonItem(&KeyB, "core:command.screen.action_b")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        ValidReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> ValidDefs;
        TestTrue(
            TEXT("Valid button list with distinct keys is accepted"),
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, ValidReq, ValidDefs));
        TestEqual(TEXT("Prepares two binding definitions"), ValidDefs.Num(), 2);
    }

    // 2. Button list missing key
    {
        GV2RuntimeCore::FScreenRequest MissingKeyReq;
        MissingKeyReq.ScreenId = "core:screen.synthetic_mechanical";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "core:schema.ui_field.synthetic_commands.v1";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(nullptr, "core:command.screen.action_a")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        MissingKeyReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> MissingDefs;
        TestFalse(
            TEXT("Button list with missing key is rejected (UiElementKeyMissing)"),
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, MissingKeyReq, MissingDefs));
        TestTrue(TEXT("Rejected candidate leaves definitions empty"), MissingDefs.IsEmpty());
    }

    // 3. Button list duplicate key
    {
        GV2RuntimeCore::FScreenRequest DupKeyReq;
        DupKeyReq.ScreenId = "core:screen.synthetic_mechanical";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "core:schema.ui_field.synthetic_commands.v1";
        const std::string KeyDup = "btn_same";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(&KeyDup, "core:command.screen.action_a"),
            MakeButtonItem(&KeyDup, "core:command.screen.action_b")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        DupKeyReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> DupDefs;
        TestFalse(
            TEXT("Button list with duplicate key is rejected (UiElementKeyDuplicate)"),
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, DupKeyReq, DupDefs));
        TestTrue(TEXT("Rejected duplicate key leaves definitions empty"), DupDefs.IsEmpty());
    }

    // 4. Button list text-derived key
    {
        GV2RuntimeCore::FScreenRequest TextKeyReq;
        TextKeyReq.ScreenId = "core:screen.synthetic_mechanical";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "core:schema.ui_field.synthetic_commands.v1";
        const std::string KeyText = "core:text.button.ok";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(&KeyText, "core:command.screen.action_a")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        TextKeyReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> TextDefs;
        TestFalse(
            TEXT("Button list with text-derived key is rejected (UiElementKeyTextDerived)"),
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, TextKeyReq, TextDefs));
        TestTrue(TEXT("Rejected text key leaves definitions empty"), TextDefs.IsEmpty());
    }

    // 5. Button list key grammar conformance: [a-z0-9_.-@:]+ (BAI-08)
    // 5a. Positive key grammar: domain ID with ':', instance ID with '@', hyphens, dots
    {
        GV2RuntimeCore::FScreenRequest ValidGrammarReq;
        ValidGrammarReq.ScreenId = "core:screen.synthetic_mechanical";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "core:schema.ui_field.synthetic_commands.v1";
        const std::string KeyDomain = "core:item.weapon.iron_sword";
        const std::string KeyActor = "actor@42";
        const std::string KeyHyphenDot = "btn-action.v1_ok";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(&KeyDomain, "core:command.screen.action_a"),
            MakeButtonItem(&KeyActor, "core:command.screen.action_b"),
            MakeButtonItem(&KeyHyphenDot, "core:command.screen.action_c")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        ValidGrammarReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> ValidDefs;
        TestTrue(
            TEXT("BAI-08: Repeated element keys with ':', '@', '-', '.' grammar are accepted"),
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, ValidGrammarReq, ValidDefs));
        TestEqual(TEXT("Prepares three binding definitions for valid keys"), ValidDefs.Num(), 3);
    }

    // 5b. Negative key grammar: uppercase, spaces, invalid symbols, text prefix, and excessive length
    {
        const TArray<std::string> InvalidGrammarKeys = {
            "BTN #1!",
            "Upper_Case_Key",
            "key with spaces",
            "key/with/slash",
            "key?question",
            "text:plain.key",
            "core:text.button.ok",
            "",
            std::string(193, 'a')
        };

        for (const std::string& BadKey : InvalidGrammarKeys)
        {
            GV2RuntimeCore::FScreenRequest InvalidKeyReq;
            InvalidKeyReq.ScreenId = "core:screen.test";
            GV2RuntimeCore::FScreenField BtnField;
            BtnField.FieldId = "buttons";
            BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
            GV2RuntimeCore::FValue::FObject ValueObj;
            ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
                MakeButtonItem(&BadKey, "core:command.screen.action_a")
            });
            BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
            InvalidKeyReq.Fields.push_back(MoveTemp(BtnField));
            TArray<FGV2UiBindingDefinition> InvalidDefs;
            TestFalse(
                *FString::Printf(TEXT("BAI-08: Invalid grammar key '%s' is rejected (UiElementKeyInvalid)"),
                    UTF8_TO_TCHAR(BadKey.c_str())),
                GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, InvalidKeyReq, InvalidDefs));
            TestTrue(TEXT("Rejected invalid key leaves definitions empty"), InvalidDefs.IsEmpty());
        }
    }

    FString PortableHeader;
    if (ReadSource(TEXT("Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h"), PortableHeader))
    {
        TestTrue(TEXT("Portable request exposes generic Screen Fields"), PortableHeader.Contains(TEXT("std::vector<FScreenField> Fields")));
        TestFalse(TEXT("Portable request has no concrete description member"), PortableHeader.Contains(TEXT("DescriptionText")));
        TestFalse(TEXT("Portable request has no concrete button member"), PortableHeader.Contains(TEXT("std::vector<FScreenButton>")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenRegistryContract,
    "GV2.Runtime.ScreenRegistry.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenRegistryContract::RunTest(const FString& Parameters)
{
    const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
    UGV2ScreenRegistry* Registry = RegistrySettings != nullptr
        ? RegistrySettings->RegistryAsset.LoadSynchronous()
        : nullptr;
    TestNotNull(TEXT("Configured Screen Registry is loadable"), Registry);
    FGV2ResolvedScreenRegistry ResolvedRegistry;
    FString BuildError;
    TestTrue(
        *FString::Printf(TEXT("Screen Registry compiles [Error: %s]"), *BuildError),
        Registry != nullptr && Registry->CompileResolvedRegistry(GV2PackageClosure::DiscoverFromGameData(), ResolvedRegistry, BuildError));
    FGV2ResolvedScreenDescriptor TestScreenDescriptor;
    FGV2ScreenResolutionRejection TestScreenRejection;
    const bool bTestScreenResolved = ResolvedRegistry.Resolve(
        TEXT("core:screen.test"),
        FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent),
        TestScreenDescriptor,
        TestScreenRejection);
    TestTrue(
        *FString::Printf(TEXT("Screen Registry contains the test screen entry [Error: %s]"), *TestScreenRejection.Message),
        bTestScreenResolved);
    UClass* TestScreenClass = bTestScreenResolved ? TestScreenDescriptor.WidgetClass : nullptr;
    TestNotNull(TEXT("Screen Registry resolves the test Screen class"), TestScreenClass);
    UClass* ScreenBaseClass = TestScreenClass != nullptr ? TestScreenClass->GetSuperClass() : nullptr;
    TestNotNull(TEXT("WBP_ScreenBase is loadable"), ScreenBaseClass);
    if (TestScreenClass != nullptr && ScreenBaseClass != nullptr)
    {
        TestTrue(
            TEXT("WBP_Testscreen inherits WBP_ScreenBase"),
            TestScreenClass->IsChildOf(ScreenBaseClass));
        TestTrue(
            TEXT("WBP_ScreenBase is abstract"),
            ScreenBaseClass->HasAnyClassFlags(CLASS_Abstract));
        TestEqual(
            TEXT("WBP_ScreenBase has the generic native parent"),
            ScreenBaseClass->GetSuperClass(),
            UGV2ScreenWidgetBase::StaticClass());
    }

    // PAH-02: a screen registered for one placement is rejected when resolved for the
    // other, through the real production Resolve() path against the configured asset --
    // not a synthetic registry. core:screen.test is registered TopLevel(location_content);
    // core:screen.test_embedded is registered Embedded.
    FGV2ResolvedScreenDescriptor CrossPlacementDescriptor;
    FGV2ScreenResolutionRejection EmbeddedRequestedAsTopLevelRejection;
    const bool bEmbeddedRequestedAsTopLevelResolved = ResolvedRegistry.Resolve(
        TEXT("core:screen.test_embedded"),
        FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent),
        CrossPlacementDescriptor,
        EmbeddedRequestedAsTopLevelRejection);
    TestFalse(
        TEXT("Screen registered Embedded is rejected when resolved as TopLevel"),
        bEmbeddedRequestedAsTopLevelResolved);
    TestTrue(
        TEXT("Rejection carries the placement_mismatch diagnostic code"),
        EmbeddedRequestedAsTopLevelRejection.Message.Contains(TEXT("core:diagnostic.ui_screen_registry.placement_mismatch")));

    FGV2ScreenResolutionRejection TopLevelRequestedAsEmbeddedRejection;
    const bool bTopLevelRequestedAsEmbeddedResolved = ResolvedRegistry.Resolve(
        TEXT("core:screen.test"),
        FGV2ScreenPlacement::Embedded(),
        CrossPlacementDescriptor,
        TopLevelRequestedAsEmbeddedRejection);
    TestFalse(
        TEXT("Screen registered TopLevel is rejected when resolved as Embedded"),
        bTopLevelRequestedAsEmbeddedResolved);
    TestTrue(
        TEXT("Rejection carries the placement_mismatch diagnostic code (Embedded request)"),
        TopLevelRequestedAsEmbeddedRejection.Message.Contains(TEXT("core:diagnostic.ui_screen_registry.placement_mismatch")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2NestedTabRejectedScreenLeavesNoPhysicalMutationTest,
    "GV2.Runtime.UI.NestedTabRejectedScreenLeavesNoPhysicalMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-08 (ADR-0043 D1, PAH-R4): FGV2TabContainerTabsPropertyConsumer::Prepare no longer
// has a generic UGV2ScreenWidgetBase::StaticClass() fallback -- an unregistered screen_id
// is a typed Prepare failure through the real production consumer (the same object
// FGV2PropertyConsumerFactory::CreateConsumer hands to PrepareUiHostProperties), whatever
// resolver (PrepareContext, legacy configured Registry, or neither) happens to be
// available. This proves the other half of that fix: a rejected Prepare leaves the
// container's own already-committed state -- what Commit()/ApplyTabEntries would
// otherwise mutate -- completely untouched, not partially applied.
bool FGV2NestedTabRejectedScreenLeavesNoPhysicalMutationTest::RunTest(const FString& Parameters)
{
    UGV2ScreenWidgetBase* SeededScreen = NewObject<UGV2ScreenWidgetBase>();

    UGV2TabContainerWidgetBase* TabContainer = NewObject<UGV2TabContainerWidgetBase>();
    TArray<FGV2TabItemEntry> SeedEntries;
    FGV2TabItemEntry SeedEntry;
    SeedEntry.Key = FName(TEXT("info"));
    SeedEntry.ScreenId = TEXT("core:screen.test_embedded");
    SeedEntries.Add(SeedEntry);
    TMap<FName, UUserWidget*> SeedWidgets;
    SeedWidgets.Add(FName(TEXT("info")), SeededScreen);
    TabContainer->ApplyTabEntries(SeedEntries, SeedWidgets);
    TabContainer->SelectTabByKey(FName(TEXT("info")));

    const int32 EntryCountBefore = TabContainer->GetTabEntries().Num();
    const FName ActiveTabBefore = TabContainer->GetActiveTabKey();
    UUserWidget* const WidgetBefore = TabContainer->GetScreenWidgetForTab(FName(TEXT("info")));
    TestEqual(TEXT("Seeded baseline has one tab"), EntryCountBefore, 1);
    TestTrue(TEXT("Seeded baseline's widget is the one just created"), WidgetBefore == SeededScreen);

    TSharedPtr<IGV2PropertyConsumer> TabsConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
        EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
    TestNotNull(TEXT("Factory created FGV2TabContainerTabsPropertyConsumer"), TabsConsumer.Get());

    FGV2UiPropertyCapability TabsCap;
    TabsCap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;

    TMap<FString, FGV2PreparedUiValue> RejectedTab;
    RejectedTab.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("rejected")));
    RejectedTab.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{FText::FromString(TEXT("Rejected"))}));
    RejectedTab.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.psc08_nonexistent"), TEXT("screen")));
    TArray<FGV2PreparedUiValue> RejectedTabs;
    RejectedTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(RejectedTab)));

    FString PrepareError;
    TestFalse(
        TEXT("Prepare with an unregistered screen_id is rejected through the real production consumer"),
        TabsConsumer->Prepare(
            FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(RejectedTabs)),
            TabsCap,
            TabContainer,
            PrepareError));
    TestTrue(
        *FString::Printf(TEXT("Rejection carries the unregistered-screen diagnostic [Error: %s]"), *PrepareError),
        PrepareError.Contains(TEXT("core:diagnostic.ui_consumer.unregistered_screen_id")));

    TestEqual(TEXT("Tab entry count is unchanged after the rejected Prepare"), TabContainer->GetTabEntries().Num(), EntryCountBefore);
    TestEqual(TEXT("Active tab is unchanged after the rejected Prepare"), TabContainer->GetActiveTabKey(), ActiveTabBefore);
    TestEqual(TEXT("Seeded tab's widget is unchanged after the rejected Prepare"), TabContainer->GetScreenWidgetForTab(FName(TEXT("info"))), WidgetBefore);
    if (TabContainer->GetTabEntries().Num() == 1)
    {
        TestEqual(TEXT("Seeded entry's key is unchanged"), TabContainer->GetTabEntries()[0].Key, SeedEntry.Key);
        TestEqual(TEXT("Seeded entry's screen_id is unchanged"), TabContainer->GetTabEntries()[0].ScreenId, SeedEntry.ScreenId);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PresentationApplyImageOperationTest,
    "GV2.Runtime.Presentation.PresentationApplyImageOperation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-09A (ADR-0043 D2/D3): exercises GV2PresentationTestFixtures::ApplyPreparedTransaction() directly, in
// isolation from any property consumer, session, or snapshot -- this module has no
// resolver type to build one with, so this synthetic transaction is exactly the shape a
// real preparer builds. Proves both halves of the new module's own contract: a valid
// operation physically mutates the widget, and an operation whose target is unavailable
// (the same observable shape a widget garbage-collected between Prepare and Apply would
// have) is a typed failure, not a crash or a silent no-op.
bool FGV2PresentationApplyImageOperationTest::RunTest(const FString& Parameters)
{
    UImage* Widget = NewObject<UImage>();

    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Box;
    Brush.ImageSize = FVector2D(42.0f, 24.0f);

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    TestTrue(TEXT("A freshly built transaction has no operations yet"), Transaction.IsEmpty());
    GV2PresentationApply::FPreparedImageResourceOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Brush = Brush;
    Transaction.AddImageResourceOperation(Operation);
    TestFalse(TEXT("A transaction with one added operation is no longer empty"), Transaction.IsEmpty());

    FString ApplyError;
    TestTrue(
        *FString::Printf(TEXT("Apply succeeds for a valid target [Error: %s]"), *ApplyError),
        GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, ApplyError));
    TestEqual(TEXT("Apply set the exact prepared brush's DrawAs"), Widget->GetBrush().DrawAs, ESlateBrushDrawType::Box);
    TestEqual(TEXT("Apply set the exact prepared brush's ImageSize"), FVector2D(Widget->GetBrush().ImageSize), FVector2D(42.0f, 24.0f));

    GV2PresentationApply::FGV2PreparedPresentationTransaction StaleTransaction;
    GV2PresentationApply::FPreparedImageResourceOperation StaleOperation;
    // TargetWidget is deliberately left unassigned -- an unset TWeakObjectPtr resolves
    // to nullptr via Get(), the same observable shape a garbage-collected target has by
    // the time Apply runs.
    StaleTransaction.AddImageResourceOperation(StaleOperation);

    FString StaleError;
    TestFalse(
        TEXT("Apply rejects an operation whose target widget is unavailable"),
        GV2PresentationTestFixtures::ApplyPreparedTransaction(StaleTransaction, StaleError));
    TestFalse(TEXT("Rejection carries a diagnostic message"), StaleError.IsEmpty());

    // PSC-11 (post-closure correction): the two distinguishable central-style outcomes.
    // A target that no longer exists is nothing left to write -- the same "collected between
    // Prepare and Apply" shape as above -- and must not fail the whole transaction, because a
    // transaction carries every screen's operations and one dead widget would take the rest
    // with it. A target that DOES exist but performs no such role is the opposite: a preparer
    // that paired a role with the wrong class, which must be diagnosable rather than a silent
    // no-op. Both are asserted here so neither can be turned into the other unnoticed.
    GV2PresentationApply::FGV2PreparedPresentationTransaction CollectedStyleTransaction;
    GV2PresentationApply::FPreparedCentralStyleOperation CollectedStyleOperation;
    CollectedStyleOperation.Payload.Set<GV2PresentationApply::FPreparedSeparatorStyle>(
        GV2PresentationApply::FPreparedSeparatorStyle());
    CollectedStyleTransaction.AddCentralStyleOperation(MoveTemp(CollectedStyleOperation));

    FString CollectedStyleError;
    TestTrue(
        TEXT("Apply skips a central-style operation whose target was collected"),
        GV2PresentationTestFixtures::ApplyPreparedTransaction(CollectedStyleTransaction, CollectedStyleError));

    GV2PresentationApply::FGV2PreparedPresentationTransaction MismatchedStyleTransaction;
    GV2PresentationApply::FPreparedCentralStyleOperation MismatchedStyleOperation;
    MismatchedStyleOperation.TargetWidget = Widget;
    MismatchedStyleOperation.Payload.Set<GV2PresentationApply::FPreparedSeparatorStyle>(
        GV2PresentationApply::FPreparedSeparatorStyle());
    MismatchedStyleTransaction.AddCentralStyleOperation(MoveTemp(MismatchedStyleOperation));

    FString MismatchedStyleError;
    TestFalse(
        TEXT("Apply rejects a central-style role its live target does not perform"),
        GV2PresentationTestFixtures::ApplyPreparedTransaction(MismatchedStyleTransaction, MismatchedStyleError));
    TestTrue(
        TEXT("The rejection names the role/target mismatch"),
        MismatchedStyleError.Contains(TEXT("central_style_target_mismatch")));

    return true;
}

namespace
{
using GV2PresentationApply::EGV2PreparedOperationKind;

// PSC-10A/PSC-11 (ADR-0043 D3): expected behavior for the exhaustive kind-walk test below,
// authored independently by reading each kind's own doc comment in
// PreparedPresentationTransaction.h -- not derived from the facade's own lambda bodies, so
// this table cannot silently agree with a regression there.
//
// PSC-11 replaced the old two-way split ("this module writes it" vs "the adapter writes it")
// with what the single facade actually distinguishes: whether a kind writes a plain
// Engine/UMG target it can reach by type, or needs the target to DECLARE a physical role.
// The third value is the property the per-role interfaces bought -- a kind whose target does
// not declare the role it needs is a diagnosable mismatch, not a silent success.
enum class EGV2ExpectedOperationRoute : uint8
{
    MutatesPlainTarget,
    RequiresDeclaredRole,
    RejectsUndeclaredTarget,
};

EGV2ExpectedOperationRoute ExpectedRouteFor(EGV2PreparedOperationKind Kind)
{
    switch (Kind)
    {
    case EGV2PreparedOperationKind::ImageResource:      return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::Boolean:            return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::EditableTextValue:  return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::ProgressBar:        return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::PlainText:          return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::RichTextRender:     return EGV2ExpectedOperationRoute::MutatesPlainTarget;
    case EGV2PreparedOperationKind::TextHint:           return EGV2ExpectedOperationRoute::MutatesPlainTarget;

    case EGV2PreparedOperationKind::ImageHost:          return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::Number:             return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::Integer:            return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::String:             return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::Binding:            return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::RichTextSpans:      return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::KeyedCollection:    return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::TabContainer:       return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
    case EGV2PreparedOperationKind::ViewportRefresh:    return EGV2ExpectedOperationRoute::RequiresDeclaredRole;

    // A declared `key` capability with no route, a text operation on something that renders
    // no text, and a style role delivered to a class that does not perform it are each a
    // defect the pipeline exists to surface rather than absorb.
    case EGV2PreparedOperationKind::Key:                return EGV2ExpectedOperationRoute::RejectsUndeclaredTarget;
    case EGV2PreparedOperationKind::Text:               return EGV2ExpectedOperationRoute::RejectsUndeclaredTarget;
    case EGV2PreparedOperationKind::CentralStyle:       return EGV2ExpectedOperationRoute::RejectsUndeclaredTarget;
    }
    checkf(false, TEXT("EGV2PreparedOperationKind has an unclassified value -- add it to ExpectedRouteFor"));
    return EGV2ExpectedOperationRoute::RequiresDeclaredRole;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ExhaustiveOperationKindWalkTest,
    "GV2.Runtime.Presentation.ExhaustiveOperationKindWalk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-10A (ADR-0043 D3): actual set of operation kinds is derived from the variant
// itself (TVariantSize_V), not a hand-typed list in this test -- and the construction
// switch below has no default, so a kind added to the variant without a corresponding
// case here fails to COMPILE, not just fails to run. Expected behavior per kind comes
// from ExpectedRouteFor's independent classification table above.
bool FGV2ExhaustiveOperationKindWalkTest::RunTest(const FString& Parameters)
{
    using GV2PresentationApply::EGV2PreparedOperationKind;

    static_assert(TVariantSize_V<GV2PresentationApply::FGV2PreparedOperationVariant> == 19,
        "A kind was added to or removed from FGV2PreparedOperationVariant -- update this "
        "literal AND the construction switch AND ExpectedRouteFor's switch below before "
        "trusting this test again.");

    for (uint8 RawKind = 0; RawKind < static_cast<uint8>(TVariantSize_V<GV2PresentationApply::FGV2PreparedOperationVariant>); ++RawKind)
    {
        const EGV2PreparedOperationKind Kind = static_cast<EGV2PreparedOperationKind>(RawKind);
        const EGV2ExpectedOperationRoute ExpectedRoute = ExpectedRouteFor(Kind);

        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        FString KindLabel;
        bool bMutationObserved = false;

        switch (Kind)
        {
        case EGV2PreparedOperationKind::ImageResource:
        {
            KindLabel = TEXT("ImageResource");
            UImage* Widget = NewObject<UImage>();
            GV2PresentationApply::FPreparedImageResourceOperation Op;
            Op.TargetWidget = Widget;
            Op.Brush.ImageSize = FVector2D(7.0f, 7.0f);
            Transaction.AddImageResourceOperation(Op);
            TestEqual(TEXT("ImageResource: variant reports the expected kind"),
                static_cast<uint8>(GV2PresentationApply::GetPreparedOperationKind(Transaction.GetOperations()[0])), static_cast<uint8>(Kind));
            FString Error;
            TestTrue(TEXT("ImageResource: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = FVector2D(Widget->GetBrush().ImageSize).X > 0.0;
            break;
        }
        case EGV2PreparedOperationKind::ImageHost:
        {
            KindLabel = TEXT("ImageHost");
            GV2PresentationApply::FPreparedImageHostOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddImageHostOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::Boolean:
        {
            KindLabel = TEXT("Boolean");
            UCheckBox* Widget = NewObject<UCheckBox>();
            Widget->SetIsChecked(false);
            GV2PresentationApply::FPreparedBooleanOperation Op;
            Op.TargetWidget = Widget;
            Op.Target = GV2PresentationApply::EPreparedBooleanTarget::CheckBoxChecked;
            Op.Value = true;
            Transaction.AddBooleanOperation(Op);
            FString Error;
            TestTrue(TEXT("Boolean: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = Widget->IsChecked();
            break;
        }
        case EGV2PreparedOperationKind::EditableTextValue:
        {
            KindLabel = TEXT("EditableTextValue");
            UEditableTextBox* Widget = NewObject<UEditableTextBox>();
            GV2PresentationApply::FPreparedEditableTextValueOperation Op;
            Op.TargetWidget = Widget;
            Op.Value = FText::FromString(TEXT("kind-walk"));
            Transaction.AddEditableTextValueOperation(Op);
            FString Error;
            TestTrue(TEXT("EditableTextValue: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = Widget->GetText().ToString() == TEXT("kind-walk");
            break;
        }
        case EGV2PreparedOperationKind::ProgressBar:
        {
            KindLabel = TEXT("ProgressBar");
            UProgressBar* Widget = NewObject<UProgressBar>();
            GV2PresentationApply::FPreparedProgressBarOperation Op;
            Op.TargetWidget = Widget;
            Op.Percent = 0.42f;
            Transaction.AddProgressBarOperation(Op);
            FString Error;
            TestTrue(TEXT("ProgressBar: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = FMath::IsNearlyEqual(Widget->GetPercent(), 0.42f, 0.001f);
            break;
        }
        case EGV2PreparedOperationKind::Number:
        {
            KindLabel = TEXT("Number");
            GV2PresentationApply::FPreparedNumberOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddNumberOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::Integer:
        {
            KindLabel = TEXT("Integer");
            GV2PresentationApply::FPreparedIntegerOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddIntegerOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::String:
        {
            KindLabel = TEXT("String");
            GV2PresentationApply::FPreparedStringOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddStringOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::Key:
        {
            KindLabel = TEXT("Key");
            GV2PresentationApply::FPreparedKeyOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddKeyOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::Binding:
        {
            KindLabel = TEXT("Binding");
            GV2PresentationApply::FPreparedBindingOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddBindingOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::RichTextSpans:
        {
            KindLabel = TEXT("RichTextSpans");
            GV2PresentationApply::FPreparedRichTextSpansOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddRichTextSpansOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::Text:
        {
            KindLabel = TEXT("Text");
            GV2PresentationApply::FPreparedTextOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddTextOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::KeyedCollection:
        {
            KindLabel = TEXT("KeyedCollection");
            GV2PresentationApply::FPreparedKeyedCollectionOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddKeyedCollectionOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::TabContainer:
        {
            KindLabel = TEXT("TabContainer");
            GV2PresentationApply::FPreparedTabContainerOperation Op;
            Op.TargetWidget = NewObject<UImage>();
            Transaction.AddTabContainerOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::PlainText:
        {
            KindLabel = TEXT("PlainText");
            UCommonTextBlock* Widget = NewObject<UCommonTextBlock>();
            GV2PresentationApply::FPreparedPlainTextOperation Op;
            Op.TargetWidget = Widget;
            Op.Text = FText::FromString(TEXT("kind-walk"));
            Op.ScalePolicy.BaseFontSize = 14.0f;
            Transaction.AddPlainTextOperation(Op);
            FString Error;
            TestTrue(TEXT("PlainText: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = Widget->GetText().ToString() == TEXT("kind-walk");
            break;
        }
        case EGV2PreparedOperationKind::RichTextRender:
        {
            KindLabel = TEXT("RichTextRender");
            UCommonRichTextBlock* Widget = NewObject<UCommonRichTextBlock>();
            GV2PresentationApply::FPreparedRichTextRenderOperation Op;
            Op.TargetWidget = Widget;
            Op.Markup = TEXT("kind-walk");
            Transaction.AddRichTextRenderOperation(Op);
            FString Error;
            TestTrue(TEXT("RichTextRender: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = Widget->GetText().ToString() == TEXT("kind-walk");
            break;
        }
        case EGV2PreparedOperationKind::TextHint:
        {
            KindLabel = TEXT("TextHint");
            UEditableTextBox* Widget = NewObject<UEditableTextBox>();
            GV2PresentationApply::FPreparedTextHintOperation Op;
            Op.TargetWidget = Widget;
            Op.Text = FText::FromString(TEXT("kind-walk"));
            Transaction.AddTextHintOperation(Op);
            FString Error;
            TestTrue(TEXT("TextHint: Apply succeeds"), GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, Error));
            bMutationObserved = Widget->GetHintText().ToString() == TEXT("kind-walk");
            break;
        }
        case EGV2PreparedOperationKind::CentralStyle:
        {
            KindLabel = TEXT("CentralStyle");
            GV2PresentationApply::FPreparedSeparatorStyle Style;
            Style.Thickness = 7.0f;
            GV2PresentationApply::FPreparedCentralStyleOperation Op;
            // PSC-11: a plain UMG target, like every other kind here, so this walk exercises
            // what the facade does with a target that declares no role.
            Op.TargetWidget = NewObject<UImage>();
            Op.Payload.Set<GV2PresentationApply::FPreparedSeparatorStyle>(Style);
            Transaction.AddCentralStyleOperation(Op);
            break;
        }
        case EGV2PreparedOperationKind::ViewportRefresh:
        {
            KindLabel = TEXT("ViewportRefresh");
            GV2PresentationApply::FPreparedViewportRefreshOperation Op;
            Op.RootWidget = NewObject<UImage>();
            Op.ViewportHeight = 720.0f;
            Transaction.AddViewportRefreshOperation(Op);
            break;
        }
        }

        TestEqual(*FString::Printf(TEXT("%s: variant index matches its own enum value"), *KindLabel),
            static_cast<uint8>(GV2PresentationApply::GetPreparedOperationKind(Transaction.GetOperations()[0])), static_cast<uint8>(Kind));

        FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();
        FString RouteError;
        const bool bRouteApplied = GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, RouteError);
        const int32 ApplyAuthorityAccesses = FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();
        TestEqual(
            *FString::Printf(TEXT("%s: Apply performs zero snapshot-authority reads"), *KindLabel),
            ApplyAuthorityAccesses,
            0);
        switch (ExpectedRoute)
        {
        case EGV2ExpectedOperationRoute::MutatesPlainTarget:
            TestTrue(*FString::Printf(TEXT("%s: classified MutatesPlainTarget and the facade physically mutated its plain target"), *KindLabel),
                bMutationObserved);
            TestTrue(*FString::Printf(TEXT("%s: classified MutatesPlainTarget and the facade reports success [Error: %s]"), *KindLabel, *RouteError),
                bRouteApplied);
            break;
        case EGV2ExpectedOperationRoute::RequiresDeclaredRole:
            TestTrue(*FString::Printf(TEXT("%s: classified RequiresDeclaredRole -- a target declaring no role is a safe no-op, not an error [Error: %s]"), *KindLabel, *RouteError),
                bRouteApplied);
            TestFalse(*FString::Printf(TEXT("%s: classified RequiresDeclaredRole -- a target declaring no role was not mutated"), *KindLabel),
                bMutationObserved);
            break;
        case EGV2ExpectedOperationRoute::RejectsUndeclaredTarget:
            TestFalse(*FString::Printf(TEXT("%s: classified RejectsUndeclaredTarget -- a target declaring no role is rejected, not absorbed"), *KindLabel),
                bRouteApplied);
            TestFalse(*FString::Printf(TEXT("%s: classified RejectsUndeclaredTarget -- the rejection carries a diagnostic"), *KindLabel),
                RouteError.IsEmpty());
            TestFalse(*FString::Printf(TEXT("%s: classified RejectsUndeclaredTarget -- nothing was mutated"), *KindLabel),
                bMutationObserved);
            break;
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenAssetRootOwnershipAudit,
    "GV2.Runtime.ScreenRegistry.AssetRootOwnershipAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenAssetRootOwnershipAudit::RunTest(const FString& Parameters)
{
    // PAH-03: before flipping an unowned /Game/ asset root from allowed to rejected, prove
    // no existing UI Widget Blueprint would be newly rejected -- the audited set comes from
    // the Asset Registry (every WidgetBlueprint under the three known UI content roots),
    // not a hand-typed list in this test, so a future .uasset cannot silently evade it.
    TArray<FGV2ContentRootOwnership> Ownership;
    FString OwnershipError;
    TestTrue(
        *FString::Printf(TEXT("PAH-05: GameData content root ownership resolves [Error: %s]"), *OwnershipError),
        UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(GV2PackageClosure::DiscoverFromGameData(), Ownership, OwnershipError));

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter UiAssetFilter;
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/UI"));
    for (const FGV2ContentRootOwnership& Entry : Ownership)
    {
        FString Path = Entry.NormalizedRoot;
        if (Path.EndsWith(TEXT("/")))
        {
            Path.LeftChopInline(1);
        }
        UiAssetFilter.PackagePaths.Add(*Path);
    }
    UiAssetFilter.bRecursivePaths = true;
    TArray<FAssetData> UiAssets;
    AssetRegistryModule.Get().GetAssets(UiAssetFilter, UiAssets);

    int32 WidgetBlueprintsAudited = 0;
    for (const FAssetData& Asset : UiAssets)
    {
        if (Asset.AssetClassPath.GetAssetName() != TEXT("WidgetBlueprint"))
        {
            continue;
        }
        ++WidgetBlueprintsAudited;
        const FString AssetPath = Asset.PackageName.ToString();
        const FString OwningPackage = UGV2ScreenRegistry::FindOwningPackageForAssetPath(AssetPath, Ownership);
        TestFalse(
            *FString::Printf(TEXT("PAH-03: '%s' under a known UI content root has a package owner"), *AssetPath),
            OwningPackage.IsEmpty());
    }
    TestTrue(TEXT("PAH-03: audit found Widget Blueprints to check"), WidgetBlueprintsAudited > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DebugStartScreenFlow,
    "GV2.Runtime.Presentation.StartButtonOpensRegisteredScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DebugStartScreenFlow::RunTest(const FString& Parameters)
{
    if (const UGV2UiTheme* ConfiguredTheme = LoadConfiguredThemeForTest())
    {
        for (const TSubclassOf<UCommonTextStyle>& StyleClass : { ConfiguredTheme->TextStyle, ConfiguredTheme->ButtonLabelStyle })
        {
            const UCommonTextStyle* Style = StyleClass != nullptr
                ? Cast<UCommonTextStyle>(StyleClass->GetDefaultObject())
                : nullptr;
            TestNotNull(TEXT("Theme text style is loadable"), Style);
            if (Style != nullptr)
            {
                FSlateFontInfo Font;
                Style->GetFont(Font);
                TestNotNull(TEXT("Theme text style has an explicit font"), Font.FontObject.Get());
                TestEqual(TEXT("Theme text style selects Regular typeface"), Font.TypefaceFontName, FName(TEXT("Regular")));
            }
        }
    }

    // TSR-03: Scoped-override RAII verification (nested scope and early exit restoration).
    {
        const bool bInitialFlag = FGV2SessionCoordinator::bTestForceIncludeSamplePackage;
        UGV2UiTheme* ConfiguredTheme = LoadConfiguredThemeForTest();
        const FString TestKey = FString::Printf(TEXT("%s:text.action.scout"), TEXT("sample"));
        const FText* InitialCatalogEntry = ConfiguredTheme ? ConfiguredTheme->FallbackTextCatalog.Find(TestKey) : nullptr;
        const TOptional<FText> InitialText = InitialCatalogEntry ? TOptional<FText>(*InitialCatalogEntry) : TOptional<FText>();

        // 1. Early exit restoration via lambda
        [&]() {
            FGV2ScopedSamplePackageOverride EarlyExitOverride;
            TestTrue(TEXT("Override enables bTestForceIncludeSamplePackage"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage);
            if (ConfiguredTheme)
            {
                TestTrue(TEXT("Override populates FallbackTextCatalog entry"), ConfiguredTheme->FallbackTextCatalog.Contains(TestKey));
            }
            return; // Early return unwinds stack
        }();

        TestEqual(TEXT("Early return restores bTestForceIncludeSamplePackage"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage, bInitialFlag);
        if (ConfiguredTheme)
        {
            const FText* RestoredEntry = ConfiguredTheme->FallbackTextCatalog.Find(TestKey);
            const TOptional<FText> RestoredText = RestoredEntry ? TOptional<FText>(*RestoredEntry) : TOptional<FText>();
            TestEqual(TEXT("Early return restores FallbackTextCatalog presence"), RestoredText.IsSet(), InitialText.IsSet());
        }

        // 2. Nested scope restoration
        {
            FGV2ScopedSamplePackageOverride OuterOverride;
            TestTrue(TEXT("Outer override enables flag"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage);
            {
                FGV2ScopedSamplePackageOverride InnerOverride;
                TestTrue(TEXT("Inner override maintains flag enabled"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage);
            }
            TestTrue(TEXT("Exiting inner scope preserves outer override flag"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage);
            if (ConfiguredTheme)
            {
                TestTrue(TEXT("Exiting inner scope preserves outer override catalog"), ConfiguredTheme->FallbackTextCatalog.Contains(TestKey));
            }
        }
        TestEqual(TEXT("Exiting outer scope restores initial flag"), FGV2SessionCoordinator::bTestForceIncludeSamplePackage, bInitialFlag);
        if (ConfiguredTheme)
        {
            const FText* RestoredEntry = ConfiguredTheme->FallbackTextCatalog.Find(TestKey);
            const TOptional<FText> RestoredText = RestoredEntry ? TOptional<FText>(*RestoredEntry) : TOptional<FText>();
            TestEqual(TEXT("Exiting outer scope restores initial catalog state"), RestoredText.IsSet(), InitialText.IsSet());
        }
    }

    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the runtime"), Runtime);
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
        UGV2ScreenWidgetBase* Screen = Cast<UGV2ScreenWidgetBase>(
            Runtime->GetActiveScreen());
        TestNotNull(TEXT("GameInstance start directly opens the registered WBP_Testscreen"), Screen);
        Runtime->EndSession();
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaTestScreenWidgetCreation,
    "GV2.Runtime.Presentation.LuaCreatesRegisteredScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaTestScreenWidgetCreation::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the GV2 runtime subsystem"), Runtime);

    UGV2ScreenWidgetBase* Screen = nullptr;
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
        Screen = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
        TestNotNull(TEXT("Lua start hook instantiates and applies WBP_Testscreen"), Screen);
        if (Screen != nullptr)
        {
            UGV2GameShellWidgetBase* Shell = Runtime->GetActiveGameShell();
            TestNotNull(TEXT("Lua start hook presents the test screen through WBP_GameShell"), Shell);
            if (Shell != nullptr)
            {
                const TArray<UUserWidget*> LayerScreens = Shell->GetScreensInLayer(
                    UGV2GameShellWidgetBase::LayerLocationContent);
                TestEqual(TEXT("Game shell location layer contains one screen"), LayerScreens.Num(), 1);
                if (LayerScreens.Num() == 1)
                {
                    TestTrue(TEXT("Active test screen is attached to the location layer"), LayerScreens[0] == Screen);
                }
            }
            // PSC-13: compare against the active session's pinned registry authority, not
            // a separately loaded/configured registry rebuilt from GameData. Otherwise the
            // test itself would reproduce PAH-R3 while claiming to prove the opposite.
            const FGV2SessionContentSnapshot* ActiveSnapshot = Runtime->GetContentSnapshotForAutomationTest();
            TestNotNull(TEXT("Active session exposes its pinned content snapshot"), ActiveSnapshot);
            if (ActiveSnapshot != nullptr)
            {
                const TArray<FString> ExpectedEditorPackageIds = {
                    TEXT("core"), TEXT("textsystem"), TEXT("sample")};
                TestEqual(
                    TEXT("Editor sample profile is the snapshot's exact ordered package set"),
                    FString::Join(ActiveSnapshot->GetOrderedPackageIds(), TEXT(",")),
                    FString::Join(ExpectedEditorPackageIds, TEXT(",")));

                // Enumerate the pinned repository instead of hard-coding a definition ID
                // owned by the optional game package. This proves that the repository
                // consumed the selected profile while preserving the core-decoupling rule:
                // engine tests may select a package fixture, but must not depend on one of
                // its published Stable IDs.
                const FString ProfilePackageId = ExpectedEditorPackageIds.Last();
                bool bHasScreenFromProfilePackage = false;
                for (const GV2ContentCore::FDefinitionId& ScreenId
                    : ActiveSnapshot->GetRepository().List("screen"))
                {
                    const GV2ContentCore::FDefinitionProvenance* Provenance =
                        ActiveSnapshot->GetRepository().GetProvenance(ScreenId);
                    if (Provenance != nullptr
                        && FString(UTF8_TO_TCHAR(Provenance->Winner.PackageId.c_str())) == ProfilePackageId)
                    {
                        bHasScreenFromProfilePackage = true;
                        break;
                    }
                }
                TestTrue(
                    TEXT("Pinned repository contains a screen supplied by the selected profile package"),
                    bHasScreenFromProfilePackage);

                bool bHasSampleLuaSource = false;
                bool bHasRhLuaSource = false;
                for (const GV2RuntimeCore::FRuntimeSource& Source : ActiveSnapshot->GetLuaSources())
                {
                    bHasSampleLuaSource |= Source.Name.starts_with("@sample/");
                    bHasRhLuaSource |= Source.Name.starts_with("@rh/");
                }
                TestTrue(TEXT("Lua source loader consumed the sample profile"), bHasSampleLuaSource);
                TestFalse(TEXT("Lua source loader did not rediscover canonical rh"), bHasRhLuaSource);
            }
            FGV2ResolvedScreenDescriptor RegisteredDescriptor;
            FGV2ScreenResolutionRejection RegisteredRejection;
            UClass* RegisteredClass = ActiveSnapshot != nullptr
                    && ActiveSnapshot->GetScreenRegistry().Resolve(
                        TEXT("core:screen.test"),
                        FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent),
                        RegisteredDescriptor,
                        RegisteredRejection)
                ? RegisteredDescriptor.WidgetClass
                : nullptr;
            TestEqual(
                TEXT("Created initial screen class comes from the active snapshot registry"),
                Screen->GetClass(),
                RegisteredClass);

            const TArray<FName> ScreenFieldIds = Screen->GetScreenFieldIds();
            // DUC-02: GreetingText (a plain UGV2TextWidgetBase, no dedicated C++ class) is
            // now addressable and configured with HostIdentity="greeting" -- the other five
            // original children still have no identity configured and remain unconfigured/
            // skipped, exactly as before.
            TestEqual(TEXT("Test screen exposes exactly the DUC-02 'greeting' field, the rest remain static"), ScreenFieldIds, TArray<FName>{FName(TEXT("greeting"))});

            UGV2TextWidgetBase* GreetingWidget = Cast<UGV2TextWidgetBase>(Screen->GetWidgetFromName(TEXT("GreetingText")));
            TestNotNull(TEXT("DUC-02: GreetingText base element is present"), GreetingWidget);
            if (GreetingWidget != nullptr)
            {
                TestEqual(
                    TEXT("DUC-02: Lua-published 'greeting' Screen Field value reaches the base element with no dedicated C++ class"),
                    GreetingWidget->GetTextContent().ToString(),
                    FString(TEXT("Hello from a base element field")));
            }

            UGV2RichTextWidgetBase* DescriptionWidget = Cast<UGV2RichTextWidgetBase>(
                Screen->GetWidgetFromName(TEXT("DescriptionText")));
            UGV2CheckboxWidgetBase* CheckboxWidget = Cast<UGV2CheckboxWidgetBase>(
                Screen->GetWidgetFromName(TEXT("CheckboxField")));
            TestNotNull(TEXT("Test screen uses the reusable checkbox component"), CheckboxWidget);
            if (CheckboxWidget != nullptr)
            {
                TestFalse(
                    TEXT("Lua initializes the checkbox as unchecked"),
                    CheckboxWidget->IsChecked());
            }
            TestNotNull(TEXT("Test screen uses the reusable rich text component"), DescriptionWidget);

            UGV2ScreenWidgetBase* CurrentScreen = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
            UGV2DropdownSelectWidgetBase* DropdownWidget = CurrentScreen != nullptr
                ? Cast<UGV2DropdownSelectWidgetBase>(
                    CurrentScreen->GetWidgetFromName(TEXT("ClassSelectField")))
                : nullptr;
            TestNotNull(TEXT("Test screen uses the reusable dropdown component"), DropdownWidget);
            if (DropdownWidget != nullptr && DropdownWidget->GetBindingHandle().IsValid())
            {
                TestEqual(
                    TEXT("Dropdown submits exactly its schema-bound selected_key interaction"),
                    DropdownWidget->SubmitSelection(TEXT("mage")),
                    EGV2SubmitUiInteractionResult::Accepted);
                CurrentScreen = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
                UGV2DropdownSelectWidgetBase* ReconciledDropdown = CurrentScreen != nullptr
                    ? Cast<UGV2DropdownSelectWidgetBase>(
                        CurrentScreen->GetWidgetFromName(TEXT("ClassSelectField")))
                    : nullptr;
                TestNotNull(TEXT("Reconciled dropdown is present"), ReconciledDropdown);
                if (ReconciledDropdown != nullptr)
                {
                    TestEqual(
                        TEXT("Lua owns and republishes the accepted dropdown selection"),
                        ReconciledDropdown->GetSelectedKey(),
                        FName(TEXT("mage")));
                }
            }
        }
        Runtime->EndSession();
    }

    if (Screen != nullptr)
    {
        Screen->RemoveFromParent();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CommittedPresentationViewportResizeTest,
    "GV2.Runtime.Presentation.CommittedPresentationRespondsToViewportResize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-14 regression: exercise a real session-owned screen through the production
// FViewport::ViewportResizedEvent path. The same already-committed widget must update its
// prepared font scale when the active game viewport changes size; publishing another Lua
// document is deliberately absent from this transition. This is the action the earlier
// per-height tests did not perform.
bool FGV2CommittedPresentationViewportResizeTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld == nullptr)
    {
        AddError(TEXT("Standalone GameInstance did not create a world"));
        return false;
    }

    FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(TestWorld);
    UGameViewportClient* const PreviousEngineViewport = GEngine->GameViewport;
    UGameViewportClient* const PreviousWorldViewport = WorldContext.GameViewport;

    UGameViewportClient* TestViewportClient = NewObject<UGameViewportClient>(GEngine);
    TSharedRef<FSceneViewport> TestViewport = MakeShared<FSceneViewport>(TSharedPtr<SViewport>());
    TestViewportClient->AddAssociation(*TestViewport);
    GEngine->GameViewport = TestViewportClient;
    WorldContext.GameViewport = TestViewportClient;
    TestViewport->SetInitialSize(FIntPoint(1280, 720));

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Viewport-resize scenario has the production runtime subsystem"), Runtime);

    UGV2ScreenWidgetBase* ScreenBeforeResize = nullptr;
    UGV2TextWidgetBase* GreetingBeforeResize = nullptr;
    int32 FontSizeAt720 = 0;
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
        ScreenBeforeResize = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
        TestNotNull(TEXT("Session publishes its initial screen before resize"), ScreenBeforeResize);
        UOverlaySlot* const ProductionLayerSlot = ScreenBeforeResize != nullptr
            ? Cast<UOverlaySlot>(ScreenBeforeResize->Slot)
            : nullptr;
        TestNotNull(TEXT("Session-published screen is attached to a GameShell Overlay layer"), ProductionLayerSlot);
        if (ProductionLayerSlot != nullptr)
        {
            TestEqual(
                TEXT("Production reconcile preserves horizontal viewport fill"),
                ProductionLayerSlot->GetHorizontalAlignment(),
                HAlign_Fill);
            TestEqual(
                TEXT("Production reconcile preserves vertical viewport fill"),
                ProductionLayerSlot->GetVerticalAlignment(),
                VAlign_Fill);
        }
        GreetingBeforeResize = ScreenBeforeResize != nullptr
            ? Cast<UGV2TextWidgetBase>(ScreenBeforeResize->GetWidgetFromName(TEXT("GreetingText")))
            : nullptr;
        TestNotNull(TEXT("Initial screen exposes the prepared text consumer"), GreetingBeforeResize);
        if (GreetingBeforeResize != nullptr && GreetingBeforeResize->GetTextBlock() != nullptr)
        {
            // SetInitialSize has no RHI-backed viewport yet and the first document may have
            // used its prepared reference-height fallback. Establish the 720p baseline by
            // sending the same real resize event the product receives; without the
            // production subscription this and the following resize would both leave the
            // committed font unchanged.
            TestViewport->UpdateViewportRHI(
                false,
                1280,
                720,
                EWindowMode::Windowed,
                PF_Unknown);
            FontSizeAt720 = GreetingBeforeResize->GetTextBlock()->GetFont().Size;
            TestTrue(TEXT("Initial 720p font size is positive"), FontSizeAt720 > 0);

            // This call changes the real FViewport size and broadcasts the engine's
            // production resize event. No document, command or test-only refresh seam is
            // invoked around it.
            TestViewport->UpdateViewportRHI(
                false,
                3840,
                2160,
                EWindowMode::Windowed,
                PF_Unknown);

            UGV2ScreenWidgetBase* const ScreenAfterResize =
                Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
            UGV2TextWidgetBase* const GreetingAfterResize = ScreenAfterResize != nullptr
                ? Cast<UGV2TextWidgetBase>(ScreenAfterResize->GetWidgetFromName(TEXT("GreetingText")))
                : nullptr;
            TestTrue(
                TEXT("Viewport refresh preserves the committed screen/widget instance"),
                ScreenAfterResize == ScreenBeforeResize && GreetingAfterResize == GreetingBeforeResize);
            if (GreetingAfterResize != nullptr && GreetingAfterResize->GetTextBlock() != nullptr)
            {
                const int32 FontSizeAt2160 = GreetingAfterResize->GetTextBlock()->GetFont().Size;
                TestTrue(
                    *FString::Printf(
                        TEXT("Same committed text responds to 720p -> 2160p resize (%d -> %d)"),
                        FontSizeAt720,
                        FontSizeAt2160),
                    FontSizeAt2160 > FontSizeAt720);
            }
        }
        Runtime->EndSession();
    }

    GEngine->GameViewport = PreviousEngineViewport;
    WorldContext.GameViewport = PreviousWorldViewport;
    TestViewportClient->RemoveAssociation(*TestViewport);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CommandReconcilePreservesPreparedTypographyTest,
    "GV2.Runtime.Presentation.CommandReconcilePreservesPreparedTypography",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A process can own more than one game viewport in PIE. Establish a 720p baseline through
// the production resize path while a deliberately different process-global viewport reports
// 2160p, then execute the real RH work command. Every command button comes from the actual
// keyed collection; no hand-maintained list can hide a newly added entry from the comparison.
// Removing the context-world preference from ResolveLiveViewportHeight makes the post-command
// Apply rescale these labels against the foreign viewport, or allowing a later CommonUI
// style pass to erase prepared typography, makes this test fail.
bool FGV2CommandReconcilePreservesPreparedTypographyTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext ScopedWorld;
    UGameInstance* GameInstance = ScopedWorld.GetGameInstance();
    UWorld* TestWorld = ScopedWorld.GetWorld();
    if (TestWorld == nullptr)
    {
        AddError(TEXT("Standalone GameInstance did not create a world"));
        return false;
    }

    FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(TestWorld);
    UGameViewportClient* const PreviousEngineViewport = GEngine->GameViewport;
    UGameViewportClient* const PreviousWorldViewport = WorldContext.GameViewport;

    UGameViewportClient* OwningViewportClient = NewObject<UGameViewportClient>(GEngine);
    UGameViewportClient* ForeignViewportClient = NewObject<UGameViewportClient>(GEngine);
    TSharedRef<FSceneViewport> OwningViewport = MakeShared<FSceneViewport>(TSharedPtr<SViewport>());
    TSharedRef<FSceneViewport> ForeignViewport = MakeShared<FSceneViewport>(TSharedPtr<SViewport>());
    OwningViewportClient->AddAssociation(*OwningViewport);
    ForeignViewportClient->AddAssociation(*ForeignViewport);
    OwningViewport->SetInitialSize(FIntPoint(1280, 720));
    ForeignViewport->SetInitialSize(FIntPoint(3840, 2160));
    WorldContext.GameViewport = OwningViewportClient;
    GEngine->GameViewport = ForeignViewportClient;

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Owning-viewport scenario has the production runtime subsystem"), Runtime);

    auto FindCommandRepeater = [](UGV2ScreenWidgetBase* Screen) -> UGV2ListViewWidgetBase*
    {
        if (Screen == nullptr || Screen->WidgetTree == nullptr)
        {
            return nullptr;
        }
        UGV2ListViewWidgetBase* Result = nullptr;
        Screen->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
        {
            if (Result != nullptr)
            {
                return;
            }
            if (UGV2DeclaredCompositeWidgetBase* Composite = Cast<UGV2DeclaredCompositeWidgetBase>(Widget);
                Composite != nullptr && Composite->GetHostIdentity() == FName(TEXT("commands")))
            {
                Result = Cast<UGV2ListViewWidgetBase>(Composite->GetWidgetFromName(TEXT("ButtonRepeater")));
            }
        });
        return Result;
    };

    TMap<FName, float> BaselineFontSizes;
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);

        // The real resize delegate supplies the owning viewport's height and normalizes the
        // already committed tree before the command. Expected values therefore come from
        // observed product state, independently of the resolver under test.
        OwningViewport->UpdateViewportRHI(
            false,
            1280,
            720,
            EWindowMode::Windowed,
            PF_Unknown);

        UGV2ScreenWidgetBase* ScreenBeforeCommand = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        UGV2ListViewWidgetBase* RepeaterBeforeCommand = FindCommandRepeater(ScreenBeforeCommand);
        TestNotNull(TEXT("RH LocationScreen exposes its production command repeater"), RepeaterBeforeCommand);

        if (RepeaterBeforeCommand != nullptr)
        {
            for (const TPair<FName, TObjectPtr<UWidget>>& Pair : RepeaterBeforeCommand->GetActiveWidgetsMap())
            {
                const UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(Pair.Value.Get());
                const UCommonTextBlock* Label = Button != nullptr ? Button->GetLabelText() : nullptr;
                TestNotNull(
                    *FString::Printf(TEXT("Command entry '%s' is a button with a label"), *Pair.Key.ToString()),
                    Label);
                if (Label != nullptr)
                {
                    BaselineFontSizes.Add(Pair.Key, Label->GetFont().Size);
                }
            }
        }
        TestTrue(TEXT("The production command collection supplies multiple enumerated entries"), BaselineFontSizes.Num() > 1);

        UGV2ButtonWidgetBase* WorkButton = RepeaterBeforeCommand != nullptr
            ? Cast<UGV2ButtonWidgetBase>(RepeaterBeforeCommand->GetEntryWidget(FName(TEXT("do_work"))))
            : nullptr;
        TestNotNull(TEXT("RH command collection exposes do_work"), WorkButton);
        if (WorkButton != nullptr)
        {
            TestEqual(TEXT("Command button belongs to the session world"), WorkButton->GetWorld(), TestWorld);
            TestEqual(
                TEXT("Command button world identifies the owning viewport"),
                WorkButton->GetWorld() != nullptr ? WorkButton->GetWorld()->GetGameViewport() : nullptr,
                OwningViewportClient);
            TestEqual(
                TEXT("Shared viewport resolver prefers the command button's owning viewport"),
                GV2PresentationApply::ResolveLiveViewportHeight(WorkButton, 1080.0f),
                720.0f);
            const EGV2SubmitUiInteractionResult SubmitResult =
                Runtime->SubmitUiInteraction(WorkButton->GetBindingHandle(), {});
            TestEqual(TEXT("The production work command is accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
        }

        UGV2ScreenWidgetBase* ScreenAfterCommand = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        UGV2ListViewWidgetBase* RepeaterAfterCommand = FindCommandRepeater(ScreenAfterCommand);
        TestNotNull(TEXT("Command reconcile republishes the command repeater"), RepeaterAfterCommand);
        if (RepeaterAfterCommand != nullptr)
        {
            TestEqual(
                TEXT("Command reconcile preserves the enumerated command set"),
                RepeaterAfterCommand->GetEntryCount(),
                BaselineFontSizes.Num());
            for (const TPair<FName, float>& Pair : BaselineFontSizes)
            {
                const UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(
                    RepeaterAfterCommand->GetEntryWidget(Pair.Key));
                const UCommonTextBlock* Label = Button != nullptr ? Button->GetLabelText() : nullptr;
                TestNotNull(
                    *FString::Printf(TEXT("Command entry '%s' survives reconcile"), *Pair.Key.ToString()),
                    Label);
                if (Label != nullptr)
                {
                    TestEqual(
                        *FString::Printf(
                            TEXT("Command entry '%s' keeps the owning-viewport font size"),
                            *Pair.Key.ToString()),
                        Label->GetFont().Size,
                        Pair.Value);
                }
            }
        }

        Runtime->EndSession();
    }

    GEngine->GameViewport = PreviousEngineViewport;
    WorldContext.GameViewport = PreviousWorldViewport;
    OwningViewportClient->RemoveAssociation(*OwningViewport);
    ForeignViewportClient->RemoveAssociation(*ForeignViewport);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2GameShellViewportFillTest,
    "GV2.Runtime.UI.GameShellViewportFill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-AF-02: the old viewport matrix put WBP_LocationScreen directly in an
// SVirtualWindow, while the first version of this test attached it through
// GameShell::AttachScreenToLayer. Both bypassed the keyed collection rebuild used by
// production reconciliation, which can replace a configured panel slot with a new slot.
// Drive the REAL reconciler here, keep expected bounds outside the widget tree, and
// enumerate every layer through GameShell's canonical layer set.
bool FGV2GameShellViewportFillTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    if (TestWorld == nullptr)
    {
        AddError(TEXT("GameShell viewport-fill scenario could not create a game world"));
        return false;
    }

    UClass* ShellClass = LoadClass<UGV2GameShellWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
    TestNotNull(TEXT("Production WBP_GameShell is loadable"), ShellClass);

    UGV2GameShellWidgetBase* Shell = ShellClass != nullptr
        ? CreateWidget<UGV2GameShellWidgetBase>(TestWorld, ShellClass)
        : nullptr;
    TestNotNull(TEXT("Production GameShell is instantiated"), Shell);

    if (Shell != nullptr)
    {
        FGV2UiDocumentViewModel Document;
        Document.UiInstanceId = TEXT("ui@psc14_viewport_fill");
        Document.Revision = 1;

        TMap<FName, FName> InstanceKeyByLayer;
        for (const FName Layer : UGV2GameShellWidgetBase::GetApprovedLayers())
        {
            FGV2ScreenInstanceViewModel Instance;
            Instance.Layer = Layer;
            Instance.InstanceKey = FName(*FString::Printf(TEXT("psc14_%s"), *Layer.ToString()));
            Instance.ScreenId = FString::Printf(TEXT("core:screen.psc14_%s_probe"), *Layer.ToString());
            InstanceKeyByLayer.Add(Layer, Instance.InstanceKey);

            if (Layer == UGV2GameShellWidgetBase::LayerLocationContent)
            {
                Document.bHasRoute = true;
                Document.Route = Instance;
            }
            else if (Layer == UGV2GameShellWidgetBase::LayerModalStack)
            {
                Document.Modals.Add(Instance);
            }
            else
            {
                Document.Overlays.Add(Instance);
            }
        }

        FGV2LayeredUiReconciler Reconciler;
        auto ScreenFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
        {
            return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
        };
        FString ReconcileError;
        const bool bReconciled = Reconciler.Reconcile(
            Shell,
            Document,
            ScreenFactory,
            ReconcileError,
            *PrepareContext);
        TestTrue(
            *FString::Printf(TEXT("Production layered reconciliation succeeds: %s"), *ReconcileError),
            bReconciled);
        if (!bReconciled)
        {
            return false;
        }

        int32 VerifiedLayerSlots = 0;
        for (const FName Layer : UGV2GameShellWidgetBase::GetApprovedLayers())
        {
            UGV2ScreenWidgetBase* LayerScreen = Reconciler.GetActiveScreen(Layer, InstanceKeyByLayer.FindRef(Layer));
            TestNotNull(
                *FString::Printf(TEXT("Reconciler publishes a screen in layer '%s'"), *Layer.ToString()),
                LayerScreen);
            UOverlaySlot* LayerSlot = LayerScreen != nullptr ? Cast<UOverlaySlot>(LayerScreen->Slot) : nullptr;
            TestNotNull(
                *FString::Printf(TEXT("Layer '%s' screen uses the authored Overlay host"), *Layer.ToString()),
                LayerSlot);
            if (LayerSlot != nullptr)
            {
                TestEqual(
                    *FString::Printf(TEXT("Layer '%s' reconciled slot fills horizontally"), *Layer.ToString()),
                    LayerSlot->GetHorizontalAlignment(),
                    HAlign_Fill);
                TestEqual(
                    *FString::Printf(TEXT("Layer '%s' reconciled slot fills vertically"), *Layer.ToString()),
                    LayerSlot->GetVerticalAlignment(),
                    VAlign_Fill);
                ++VerifiedLayerSlots;
            }
        }
        TestEqual(
            TEXT("Every canonical GameShell layer has an inspected production slot"),
            VerifiedLayerSlots,
            UGV2GameShellWidgetBase::GetApprovedLayers().Num());

        TSharedPtr<SWidget> ShellSlate = Shell->TakeWidget();
        TestTrue(TEXT("Production GameShell produces a Slate widget"), ShellSlate.IsValid());
        if (ShellSlate.IsValid())
        {
            TSharedRef<SVirtualWindow> VirtualWindow =
                SNew(SVirtualWindow).Size(FVector2D(1280.0f, 720.0f));
            VirtualWindow->SetContent(ShellSlate.ToSharedRef());

            const FVector2D Resolutions[] = {
                FVector2D(1280.0f, 720.0f),
                FVector2D(1920.0f, 1080.0f),
                FVector2D(2560.0f, 1080.0f),
            };
            int32 VerifiedLayerGeometries = 0;
            for (const FVector2D& Resolution : Resolutions)
            {
                Shell->InvalidateLayoutAndVolatility();
                GV2SimulateResponsiveFrame(VirtualWindow, Resolution);

                const FVector2D ShellSize =
                    Shell->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                TestTrue(
                    *FString::Printf(
                        TEXT("GameShell fills independent viewport bounds %s (actual %s)"),
                        *Resolution.ToString(),
                        *ShellSize.ToString()),
                    ShellSize.Equals(Resolution, 1.0f));

                for (const FName Layer : UGV2GameShellWidgetBase::GetApprovedLayers())
                {
                    UPanelWidget* Host = Shell->GetHostForLayer(Layer);
                    TestNotNull(
                        *FString::Printf(TEXT("GameShell exposes authored host for layer '%s'"), *Layer.ToString()),
                        Host);
                    if (Host != nullptr && Host->GetCachedWidget().IsValid())
                    {
                        const FVector2D HostSize =
                            Host->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                        TestTrue(
                            *FString::Printf(
                                TEXT("Layer '%s' fills independent viewport bounds %s (actual %s)"),
                                *Layer.ToString(),
                                *Resolution.ToString(),
                                *HostSize.ToString()),
                            HostSize.Equals(Resolution, 1.0f));
                        ++VerifiedLayerGeometries;
                    }

                    UGV2ScreenWidgetBase* LayerScreen = Reconciler.GetActiveScreen(
                        Layer,
                        InstanceKeyByLayer.FindRef(Layer));
                    if (LayerScreen != nullptr && LayerScreen->GetCachedWidget().IsValid())
                    {
                        const FVector2D ScreenSize =
                            LayerScreen->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                        TestTrue(
                            *FString::Printf(
                                TEXT("Reconciled screen in layer '%s' fills independent viewport bounds %s (actual %s)"),
                                *Layer.ToString(),
                                *Resolution.ToString(),
                                *ScreenSize.ToString()),
                            ScreenSize.Equals(Resolution, 1.0f));
                    }
                }
            }

            TestEqual(
                TEXT("Every canonical GameShell layer is geometry-checked at every resolution"),
                VerifiedLayerGeometries,
                static_cast<int32>(UE_ARRAY_COUNT(Resolutions))
                    * UGV2GameShellWidgetBase::GetApprovedLayers().Num());
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2InputFieldWidgetContract,
    "GV2.Runtime.UIKit.InputFieldWidgetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2InputFieldWidgetContract::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured theme is available for prepared text fixtures"), Theme);

    UClass* WidgetClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"));
    TestNotNull(TEXT("WBP_InputField_C is loadable"), WidgetClass);
    if (WidgetClass == nullptr)
    {
        return false;
    }

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2InputFieldWidgetBase* InputFieldWidget = TestWorld != nullptr
        ? CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, WidgetClass)
        : nullptr;
    TestNotNull(TEXT("InputFieldWidget instantiates"), InputFieldWidget);

    if (InputFieldWidget != nullptr)
    {
        TestTrue(
            TEXT("InputFieldWidget implements IGV2UiPropertyHost"),
            InputFieldWidget->GetClass()->ImplementsInterface(UGV2UiPropertyHost::StaticClass()));
        TestTrue(
            TEXT("InputFieldWidget implements IGV2UiBindingTarget"),
            InputFieldWidget->GetClass()->ImplementsInterface(UGV2UiBindingTarget::StaticClass()));
        TestTrue(
            TEXT("InputFieldWidget implements IGV2UiStyleConsumer"),
            InputFieldWidget->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()));

        InputFieldWidget->SetKey(TEXT("user_name"));
        InputFieldWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("core:input.user_name")));
        InputFieldWidget->SetValue(TEXT("King"));
        InputFieldWidget->SetMaxLength(20);
        InputFieldWidget->SetIsReadOnly(false);

        if (Theme != nullptr)
        {
            InputFieldWidget->ApplyText(
                MakeResolvedLiteralTextForTest(*Theme, TEXT("Player Name")));
            InputFieldWidget->ApplyPlaceholderText(
                MakeResolvedLiteralTextForTest(*Theme, TEXT("Enter name...")));
        }

        TestEqual(TEXT("Key matches"), InputFieldWidget->GetKey(), FName(TEXT("user_name")));
        TestEqual(TEXT("Value matches"), InputFieldWidget->GetValue(), FString(TEXT("King")));
        TestEqual(TEXT("MaxLength matches"), InputFieldWidget->GetMaxLength(), static_cast<int64>(20));
        TestFalse(TEXT("IsReadOnly matches"), InputFieldWidget->GetIsReadOnly());

        EGV2SubmitUiInteractionResult SubmitResult = InputFieldWidget->SubmitTextValue(TEXT("NewKing"));
        TestEqual(
            TEXT("SubmitTextValue returns technical result from emitter"),
            SubmitResult,
            EGV2SubmitUiInteractionResult::RuntimeNotReady);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiTabContainerConsumerContractTest,
    "GV2.Runtime.UI.TabContainerConsumerContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiTabContainerConsumerContractTest::RunTest(const FString& Parameters)
{
    if (UGV2UiTheme* Theme = LoadConfiguredThemeForTest())
    {
        Theme->TextCatalog.FindOrAdd(TEXT("core:text.duc09_day"), FText::FromString(TEXT("Monday")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inventory"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_use"), FText::FromString(TEXT("Use Potion")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_skills"), FText::FromString(TEXT("Skills")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_learn"), FText::FromString(TEXT("Learn Fireball")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inv"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_a"), FText::FromString(TEXT("Title A")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_b"), FText::FromString(TEXT("Title B")));
    }

    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    const FGV2ResolvedUiTheme& Theme = PrepareContext->GetTheme();

    // =========================================================================
    // UIF-22: Registry Layer 'embedded' and placement rules
    // =========================================================================
    {
        TestTrue(TEXT("embedded is a valid registry layer"), UGV2ScreenRegistry::IsValidLayer(TEXT("embedded")));
        TestFalse(TEXT("embedded is not allowed for top-level route/overlays/modals"), UGV2ScreenRegistry::IsLayerAllowedForTopLevel(TEXT("embedded")));
        TestTrue(TEXT("embedded is allowed for nested/embedded content"), UGV2ScreenRegistry::IsLayerAllowedForEmbedded(TEXT("embedded")));
        TestFalse(TEXT("location_content is not allowed for embedded tab content"), UGV2ScreenRegistry::IsLayerAllowedForEmbedded(TEXT("location_content")));
        TestFalse(TEXT("modal_stack is not allowed for embedded tab content"), UGV2ScreenRegistry::IsLayerAllowedForEmbedded(TEXT("modal_stack")));
    }

    // =========================================================================
    // UIF-23 & UIF-24: Tab Container Schema Adapter, Consumer & Off-Tree Validation
    // =========================================================================
    {
        TSharedPtr<IGV2PropertyConsumer> Consumer = FGV2PropertyConsumerFactory::CreateConsumer(
            EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
        TestNotNull(TEXT("TabContainer consumer created"), Consumer.Get());
        Consumer->SetPrepareContext(PrepareContext);

        FGV2UiPropertyCapability TabCap;
        TabCap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;

        // 1. Rejection of empty tabs
        TArray<FGV2PreparedUiValue> EmptyTabs;
        FString PrepErr;
        TestFalse(TEXT("Empty tabs list rejected"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(EmptyTabs)), TabCap, nullptr, PrepErr));

        // 2. Rejection of duplicate tab keys
        TArray<FGV2PreparedUiValue> DupTabs;
        TMap<FString, FGV2PreparedUiValue> T1;
        T1.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("tab_a")));
        T1.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Tab A"))));
        T1.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.tab_a"), TEXT("screen")));
        DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(T1)));

        TMap<FString, FGV2PreparedUiValue> T2;
        T2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("tab_a"))); // duplicate key
        T2.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Tab B"))));
        T2.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.tab_b"), TEXT("screen")));
        DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(T2)));

        TestFalse(TEXT("Duplicate tab keys rejected"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DupTabs)), TabCap, nullptr, PrepErr));

        // 3. Valid tabs array preparation
        TArray<FGV2PreparedUiValue> ValidTabs;
        TMap<FString, FGV2PreparedUiValue> V1;
        V1.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("inventory")));
        V1.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Inventory"))));
        V1.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test_embedded"), TEXT("screen")));
        ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(V1)));

        TMap<FString, FGV2PreparedUiValue> V2;
        V2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("skills")));
        V2.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Skills"))));
        V2.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test_embedded"), TEXT("screen")));
        ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(V2)));

        TestTrue(TEXT("Valid tabs prepare succeeds"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(ValidTabs)), TabCap, nullptr, PrepErr));
    }

    // =========================================================================
    // UIF-27: DUC-09 -- nested screen fields through the standard envelope
    // =========================================================================
    {
        // 27a. Materializer: EUiFieldKind::ScreenFields resolves each envelope's
        // own schema_id through the real schema cache and materializes its value
        // through the exact same ValidateUiFieldValue + ProjectMaterializedValue
        // pair a top-level screen field uses -- no schema synthesized from a
        // widget's capability, no separate protocol.
        {
            if (UGV2UiTheme* MutableConfiguredTheme = LoadConfiguredThemeForTest())
            {
                MutableConfiguredTheme->TextCatalog.FindOrAdd(
                    TEXT("core:text.duc09_day"),
                    FText::FromString(TEXT("Monday")));
            }

            using FContentObject = GV2ContentCore::FValue::FObject;

            GV2ContentCore::FCompiledUiFieldSpec OuterSchema;
            OuterSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
            OuterSchema.Fields.push_back({ "fields", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::ScreenFields) });

            const FContentObject TextObj{
                { "text_id", GV2ContentCore::FValue(std::string("core:text.duc09_day")) },
            };
            const FContentObject InnerValueObj{
                { "day", GV2ContentCore::FValue(TextObj) },
                { "value", GV2ContentCore::FValue(0.5) },
            };
            const FContentObject EnvelopeObj{
                { "field_id", GV2ContentCore::FValue(std::string("day_block")) },
                { "schema_id", GV2ContentCore::FValue(std::string("core:schema.ui_field.synthetic_declared_composite.v1")) },
                { "value", GV2ContentCore::FValue(InnerValueObj) },
            };
            const GV2ContentCore::FValue::FArray EnvelopesArray{ GV2ContentCore::FValue(EnvelopeObj) };
            const FContentObject OuterObj{
                { "fields", GV2ContentCore::FValue(EnvelopesArray) },
            };
            const GV2ContentCore::FValue OuterContentValue(OuterObj);

            GV2ContentCore::FValue OuterMaterialized;
            std::vector<GV2ContentCore::FDiagnostic> OuterDiags;
            GV2ContentCore::FValidationDiagnosticContext OuterCtx;
            OuterCtx.SchemaId = "test:schema.duc09_screen_fields_probe.v1";
            const bool bOuterValidated = GV2ContentCore::ValidateUiFieldValue(
                OuterContentValue, OuterSchema, OuterMaterialized, nullptr, "", OuterCtx, OuterDiags);
            TestTrue(TEXT("DUC-09: outer object with a screen_fields property validates"), bOuterValidated);

            GV2ScreenFieldMaterializer::FMaterializeContext MatCtx;
            TArray<FGV2UiBindingHandle> NoHandles;
            int32 HandleCursor = 0;
            MatCtx.Handles = &NoHandles;
            MatCtx.HandleCursor = &HandleCursor;
            MatCtx.PrepareContext = PrepareContext;

            FGV2PreparedUiValue ProjectedOuter;
            const bool bProjected = bOuterValidated && GV2ScreenFieldMaterializer::ProjectMaterializedValue(
                MatCtx, OuterSchema, OuterMaterialized, ProjectedOuter);
            TestTrue(TEXT("DUC-09: screen_fields materializes through the real schema cache"), bProjected);

            if (bProjected && ProjectedOuter.IsObject())
            {
                const FGV2PreparedUiValue* FieldsProjected = ProjectedOuter.AsObject().FindField(TEXT("fields"));
                TestNotNull(TEXT("DUC-09: projected value has a 'fields' property"), FieldsProjected);
                if (FieldsProjected != nullptr && FieldsProjected->IsArray())
                {
                    const TArray<FGV2PreparedUiValue>& Envelopes = FieldsProjected->AsArray().GetElements();
                    TestEqual(TEXT("DUC-09: one nested field envelope produced"), Envelopes.Num(), 1);
                    if (Envelopes.Num() == 1 && Envelopes[0].IsObject())
                    {
                        const FGV2PreparedUiObject& EnvelopeOut = Envelopes[0].AsObject();
                        const FGV2PreparedUiValue* FieldIdOut = EnvelopeOut.FindField(TEXT("field_id"));
                        const FGV2PreparedUiValue* SchemaIdOut = EnvelopeOut.FindField(TEXT("schema_id"));
                        const FGV2PreparedUiValue* ValueOut = EnvelopeOut.FindField(TEXT("value"));
                        if (TestTrue(TEXT("DUC-09: envelope field_id is a Key"), FieldIdOut != nullptr && FieldIdOut->IsKey()))
                        {
                            TestEqual(TEXT("DUC-09: envelope field_id value"), FieldIdOut->AsKey(), FString(TEXT("day_block")));
                        }
                        if (TestTrue(TEXT("DUC-09: envelope schema_id is a String"), SchemaIdOut != nullptr && SchemaIdOut->IsString()))
                        {
                            TestEqual(TEXT("DUC-09: envelope schema_id value"), SchemaIdOut->AsString(), FString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
                        }
                        if (TestTrue(TEXT("DUC-09: envelope value is a materialized Object"), ValueOut != nullptr && ValueOut->IsObject()))
                        {
                            const FGV2PreparedUiValue* DayOut = ValueOut->AsObject().FindField(TEXT("day"));
                            const FGV2PreparedUiValue* NumOut = ValueOut->AsObject().FindField(TEXT("value"));
                            if (TestTrue(TEXT("DUC-09: nested day resolved as real Text, not passed through opaque"), DayOut != nullptr && DayOut->IsText()))
                            {
                                TestEqual(TEXT("DUC-09: nested day text resolves through the real text pipeline"), DayOut->AsText().Text.ToString(), TEXT("Monday"));
                            }
                            TestTrue(TEXT("DUC-09: nested value materialized as Number"), NumOut != nullptr && NumOut->IsNumber());
                        }
                    }
                }
            }

            // Negative: an envelope naming an unknown schema_id is rejected, not
            // silently passed through as opaque content.
            const FContentObject BadEnvelopeObj{
                { "field_id", GV2ContentCore::FValue(std::string("day_block")) },
                { "schema_id", GV2ContentCore::FValue(std::string("core:schema.ui_field.nonexistent_probe.v1")) },
                { "value", GV2ContentCore::FValue(FContentObject{}) },
            };
            const GV2ContentCore::FValue::FArray BadEnvelopesArray{ GV2ContentCore::FValue(BadEnvelopeObj) };
            const FContentObject BadOuterObj{
                { "fields", GV2ContentCore::FValue(BadEnvelopesArray) },
            };
            const GV2ContentCore::FValue BadOuterContentValue(BadOuterObj);

            GV2ContentCore::FValue BadOuterMaterialized;
            std::vector<GV2ContentCore::FDiagnostic> BadOuterDiags;
            GV2ContentCore::FValidationDiagnosticContext BadOuterCtx;
            BadOuterCtx.SchemaId = "test:schema.duc09_screen_fields_probe.v1";
            const bool bBadValidated = GV2ContentCore::ValidateUiFieldValue(
                BadOuterContentValue, OuterSchema, BadOuterMaterialized, nullptr, "", BadOuterCtx, BadOuterDiags);
            TestTrue(TEXT("DUC-09: shallow validate still passes (schema_id resolution deferred to materialization)"), bBadValidated);

            FGV2PreparedUiValue BadProjected;
            int32 BadHandleCursor = 0;
            GV2ScreenFieldMaterializer::FMaterializeContext BadMatCtx;
            BadMatCtx.Handles = &NoHandles;
            BadMatCtx.HandleCursor = &BadHandleCursor;
            BadMatCtx.PrepareContext = PrepareContext;
            const bool bBadProjected = bBadValidated && GV2ScreenFieldMaterializer::ProjectMaterializedValue(
                BadMatCtx, OuterSchema, BadOuterMaterialized, BadProjected);
            TestFalse(TEXT("DUC-09: unknown nested schema_id is rejected, not silently passed through"), bBadProjected);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiNestedScreenReconciliationContractTest,
    "GV2.Runtime.UI.NestedScreenReconciliationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiNestedScreenReconciliationContractTest::RunTest(const FString& Parameters)
{
    if (UGV2UiTheme* Theme = LoadConfiguredThemeForTest())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inventory"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_use"), FText::FromString(TEXT("Use Potion")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_skills"), FText::FromString(TEXT("Skills")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_learn"), FText::FromString(TEXT("Learn Fireball")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inv"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_a"), FText::FromString(TEXT("Title A")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_b"), FText::FromString(TEXT("Title B")));
    }

    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    const FGV2ResolvedUiTheme& Theme = PrepareContext->GetTheme();

    // 27b. Consumer: FGV2TabContainerTabsPropertyConsumer turns an already-
    // materialized envelope array into a real TArray<FGV2ScreenFieldValue>
    // and applies it through the child screen's own public
    // PrepareScreenFields / CommitScreenFields -- the same two-phase API a
    // top-level screen uses, not a hand-rolled mutation plan.
    {
        GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
        UGameInstance* GameInstance = WorldContext.GetGameInstance();
        UWorld* TestWorld = WorldContext.GetWorld();

            // Child screen: a real UGV2ScreenWidgetBase with a nested declared
            // composite (DUC-08 shape) exposing exactly the two properties
            // core:schema.ui_field.synthetic_declared_composite.v1 declares.
            UGV2ScreenWidgetBase* ChildScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            ChildScreen->WidgetTree = NewObject<UWidgetTree>(ChildScreen);
            UGV2DeclaredCompositeWidgetBase* DayBlock = ChildScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("DayBlock"));
            ChildScreen->WidgetTree->RootWidget = DayBlock;
            DayBlock->WidgetTree = NewObject<UWidgetTree>(DayBlock);
            UVerticalBox* DayBlockRoot = DayBlock->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            DayBlock->WidgetTree->RootWidget = DayBlockRoot;
            UGV2TextWidgetBase* DayText = DayBlock->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            UGV2ProgressBarWidgetBase* ValueBar = DayBlock->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(UGV2ProgressBarWidgetBase::StaticClass(), TEXT("ValueBar"));
            DayBlockRoot->AddChildToVerticalBox(DayText);
            DayBlockRoot->AddChildToVerticalBox(ValueBar);
            DayBlock->SetHostIdentity(FName(TEXT("day_block")));
            DayBlock->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });
            DayBlock->DeclaredCapabilities.Add({ FName(TEXT("value")), FName(TEXT("ValueBar")), EGV2DeclaredUiCapabilityKind::Number });

            // A second tab lets the first, reused child commit successfully before
            // the second one fails. That is the nested-screen form of an outer
            // transaction rollback: the first child must restore both its widgets and
            // the snapshot observed by its next Prepare.
            UGV2ScreenWidgetBase* FailureScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            FailureScreen->WidgetTree = NewObject<UWidgetTree>(FailureScreen);
            UGV2DeclaredCompositeWidgetBase* FailureDayBlock = FailureScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FailureDayBlock"));
            FailureScreen->WidgetTree->RootWidget = FailureDayBlock;
            FailureDayBlock->WidgetTree = NewObject<UWidgetTree>(FailureDayBlock);
            UVerticalBox* FailureDayBlockRoot = FailureDayBlock->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            FailureDayBlock->WidgetTree->RootWidget = FailureDayBlockRoot;
            UGV2TextWidgetBase* FailureDayText = FailureDayBlock->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            UGV2ProgressBarWidgetBase* FailureValueBar = FailureDayBlock->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(UGV2ProgressBarWidgetBase::StaticClass(), TEXT("ValueBar"));
            FailureDayBlockRoot->AddChildToVerticalBox(FailureDayText);
            FailureDayBlockRoot->AddChildToVerticalBox(FailureValueBar);
            FailureDayBlock->SetHostIdentity(FName(TEXT("day_block")));
            FailureDayBlock->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });
            FailureDayBlock->DeclaredCapabilities.Add({ FName(TEXT("value")), FName(TEXT("ValueBar")), EGV2DeclaredUiCapabilityKind::Number });

            // Seed the tab container's screen-widget map directly (GetScreenWidgetForTab)
            // so Prepare finds this real child screen off-tree, the same way it would
            // reuse an already-reconciled tab on a later revision -- no Screen Registry
            // needed for this off-tree unit test, exactly like UIF-23/24 above.
            UGV2TabContainerWidgetBase* NestedTabContainer = NewObject<UGV2TabContainerWidgetBase>();
            TArray<FGV2TabItemEntry> SeedEntries;
            FGV2TabItemEntry SeedEntry;
            SeedEntry.Key = FName(TEXT("info"));
            SeedEntry.ScreenId = TEXT("core:screen.test_embedded");
            SeedEntries.Add(SeedEntry);
            FGV2TabItemEntry FailureSeedEntry;
            FailureSeedEntry.Key = FName(TEXT("failure"));
            FailureSeedEntry.ScreenId = TEXT("core:screen.test_embedded");
            SeedEntries.Add(FailureSeedEntry);
            TMap<FName, UUserWidget*> SeedWidgets;
            SeedWidgets.Add(FName(TEXT("info")), ChildScreen);
            SeedWidgets.Add(FName(TEXT("failure")), FailureScreen);
            NestedTabContainer->ApplyTabEntries(SeedEntries, SeedWidgets);

            TSharedPtr<IGV2PropertyConsumer> NestedConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
            TestNotNull(TEXT("DUC-09: tab container consumer created"), NestedConsumer.Get());
            NestedConsumer->SetPrepareContext(PrepareContext);

            FGV2UiPropertyCapability NestedTabCap;
            NestedTabCap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;

            const FGV2TextViewModel DayVM =
                MakeResolvedLiteralTextForTest(Theme, TEXT("Tuesday"));
            TMap<FString, FGV2PreparedUiValue> InnerFields;
            InnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(DayVM));
            InnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.7));

            TArray<TPair<FString, FGV2PreparedUiValue>> EnvelopeFields;
            EnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            EnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            EnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(InnerFields)));
            TArray<FGV2PreparedUiValue> FieldsArray;
            FieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(EnvelopeFields)));

            TMap<FString, FGV2PreparedUiValue> TabMap;
            TabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("info")));
            TabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Info"))));
            TabMap.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test_embedded"), TEXT("screen")));
            TabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(FieldsArray)));

            // GBF-05: the first revision must publish BOTH tabs. Commit ends in
            // ApplyTabEntries, which empties the tab-widget map and refills it from
            // the committed revision only -- correct behaviour for a tab that left
            // the document, but it evicts any tab this fixture seeded and did not
            // publish. A tab absent from revision 1 is a *new* tab in revision 2,
            // and a new tab is always instantiated from the Screen Registry, not
            // from the seeded widget: core:screen.test_embedded resolves to WBP_Testscreen,
            // whose screen field host is 'greeting', so a day_block payload is
            // rightly rejected. Publishing both tabs keeps both widgets reused,
            // which is what this scenario is about.
            TMap<FString, FGV2PreparedUiValue> BaselineFailureInner;
            BaselineFailureInner.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Baseline"))));
            BaselineFailureInner.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.5));
            TArray<TPair<FString, FGV2PreparedUiValue>> BaselineFailureEnvelope;
            BaselineFailureEnvelope.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            BaselineFailureEnvelope.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            BaselineFailureEnvelope.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BaselineFailureInner)));
            TArray<FGV2PreparedUiValue> BaselineFailureFields;
            BaselineFailureFields.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BaselineFailureEnvelope)));
            TMap<FString, FGV2PreparedUiValue> BaselineFailureTabMap = TabMap;
            BaselineFailureTabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("failure")));
            BaselineFailureTabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Failure"))));
            BaselineFailureTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(BaselineFailureFields)));

            TArray<FGV2PreparedUiValue> Tabs;
            Tabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(TabMap)));
            Tabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BaselineFailureTabMap)));

            FString NestedPrepErr;
            const bool bNestedPrepared = NestedConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(Tabs)), NestedTabCap, NestedTabContainer, NestedPrepErr);
            TestTrue(*FString::Printf(TEXT("DUC-09: tab with nested fields prepares [Error: %s]"), *NestedPrepErr), bNestedPrepared);

            FString NestedCommitErr;
            const bool bNestedCommitted = bNestedPrepared && NestedConsumer->Commit(NestedTabContainer, NestedCommitErr);
            TestTrue(*FString::Printf(TEXT("DUC-09: tab with nested fields commits [Error: %s]"), *NestedCommitErr), bNestedCommitted);

            TestEqual(TEXT("DUC-09: nested field applied to the real DayText widget"), DayText->GetTextContent().ToString(), TEXT("Tuesday"));
            TestEqual(TEXT("DUC-09: nested field applied to the real ValueBar widget"), ValueBar->GetProgress(), 0.7f);
            TestEqual(TEXT("GBF-05: seeded failure tab widget is the one that received the baseline revision"),
                FailureDayText->GetTextContent().ToString(), TEXT("Baseline"));
            TestEqual(TEXT("GBF-05: both published tabs survive ApplyTabEntries as reused widgets"),
                Cast<UGV2ScreenWidgetBase>(NestedTabContainer->GetScreenWidgetForTab(FName(TEXT("failure")))), FailureScreen);

            const FGV2TextViewModel UpdatedDayVM =
                MakeResolvedLiteralTextForTest(Theme, TEXT("Wednesday"));
            TMap<FString, FGV2PreparedUiValue> UpdatedInnerFields;
            UpdatedInnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(UpdatedDayVM));
            UpdatedInnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.2));
            TArray<TPair<FString, FGV2PreparedUiValue>> UpdatedEnvelopeFields;
            UpdatedEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            UpdatedEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            UpdatedEnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UpdatedInnerFields)));
            TArray<FGV2PreparedUiValue> UpdatedFieldsArray;
            UpdatedFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UpdatedEnvelopeFields)));
            TMap<FString, FGV2PreparedUiValue> UpdatedInfoTabMap = TabMap;
            UpdatedInfoTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(UpdatedFieldsArray)));

            TMap<FString, FGV2PreparedUiValue> FailureInnerFields;
            FailureInnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Never committed"))));
            FailureInnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.1));
            TArray<TPair<FString, FGV2PreparedUiValue>> FailureEnvelopeFields;
            FailureEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            FailureEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            FailureEnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureInnerFields)));
            TArray<FGV2PreparedUiValue> FailureFieldsArray;
            FailureFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureEnvelopeFields)));
            TMap<FString, FGV2PreparedUiValue> FailureTabMap = TabMap;
            FailureTabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("failure")));
            FailureTabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Failure"))));
            FailureTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(FailureFieldsArray)));
            TArray<FGV2PreparedUiValue> NestedFailureTabs;
            NestedFailureTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UpdatedInfoTabMap)));
            NestedFailureTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureTabMap)));

            FString NestedRollbackPrepareError;
            const bool bNestedRollbackPrepared = NestedConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(NestedFailureTabs)), NestedTabCap, NestedTabContainer, NestedRollbackPrepareError);
            TestTrue(*FString::Printf(TEXT("GBF-05: nested reused child prepares candidate before sibling fault [Error: %s]"), *NestedRollbackPrepareError),
                bNestedRollbackPrepared);
            const auto NestedFailureInjector = [](const FString& PropertyPath) -> bool
            {
                return PropertyPath.Contains(TEXT("failure"));
            };
            // The injected fault is the point of this scenario, and the nested screen
            // logs it at Error level on both the faulting commit and the retry below.
            // Declaring it keeps the expected diagnostic from failing the test while
            // still failing if the count changes.
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed on 'day'"), EAutomationExpectedErrorFlags::Contains, 2);

            FString NestedRollbackCommitError;
            const bool bNestedRollbackCommitted = NestedConsumer->CommitWithFailureInjector(
                NestedTabContainer, NestedRollbackCommitError, NestedFailureInjector, TEXT("tabs"));
            TestFalse(*FString::Printf(TEXT("GBF-05: nested sibling fault rejects the tab transaction [Error: %s]"), *NestedRollbackCommitError),
                bNestedRollbackCommitted);
            TestEqual(TEXT("GBF-05: nested reused child physically rolls back its text"), DayText->GetTextContent().ToString(), TEXT("Tuesday"));
            TestEqual(TEXT("GBF-05: nested reused child physically rolls back its number"), ValueBar->GetProgress(), 0.7f);
            const FGV2PreparedUiValue* NestedCommittedDay =
                GetUiHostSemanticState(DayBlock->GetPropertyHostState()).GetLastCommittedProperties().FindField(TEXT("day"));
            TestNotNull(TEXT("GBF-05: nested reused child restores committed day metadata"), NestedCommittedDay);
            if (NestedCommittedDay != nullptr)
            {
                TestEqual(TEXT("GBF-05: nested reused child metadata matches the physical baseline"),
                    NestedCommittedDay->AsText().Text.ToString(), TEXT("Tuesday"));
            }
            TestEqual(TEXT("GBF-05: nested reused child restores prior schema id"),
                GetUiHostSemanticState(DayBlock->GetPropertyHostState()).GetLastCommittedSchemaId(), TEXT("core:schema.ui_field.synthetic_declared_composite.v1"));
            TArray<FGV2PreparedUiValue> NestedRetryTabs;
            NestedRetryTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(TabMap)));
            NestedRetryTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureTabMap)));
            FString NestedNextPrepareError;
            const bool bNestedNextPrepared = NestedConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(NestedRetryTabs)), NestedTabCap, NestedTabContainer, NestedNextPrepareError);
            TestTrue(*FString::Printf(TEXT("GBF-05: next nested Prepare reads restored revision [Error: %s]"), *NestedNextPrepareError),
                bNestedNextPrepared);
            FString NestedNextCommitError;
            TestFalse(TEXT("GBF-05: later nested sibling fault still rejects after retry Prepare"),
                NestedConsumer->CommitWithFailureInjector(NestedTabContainer, NestedNextCommitError, NestedFailureInjector, TEXT("tabs")));
            TestEqual(TEXT("GBF-05: later nested rollback proves the next Prepare used baseline accounting"),
                DayText->GetTextContent().ToString(), TEXT("Tuesday"));

            // GBF-05: a rejected Prepare must leave nothing committable behind.
            // The rejection below happens on the SECOND tab, after the first tab's
            // plan is already built, which is precisely the state that used to
            // survive into Commit and apply the prefix of a transaction nobody
            // accepted. Removing the discard guard in
            // FGV2TabContainerTabsPropertyConsumer::Prepare turns DayText into
            // "Thursday" here and makes this assertion red.
            TMap<FString, FGV2PreparedUiValue> ThursdayInner;
            ThursdayInner.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Thursday"))));
            ThursdayInner.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.9));
            TArray<TPair<FString, FGV2PreparedUiValue>> ThursdayEnvelope;
            ThursdayEnvelope.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            ThursdayEnvelope.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            ThursdayEnvelope.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ThursdayInner)));
            TArray<FGV2PreparedUiValue> ThursdayFields;
            ThursdayFields.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ThursdayEnvelope)));
            TMap<FString, FGV2PreparedUiValue> ThursdayInfoTabMap = TabMap;
            ThursdayInfoTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(ThursdayFields)));

            TMap<FString, FGV2PreparedUiValue> TitlelessTabMap = FailureTabMap;
            TitlelessTabMap.Remove(TEXT("title"));

            TArray<FGV2PreparedUiValue> DiscardTabs;
            DiscardTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(ThursdayInfoTabMap)));
            DiscardTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(TitlelessTabMap)));

            FString DiscardPrepareError;
            const bool bDiscardPrepared = NestedConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DiscardTabs)), NestedTabCap, NestedTabContainer, DiscardPrepareError);
            TestFalse(TEXT("GBF-05: Prepare rejects a candidate whose second tab is malformed"), bDiscardPrepared);
            TestTrue(TEXT("GBF-05: rejection names the malformed tab"),
                DiscardPrepareError.Contains(TEXT("missing_tab_title")));
            FString DiscardCommitError;
            NestedConsumer->Commit(NestedTabContainer, DiscardCommitError);
            TestEqual(TEXT("GBF-05: Commit after a rejected Prepare applies nothing"),
                DayText->GetTextContent().ToString(), TEXT("Tuesday"));
            TestEqual(TEXT("GBF-05: Commit after a rejected Prepare leaves the number alone"),
                ValueBar->GetProgress(), 0.7f);
            TestEqual(TEXT("GBF-05: Commit after a rejected Prepare does not publish an empty tab list"),
                Cast<UGV2ScreenWidgetBase>(NestedTabContainer->GetScreenWidgetForTab(FName(TEXT("info")))), ChildScreen);

            // Negative: an *extra* field_id the child screen has no host for is
            // rejected, not silently ignored (DUC-09's own Done criterion) --
            // "day_block" is included too so the only failure is the unknown
            // extra field, not the (separately-enforced) missing-value-for-host case.
            TArray<TPair<FString, FGV2PreparedUiValue>> UnknownEnvelopeFields;
            UnknownEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("nonexistent_field")));
            UnknownEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("core:schema.ui_field.synthetic_declared_composite.v1")));
            UnknownEnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(InnerFields)));
            TArray<FGV2PreparedUiValue> UnknownFieldsArray;
            UnknownFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(EnvelopeFields)));
            UnknownFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UnknownEnvelopeFields)));

            TMap<FString, FGV2PreparedUiValue> UnknownTabMap = TabMap;
            UnknownTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(UnknownFieldsArray)));
            TArray<FGV2PreparedUiValue> UnknownTabs;
            UnknownTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UnknownTabMap)));

            FString UnknownPrepErr;
            const bool bUnknownPrepared = NestedConsumer->Prepare(
                FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(UnknownTabs)), NestedTabCap, NestedTabContainer, UnknownPrepErr);
            TestFalse(
                *FString::Printf(TEXT("DUC-09: unknown nested field_id is rejected, not silently ignored [Error: %s]"), *UnknownPrepErr),
                bUnknownPrepared);
            TestTrue(
                *FString::Printf(TEXT("DUC-09: rejection names the unknown field [Error: %s]"), *UnknownPrepErr),
                UnknownPrepErr.Contains(TEXT("unknown field")));
        }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiTabContainerLifecycleAndCycleContractTest,
    "GV2.Runtime.UI.TabContainerLifecycleAndCycleContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiTabContainerLifecycleAndCycleContractTest::RunTest(const FString& Parameters)
{
    if (UGV2UiTheme* Theme = LoadConfiguredThemeForTest())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inventory"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_use"), FText::FromString(TEXT("Use Potion")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_skills"), FText::FromString(TEXT("Skills")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_learn"), FText::FromString(TEXT("Learn Fireball")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inv"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_a"), FText::FromString(TEXT("Title A")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_b"), FText::FromString(TEXT("Title B")));
    }

    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    const FGV2ResolvedUiTheme& Theme = PrepareContext->GetTheme();

    // =========================================================================
    // DUC-11: composition-cycle guard for screen_id-based nested screens
    // =========================================================================
    {
        GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
        UGameInstance* GameInstance = WorldContext.GetGameInstance();
        UWorld* TestWorld = WorldContext.GetWorld();

        // Builds a minimal screen: a DeclaredComposite host declaring one
        // NestedScreen-kind property ("tabs") targeting a real child
        // UGV2TabContainerWidgetBase -- the exact shape DUC-09/10 use for a
        // screen that itself hosts a nested tab set.
        auto MakeTabsHostScreen = [TestWorld]() -> UGV2ScreenWidgetBase*
        {
            UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            Screen->WidgetTree = NewObject<UWidgetTree>(Screen);
            UGV2DeclaredCompositeWidgetBase* TabsHost = Screen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TabsHost"));
            Screen->WidgetTree->RootWidget = TabsHost;
            TabsHost->WidgetTree = NewObject<UWidgetTree>(TabsHost);
            UGV2TabContainerWidgetBase* TabsChild = TabsHost->WidgetTree->ConstructWidget<UGV2TabContainerWidgetBase>(
                UGV2TabContainerWidgetBase::StaticClass(), TEXT("TabsChild"));
            TabsHost->WidgetTree->RootWidget = TabsChild;
            TabsHost->SetHostIdentity(FName(TEXT("nested_tabs")));
            TabsHost->DeclaredCapabilities.Add({ FName(TEXT("tabs")), FName(TEXT("TabsChild")), EGV2DeclaredUiCapabilityKind::NestedScreen });
            return Screen;
        };

        // Builds the screen-field payload for the "nested_tabs" host: a single
        // tab (no nested "fields" of its own -- irrelevant here, since the
        // cycle check runs before any recursion into a child screen) whose
        // screen_id is TargetScreenId.
        auto MakeTabsFieldValue = [&Theme](const FString& TargetScreenId) -> FGV2ScreenFieldValue
        {
            TMap<FString, FGV2PreparedUiValue> TabMap;
            TabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("back")));
            TabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(MakeResolvedLiteralTextForTest(Theme, TEXT("Back"))));
            TabMap.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TargetScreenId, TEXT("screen")));
            TArray<FGV2PreparedUiValue> Tabs;
            Tabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(TabMap)));

            TMap<FString, FGV2PreparedUiValue> HostFields;
            HostFields.Add(TEXT("tabs"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(Tabs)));

            auto TabsArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Array);
            TabsArraySpec->KeyedBy = std::string("key");

            GV2ContentCore::FCompiledUiFieldSpec HostSchema;
            HostSchema.Kind = GV2ContentCore::EUiFieldKind::Object;
            HostSchema.Fields.push_back({ "tabs", false, TabsArraySpec });

            FGV2ScreenFieldValue Value;
            Value.FieldId = FName(TEXT("nested_tabs"));
            Value.SchemaId = TEXT("test:schema.duc11_tabs_host_probe.v1");
            Value.PreparedValue = FGV2PreparedUiObject::Create(HostFields);
            Value.CompiledSchema = std::make_shared<const GV2ContentCore::FCompiledUiFieldSpec>(HostSchema);
            return Value;
        };

        // 11a. Direct cycle: the screen currently being prepared (chain = [A])
        // contains a tab whose own screen_id is A itself.
        {
            UGV2ScreenWidgetBase* ScreenA = MakeTabsHostScreen();
            TArray<FGV2ScreenFieldValue> Fields{ MakeTabsFieldValue(TEXT("core:screen.duc11_a")) };
            const TArray<FString> Chain{ TEXT("core:screen.duc11_a") };
            FGV2ScreenMutationPlan Plan;
            FString Error;
            const bool bPrepared = ScreenA->PrepareScreenFields(Fields, Plan, Error, &Chain, PrepareContext);
            TestFalse(*FString::Printf(TEXT("DUC-11: direct self-reference is rejected [Error: %s]"), *Error), bPrepared);
            TestTrue(*FString::Printf(TEXT("DUC-11: direct cycle diagnostic code [Error: %s]"), *Error), Error.Contains(TEXT("core:diagnostic.ui_composition.cycle_detected")));
            TestTrue(*FString::Printf(TEXT("DUC-11: direct cycle renders A -> A [Error: %s]"), *Error), Error.Contains(TEXT("core:screen.duc11_a -> core:screen.duc11_a")));
        }

        // 11b. Indirect cycle: screen B, reached through A's own tab (chain =
        // [A, B]), contains a tab whose screen_id is the ancestor A, not B.
        {
            UGV2ScreenWidgetBase* ScreenB = MakeTabsHostScreen();
            TArray<FGV2ScreenFieldValue> Fields{ MakeTabsFieldValue(TEXT("core:screen.duc11_a")) };
            const TArray<FString> Chain{ TEXT("core:screen.duc11_a"), TEXT("core:screen.duc11_b") };
            FGV2ScreenMutationPlan Plan;
            FString Error;
            const bool bPrepared = ScreenB->PrepareScreenFields(Fields, Plan, Error, &Chain, PrepareContext);
            TestFalse(*FString::Printf(TEXT("DUC-11: indirect cycle through an ancestor is rejected [Error: %s]"), *Error), bPrepared);
            TestTrue(*FString::Printf(TEXT("DUC-11: indirect cycle diagnostic code [Error: %s]"), *Error), Error.Contains(TEXT("core:diagnostic.ui_composition.cycle_detected")));
            TestTrue(*FString::Printf(TEXT("DUC-11: indirect cycle renders full A -> B -> A chain [Error: %s]"), *Error), Error.Contains(TEXT("core:screen.duc11_a -> core:screen.duc11_b -> core:screen.duc11_a")));
        }

        // 11c. Positive control: a screen_id absent from the chain is never
        // rejected by the cycle guard itself -- whatever else may fail about
        // it (e.g. it not being registered), it must not be cycle_detected.
        {
            UGV2ScreenWidgetBase* ScreenC = MakeTabsHostScreen();
            TArray<FGV2ScreenFieldValue> Fields{ MakeTabsFieldValue(TEXT("core:screen.duc11_unrelated")) };
            const TArray<FString> Chain{ TEXT("core:screen.duc11_a"), TEXT("core:screen.duc11_b") };
            FGV2ScreenMutationPlan Plan;
            FString Error;
            const bool bPreparedC = ScreenC->PrepareScreenFields(Fields, Plan, Error, &Chain, PrepareContext);
            (void)bPreparedC;
            TestFalse(*FString::Printf(TEXT("DUC-11: unrelated screen_id is not rejected as a cycle [Error: %s]"), *Error), Error.Contains(TEXT("cycle_detected")));
        }
    }

    // =========================================================================
    // UIF-25: UI-Local Active Tab State & Widget Lifecycle
    // =========================================================================
    {
        UGV2TabContainerWidgetBase* TabWidget = NewObject<UGV2TabContainerWidgetBase>();
        TestNotNull(TEXT("Tab widget created"), TabWidget);

        TabWidget->ApplyDefaultTabKey(FName("skills"));

        TArray<FGV2TabItemEntry> Entries;
        FGV2TabItemEntry E1;
        E1.Key = FName("inventory");
        E1.ScreenId = TEXT("core:screen.tab_inventory");
        Entries.Add(E1);

        FGV2TabItemEntry E2;
        E2.Key = FName("skills");
        E2.ScreenId = TEXT("core:screen.tab_skills");
        Entries.Add(E2);

        TMap<FName, UUserWidget*> Widgets;
        TabWidget->ApplyTabEntries(Entries, Widgets);

        TestEqual(TEXT("Initial active tab is DefaultTabKey (skills)"), TabWidget->GetActiveTabKey(), FName("skills"));
        TestEqual(TEXT("Active tab index is 1"), TabWidget->GetActiveTabIndex(), 1);

        // Switch tab locally
        TestTrue(TEXT("SelectTabByKey to inventory succeeds"), TabWidget->SelectTabByKey(FName("inventory")));
        TestEqual(TEXT("Active tab changed to inventory"), TabWidget->GetActiveTabKey(), FName("inventory"));
        TestEqual(TEXT("Active tab index changed to 0"), TabWidget->GetActiveTabIndex(), 0);

        // Reconcile new revision with same tabs -> preserves active tab (inventory), not resetting to default
        TabWidget->ApplyTabEntries(Entries, Widgets);
        TestEqual(TEXT("Active tab preserved across revision (inventory)"), TabWidget->GetActiveTabKey(), FName("inventory"));

        // Reconcile revision where active tab (inventory) was removed -> falls back to default (skills)
        TArray<FGV2TabItemEntry> SkillsOnlyEntries;
        SkillsOnlyEntries.Add(Entries[1]);
        TabWidget->ApplyTabEntries(SkillsOnlyEntries, Widgets);
        TestEqual(TEXT("Fallback to default tab when active tab removed"), TabWidget->GetActiveTabKey(), FName("skills"));
    }

    // =========================================================================
    // UIF-26: Semantic Input Filtering (Only Active Tab is Interactive)
    // =========================================================================
    {
        struct FSampleOverrideScope
        {
            FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
            ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
        } Scope;

        FGV2SessionCoordinator Coordinator;
        Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool { return true; });
        const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
        const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CorePackageRoot);
        GV2ContentCore::FRepositoryReadHandle ReadHandle;
        if (RepoBuild.IsSuccess())
        {
            ReadHandle = RepoBuild.GetCandidate().GetReadHandle();
        }
        TestTrue(TEXT("Coordinator StartSession succeeds"), Coordinator.StartSession(ReadHandle, 1));

        bool bDocumentHandled = false;
        Coordinator.SetDocumentSink([&bDocumentHandled](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool
        {
            bDocumentHandled = true;
            return true;
        });

        // Publish bindings
        TArray<FGV2UiBindingDefinition> Definitions;
        {
            FGV2UiBindingDefinition Def1;
            Def1.CommandId = TEXT("core:command.test.step");
            Def1.NodeKeyPath = { TEXT("location_content"), TEXT("main"), TEXT("tabs"), TEXT("inventory"), TEXT("inv_buttons"), TEXT("use_potion") };
            Definitions.Add(Def1);

            FGV2UiBindingDefinition Def2;
            Def2.CommandId = TEXT("core:command.test.step");
            Def2.NodeKeyPath = { TEXT("location_content"), TEXT("main"), TEXT("tabs"), TEXT("skills"), TEXT("skill_buttons"), TEXT("learn_fireball") };
            Definitions.Add(Def2);
        }

        TArray<FGV2UiBindingHandle> Handles;
        TestTrue(TEXT("PublishUiBindings succeeds"), Coordinator.PublishUiBindings(TEXT("ui@1:1"), 2, Definitions, Handles));
        if (TestEqual(TEXT("2 handles published"), Handles.Num(), 2))
        {
            const FGV2UiBindingHandle InventoryBtnHandle = Handles[0];
            const FGV2UiBindingHandle SkillsBtnHandle = Handles[1];

            // Active tab initially set to inventory
            Coordinator.SetActiveTab(TEXT("location_content/main/tabs"), TEXT("inventory"));
            TestEqual(TEXT("Coordinator active tab is inventory"), Coordinator.GetActiveTab(TEXT("location_content/main/tabs")), TEXT("inventory"));

            // 1. Submit interaction on ACTIVE tab (inventory) -> Accepted
            const EGV2SubmitUiInteractionResult Result1 = Coordinator.SubmitUiInteraction(InventoryBtnHandle, {});
            TestEqual(TEXT("Active tab handle accepted"), Result1, EGV2SubmitUiInteractionResult::Accepted);

            // 2. Submit interaction on INACTIVE tab (skills) -> StaleBindingHandle
            const EGV2SubmitUiInteractionResult Result2 = Coordinator.SubmitUiInteraction(SkillsBtnHandle, {});
            TestEqual(TEXT("Inactive tab handle rejected as stale"), Result2, EGV2SubmitUiInteractionResult::StaleBindingHandle);

            // 3. Switch active tab locally to skills (no commands / no revision mutation)
            Coordinator.SetActiveTab(TEXT("location_content/main/tabs"), TEXT("skills"));
            TestEqual(TEXT("Coordinator active tab is now skills"), Coordinator.GetActiveTab(TEXT("location_content/main/tabs")), TEXT("skills"));

            // 4. Submit interaction on new ACTIVE tab (skills) -> Accepted
            const EGV2SubmitUiInteractionResult Result3 = Coordinator.SubmitUiInteraction(SkillsBtnHandle, {});
            TestEqual(TEXT("Skills handle accepted after tab switch"), Result3, EGV2SubmitUiInteractionResult::Accepted);

            // 5. Submit interaction on newly INACTIVE tab (inventory) -> StaleBindingHandle
            const EGV2SubmitUiInteractionResult Result4 = Coordinator.SubmitUiInteraction(InventoryBtnHandle, {});
            TestEqual(TEXT("Inventory handle rejected after tab switch"), Result4, EGV2SubmitUiInteractionResult::StaleBindingHandle);

            // 6. Negative test: When container path has NO active tab configured -> ALL tab handles are rejected as StaleBindingHandle
            Coordinator.SetActiveTab(TEXT("location_content/main/tabs"), TEXT(""));
            const EGV2SubmitUiInteractionResult ResultNeg1 = Coordinator.SubmitUiInteraction(InventoryBtnHandle, {});
            TestEqual(TEXT("Unset active tab rejects inventory handle"), ResultNeg1, EGV2SubmitUiInteractionResult::StaleBindingHandle);
            const EGV2SubmitUiInteractionResult ResultNeg2 = Coordinator.SubmitUiInteraction(SkillsBtnHandle, {});
            TestEqual(TEXT("Unset active tab rejects skills handle"), ResultNeg2, EGV2SubmitUiInteractionResult::StaleBindingHandle);
        }

        // 7. Full widget + FGV2UiInteractionEmitter + UGV2RuntimeSubsystem integration test
        {
            GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
            UGameInstance* GameInstance = WorldContext.GetGameInstance();
            UWorld* TestWorld = WorldContext.GetWorld();

            UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
            TestNotNull(TEXT("Runtime subsystem exists for tab container test"), Runtime);

            if (Runtime != nullptr && TestWorld != nullptr)
            {
                UGV2TabContainerWidgetBase* IntegrationTabWidget = CreateWidget<UGV2TabContainerWidgetBase>(TestWorld, UGV2TabContainerWidgetBase::StaticClass());
                TestNotNull(TEXT("Integration tab widget created"), IntegrationTabWidget);

                if (IntegrationTabWidget != nullptr)
                {
                    IntegrationTabWidget->ConfiguredScreenFieldId = FName(TEXT("tabs"));
                    IntegrationTabWidget->SetContainerPath(TEXT("location_content/main/tabs"));
                    IntegrationTabWidget->ApplyDefaultTabKey(FName("inventory"));

                    TArray<FGV2TabItemEntry> Entries;
                    FGV2TabItemEntry E1;
                    E1.Key = FName("inventory");
                    E1.ScreenId = TEXT("core:screen.tab_inventory");
                    Entries.Add(E1);

                    FGV2TabItemEntry E2;
                    E2.Key = FName("skills");
                    E2.ScreenId = TEXT("core:screen.tab_skills");
                    Entries.Add(E2);

                    TMap<FName, UUserWidget*> Widgets;
                    IntegrationTabWidget->ApplyTabEntries(Entries, Widgets);

                    TestEqual(TEXT("Runtime subsystem synced initial active tab (inventory)"), Runtime->GetActiveTab(TEXT("location_content/main/tabs")), TEXT("inventory"));

                    // Switch tab via widget
                    IntegrationTabWidget->SelectTabByKey(FName("skills"));
                    TestEqual(TEXT("Runtime subsystem synced switched active tab (skills)"), Runtime->GetActiveTab(TEXT("location_content/main/tabs")), TEXT("skills"));

                    // Switch back to inventory
                    IntegrationTabWidget->SelectTabByKey(FName("inventory"));
                    TestEqual(TEXT("Runtime subsystem synced switched active tab (inventory)"), Runtime->GetActiveTab(TEXT("location_content/main/tabs")), TEXT("inventory"));
                }
            }
        }
    }

    return true;
}

namespace
{
// PEP-07: extracts the {...} body of UGV2RuntimeSubsystem::FunctionName's own definition
// (not merely a call site) via brace matching -- good enough for the four small,
// non-overloaded functions GV2.Runtime.Presentation.HoverEffectNeverCrossesLua targets.
// Returns empty on no match, which the caller must treat as a scan failure, not a vacuous
// pass over nothing.
FString ExtractQualifiedFunctionBody(const FString& Source, const FString& ClassName, const FString& FunctionName)
{
    const FString Qualifier = ClassName + TEXT("::") + FunctionName;
    const int32 QualifierIndex = Source.Find(Qualifier, ESearchCase::CaseSensitive);
    if (QualifierIndex == INDEX_NONE)
    {
        return FString();
    }
    const int32 OpenBrace = Source.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, QualifierIndex);
    if (OpenBrace == INDEX_NONE)
    {
        return FString();
    }
    int32 Depth = 0;
    for (int32 Index = OpenBrace; Index < Source.Len(); ++Index)
    {
        if (Source[Index] == TEXT('{'))
        {
            ++Depth;
        }
        else if (Source[Index] == TEXT('}'))
        {
            --Depth;
            if (Depth == 0)
            {
                return Source.Mid(OpenBrace, Index - OpenBrace + 1);
            }
        }
    }
    return FString();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverEffectNeverCrossesLuaContract,
    "GV2.Runtime.Presentation.HoverEffectNeverCrossesLua",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PEP-07 (SemanticInput.md:110, WidgetRegistry.md:278): hover/unhover must never cross the
// Lua boundary. This walks the actual call sites in the four functions the hover path now
// runs through, not just a comment claiming they don't -- a text scan, symmetric to DCA-15's
// own idiom (GV2LayoutInvariantSourceTests.cpp), over a small, explicit function set rather
// than a general call-graph walker.
bool FGV2HoverEffectNeverCrossesLuaContract::RunTest(const FString& Parameters)
{
    const FString SubsystemPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp"));
    FString Source;
    TestTrue(TEXT("GV2RuntimeSubsystem.cpp is readable"), FFileHelper::LoadFileToString(Source, *SubsystemPath));
    if (Source.IsEmpty())
    {
        return false;
    }

    const TArray<FString> HoverFunctionNames = {
        TEXT("OpenHoverOverlay"),
        TEXT("CloseHoverOverlay"),
        TEXT("PublishHoverEffect"),
        TEXT("DrainPresentationEffects"),
    };

    // The exact set of symbols that name a Lua-crossing entry point reachable from this
    // module -- anything reaching one of these would let hover become a gameplay event.
    const TArray<FString> LuaCrossingSymbols = {
        TEXT("DispatchSemanticInput"),
        TEXT("DispatchCommand"),
        TEXT("SubmitUiInteraction"),
        TEXT("SubmitPresentationInteraction"),
        TEXT("lua_"),
        TEXT("CallLua"),
    };

    int32 FunctionsScanned = 0;
    for (const FString& FunctionName : HoverFunctionNames)
    {
        const FString Body = ExtractQualifiedFunctionBody(Source, TEXT("UGV2RuntimeSubsystem"), FunctionName);
        TestFalse(
            *FString::Printf(TEXT("UGV2RuntimeSubsystem::%s's definition is found and extractable"), *FunctionName),
            Body.IsEmpty());
        if (Body.IsEmpty())
        {
            continue;
        }
        ++FunctionsScanned;
        for (const FString& Symbol : LuaCrossingSymbols)
        {
            TestFalse(
                *FString::Printf(TEXT("%s does not reach the Lua-crossing symbol '%s'"), *FunctionName, *Symbol),
                Body.Contains(Symbol));
        }
    }
    TestEqual(TEXT("All four hover-path functions were actually scanned"), FunctionsScanned, HoverFunctionNames.Num());

    // "One counter, one queue, one drain point" -- exactly one production call site may call
    // TakePendingEffects; a second would let two independent drain loops race the same queue.
    TArray<FString> SourceFiles;
    const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GV2/Private"));
    IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*.cpp"), true, false, false);

    int32 DrainCallSiteCount = 0;
    TArray<FString> DrainCallSiteFiles;
    for (const FString& FilePath : SourceFiles)
    {
        if (FilePath.Contains(TEXT("/Tests/")))
        {
            continue;
        }
        FString FileSource;
        if (!FFileHelper::LoadFileToString(FileSource, *FilePath))
        {
            continue;
        }
        int32 Count = 0;
        int32 SearchIndex = 0;
        for (;;)
        {
            const int32 Found = FileSource.Find(TEXT(".TakePendingEffects("), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex);
            if (Found == INDEX_NONE)
            {
                break;
            }
            ++Count;
            SearchIndex = Found + 1;
        }
        if (Count > 0)
        {
            DrainCallSiteCount += Count;
            DrainCallSiteFiles.Add(FString::Printf(TEXT("%s (%d)"), *FPaths::GetCleanFilename(FilePath), Count));
        }
    }
    TestEqual(
        *FString::Printf(TEXT("Exactly one production call site drains TakePendingEffects: %s"), *FString::Join(DrainCallSiteFiles, TEXT(", "))),
        DrainCallSiteCount,
        1);

    return true;
}

#endif
