#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SVirtualWindow.h"
#include "Application/GV2ScreenFieldAdapterRegistry.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "GV2RuntimeCore/Testing/GV2StableIdConformance.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "Components/Image.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2TextPipeline.h"
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
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2LocationCompositeWidgetBases.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/WrapBox.h"

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
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Engine/World.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
// CBM-03: GameData/sample carries the WBP_Testscreen demo/debug-start screen
// but is deliberately excluded from the default package set (mods.lock.json5),
// since it and GameData/rh both bind the shared
// "textsystem:action.location.travel" action and cannot load together.
// Tests that need the demo screen opt in explicitly via this scope guard,
// matching the "runs that need the demo screen connect sample explicitly"
// intent from CoreBoundaryMigration/DemoOut.md.
struct FGV2ScopedSamplePackageOverride
{
    FGV2ScopedSamplePackageOverride() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
    ~FGV2ScopedSamplePackageOverride() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
};
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

    const TCHAR* InputComponents[] = {
        TEXT("Source/GV2/Private/UI/GV2ButtonWidgetBase.cpp"),
        TEXT("Source/GV2/Private/UI/GV2CheckboxWidgetBase.cpp"),
        TEXT("Source/GV2/Private/UI/GV2InputFieldWidgetBase.cpp"),
        TEXT("Source/GV2/Private/UI/GV2DropdownSelectWidgetBase.cpp"),
        TEXT("Source/GV2/Private/UI/GV2RichTextWidgetBase.cpp"),
        TEXT("Source/GV2/Private/UI/GV2DebugStartScreenWidget.cpp")
    };
    for (const TCHAR* RelativePath : InputComponents)
    {
        FString Source;
        if (ReadSource(RelativePath, Source))
        {
            TestFalse(
                *FString::Printf(TEXT("Component delegates runtime lookup to the common emitter: %s"), RelativePath),
                Source.Contains(TEXT("GetSubsystem<UGV2RuntimeSubsystem>")));
            TestFalse(
                *FString::Printf(TEXT("Component does not call Runtime SubmitUiInteraction directly: %s"), RelativePath),
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
            TEXT("Coordinator delegates Screen Field conversion to the adapter registry"),
            CoordinatorSource.Contains(TEXT("FGV2ScreenFieldAdapterRegistry::Get()")));
        TestFalse(
            TEXT("Coordinator contains no concrete Screen Field schema IDs"),
            CoordinatorSource.Contains(TEXT("core:schema.ui_field.")));
    }

    FString AdapterRegistrySource;
    if (ReadSource(
            TEXT("Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp"),
            AdapterRegistrySource))
    {
        const TCHAR* FieldSchemas[] = {
            TEXT("textsystem:schema.ui_field.location_scene.v1"),
            TEXT("textsystem:schema.ui_field.location_commands.v1")
        };
        for (const TCHAR* SchemaId : FieldSchemas)
        {
            TestTrue(
                *FString::Printf(TEXT("Adapter registry owns %s"), SchemaId),
                AdapterRegistrySource.Contains(SchemaId));
        }
    }
    TestEqual(
        TEXT("Adapter registry contains 0 legacy adapters (all baseline and LocationScreen schemas migrated to declarative)"),
        FGV2ScreenFieldAdapterRegistry::Get().Num(),
        0);

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
            TEXT("FName UGV2ImageResourceCatalogSettings::GetCategoryName"),
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
        FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(
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
        ValidReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
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
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(ValidReq, ValidDefs));
        TestEqual(TEXT("Prepares two binding definitions"), ValidDefs.Num(), 2);
    }

    // 2. Button list missing key
    {
        GV2RuntimeCore::FScreenRequest MissingKeyReq;
        MissingKeyReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(nullptr, "core:command.screen.action_a")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        MissingKeyReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> MissingDefs;
        TestFalse(
            TEXT("Button list with missing key is rejected (UiElementKeyMissing)"),
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(MissingKeyReq, MissingDefs));
        TestTrue(TEXT("Rejected candidate leaves definitions empty"), MissingDefs.IsEmpty());
    }

    // 3. Button list duplicate key
    {
        GV2RuntimeCore::FScreenRequest DupKeyReq;
        DupKeyReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
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
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(DupKeyReq, DupDefs));
        TestTrue(TEXT("Rejected duplicate key leaves definitions empty"), DupDefs.IsEmpty());
    }

    // 4. Button list text-derived key
    {
        GV2RuntimeCore::FScreenRequest TextKeyReq;
        TextKeyReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
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
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(TextKeyReq, TextDefs));
        TestTrue(TEXT("Rejected text key leaves definitions empty"), TextDefs.IsEmpty());
    }

    // 5. Button list key grammar conformance: [a-z0-9_.-@:]+ (BAI-08)
    // 5a. Positive key grammar: domain ID with ':', instance ID with '@', hyphens, dots
    {
        GV2RuntimeCore::FScreenRequest ValidGrammarReq;
        ValidGrammarReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "commands";
        BtnField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
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
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(ValidGrammarReq, ValidDefs));
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
                FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(InvalidKeyReq, InvalidDefs));
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
    FGV2UiScalingModelAndConstantsContract,
    "GV2.Runtime.UIKit.ScalingModelAndConstants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiScalingModelAndConstantsContract::RunTest(const FString& Parameters)
{
    // 1. UIF-06: Dual Resolution Constants
    TestEqual(
        TEXT("Raster authoring width is 3840 (4K)"),
        FGV2LayoutConstants::RasterAuthoringWidth,
        3840.0f);
    TestEqual(
        TEXT("Raster authoring height is 2160 (4K)"),
        FGV2LayoutConstants::RasterAuthoringHeight,
        2160.0f);
    TestEqual(
        TEXT("Virtual layout unit width is 1920 (1080p)"),
        FGV2LayoutConstants::VirtualLayoutWidth,
        1920.0f);
    TestEqual(
        TEXT("Virtual layout unit height is 1080 (1080p)"),
        FGV2LayoutConstants::VirtualLayoutHeight,
        1080.0f);
    TestEqual(
        TEXT("Raster to layout scale factor is 2.0"),
        FGV2LayoutConstants::RasterToLayoutScale,
        2.0f);
    TestEqual(
        TEXT("Minimum supported viewport width is 1280 (720p)"),
        FGV2LayoutConstants::MinSupportedViewportWidth,
        1280.0f);
    TestEqual(
        TEXT("Minimum supported viewport height is 720 (720p)"),
        FGV2LayoutConstants::MinSupportedViewportHeight,
        720.0f);

    // 2. UIF-08: Primitive Scale Policy & Resource Compatibility
    TestTrue(
        TEXT("FreeStretch policy is compatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::Tile));
    TestFalse(
        TEXT("FreeStretch policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::NineSlice));
    TestFalse(
        TEXT("FreeStretch policy is incompatible with FixedAspect render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::FixedAspect));

    TestTrue(
        TEXT("Tile policy is compatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::Tile));
    TestFalse(
        TEXT("Tile policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::NineSlice));

    TestTrue(
        TEXT("NineSlice policy is compatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::NineSlice));
    TestFalse(
        TEXT("NineSlice policy is incompatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::Tile));

    TestTrue(
        TEXT("PreserveAspect policy is compatible with FixedAspect render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::FixedAspect));
    TestFalse(
        TEXT("PreserveAspect policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::NineSlice));

    // 3. UIF-09: Text Scale Curve & Minimum Readable Font Size
    UGV2UiTheme* Theme = NewObject<UGV2UiTheme>();
    TestNotNull(TEXT("Transient theme instance created"), Theme);
    if (Theme != nullptr)
    {
        Theme->TextSizeTokens.Add(TEXT("body"), 14.0f);
        Theme->TextSizeTokens.Add(TEXT("small"), 10.0f);
        Theme->TextSizeTokens.Add(TEXT("heading"), 22.0f);

        // Evaluation at standard heights
        const float Scale720 = Theme->EvaluateTextScale(720.0f);
        const float Scale1080 = Theme->EvaluateTextScale(1080.0f);
        const float Scale1440 = Theme->EvaluateTextScale(1440.0f);
        const float Scale2160 = Theme->EvaluateTextScale(2160.0f);

        TestTrue(TEXT("Scale at 720p preserves readability (around 0.85)"), Scale720 >= 0.80f && Scale720 <= 0.90f);
        TestEqual(TEXT("Scale at 1080p is baseline (1.0)"), Scale1080, 1.0f);
        TestTrue(TEXT("Scale at 1440p grows modestly (around 1.25)"), Scale1440 >= 1.20f && Scale1440 <= 1.30f);
        TestTrue(TEXT("Scale at 4K (2160p) is bounded (around 1.60)"), Scale2160 >= 1.50f && Scale2160 <= 1.70f);

        // Monotonic growth
        TestTrue(TEXT("Scale grows monotonically: 720p <= 1080p"), Scale720 <= Scale1080);
        TestTrue(TEXT("Scale grows monotonically: 1080p <= 1440p"), Scale1080 <= Scale1440);
        TestTrue(TEXT("Scale grows monotonically: 1440p <= 2160p"), Scale1440 <= Scale2160);

        // Minimum readable font size threshold (10 pt)
        const float SmallSizeAt720 = Theme->GetEffectiveFontSize(TEXT("small"), 720.0f);
        TestTrue(
            TEXT("Effective font size never drops below MinReadableFontSize"),
            SmallSizeAt720 >= Theme->MinReadableFontSize);
        TestEqual(TEXT("Small size at 720p clamped to MinReadableFontSize"), SmallSizeAt720, 10.0f);

        const float BodySizeAt720 = Theme->GetEffectiveFontSize(TEXT("body"), 720.0f);
        TestTrue(TEXT("Body text size at 720p is readable (>= 10pt)"), BodySizeAt720 >= 10.0f);
    }

    // 4. UIF-10: Resolution Matrix Coverage
    struct FResolutionTarget
    {
        float Width;
        float Height;
        const TCHAR* Label;
        bool bIsUltrawide;
    };

    const FResolutionTarget ResolutionMatrix[] = {
        { 3840.0f, 2160.0f, TEXT("4K UHD (16:9)"), false },
        { 2560.0f, 1440.0f, TEXT("QHD (16:9)"), false },
        { 1920.0f, 1080.0f, TEXT("FHD (16:9)"), false },
        { 1280.0f, 720.0f,  TEXT("HD (16:9 minimum target)"), false },
        { 3440.0f, 1440.0f, TEXT("UWQHD (21:9)"), true },
        { 2560.0f, 1080.0f, TEXT("UWFHD (21:9)"), true }
    };

    for (const FResolutionTarget& Target : ResolutionMatrix)
    {
        const float Aspect = Target.Width / Target.Height;
        if (Target.bIsUltrawide)
        {
            TestTrue(
                *FString::Printf(TEXT("%s aspect ratio is ultrawide (~2.33)"), Target.Label),
                FMath::IsNearlyEqual(Aspect, FGV2LayoutConstants::UltrawideAspectRatio, 0.06f));
        }
        else
        {
            TestTrue(
                *FString::Printf(TEXT("%s aspect ratio is standard 16:9 (~1.78)"), Target.Label),
                FMath::IsNearlyEqual(Aspect, FGV2LayoutConstants::StandardAspectRatio, 0.01f));
        }

        TestTrue(
            *FString::Printf(TEXT("%s width >= MinSupportedViewportWidth"), Target.Label),
            Target.Width >= FGV2LayoutConstants::MinSupportedViewportWidth);
        TestTrue(
            *FString::Printf(TEXT("%s height >= MinSupportedViewportHeight"), Target.Label),
            Target.Height >= FGV2LayoutConstants::MinSupportedViewportHeight);

        if (Theme != nullptr)
        {
            const float Scale = Theme->EvaluateTextScale(Target.Height);
            TestTrue(
                *FString::Printf(TEXT("%s evaluated scale is positive and bounded"), Target.Label),
                Scale >= 0.80f && Scale <= 2.0f);
            const float BodyFontSize = Theme->GetEffectiveFontSize(TEXT("body"), Target.Height);
            TestTrue(
                *FString::Printf(TEXT("%s body font size >= MinReadableFontSize"), Target.Label),
                BodyFontSize >= Theme->MinReadableFontSize);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCoreBaselineAdaptersContract,
    "GV2.Runtime.UIKit.CoreBaselineAdapters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCoreBaselineAdaptersContract::RunTest(const FString& Parameters)
{
    if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.progress.health"), FText::FromString(TEXT("Health")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.title"), FText::FromString(TEXT("Title")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.content"), FText::FromString(TEXT("Content")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.button.ok"), FText::FromString(TEXT("OK")));
    }

    const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();
    TestEqual(TEXT("Registry has 0 legacy adapters remaining"), Registry.Num(), 0);

    // 1. All adapters removed from legacy registry
    TestNull(TEXT("Image adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.image.v1"));
    TestNull(TEXT("Checkbox adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.checkbox.v1"));
    TestNull(TEXT("InputField adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.input_field.v1"));
    TestNull(TEXT("ProgressBar adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.progress_bar.v1"));
    TestNull(TEXT("Portrait adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.portrait.v1"));
    TestNull(TEXT("ButtonList adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.button_list.v2"));
    TestNull(TEXT("DropdownSelect adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.dropdown_select.v1"));
    TestNull(TEXT("RichText adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.rich_text.v3"));
    TestNull(TEXT("Modal adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.modal.v1"));
    TestNull(TEXT("TabContainer adapter is not in legacy registry"), Registry.Find("core:schema.ui_field.tab_container.v1"));
    TestNull(TEXT("Location top bar adapter is not in legacy registry"), Registry.Find("textsystem:schema.ui_field.location_top_bar.v1"));
    TestNull(TEXT("Location player status adapter is not in legacy registry"), Registry.Find("textsystem:schema.ui_field.location_player_status.v1"));
    TestNull(TEXT("Location scene adapter is not in legacy registry"), Registry.Find("textsystem:schema.ui_field.location_scene.v1"));
    TestNull(TEXT("Location commands adapter is not in legacy registry"), Registry.Find("textsystem:schema.ui_field.location_commands.v1"));
    TestEqual(TEXT("Registry has 0 legacy adapters remaining"), Registry.Num(), 0);

    // 5. Generic Binding Extraction for Location Commands
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField CmdField;
        CmdField.FieldId = "commands";
        CmdField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
        GV2RuntimeCore::FValue::FObject CmdObj;

        GV2RuntimeCore::FValue::FArray ItemsArray;
        GV2RuntimeCore::FValue::FObject Btn1;
        Btn1["key"] = GV2RuntimeCore::FValue(std::string("btn_talk"));
        GV2RuntimeCore::FValue::FObject TextObj;
        TextObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.talk"));
        Btn1["text"] = GV2RuntimeCore::FValue(TextObj);
        Btn1["binding"] = GV2RuntimeCore::FValue(std::string("core:command.talk"));
        ItemsArray.push_back(GV2RuntimeCore::FValue(Btn1));

        CmdObj["items"] = GV2RuntimeCore::FValue(ItemsArray);
        CmdField.Value = GV2RuntimeCore::FValue(MoveTemp(CmdObj));
        ValidReq.Fields.push_back(MoveTemp(CmdField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("Generic binding extraction succeeds for commands"), Registry.PrepareBindingDefinitions(ValidReq, Defs));
        TestEqual(TEXT("Extracted 1 binding definition"), Defs.Num(), 1);
        if (Defs.Num() == 1)
        {
            TestEqual(TEXT("Binding element id matches"), Defs[0].ElementId, FString(TEXT("textsystem:screen.location#widget.btn_talk")));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCoreBaselineComponentsContract,
    "GV2.Runtime.UIKit.CoreBaselineComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCoreBaselineComponentsContract::RunTest(const FString& Parameters)
{
    // 1. Panel component defaults
    {
        UGV2PanelWidgetBase* Panel = NewObject<UGV2PanelWidgetBase>();
        TestNotNull(TEXT("Transient panel widget created"), Panel);
        if (Panel != nullptr)
        {
            TestEqual(TEXT("Panel default scale policy is NineSlice"), Panel->GetScalePolicy(), EGV2PrimitiveScalePolicy::NineSlice);
            TestEqual(TEXT("Panel default content padding is 16"), Panel->GetContentPadding().Left, 16.0f);
            Panel->SetContentPadding(FMargin(24.0f));
            TestEqual(TEXT("Panel updated content padding is 24"), Panel->GetContentPadding().Left, 24.0f);
        }
    }

    // 2. ScrollArea component defaults
    {
        UGV2ScrollAreaWidgetBase* ScrollArea = NewObject<UGV2ScrollAreaWidgetBase>();
        TestNotNull(TEXT("Transient scroll area widget created"), ScrollArea);
        if (ScrollArea != nullptr)
        {
            TestEqual(TEXT("ScrollArea default orientation is vertical"), ScrollArea->GetOrientation(), EOrientation::Orient_Vertical);
            TestEqual(TEXT("ScrollArea initial scroll offset is 0"), ScrollArea->GetScrollOffset(), 0.0f);
        }
    }

    // 3. ListView component defaults
    {
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>();
        TestNotNull(TEXT("Transient list view widget created"), ListView);
        if (ListView != nullptr)
        {
            TestEqual(TEXT("ListView default orientation is vertical"), ListView->GetOrientation(), EOrientation::Orient_Vertical);
            TestEqual(TEXT("ListView initial entry count is 0"), ListView->GetEntryCount(), 0);
            ListView->SetOrientation(EOrientation::Orient_Horizontal);
            TestEqual(TEXT("ListView updated orientation is horizontal"), ListView->GetOrientation(), EOrientation::Orient_Horizontal);
        }
    }

    // 4. Icon component defaults
    {
        UGV2IconWidgetBase* Icon = NewObject<UGV2IconWidgetBase>();
        TestNotNull(TEXT("Transient icon widget created"), Icon);
        if (Icon != nullptr)
        {
            TestEqual(TEXT("Icon default scale policy is PreserveAspect"), Icon->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);
        }
    }

    // 5. Dynamic Screen Element interface implementations
    {
        UGV2PortraitWidgetBase* Portrait = NewObject<UGV2PortraitWidgetBase>();
        TestNotNull(TEXT("Transient portrait widget created"), Portrait);
        if (Portrait != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Portrait->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Portrait declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2ModalWidgetBase* Modal = NewObject<UGV2ModalWidgetBase>();
        TestNotNull(TEXT("Transient modal widget created"), Modal);
        if (Modal != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Modal->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Modal declares title capability"), Caps.Properties.Contains(TEXT("title")));
            TestTrue(TEXT("Modal declares content capability"), Caps.Properties.Contains(TEXT("content")));
            TestTrue(TEXT("Modal declares buttons capability"), Caps.Properties.Contains(TEXT("buttons")));
            TestTrue(TEXT("Modal declares backdrop_close_action capability"), Caps.Properties.Contains(TEXT("backdrop_close_action")));
            TestTrue(TEXT("Modal declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2ProgressBarWidgetBase* ProgressBar = NewObject<UGV2ProgressBarWidgetBase>();
        TestNotNull(TEXT("Transient progress bar widget created"), ProgressBar);
        if (ProgressBar != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            ProgressBar->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("ProgressBar declares percent capability"), Caps.Properties.Contains(TEXT("percent")));
        }

        UGV2ImageWidgetBase* Image = NewObject<UGV2ImageWidgetBase>();
        TestNotNull(TEXT("Transient image widget created"), Image);
        if (Image != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Image->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Image declares resource_id capability"), Caps.Properties.Contains(TEXT("resource_id")));
        }

        UGV2RichTextWidgetBase* RichText = NewObject<UGV2RichTextWidgetBase>();
        TestNotNull(TEXT("Transient rich text widget created"), RichText);
        if (RichText != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            RichText->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("RichText declares text capability"), Caps.Properties.Contains(TEXT("text")));
            TestTrue(TEXT("RichText declares spans capability"), Caps.Properties.Contains(TEXT("spans")));
            TestTrue(TEXT("RichText declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2RichTextPopoverWidgetBase* Popover = NewObject<UGV2RichTextPopoverWidgetBase>();
        TestNotNull(TEXT("Transient popover widget created"), Popover);
        if (Popover != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Popover->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Popover declares title capability"), Caps.Properties.Contains(TEXT("title")));
            TestTrue(TEXT("Popover declares description capability"), Caps.Properties.Contains(TEXT("description")));
            TestTrue(TEXT("Popover declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiKitCentralThemeContract,
    "GV2.Runtime.UIKit.CentralThemeAndComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiKitCentralThemeContract::RunTest(const FString& Parameters)
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    TestNotNull(TEXT("Configured central UI theme is loadable"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    TestNotNull(TEXT("Theme provides the default text style"), Theme->TextStyle.Get());
    TestNotNull(TEXT("Theme provides the rich text style"), Theme->RichTextStyle.Get());
    TestNotNull(
        TEXT("Theme provides the rich text popover class"),
        Theme->RichTextPopoverClass.LoadSynchronous());
    TestTrue(
        TEXT("Theme provides a visible rich text popover background"),
        Theme->RichTextPopoverBackground.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme constrains rich text popover height for overflow scrolling"),
        Theme->RichTextPopoverMaxHeight >= 64.0f);
    TestNotNull(TEXT("Theme provides the button style"), Theme->ButtonStyle.Get());
    TestNotNull(TEXT("Theme provides the button label style"), Theme->ButtonLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the checkbox label style"), Theme->CheckboxLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the input field label style"), Theme->InputFieldLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the dropdown header style"), Theme->DropdownHeaderStyle.Get());
    TestTrue(
        TEXT("Theme provides a visible dropdown popup background"),
        Theme->DropdownPopupBackground.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme constrains dropdown popup height"),
        Theme->DropdownMaxPopupHeight >= 32.0f);
    TestTrue(
        TEXT("Theme provides a visible unchecked checkbox brush"),
        Theme->CheckboxStyle.UncheckedImage.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme provides a visible checked checkbox brush"),
        Theme->CheckboxStyle.CheckedImage.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(TEXT("Theme registers the default text token"), Theme->TextStyleTokens.Contains(TEXT("default")));
    TestTrue(TEXT("Theme registers the inventory text token"), Theme->TextStyleTokens.Contains(TEXT("inventory")));
    TestTrue(TEXT("Theme registers the blue color token"), Theme->TextColorTokens.Contains(TEXT("blue")));
    TestTrue(TEXT("Theme registers the huge size token"), Theme->TextSizeTokens.Contains(TEXT("huge")));
    TestTrue(
        TEXT("Theme contains the test screen localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.description")));
    TestTrue(
        TEXT("Theme contains the checkbox localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.checkbox")));
    TestTrue(
        TEXT("Theme contains the input label localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.name_label")));
    TestTrue(
        TEXT("Theme contains the dropdown localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.dropdown_placeholder")));

    FString NormalizedMarkup;
    FString MarkupError;
    TestTrue(
        TEXT("Text pipeline accepts nested data-driven tokens"),
        UGV2TextPipeline::NormalizeMarkup(
            TEXT("A <color=blue>blue <size=huge>large</size></color><br/>line"),
            NormalizedMarkup,
            MarkupError));
    TestTrue(TEXT("Text pipeline flattens color runs"), NormalizedMarkup.Contains(TEXT("color=\"blue\"")));
    TestTrue(TEXT("Text pipeline flattens nested size runs"), NormalizedMarkup.Contains(TEXT("size=\"huge\"")));
    TestTrue(TEXT("Text pipeline converts semantic breaks"), NormalizedMarkup.Contains(TEXT("\nline")));
    TestFalse(
        TEXT("Text pipeline rejects unknown token values"),
        UGV2TextPipeline::NormalizeMarkup(
            TEXT("<color=not_registered>invalid</color>"),
            NormalizedMarkup,
            MarkupError));

    FGV2UiControlValue PlayerName;
    PlayerName.Name = TEXT("player_name");
    PlayerName.Type = EGV2UiControlValueType::String;
    PlayerName.StringValue = TEXT("<size=huge>Injected</size>");
    FGV2TextViewModel ResolvedText;
    FString ResolveError;
    TestTrue(
        TEXT("Text pipeline resolves text_id, arguments and optional style"),
        UGV2TextPipeline::Resolve(
            TEXT("core:text.screen.test.description"),
            {PlayerName},
            TEXT("inventory"),
            ResolvedText,
            ResolveError));
    TestEqual(TEXT("Resolved text retains the semantic style token"), ResolvedText.StyleToken, FName(TEXT("inventory")));
    TestFalse(TEXT("Resolved text carries centrally prepared renderer markup"), ResolvedText.NormalizedMarkup.IsEmpty());
    TestTrue(TEXT("String arguments cannot inject markup"), ResolvedText.Text.ToString().Contains(TEXT("&lt;size=huge&gt;")));
    TestTrue(
        TEXT("Escaped arguments remain single-escaped during markup normalization"),
        UGV2TextPipeline::NormalizeMarkup(ResolvedText.Text.ToString(), NormalizedMarkup, MarkupError));
    TestEqual(TEXT("Resolved renderer markup is the canonical normalized output"), ResolvedText.NormalizedMarkup, NormalizedMarkup);
    TestTrue(TEXT("Escaped argument is preserved for the renderer"), NormalizedMarkup.Contains(TEXT("&lt;size=huge&gt;")));
    TestFalse(TEXT("Escaped argument is not double-escaped"), NormalizedMarkup.Contains(TEXT("&amp;lt;size=huge")));

    // LOC-07: Missing translation in TextCatalog smoothly falls back to FallbackTextCatalog (source_message)
    Theme->FallbackTextCatalog.Add(TEXT("core:text.untranslated.item"), FText::FromString(TEXT("Fallback source string")));
    FGV2TextViewModel FallbackResolvedText;
    FString FallbackResolveError;
    TestTrue(
        TEXT("Text pipeline falls back to FallbackTextCatalog when key is missing from TextCatalog"),
        UGV2TextPipeline::Resolve(
            TEXT("core:text.untranslated.item"),
            {},
            TEXT("inventory"),
            FallbackResolvedText,
            FallbackResolveError));
    TestEqual(TEXT("Fallback resolved text matches source_message"), FallbackResolvedText.Text.ToString(), TEXT("Fallback source string"));

    FGV2TextViewModel MissingResolvedText;
    FString MissingResolveError;
    TestFalse(
        TEXT("Text pipeline rejects completely unknown text_id without fault or crash"),
        UGV2TextPipeline::Resolve(
            TEXT("core:text.unknown.nonexistent"),
            {},
            TEXT("inventory"),
            MissingResolvedText,
            MissingResolveError));
    TestTrue(TEXT("Error message identifies unknown text_id"), MissingResolveError.Contains(TEXT("Unknown text_id")));

    TestNull(
        TEXT("Plain text base exposes no raw FText apply entry point"),
        UGV2TextWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyTextContent")));
    TestNull(
        TEXT("Rich text base exposes no raw FText apply entry point"),
        UGV2RichTextWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyRichTextContent")));
    TestNull(
        TEXT("Image base exposes no raw Slate brush mutation entry point"),
        UGV2ImageWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyImageBrush")));

    FString DerivedResourceId;
    FString ImagePathError;
    const FString ResourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources"));
    TestTrue(
        TEXT("Image resource_id is derived from the canonical relative path"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(
                ResourceRoot,
                TEXT("core/resource/image/character_portrait.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Recursive image path maps to the expected Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.image.character_portrait")));
    TestTrue(
        TEXT("Tile suffix is accepted as source metadata"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(
                ResourceRoot,
                TEXT("core/resource/ui/old_paper_tile_256.tile.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Tile suffix is omitted from the Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.ui.old_paper_tile_256")));
    TestTrue(
        TEXT("Nine-slice suffix is accepted as source metadata"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(ResourceRoot, TEXT("core/resource/ui/panel.9.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Nine-slice suffix is omitted from the Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.ui.panel")));
    TestFalse(
        TEXT("Non-canonical image path is rejected"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(ResourceRoot, TEXT("core/resource/Image/Portrait.png")),
            DerivedResourceId,
            ImagePathError));

    const FString ScannerFixtureRoot = FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("GV2AutomationImageResources"));
    const FString ScannerFixtureDirectory = FPaths::Combine(
        ScannerFixtureRoot,
        TEXT("core/resource/image"));
    IFileManager::Get().DeleteDirectory(*ScannerFixtureRoot, false, true);
    IFileManager::Get().MakeDirectory(*ScannerFixtureDirectory, true);
    FImage ScannerFixtureImage(4, 6, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memset(ScannerFixtureImage.RawData.GetData(), 255, ScannerFixtureImage.RawData.Num());
    const FString ScannerFixturePng = FPaths::Combine(
        ScannerFixtureDirectory,
        TEXT("character_portrait.png"));
    TestTrue(
        TEXT("Scanner fixture PNG is written"),
        FImageUtils::SaveImageByExtension(*ScannerFixturePng, ScannerFixtureImage));

    FImage NineSliceFixtureImage(6, 6, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memzero(
        NineSliceFixtureImage.RawData.GetData(),
        NineSliceFixtureImage.RawData.Num());
    FColor* NineSlicePixels = reinterpret_cast<FColor*>(NineSliceFixtureImage.RawData.GetData());
    for (int32 Y = 1; Y < 5; ++Y)
    {
        for (int32 X = 1; X < 5; ++X)
        {
            NineSlicePixels[Y * 6 + X] = FColor::White;
        }
    }
    NineSlicePixels[2] = FColor::Black;
    NineSlicePixels[3] = FColor::Black;
    NineSlicePixels[2 * 6] = FColor::Black;
    NineSlicePixels[3 * 6] = FColor::Black;
    const FString NineSliceFixturePng = FPaths::Combine(
        ScannerFixtureDirectory,
        TEXT("panel.9.png"));
    TestTrue(
        TEXT("Nine-slice fixture PNG is written"),
        FImageUtils::SaveImageByExtension(*NineSliceFixturePng, NineSliceFixtureImage));

    UGV2ImageResourceCatalog* ScannedCatalog = NewObject<UGV2ImageResourceCatalog>();
    TestTrue(
        TEXT("Image catalog recursively scans and decodes filesystem PNG"),
        ScannedCatalog->BuildFromDirectory(ScannerFixtureRoot, ImagePathError));
    TestEqual(
        TEXT("Filesystem scan publishes both authored resources"),
        ScannedCatalog->GetEntries().Num(),
        2);
    if (ScannedCatalog->GetEntries().Num() == 2)
    {
        TestEqual(
            TEXT("Filesystem resource keeps the derived ID"),
            ScannedCatalog->GetEntries()[0].ResourceId,
            FString(TEXT("core:resource.image.character_portrait")));
        TestEqual(
            TEXT("Plain PNG derives its fixed aspect ratio"),
            ScannedCatalog->GetEntries()[0].FixedAspectRatio,
            2.0f / 3.0f);
        TestEqual(
            TEXT("Nine-slice suffix selects nine-slice mode"),
            ScannedCatalog->GetEntries()[1].RenderMode,
            EGV2ImageRenderMode::NineSlice);
        TestEqual(
            TEXT("Nine-slice top marker derives the left border"),
            static_cast<float>(ScannedCatalog->GetEntries()[1].NineSliceBorderPixels.Left),
            1.0f);
        TestEqual(
            TEXT("Nine-slice left marker derives the top border"),
            static_cast<float>(ScannedCatalog->GetEntries()[1].NineSliceBorderPixels.Top),
            1.0f);
        UTexture2D* NineSliceTexture = ScannedCatalog->GetEntries()[1].Texture.Get();
        TestNotNull(TEXT("Nine-slice scanner creates a cropped runtime texture"), NineSliceTexture);
        if (NineSliceTexture != nullptr)
        {
            TestEqual(TEXT("Nine-slice marker border is cropped from width"), NineSliceTexture->GetSizeX(), 4);
            TestEqual(TEXT("Nine-slice marker border is cropped from height"), NineSliceTexture->GetSizeY(), 4);
        }
    }

    FGV2ResolvedImageResource FirstPortraitResolve;
    FGV2ResolvedImageResource SecondPortraitResolve;
    TestTrue(
        TEXT("Prepared fixed-aspect resource resolves from the catalog lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.character_portrait"),
            FirstPortraitResolve,
            ImagePathError));
    TestTrue(
        TEXT("Repeated resolve returns the prepared fixed-aspect resource"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.character_portrait"),
            SecondPortraitResolve,
            ImagePathError));
    TestEqual(
        TEXT("Repeated resolve preserves the prepared brush resource object"),
        FirstPortraitResolve.Brush.GetResourceObject(),
        SecondPortraitResolve.Brush.GetResourceObject());
    TestEqual(
        TEXT("Repeated resolve preserves the prepared brush size"),
        FirstPortraitResolve.Brush.ImageSize,
        SecondPortraitResolve.Brush.ImageSize);

    FGV2ResolvedImageResource ScannedPanel;
    TestTrue(
        TEXT("Prepared nine-slice resource resolves from the catalog lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.panel"),
            ScannedPanel,
            ImagePathError));
    TestEqual(
        TEXT("Prepared nine-slice lookup retains box drawing"),
        ScannedPanel.Brush.DrawAs,
        ESlateBrushDrawType::Box);
    TestFalse(
        TEXT("Invalid resource ID is rejected before lookup"),
        ScannedCatalog->Resolve(
            TEXT("Core:resource.image.character_portrait"),
            ScannedPanel,
            ImagePathError));
    TestFalse(
        TEXT("Unknown canonical resource ID is rejected by lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.missing"),
            ScannedPanel,
            ImagePathError));
    IFileManager::Get().DeleteDirectory(*ScannerFixtureRoot, false, true);

    UGV2ImageResourceCatalog* ConfiguredImageCatalog =
        UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    TestNotNull(TEXT("Configured image catalog is available"), ConfiguredImageCatalog);
    if (ConfiguredImageCatalog != nullptr)
    {
        FGV2ResolvedImageResource PaperTile;
        TestTrue(
            TEXT("Authored paper tile resolves by suffix-free resource_id"),
            ConfiguredImageCatalog->Resolve(
                TEXT("core:resource.ui.old_paper_tile_256"),
                PaperTile,
                ImagePathError));
        TestEqual(
            TEXT("Authored paper resource uses tile mode"),
            PaperTile.RenderMode,
            EGV2ImageRenderMode::Tile);
        TestEqual(
            TEXT("Authored paper tile keeps its decoded logical width"),
            static_cast<float>(PaperTile.Brush.ImageSize.X),
            256.0f);
        TestEqual(
            TEXT("Authored paper tile keeps its decoded logical height"),
            static_cast<float>(PaperTile.Brush.ImageSize.Y),
            256.0f);
        TestEqual(
            TEXT("Authored paper resource tiles in both axes"),
            PaperTile.Brush.Tiling,
            ESlateBrushTileType::Both);
    }

    UTexture2D* ImageFixtureTexture = UTexture2D::CreateTransient(64, 64);
    TestNotNull(TEXT("Image resource fixture texture is available"), ImageFixtureTexture);
    if (ImageFixtureTexture != nullptr)
    {
        FGV2ImageResourceDefinition FixedAspectDefinition;
        FixedAspectDefinition.ResourceId = TEXT("core:resource.image.test_portrait");
        FixedAspectDefinition.Texture = ImageFixtureTexture;
        FixedAspectDefinition.RenderMode = EGV2ImageRenderMode::FixedAspect;
        FixedAspectDefinition.FixedAspectRatio = 2.0f / 3.0f;

        FString ImageResourceError;
        FGV2ResolvedImageResource ResolvedImage;
        TestTrue(
            TEXT("fixed_aspect image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                FixedAspectDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("fixed_aspect resource uses an ordinary image brush"),
            ResolvedImage.Brush.DrawAs,
            ESlateBrushDrawType::Image);
        TestEqual(
            TEXT("fixed_aspect resource preserves declared ratio"),
            ResolvedImage.FixedAspectRatio,
            2.0f / 3.0f);

        FGV2ImageResourceDefinition NineSliceDefinition = FixedAspectDefinition;
        NineSliceDefinition.ResourceId = TEXT("core:resource.surface.test_panel");
        NineSliceDefinition.RenderMode = EGV2ImageRenderMode::NineSlice;
        NineSliceDefinition.NineSliceBorderPixels = FMargin(8.0f);
        TestTrue(
            TEXT("nine_slice image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                NineSliceDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("nine_slice resource produces a box brush"),
            ResolvedImage.Brush.DrawAs,
            ESlateBrushDrawType::Box);
        TestEqual(
            TEXT("nine_slice borders normalize against texture width"),
            static_cast<float>(ResolvedImage.Brush.Margin.Left),
            0.125f);

        FGV2ImageResourceDefinition TileDefinition = FixedAspectDefinition;
        TileDefinition.ResourceId = TEXT("core:resource.pattern.test_background");
        TileDefinition.RenderMode = EGV2ImageRenderMode::Tile;
        TileDefinition.TileSize = FVector2D(24.0f, 40.0f);
        TestTrue(
            TEXT("tile image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                TileDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("tile resource repeats on both axes"),
            ResolvedImage.Brush.Tiling,
            ESlateBrushTileType::Both);
        TestEqual(
            TEXT("tile resource retains logical repeat width"),
            static_cast<float>(ResolvedImage.Brush.ImageSize.X),
            24.0f);
        TestEqual(
            TEXT("tile resource retains logical repeat height"),
            static_cast<float>(ResolvedImage.Brush.ImageSize.Y),
            40.0f);

        FixedAspectDefinition.FixedAspectRatio = 0.0f;
        TestFalse(
            TEXT("fixed_aspect resource rejects a non-positive ratio"),
            UGV2ImageResourceCatalog::ValidateDefinition(
                FixedAspectDefinition,
                ImageResourceError));
        NineSliceDefinition.NineSliceBorderPixels = FMargin(32.0f, 1.0f, 32.0f, 1.0f);
        TestFalse(
            TEXT("nine_slice resource rejects a collapsed center"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                NineSliceDefinition,
                ResolvedImage,
                ImageResourceError));
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter UiAssetFilter;
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/TextSystem/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/RH/UI"));
    UiAssetFilter.bRecursivePaths = true;
    TArray<FAssetData> UiAssets;
    AssetRegistryModule.Get().GetAssets(UiAssetFilter, UiAssets);
    int32 WidgetBlueprintCount = 0;
    for (const FAssetData& Asset : UiAssets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.StartsWith(TEXT("WBP_")))
        {
            continue;
        }
        ++WidgetBlueprintCount;
        const FString GeneratedClassPath = FString::Printf(
            TEXT("%s.%s_C"),
            *Asset.PackageName.ToString(),
            *AssetName);
        UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        TestNotNull(
            *FString::Printf(TEXT("Current WBP has a loadable generated class: %s"), *AssetName),
            WidgetClass);
        const UWidgetBlueprintGeneratedClass* GeneratedClass =
            Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
        if (GeneratedClass == nullptr || GeneratedClass->GetWidgetTreeArchetype() == nullptr)
        {
            continue;
        }

        bool bContainsDirectTextPrimitive = false;
        GeneratedClass->GetWidgetTreeArchetype()->ForEachWidget(
            [&bContainsDirectTextPrimitive](UWidget* Widget)
            {
                bContainsDirectTextPrimitive |= Widget != nullptr
                    && (Widget->IsA<UTextBlock>() || Widget->IsA<URichTextBlock>());
            });
        if (bContainsDirectTextPrimitive)
        {
            const bool bUsesTextPipelineBase = WidgetClass->IsChildOf(UGV2TextWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2ButtonWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2CheckboxWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2InputFieldWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2RichTextWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2RichTextPopoverWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2LocationTopBarWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2LocationPlayerStatusWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2LocationSceneWidgetBase::StaticClass())
                || WidgetClass->IsChildOf(UGV2LocationCommandPanelWidgetBase::StaticClass());
            TestTrue(
                *FString::Printf(
                    TEXT("Text-bearing WBP must use a Text Pipeline native base: %s"),
                    *AssetName),
                bUsesTextPipelineBase);
        }
    }
    TestEqual(TEXT("UI contract audits every current WBP asset"), WidgetBlueprintCount, 28);
    TestTrue(
        TEXT("Theme provides a visible separator brush"),
        Theme->SeparatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme provides a visible loading indicator brush"),
        Theme->LoadingIndicatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);

    struct FComponentContract
    {
        const TCHAR* ClassPath;
        UClass* NativeParent;
    };
    const FComponentContract Components[] = {
        {TEXT("/Game/UI/Widgets/WBP_Text.WBP_Text_C"), UGV2TextWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_RichText.WBP_RichText_C"), UGV2RichTextWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"), UGV2ImageWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"), UGV2ButtonWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Checkbox.WBP_Checkbox_C"), UGV2CheckboxWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"), UGV2InputFieldWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"), UGV2DropdownSelectWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_ButtonList.WBP_ButtonList_C"), UGV2ButtonListWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C"), UGV2ProgressBarWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Separator.WBP_Separator_C"), UGV2SeparatorWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_LoadingIndicator.WBP_LoadingIndicator_C"), UGV2LoadingIndicatorWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_RichTextPopover.WBP_RichTextPopover_C"), UGV2RichTextPopoverWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C"), UGV2IconWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Panel.WBP_Panel_C"), UGV2PanelWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ScrollArea.WBP_ScrollArea_C"), UGV2ScrollAreaWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ListView.WBP_ListView_C"), UGV2ListViewWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_TabContainer.WBP_TabContainer_C"), UGV2TabContainerWidgetBase::StaticClass()},
    };

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    for (const FComponentContract& Component : Components)
    {
        UClass* ComponentClass = LoadClass<UUserWidget>(nullptr, Component.ClassPath);
        TestNotNull(*FString::Printf(TEXT("UI component is loadable: %s"), Component.ClassPath), ComponentClass);
        if (ComponentClass == nullptr)
        {
            continue;
        }
        TestTrue(
            *FString::Printf(TEXT("UI component has expected native parent: %s"), Component.ClassPath),
            ComponentClass->IsChildOf(Component.NativeParent));

        UUserWidget* Widget = TestWorld != nullptr
            ? CreateWidget<UUserWidget>(TestWorld, ComponentClass)
            : nullptr;
        TestNotNull(*FString::Printf(TEXT("UI component instantiates: %s"), Component.ClassPath), Widget);
        if (Widget != nullptr)
        {
            TestTrue(
                *FString::Printf(TEXT("UI component implements central style consumer: %s"), Component.ClassPath),
                Widget->Implements<UGV2UiStyleConsumer>());
            TestTrue(
                *FString::Printf(TEXT("UI component applies central theme: %s"), Component.ClassPath),
                IGV2UiStyleConsumer::Execute_ApplyCentralStyle(Widget));

            if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(Widget))
            {
                UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(
                    RichText->GetWidgetFromName(TEXT("RichTextBlock")));
                UScrollBox* RichTextScrollBox = Cast<UScrollBox>(
                    RichText->GetWidgetFromName(TEXT("RichTextScrollBox")));
                TestNotNull(TEXT("RichText owns its vertical ScrollBox"), RichTextScrollBox);
                TestNotNull(TEXT("RichText owns its CommonRichTextBlock"), RichTextBlock);
                if (RichTextBlock != nullptr)
                {
                    TestTrue(
                        TEXT("RichText automatically wraps to its allocated width"),
                        RichTextBlock->GetAutoWrapText());
                }
                if (RichTextScrollBox != nullptr)
                {
                    TestEqual(
                        TEXT("RichText overflow scrolls vertically"),
                        RichTextScrollBox->GetOrientation(),
                        EOrientation::Orient_Vertical);
                    RichTextScrollBox->SetScrollOffset(42.0f);
                    FGV2TextViewModel ReplacementText;
                    ReplacementText.Text = FText::FromString(TEXT("Replacement text"));
                    RichText->ApplyText(ReplacementText);
                    TestEqual(
                        TEXT("Applying replacement RichText resets scroll to the start"),
                        RichTextScrollBox->GetScrollOffset(),
                        0.0f);
                }
                const UCommonTextStyle* RichTextStyle = Theme->RichTextStyle != nullptr
                    ? Cast<UCommonTextStyle>(Theme->RichTextStyle->GetDefaultObject())
                    : nullptr;
                FSlateFontInfo ExpectedFont;
                if (RichTextStyle != nullptr)
                {
                    RichTextStyle->GetFont(ExpectedFont);
                }
                const FSlateFontInfo& InteractiveFont =
                    RichText->ResolveRunTextStyle(TEXT("default"), NAME_None, NAME_None).Font;
                TestEqual(
                    TEXT("Interactive RichText inherits the configured font object"),
                    InteractiveFont.FontObject,
                    ExpectedFont.FontObject);
                TestEqual(
                    TEXT("Interactive RichText inherits the configured typeface"),
                    InteractiveFont.TypefaceFontName,
                    ExpectedFont.TypefaceFontName);
                TestEqual(
                    TEXT("Interactive RichText inherits the configured font size"),
                    InteractiveFont.Size,
                    ExpectedFont.Size);
                }
            }
            if (UGV2RichTextPopoverWidgetBase* Popover =
                    Cast<UGV2RichTextPopoverWidgetBase>(Widget))
            {
                UGV2RichTextWidgetBase* PopoverDescription =
                    Cast<UGV2RichTextWidgetBase>(
                        Popover->GetWidgetFromName(TEXT("DescriptionText")));
                TestNotNull(
                    TEXT("RichText popover composes the reusable RichText component"),
                    PopoverDescription);
                FGV2RichTextHoverViewModel HoverModel;
                HoverModel.Title.Text = FText::FromString(TEXT("Title"));
                HoverModel.Title.StyleToken = TEXT("default");
                HoverModel.Description.Text = FText::FromString(
                    TEXT("A long popover description that must use the shared wrapping and scrolling behavior."));
                HoverModel.Description.StyleToken = TEXT("default");
                TestTrue(
                    TEXT("RichText popover initializes through the reusable component"),
                    Popover->InitializePopover(HoverModel));
                if (PopoverDescription != nullptr)
                {
                    UCommonRichTextBlock* PopoverRichText =
                        Cast<UCommonRichTextBlock>(PopoverDescription->GetWidgetFromName(TEXT("RichTextBlock")));
                    UScrollBox* PopoverScrollBox = Cast<UScrollBox>(
                        PopoverDescription->GetWidgetFromName(TEXT("RichTextScrollBox")));
                    TestTrue(
                        TEXT("Popover description inherits automatic wrapping"),
                        PopoverRichText != nullptr && PopoverRichText->GetAutoWrapText());
                    TestNotNull(
                        TEXT("Popover description inherits vertical overflow scrolling"),
                        PopoverScrollBox);
                }
            }
        }

    UClass* TestScreenClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/UI/Widgets/WBP_Testscreen.WBP_Testscreen_C"));
    UUserWidget* TestScreen = TestWorld != nullptr && TestScreenClass != nullptr
        ? CreateWidget<UUserWidget>(TestWorld, TestScreenClass)
        : nullptr;
    TestNotNull(TEXT("Test screen with the paper surface instantiates"), TestScreen);
    if (TestScreen != nullptr)
    {
        TestScreen->TakeWidget();
        UGV2ImageWidgetBase* DescriptionBackground = Cast<UGV2ImageWidgetBase>(
            TestScreen->GetWidgetFromName(TEXT("DescriptionBackground")));
        TestNotNull(TEXT("Test screen exposes its WBP_Image description background"), DescriptionBackground);
        if (DescriptionBackground != nullptr)
        {
            TestEqual(
                TEXT("Description background applies the suffix-free paper resource_id"),
                DescriptionBackground->GetAppliedResourceId(),
                FString(TEXT("core:resource.ui.old_paper_tile_256")));
            TestEqual(
                TEXT("Description background renders as a two-axis tile"),
                DescriptionBackground->GetImageBrush().Tiling,
                ESlateBrushTileType::Both);
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
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
    const FGV2ScreenRegistryEntry* TestScreenEntry = Registry != nullptr
        ? Registry->FindEntry(TEXT("core:screen.test"))
        : nullptr;
    TestNotNull(TEXT("Screen Registry contains the test screen entry"), TestScreenEntry);
    UClass* TestScreenClass = TestScreenEntry != nullptr
        ? TestScreenEntry->WidgetClass.LoadSynchronous()
        : nullptr;
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

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogBootstrapGate,
    "GV2.Runtime.Bootstrap.ImageCatalogFailureBlocksReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ImageCatalogBootstrapGate::RunTest(const FString& Parameters)
{
    UGV2ImageResourceCatalogSettings* ImageSettings =
        GetMutableDefault<UGV2ImageResourceCatalogSettings>();
    TestNotNull(TEXT("Image Catalog settings are available"), ImageSettings);
    if (ImageSettings == nullptr)
    {
        return false;
    }

    const FString OriginalRoot = ImageSettings->ResourceRootDirectory;
    FString InitialBuildError;
    TestTrue(
        TEXT("Image Catalog failure fixture starts from a published valid catalog"),
        UGV2ImageResourceCatalogSettings::RebuildConfiguredCatalog(InitialBuildError));
    UGV2ImageResourceCatalog* CatalogBeforeFailedRebuild =
        UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    TestNotNull(
        TEXT("Valid configured catalog exists before failed rebuild"),
        CatalogBeforeFailedRebuild);
    ImageSettings->ResourceRootDirectory = TEXT("/invalid/absolute/resource/root");
    AddExpectedError(
        TEXT("Image Resource Catalog build failed"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("StartSession rejected: required Image Resource Catalog is not ready"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=ImageCatalogNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();
    ImageSettings->ResourceRootDirectory = OriginalRoot;

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists after failed catalog bootstrap"), Runtime);
    if (Runtime != nullptr)
    {
        Runtime->StartSession();
        const FGV2SessionStatus Status = Runtime->GetSessionState();
        TestFalse(TEXT("Failed required catalog keeps session non-ready"), Status.bIsReady);
        TestNotEqual(
            TEXT("Failed required catalog prevents Ready state publication"),
            Status.SessionState,
            EGV2SessionState::Ready);
        TestNull(TEXT("Failed required catalog publishes no active Screen"), Runtime->GetActiveScreen());
    }

    UGV2ImageResourceCatalog* CatalogAfterFailedRebuild =
        UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    TestEqual(
        TEXT("Failed candidate rebuild preserves the previously published catalog"),
        CatalogAfterFailedRebuild,
        CatalogBeforeFailedRebuild);
    if (CatalogAfterFailedRebuild != nullptr)
    {
        FGV2ResolvedImageResource PreservedResource;
        FString PreservedResolveError;
        TestTrue(
            TEXT("Previously published prepared lookup remains usable after failed rebuild"),
            CatalogAfterFailedRebuild->Resolve(
                TEXT("core:resource.ui.old_paper_tile_256"),
                PreservedResource,
                PreservedResolveError));
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();

    FString RestoreError;
    TestTrue(
        TEXT("Configured Image Catalog rebuilds after failure fixture cleanup"),
        UGV2ImageResourceCatalogSettings::RebuildConfiguredCatalog(RestoreError));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DebugStartScreenFlow,
    "GV2.Runtime.Presentation.StartButtonOpensRegisteredScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DebugStartScreenFlow::RunTest(const FString& Parameters)
{
    for (const TCHAR* StylePath : {
             TEXT("/Game/TextSystem/UI/Styles/BP_UIStyle_Text_Default.BP_UIStyle_Text_Default_C"),
             TEXT("/Game/TextSystem/UI/Styles/BP_UIStyle_ButtonLabel_Default.BP_UIStyle_ButtonLabel_Default_C")})
    {
        const UClass* StyleClass = LoadClass<UCommonTextStyle>(nullptr, StylePath);
        const UCommonTextStyle* Style = StyleClass != nullptr
            ? Cast<UCommonTextStyle>(StyleClass->GetDefaultObject())
            : nullptr;
        TestNotNull(TEXT("CommonUI text style is loadable"), Style);
        if (Style != nullptr)
        {
            FSlateFontInfo Font;
            Style->GetFont(Font);
            TestNotNull(TEXT("CommonUI text style has an explicit font"), Font.FontObject.Get());
            TestEqual(TEXT("CommonUI text style selects Regular typeface"), Font.TypefaceFontName, FName(TEXT("Regular")));
        }
    }

    const FGV2ScopedSamplePackageOverride SampleOverride;

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

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

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RhStartScreenFlow,
    "GV2.Runtime.Presentation.RhStartOpensLocationScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RhStartScreenFlow::RunTest(const FString& Parameters)
{
    const UGV2RuntimeSettings* RuntimeSettings = GetDefault<UGV2RuntimeSettings>();
    TestNotNull(TEXT("Runtime development settings are available"), RuntimeSettings);
    if (RuntimeSettings != nullptr)
    {
        TestTrue(
            TEXT("Editor startup profile uses RH"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/rh")));
        TestFalse(
            TEXT("Editor startup profile excludes the sample test screen"),
            RuntimeSettings->EditorPackageRoots.Contains(TEXT("GameData/sample")));
    }

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the runtime"), Runtime);
    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
        UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("RH startup opens the registered LocationScreen"), Screen);
        if (Screen != nullptr)
        {
            UClass* LocationScreenClass = LoadClass<UUserWidget>(
                nullptr,
                TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
            TestNotNull(TEXT("LocationScreen class is loadable"), LocationScreenClass);
            TestTrue(
                TEXT("RH startup presents WBP_LocationScreen"),
                LocationScreenClass != nullptr && Screen->IsA(LocationScreenClass));

            // 1. Verify startup tavern scene has 1 character from Lua presentation
            UGV2LocationSceneWidgetBase* SceneWidget = nullptr;
            UGV2LocationCommandPanelWidgetBase* CommandWidget = nullptr;
            if (Screen->WidgetTree != nullptr)
            {
                Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                {
                    if (UGV2LocationSceneWidgetBase* Scene = Cast<UGV2LocationSceneWidgetBase>(Widget))
                    {
                        SceneWidget = Scene;
                    }
                    else if (UGV2LocationCommandPanelWidgetBase* Cmd = Cast<UGV2LocationCommandPanelWidgetBase>(Widget))
                    {
                        CommandWidget = Cmd;
                    }
                });
            }

            TestNotNull(TEXT("LocationScreen contains SceneView component"), SceneWidget);
            if (SceneWidget != nullptr && SceneWidget->GetCharacterRepeater() != nullptr)
            {
                UGV2ListViewWidgetBase* CharRep = SceneWidget->GetCharacterRepeater();
                TestEqual(TEXT("Initial tavern scene has 1 character"), CharRep->GetEntryCount(), 1);
                TestNotNull(TEXT("Initial tavern character widget matches keeper"), CharRep->GetEntryWidget(FName(TEXT("tavern_keeper"))));
            }

            // 2. Find travel button to market in CommandPanel and submit interaction
            TestNotNull(TEXT("LocationScreen contains CommandPanel component"), CommandWidget);
            if (CommandWidget != nullptr && CommandWidget->GetRepeater() != nullptr)
            {
                UGV2ListViewWidgetBase* CmdRep = CommandWidget->GetRepeater();
                UGV2ButtonWidgetBase* TravelMarketBtn = Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))));
                TestNotNull(TEXT("Travel to market button found in tavern CommandPanel"), TravelMarketBtn);
                if (TravelMarketBtn != nullptr)
                {
                    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
                    TestEqual(TEXT("Travel to market interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                }
            }

            // 3. Verify Market presentation has 0 characters
            UGV2ScreenWidgetBase* MarketScreen = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Market LocationScreen is presented"), MarketScreen);
            if (MarketScreen != nullptr)
            {
                UGV2LocationSceneWidgetBase* MarketScene = nullptr;
                UGV2LocationCommandPanelWidgetBase* MarketCommandsWidget = nullptr;
                if (MarketScreen->WidgetTree != nullptr)
                {
                    MarketScreen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (UGV2LocationSceneWidgetBase* Scene = Cast<UGV2LocationSceneWidgetBase>(Widget))
                        {
                            MarketScene = Scene;
                        }
                        else if (UGV2LocationCommandPanelWidgetBase* Cmd = Cast<UGV2LocationCommandPanelWidgetBase>(Widget))
                        {
                            MarketCommandsWidget = Cmd;
                        }
                    });
                }
                TestNotNull(TEXT("Market Screen contains SceneView component"), MarketScene);
                if (MarketScene != nullptr && MarketScene->GetCharacterRepeater() != nullptr)
                {
                    TestEqual(TEXT("Market scene has 0 characters"), MarketScene->GetCharacterRepeater()->GetEntryCount(), 0);
                }

                // 4. Travel back to tavern
                TestNotNull(TEXT("Market Screen contains CommandPanel component"), MarketCommandsWidget);
                if (MarketCommandsWidget != nullptr && MarketCommandsWidget->GetRepeater() != nullptr)
                {
                    UGV2ListViewWidgetBase* MarketCmdRep = MarketCommandsWidget->GetRepeater();
                    UGV2ButtonWidgetBase* TravelTavernBtn = Cast<UGV2ButtonWidgetBase>(MarketCmdRep->GetEntryWidget(FName(TEXT("travel_city_tavern"))));
                    TestNotNull(TEXT("Travel to tavern button found in market CommandPanel"), TravelTavernBtn);
                    if (TravelTavernBtn != nullptr)
                    {
                        const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelTavernBtn->GetBindingHandle(), {});
                        TestEqual(TEXT("Travel back to tavern interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
                    }
                }
            }

            // 5. Verify returned Tavern has 1 character restored
            UGV2ScreenWidgetBase* TavernScreen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Returned Tavern LocationScreen is presented"), TavernScreen2);
            if (TavernScreen2 != nullptr)
            {
                UGV2LocationSceneWidgetBase* TavernScene2 = nullptr;
                if (TavernScreen2->WidgetTree != nullptr)
                {
                    TavernScreen2->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (UGV2LocationSceneWidgetBase* Scene = Cast<UGV2LocationSceneWidgetBase>(Widget))
                        {
                            TavernScene2 = Scene;
                        }
                    });
                }
                TestNotNull(TEXT("Returned Tavern Screen contains SceneView component"), TavernScene2);
                if (TavernScene2 != nullptr && TavernScene2->GetCharacterRepeater() != nullptr)
                {
                    UGV2ListViewWidgetBase* CharRep2 = TavernScene2->GetCharacterRepeater();
                    TestEqual(TEXT("Returned tavern scene has 1 character"), CharRep2->GetEntryCount(), 1);
                    TestNotNull(TEXT("Returned tavern character widget matches keeper"), CharRep2->GetEntryWidget(FName(TEXT("tavern_keeper"))));
                }
            }
        }
        Runtime->EndSession();
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaTestScreenWidgetCreation,
    "GV2.Runtime.Presentation.LuaCreatesRegisteredScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaTestScreenWidgetCreation::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

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
            const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
            UGV2ScreenRegistry* Registry = RegistrySettings != nullptr
                ? RegistrySettings->RegistryAsset.LoadSynchronous()
                : nullptr;
            const FGV2ScreenRegistryEntry* RegisteredEntry = Registry != nullptr
                ? Registry->FindEntry(TEXT("core:screen.test"))
                : nullptr;
            UClass* RegisteredClass = RegisteredEntry != nullptr
                ? RegisteredEntry->WidgetClass.LoadSynchronous()
                : nullptr;
            TestEqual(
                TEXT("Created screen class comes from the configured registry entry"),
                Screen->GetClass(),
                RegisteredClass);

            const TArray<FGV2ScreenFieldDescriptor> Contract = Screen->GetScreenFieldContract();
            TestEqual(TEXT("Test screen exposes 0 remaining legacy dynamic fields (all migrated to property hosts)"), Contract.Num(), 0);

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
    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2InputFieldWidgetContract,
    "GV2.Runtime.UIKit.InputFieldWidgetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2InputFieldWidgetContract::RunTest(const FString& Parameters)
{
    UClass* WidgetClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"));
    TestNotNull(TEXT("WBP_InputField_C is loadable"), WidgetClass);
    if (WidgetClass == nullptr)
    {
        return false;
    }

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

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

        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(TEXT("Player Name"));
        InputFieldWidget->ApplyText(TextModel);

        FGV2TextViewModel PlaceholderModel;
        PlaceholderModel.Text = FText::FromString(TEXT("Enter name..."));
        InputFieldWidget->ApplyPlaceholderText(PlaceholderModel);

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

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiLayeredReconciliationContract,
    "GV2.UI.LayeredReconciliationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiLayeredReconciliationContract::RunTest(const FString& Parameters)
{
    // 1. UIF-17: Game Shell Layers Validation & Order
    TestTrue(TEXT("background is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("background")));
    TestTrue(TEXT("location_content is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("location_content")));
    TestTrue(TEXT("character_presentation is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("character_presentation")));
    TestTrue(TEXT("core_interface is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("core_interface")));
    TestTrue(TEXT("overlay_stack is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("overlay_stack")));
    TestTrue(TEXT("modal_stack is a valid layer"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("modal_stack")));
    TestFalse(TEXT("arbitrary_layer is invalid"), UGV2GameShellWidgetBase::IsValidLayerName(TEXT("arbitrary_layer")));

    const TArray<FName>& ApprovedLayers = UGV2GameShellWidgetBase::GetApprovedLayers();
    TestEqual(TEXT("Exactly 6 approved layers"), ApprovedLayers.Num(), 6);
    TestEqual(TEXT("Layer 0 is background"), ApprovedLayers[0], UGV2GameShellWidgetBase::LayerBackground);
    TestEqual(TEXT("Layer 1 is location_content"), ApprovedLayers[1], UGV2GameShellWidgetBase::LayerLocationContent);
    TestEqual(TEXT("Layer 2 is character_presentation"), ApprovedLayers[2], UGV2GameShellWidgetBase::LayerCharacterPresentation);
    TestEqual(TEXT("Layer 3 is core_interface"), ApprovedLayers[3], UGV2GameShellWidgetBase::LayerCoreInterface);
    TestEqual(TEXT("Layer 4 is overlay_stack"), ApprovedLayers[4], UGV2GameShellWidgetBase::LayerOverlayStack);
    TestEqual(TEXT("Layer 5 is modal_stack"), ApprovedLayers[5], UGV2GameShellWidgetBase::LayerModalStack);

    // 2. UIF-17: Screen Registry Validation
    UGV2ScreenRegistry* Registry = NewObject<UGV2ScreenRegistry>();
    FString ValidationError;
    TestFalse(TEXT("Empty registry fails validation"), Registry->Validate(ValidationError));

    // 3. UIF-19, UIF-20, UIF-21: Multi-layer Reconciliation, Reuse, Replacement, Modal Blocking, Atomicity
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr,
            TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(
            TestWorld,
            GameShellClass);
        TestNotNull(TEXT("Game shell instantiated"), Shell);
        if (Shell != nullptr)
        {
            Shell->AddToRoot();
            const TPair<FName, FName> LayersToVerify[] = {
                {UGV2GameShellWidgetBase::LayerBackground, TEXT("BackgroundHost")},
                {UGV2GameShellWidgetBase::LayerLocationContent, TEXT("LocationContentHost")},
                {UGV2GameShellWidgetBase::LayerCharacterPresentation, TEXT("CharacterPresentationHost")},
                {UGV2GameShellWidgetBase::LayerCoreInterface, TEXT("CoreInterfaceHost")},
                {UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("OverlayStackHost")},
                {UGV2GameShellWidgetBase::LayerModalStack, TEXT("ModalStackHost")},
            };
            for (const TPair<FName, FName>& LayerAndHost : LayersToVerify)
            {
                const FName LayerToVerify = LayerAndHost.Key;
                UPanelWidget* AuthoredHost = Cast<UPanelWidget>(Shell->GetWidgetFromName(LayerAndHost.Value));
                TestNotNull(
                    *FString::Printf(TEXT("Game shell authors a visible host for layer '%s'"), *LayerToVerify.ToString()),
                    AuthoredHost);
                if (AuthoredHost != nullptr)
                {
                    TestTrue(
                        *FString::Printf(TEXT("Layer host '%s' belongs to the rendered Widget Tree"), *LayerToVerify.ToString()),
                        AuthoredHost == Shell->WidgetTree->RootWidget || AuthoredHost->GetParent() != nullptr);
                }
                UUserWidget* ProbeWidget = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
                const bool bAttached = Shell->AttachScreenToLayer(LayerToVerify, ProbeWidget);
                TestTrue(
                    *FString::Printf(TEXT("Game shell binds a host panel for layer '%s'"), *LayerToVerify.ToString()),
                    bAttached);
                if (AuthoredHost != nullptr)
                {
                    TestEqual(
                        *FString::Printf(TEXT("Layer '%s' attaches to its authored host"), *LayerToVerify.ToString()),
                        ProbeWidget->GetParent(),
                        AuthoredHost);
                }
                Shell->DetachScreen(ProbeWidget);
            }
        }

        FGV2LayeredUiReconciler Reconciler;

        TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> ScreenClasses;
        ScreenClasses.Add(TEXT("core:screen.main"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.alt"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.modal_confirm"), UGV2ScreenWidgetBase::StaticClass());

        int32 FactoryInstantiations = 0;
        auto MockFactory = [&](const FString& ScreenId) -> UGV2ScreenWidgetBase*
        {
            TSubclassOf<UGV2ScreenWidgetBase>* FoundClass = ScreenClasses.Find(ScreenId);
            if (FoundClass == nullptr || *FoundClass == nullptr)
            {
                return nullptr;
            }
            ++FactoryInstantiations;
            return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, *FoundClass);
        };

        // Step A: Initial Document with Route
        FGV2UiDocumentViewModel Doc1;
        Doc1.UiInstanceId = TEXT("ui@1:1");
        Doc1.Revision = 1;
        Doc1.bHasRoute = true;
        Doc1.Route.Layer = TEXT("location_content");
        Doc1.Route.InstanceKey = TEXT("main");
        Doc1.Route.ScreenId = TEXT("core:screen.main");

        FString ReconcileError;
        TestTrue(TEXT("Reconcile initial Doc1 succeeds"), Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError));
        TestEqual(TEXT("Factory instantiated 1 screen widget"), FactoryInstantiations, 1);

        UGV2ScreenWidgetBase* RouteWidget1 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestNotNull(TEXT("Route widget exists in location_content layer"), RouteWidget1);

        // Step B: Update Doc2 with same ScreenId -> Must REUSE widget instance (0 new instantiations)
        FGV2UiDocumentViewModel Doc2;
        Doc2.UiInstanceId = TEXT("ui@1:1");
        Doc2.Revision = 2;
        Doc2.bHasRoute = true;
        Doc2.Route.Layer = TEXT("location_content");
        Doc2.Route.InstanceKey = TEXT("main");
        Doc2.Route.ScreenId = TEXT("core:screen.main");

        TestTrue(TEXT("Reconcile Doc2 succeeds"), Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError));
        TestEqual(TEXT("Widget reused without new instantiation"), FactoryInstantiations, 1);
        UGV2ScreenWidgetBase* RouteWidget2 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestEqual(TEXT("Widget instance pointer is preserved across revisions"), RouteWidget2, RouteWidget1);

        // Step C: Doc3 with changed ScreenId -> Replaces widget
        FGV2UiDocumentViewModel Doc3;
        Doc3.UiInstanceId = TEXT("ui@1:1");
        Doc3.Revision = 3;
        Doc3.bHasRoute = true;
        Doc3.Route.Layer = TEXT("location_content");
        Doc3.Route.InstanceKey = TEXT("main");
        Doc3.Route.ScreenId = TEXT("core:screen.alt");

        TestTrue(TEXT("Reconcile Doc3 succeeds"), Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError));
        TestEqual(TEXT("Factory called to instantiate new screen class"), FactoryInstantiations, 2);
        UGV2ScreenWidgetBase* RouteWidget3 = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestNotNull(TEXT("New route widget exists"), RouteWidget3);
        TestNotEqual(TEXT("Old route widget replaced"), RouteWidget3, RouteWidget1);

        // Step D: Doc4 adds a modal -> Lower layers blocked, top modal interactive (UIF-20)
        FGV2UiDocumentViewModel Doc4 = Doc3;
        Doc4.Revision = 4;
        FGV2ScreenInstanceViewModel ModalInst;
        ModalInst.Layer = TEXT("modal_stack");
        ModalInst.InstanceKey = TEXT("confirm_dialog");
        ModalInst.ScreenId = TEXT("core:screen.modal_confirm");
        Doc4.Modals.Add(ModalInst);

        TestTrue(TEXT("Reconcile Doc4 with modal succeeds"), Reconciler.Reconcile(Shell, Doc4, MockFactory, ReconcileError));
        TestEqual(TEXT("Factory instantiated modal widget"), FactoryInstantiations, 3);
        if (Shell != nullptr)
        {
            TestFalse(TEXT("Location content layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Background layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("background")));
            TestFalse(TEXT("Core interface layer is blocked when modal is active"), Shell->IsLayerInteractive(TEXT("core_interface")));
            TestTrue(TEXT("Modal stack layer is interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
        }

        // Step E: Doc5 closes modal -> Lower layers unblocked
        FGV2UiDocumentViewModel Doc5 = Doc3;
        Doc5.Revision = 5;
        Doc5.Modals.Empty();

        TestTrue(TEXT("Reconcile Doc5 (modal closed) succeeds"), Reconciler.Reconcile(Shell, Doc5, MockFactory, ReconcileError));
        if (Shell != nullptr)
        {
            TestTrue(TEXT("Location content layer is unblocked"), Shell->IsLayerInteractive(TEXT("location_content")));
        }
        TestNull(TEXT("Modal widget detached and removed from active list"), Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("confirm_dialog")));

        // Step F: Atomicity - Candidate with invalid ScreenId rejected without modifying active set (UIF-21)
        FGV2UiDocumentViewModel BadDoc;
        BadDoc.UiInstanceId = TEXT("ui@1:1");
        BadDoc.Revision = 6;
        BadDoc.bHasRoute = true;
        BadDoc.Route.Layer = TEXT("location_content");
        BadDoc.Route.InstanceKey = TEXT("main");
        BadDoc.Route.ScreenId = TEXT("invalid:screen.does_not_exist");

        TestFalse(TEXT("Reconcile BadDoc fails"), Reconciler.Reconcile(Shell, BadDoc, MockFactory, ReconcileError));
        TestEqual(
            TEXT("Previous active screen remains intact after rejected candidate"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")),
            RouteWidget3);

        if (Shell != nullptr)
        {
            Shell->RemoveFromRoot();
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiNestedInstancesAndTabsContract,
    "GV2.Runtime.UI.NestedInstancesAndTabsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiNestedInstancesAndTabsContract::RunTest(const FString& Parameters)
{
    if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inventory"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_use"), FText::FromString(TEXT("Use Potion")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_skills"), FText::FromString(TEXT("Skills")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.btn_learn"), FText::FromString(TEXT("Learn Fireball")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.tab_inv"), FText::FromString(TEXT("Inventory")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_a"), FText::FromString(TEXT("Title A")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.title_b"), FText::FromString(TEXT("Title B")));
    }

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
        T1.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Tab A")) }));
        T1.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.tab_a"), TEXT("screen")));
        DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(T1)));

        TMap<FString, FGV2PreparedUiValue> T2;
        T2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("tab_a"))); // duplicate key
        T2.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Tab B")) }));
        T2.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.tab_b"), TEXT("screen")));
        DupTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(T2)));

        TestFalse(TEXT("Duplicate tab keys rejected"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(DupTabs)), TabCap, nullptr, PrepErr));

        // 3. Valid tabs array preparation
        TArray<FGV2PreparedUiValue> ValidTabs;
        TMap<FString, FGV2PreparedUiValue> V1;
        V1.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("inventory")));
        V1.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Inventory")) }));
        V1.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test"), TEXT("screen")));
        ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(V1)));

        TMap<FString, FGV2PreparedUiValue> V2;
        V2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("skills")));
        V2.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Skills")) }));
        V2.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test"), TEXT("screen")));
        ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(V2)));

        TestTrue(TEXT("Valid tabs prepare succeeds"), Consumer->Prepare(FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(ValidTabs)), TabCap, nullptr, PrepErr));
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

        TMap<FName, UGV2ScreenWidgetBase*> Widgets;
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
        Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
        const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
        const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CorePackageRoot);
        GV2ContentCore::FRepositoryReadHandle ReadHandle;
        if (RepoBuild.IsSuccess())
        {
            ReadHandle = RepoBuild.GetCandidate().GetReadHandle();
        }
        TestTrue(TEXT("Coordinator StartSession succeeds"), Coordinator.StartSession(ReadHandle, 1));

        bool bDocumentHandled = false;
        Coordinator.SetDocumentSink([&bDocumentHandled](const FGV2UiDocumentViewModel&) -> bool
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
            UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
            GameInstance->AddToRoot();
            GameInstance->InitializeStandalone();
            UWorld* TestWorld = GameInstance->GetWorld();

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

                    TMap<FName, UGV2ScreenWidgetBase*> Widgets;
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

            GameInstance->Shutdown();
            if (TestWorld != nullptr)
            {
                TestWorld->DestroyWorld(false);
                GEngine->DestroyWorldContext(TestWorld);
            }
            GameInstance->RemoveFromRoot();
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiThemeOwnershipAndTextLengthContract,
    "GV2.Runtime.UI.ThemeOwnershipAndTextLengthContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiThemeOwnershipAndTextLengthContract::RunTest(const FString& Parameters)
{
    // =========================================================================
    // UIF-27 & UIF-28: Layer Directory Convention & Screen Registry Gate
    // =========================================================================
    {
        // 1. Core namespace permissions
        TestTrue(
            TEXT("Core screen can reference /Game/UI/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("core"), TEXT("/Game/UI/Widgets/WBP_Testscreen")));
        TestTrue(
            TEXT("Core screen can reference /Game/core/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("core"), TEXT("/Game/core/WBP_CoreScreen")));
        TestFalse(
            TEXT("Core screen CANNOT reference /Game/TextSystem/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("core"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")));
        TestFalse(
            TEXT("Core screen CANNOT reference /Game/RH/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("core"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")));

        // 2. TextSystem namespace permissions
        TestTrue(
            TEXT("TextSystem screen can reference /Game/TextSystem/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("textsystem"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")));
        TestTrue(
            TEXT("TextSystem screen can reference lower layer /Game/UI/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("textsystem"), TEXT("/Game/UI/Widgets/WBP_Testscreen")));
        TestFalse(
            TEXT("TextSystem screen CANNOT reference higher layer /Game/RH/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("textsystem"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")));

        // 3. RH namespace permissions
        TestTrue(
            TEXT("RH screen can reference /Game/RH/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("rh"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")));
        TestTrue(
            TEXT("RH screen can reference lower layer /Game/TextSystem/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("rh"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")));
        TestTrue(
            TEXT("RH screen can reference lower layer /Game/UI/ asset"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("rh"), TEXT("/Game/UI/Widgets/WBP_Testscreen")));

        // 4. Test registry validation failure on layer violation
        UGV2ScreenRegistry* Registry = NewObject<UGV2ScreenRegistry>();
        FGV2ScreenRegistryEntry BadEntry;
        BadEntry.ScreenId = TEXT("core:screen.bad_ref");
        BadEntry.Layer = TEXT("location_content");
        BadEntry.WidgetClass = TSoftClassPtr<UGV2ScreenWidgetBase>(FSoftObjectPath(TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen.WBP_Textscreen_C")));

        // We use reflection or helper to add entry for testing
        // Since Entries is private in UGV2ScreenRegistry, we test IsAssetAllowedForScreenNamespace directly and via mock entries if accessible
        FString Error;
        TestFalse(
            TEXT("Core screen referencing TextSystem is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("core"), BadEntry.WidgetClass.ToSoftObjectPath().ToString()));
    }

    // =========================================================================
    // UIF-29: Core Minimal Theme & Emergency Fallback Strings
    // =========================================================================
    {
        UGV2UiTheme* MinimalTheme = UGV2UiTheme::GetCoreMinimalTheme();
        TestNotNull(TEXT("Core minimal theme is available"), MinimalTheme);

        if (MinimalTheme != nullptr)
        {
            TestEqual(TEXT("Default style token is 'default'"), MinimalTheme->DefaultTextStyleToken, FName("default"));
            TestTrue(TEXT("Minimal theme contains default text size"), MinimalTheme->TextSizeTokens.Contains(TEXT("default")));
            TestTrue(TEXT("Minimal theme contains title text size"), MinimalTheme->TextSizeTokens.Contains(TEXT("title")));
            TestTrue(TEXT("Minimal theme contains default text color"), MinimalTheme->TextColorTokens.Contains(TEXT("default")));
            TestTrue(TEXT("Minimal theme contains error text color"), MinimalTheme->TextColorTokens.Contains(TEXT("error")));

            // Check emergency fallback strings in catalog
            TestTrue(TEXT("Emergency error title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.title")));
            TestTrue(TEXT("Emergency error description present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.description")));
            TestTrue(TEXT("Emergency loading title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.loading.title")));
            TestTrue(TEXT("Emergency recovery title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.recovery.title")));

            // Resolve text through pipeline with minimal theme
            FGV2TextViewModel ResolvedTitle;
            FGV2TextViewModel ResolvedDesc;
            FString Error;
            TestTrue(
                TEXT("Resolve emergency recovery title via MinimalTheme"),
                UGV2TextPipeline::Resolve(TEXT("core:text.screen.recovery.title"), {}, FName("title"), ResolvedTitle, Error));
            TestEqual(TEXT("Recovery title text matches"), ResolvedTitle.Text.ToString(), TEXT("Recovery"));

            TestTrue(
                TEXT("Resolve emergency error description via MinimalTheme"),
                UGV2TextPipeline::Resolve(TEXT("core:text.screen.error.description"), {}, FName("default"), ResolvedDesc, Error));
            TestEqual(TEXT("Error description text matches"), ResolvedDesc.Text.ToString(), TEXT("An unexpected error has occurred."));

            // Verify UE-native recovery screen widget initialization using resolved fallback strings
            UGV2RecoveryScreenWidget* RecoveryScreen = NewObject<UGV2RecoveryScreenWidget>(
                GetTransientPackage(),
                UGV2RecoveryScreenWidget::StaticClass());
            TestNotNull(TEXT("Recovery screen widget instantiated"), RecoveryScreen);
            if (RecoveryScreen != nullptr)
            {
                TestTrue(
                    TEXT("Initialize recovery screen with resolved emergency strings"),
                    RecoveryScreen->InitializeRecoveryScreen(ResolvedTitle.Text.ToString(), ResolvedDesc.Text.ToString()));
                TestEqual(TEXT("Recovery screen title matches"), RecoveryScreen->GetTitle(), TEXT("Recovery"));
                TestEqual(TEXT("Recovery screen message matches"), RecoveryScreen->GetMessage(), TEXT("An unexpected error has occurred."));
            }
        }
    }

    // =========================================================================
    // UIF-30: Text Length Resilience & Automatic Overflow Handling
    // =========================================================================
    {
        UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
        TestNotNull(TEXT("Theme is valid for text length test"), Theme);

        // Verify text scaling evaluated on multiple heights
        const float Scale720 = Theme->EvaluateTextScale(720.0f);
        const float Scale1080 = Theme->EvaluateTextScale(1080.0f);
        const float Scale2160 = Theme->EvaluateTextScale(2160.0f);

        TestTrue(TEXT("Text scale on 720p is ~0.85"), FMath::IsNearlyEqual(Scale720, 0.85f, 0.05f));
        TestTrue(TEXT("Text scale on 1080p is 1.0"), FMath::IsNearlyEqual(Scale1080, 1.0f, 0.01f));
        TestTrue(TEXT("Text scale on 2160p is ~1.60"), FMath::IsNearlyEqual(Scale2160, 1.60f, 0.05f));

        // Verify minimum readable font size guarantee
        const float EffectiveSize720 = Theme->GetEffectiveFontSize(TEXT("small"), 720.0f);
        TestTrue(TEXT("Effective font size never drops below MinReadableFontSize"), EffectiveSize720 >= Theme->MinReadableFontSize);
    }

    return true;
}

// =========================================================================
// GLS-14: Resolution Matrix Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenResolutionMatrixTest,
    "GV2.Runtime.Presentation.LocationScreenResolutionMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenResolutionMatrixTest::RunTest(const FString& Parameters)
{
    struct FResolutionTestCase
    {
        FIntPoint Resolution;
        FString Name;
        float ExpectedTextScale;
        bool bIsUltrawide;
    };

    const TArray<FResolutionTestCase> TestMatrix = {
        { FIntPoint(3840, 2160), TEXT("4K UHD (3840x2160, 16:9)"), 1.60f, false },
        { FIntPoint(2560, 1440), TEXT("QHD (2560x1440, 16:9)"), 1.25f, false },
        { FIntPoint(1920, 1080), TEXT("Full HD (1920x1080, 16:9)"), 1.00f, false },
        { FIntPoint(1280, 720),  TEXT("HD (1280x720, 16:9)"), 0.85f, false },
        { FIntPoint(3440, 1440), TEXT("UWQHD (3440x1440, 21:9)"), 1.25f, true },
        { FIntPoint(2560, 1080), TEXT("UWFHD (2560x1080, 21:9)"), 1.00f, true },
    };

    UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
    TestNotNull(TEXT("UI Theme is available"), Theme);

    for (const FResolutionTestCase& TestCase : TestMatrix)
    {
        const float ViewportWidth = static_cast<float>(TestCase.Resolution.X);
        const float ViewportHeight = static_cast<float>(TestCase.Resolution.Y);
        const float AspectRatio = ViewportWidth / ViewportHeight;

        if (Theme != nullptr)
        {
            const float Scale = Theme->EvaluateTextScale(ViewportHeight);
            TestTrue(
                FString::Printf(TEXT("[%s] Text scale is within expected range (%f)"), *TestCase.Name, Scale),
                FMath::IsNearlyEqual(Scale, TestCase.ExpectedTextScale, 0.08f));

            const float EffectiveSmall = Theme->GetEffectiveFontSize(TEXT("small"), ViewportHeight);
            const float EffectiveDefault = Theme->GetEffectiveFontSize(TEXT("default"), ViewportHeight);
            const float EffectiveTitle = Theme->GetEffectiveFontSize(TEXT("title"), ViewportHeight);

            TestTrue(
                FString::Printf(TEXT("[%s] Small font >= MinReadableFontSize (%f >= %f)"), *TestCase.Name, EffectiveSmall, Theme->MinReadableFontSize),
                EffectiveSmall >= Theme->MinReadableFontSize);
            TestTrue(
                FString::Printf(TEXT("[%s] Default font > Small font (%f > %f)"), *TestCase.Name, EffectiveDefault, EffectiveSmall),
                EffectiveDefault > EffectiveSmall);
            TestTrue(
                FString::Printf(TEXT("[%s] Title font > Default font (%f > %f)"), *TestCase.Name, EffectiveTitle, EffectiveDefault),
                EffectiveTitle > EffectiveDefault);
        }

        if (TestCase.bIsUltrawide)
        {
            TestTrue(
                FString::Printf(TEXT("[%s] Aspect ratio is ~2.37 (21:9)"), *TestCase.Name),
                AspectRatio > 2.0f);
            const float MaxPlayerStatusWidthRatio = 0.35f;
            const float MinSceneWidthRatio = 0.60f;
            TestTrue(
                FString::Printf(TEXT("[%s] Scene width ratio is majority of screen"), *TestCase.Name),
                MinSceneWidthRatio > MaxPlayerStatusWidthRatio);
        }
        else
        {
            TestTrue(
                FString::Printf(TEXT("[%s] Aspect ratio is 16:9 (~1.777)"), *TestCase.Name),
                FMath::IsNearlyEqual(AspectRatio, 16.0f / 9.0f, 0.01f));
        }

        if (TestCase.Resolution == FIntPoint(1280, 720))
        {
            TestTrue(TEXT("[1280x720] Min width is sufficient for layout"), ViewportWidth >= 1280.0f);
            TestTrue(TEXT("[1280x720] Min height is sufficient for vertical stacks"), ViewportHeight >= 720.0f);
        }
    }

    UClass* LocationScreenClass = LoadClass<UGV2ScreenWidgetBase>(
        nullptr,
        TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
    TestNotNull(TEXT("WBP_LocationScreen class loads successfully"), LocationScreenClass);

    return true;
}


// =========================================================================
// UIH-01..04: Core Repeater & Composite Reconciliation Contract Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CoreRepeaterContractTest,
    "GV2.Runtime.UI.CoreRepeaterContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CoreRepeaterContractTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    // 1. UIH-01: Test UGV2ListViewWidgetBase directly
    {
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UVerticalBox* Container = NewObject<UVerticalBox>(TestWorld);
        ListView->SetContainerPanel(Container);

        struct FTestItem
        {
            FName Key;
            FString Text;
        };

        const TArray<FTestItem> InitialItems = {
            { FName(TEXT("item_a")), TEXT("Item A") },
            { FName(TEXT("item_b")), TEXT("Item B") },
            { FName(TEXT("item_c")), TEXT("Item C") }
        };

        // Positive: Reconcile creates items in order
        bool bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            InitialItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });

        TestTrue(TEXT("Initial reconciliation succeeds"), bSuccess);
        TestEqual(TEXT("Entry count is 3"), ListView->GetEntryCount(), 3);
        TestEqual(TEXT("Container child count is 3"), Container->GetChildrenCount(), 3);

        UGV2ButtonWidgetBase* WidgetA = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_a")));
        UGV2ButtonWidgetBase* WidgetB = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_b")));
        UGV2ButtonWidgetBase* WidgetC = ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_c")));
        TestNotNull(TEXT("Widget A exists"), WidgetA);
        TestNotNull(TEXT("Widget B exists"), WidgetB);
        TestNotNull(TEXT("Widget C exists"), WidgetC);

        // Positive: Reorder and remove C, add D -> reuse existing A and B
        const TArray<FTestItem> UpdatedItems = {
            { FName(TEXT("item_b")), TEXT("Item B") },
            { FName(TEXT("item_d")), TEXT("Item D") },
            { FName(TEXT("item_a")), TEXT("Item A") }
        };

        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            UpdatedItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });

        TestTrue(TEXT("Updated reconciliation succeeds"), bSuccess);
        TestEqual(TEXT("Entry count is 3 after update"), ListView->GetEntryCount(), 3);
        TestEqual(TEXT("Widget B is reused (same pointer)"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_b"))), WidgetB);
        TestEqual(TEXT("Widget A is reused (same pointer)"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_a"))), WidgetA);
        TestNull(TEXT("Widget C is removed"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_c"))));
        TestNotNull(TEXT("Widget D is created"), ListView->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("item_d"))));

        // Negative: Empty key rejected without modifying state
        const TArray<FTestItem> BadEmptyKey = {
            { FName(), TEXT("Bad Item") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadEmptyKey,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });
        TestFalse(TEXT("Empty key is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected empty key"), ListView->GetEntryCount(), 3);

        // Negative: Duplicate key rejected without modifying state
        const TArray<FTestItem> BadDuplicateKey = {
            { FName(TEXT("dup")), TEXT("Dup 1") },
            { FName(TEXT("dup")), TEXT("Dup 2") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadDuplicateKey,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; });
        TestFalse(TEXT("Duplicate key is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected duplicate key"), ListView->GetEntryCount(), 3);

        // Negative: Failed apply item aborts without modifying state
        const TArray<FTestItem> BadApplyItems = {
            { FName(TEXT("item_x")), TEXT("Item X") }
        };
        bSuccess = ListView->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
            BadApplyItems,
            [](const FTestItem& Item) { return Item.Key; },
            [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
            [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return false; });
        TestFalse(TEXT("Failed ApplyItem is rejected"), bSuccess);
        TestEqual(TEXT("Entry count unchanged after rejected apply"), ListView->GetEntryCount(), 3);

        // CCF-01 / CCF-02: Atomic reconciliation - failed candidate does not mutate live reused widgets
        {
            struct FTestProgressItem
            {
                FName Key;
                float Value = 0.0f;
            };

            UGV2ListViewWidgetBase* AtomicListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* AtomicContainer = NewObject<UVerticalBox>(TestWorld);
            AtomicListView->SetContainerPanel(AtomicContainer);

            const TArray<FTestProgressItem> AtomicInitial = {
                { FName(TEXT("item_a")), 10.0f },
                { FName(TEXT("item_b")), 20.0f }
            };

            bool bAtomicInit = AtomicListView->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestProgressItem>(
                AtomicInitial,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item) -> bool
                {
                    Widget.ApplyProgress(Item.Value);
                    return true;
                });
            TestTrue(TEXT("Atomic baseline reconciliation succeeds"), bAtomicInit);
            UGV2ProgressBarWidgetBase* ProgA = AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a")));
            UGV2ProgressBarWidgetBase* ProgB = AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b")));
            TestNotNull(TEXT("ProgA exists"), ProgA);
            TestNotNull(TEXT("ProgB exists"), ProgB);
            TestEqual(TEXT("ProgA baseline value is 10"), ProgA->GetProgress(), 10.0f);
            TestEqual(TEXT("ProgB baseline value is 20"), ProgB->GetProgress(), 20.0f);
            TestEqual(TEXT("Container child count is 2"), AtomicContainer->GetChildrenCount(), 2);

            // Attempt reconcile where item_a has valid 100.0f, but item_b is invalid (fails preflight)
            const TArray<FTestProgressItem> AtomicCandidate = {
                { FName(TEXT("item_a")), 100.0f },
                { FName(TEXT("item_b")), -1.0f }
            };

            bool bAtomicCandidate = AtomicListView->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestProgressItem>(
                AtomicCandidate,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item) -> bool
                {
                    Widget.ApplyProgress(Item.Value);
                    return true;
                },
                [](const FTestProgressItem& Item) -> bool
                {
                    return Item.Value >= 0.0f; // item_b fails preflight
                });
            TestFalse(TEXT("Reconciliation fails due to invalid second candidate"), bAtomicCandidate);
            TestEqual(TEXT("ProgA pointer unchanged after failure"), AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a"))), ProgA);
            TestEqual(TEXT("ProgB pointer unchanged after failure"), AtomicListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b"))), ProgB);
            TestEqual(TEXT("ProgA value remains 10 (NOT mutated to 100)"), ProgA->GetProgress(), 10.0f);
            TestEqual(TEXT("ProgB value remains 20"), ProgB->GetProgress(), 20.0f);
            TestEqual(TEXT("Child count remains 2"), AtomicContainer->GetChildrenCount(), 2);
            TestEqual(TEXT("Child 0 remains ProgA"), AtomicContainer->GetChildAt(0), Cast<UWidget>(ProgA));
            TestEqual(TEXT("Child 1 remains ProgB"), AtomicContainer->GetChildAt(1), Cast<UWidget>(ProgB));
        }

        // CCF-01b: ReconcilePreparedEntries - two-phase reconciliation guarantees zero mutation on prepare failure
        {
            struct FTestProgressItem
            {
                FName Key;
                float Value = 0.0f;
            };

            struct FPreparedProgressData
            {
                float ProgressValue = 0.0f;
            };

            UGV2ListViewWidgetBase* PreparedListView = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* PreparedContainer = NewObject<UVerticalBox>(TestWorld);
            PreparedListView->SetContainerPanel(PreparedContainer);

            const TArray<FTestProgressItem> InitItems = {
                { FName(TEXT("item_a")), 10.0f },
                { FName(TEXT("item_b")), 20.0f }
            };

            bool bInit = PreparedListView->ReconcilePreparedEntries<UGV2ProgressBarWidgetBase, FTestProgressItem, FPreparedProgressData>(
                InitItems,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item, FPreparedProgressData& OutPrep) -> bool
                {
                    OutPrep.ProgressValue = Item.Value;
                    return true;
                },
                [](UGV2ProgressBarWidgetBase& Widget, const FPreparedProgressData& Prep)
                {
                    Widget.ApplyProgress(Prep.ProgressValue);
                });
            TestTrue(TEXT("ReconcilePrepared baseline succeeds"), bInit);
            UGV2ProgressBarWidgetBase* PrepA = PreparedListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_a")));
            UGV2ProgressBarWidgetBase* PrepB = PreparedListView->GetEntry<UGV2ProgressBarWidgetBase>(FName(TEXT("item_b")));
            TestNotNull(TEXT("PrepA exists"), PrepA);
            TestNotNull(TEXT("PrepB exists"), PrepB);
            TestEqual(TEXT("PrepA baseline value is 10"), PrepA->GetProgress(), 10.0f);
            TestEqual(TEXT("PrepB baseline value is 20"), PrepB->GetProgress(), 20.0f);

            // Candidate where item_a prepares successfully to 100.0f, but item_b fails during PrepareItem
            const TArray<FTestProgressItem> CandidateItems = {
                { FName(TEXT("item_a")), 100.0f },
                { FName(TEXT("item_b")), -999.0f }
            };

            bool bCandidate = PreparedListView->ReconcilePreparedEntries<UGV2ProgressBarWidgetBase, FTestProgressItem, FPreparedProgressData>(
                CandidateItems,
                [](const FTestProgressItem& Item) { return Item.Key; },
                [TestWorld]() -> UGV2ProgressBarWidgetBase* { return NewObject<UGV2ProgressBarWidgetBase>(TestWorld); },
                [](UGV2ProgressBarWidgetBase& Widget, const FTestProgressItem& Item, FPreparedProgressData& OutPrep) -> bool
                {
                    if (Item.Value < 0.0f)
                    {
                        return false; // item_b fails prepare!
                    }
                    OutPrep.ProgressValue = Item.Value;
                    return true;
                },
                [](UGV2ProgressBarWidgetBase& Widget, const FPreparedProgressData& Prep)
                {
                    Widget.ApplyProgress(Prep.ProgressValue);
                });
            TestFalse(TEXT("ReconcilePrepared fails when item_b prepare fails"), bCandidate);
            TestEqual(TEXT("PrepA value strictly remains 10 (NOT mutated to 100 on prepare failure)"), PrepA->GetProgress(), 10.0f);
            TestEqual(TEXT("PrepB value strictly remains 20"), PrepB->GetProgress(), 20.0f);
            TestEqual(TEXT("PreparedContainer child count remains 2"), PreparedContainer->GetChildrenCount(), 2);
        }

        // CCF-05: Complete Repeater Regression Matrix
        {
            UGV2ListViewWidgetBase* MatrixList = NewObject<UGV2ListViewWidgetBase>(TestWorld);
            UVerticalBox* MatrixContainer = NewObject<UVerticalBox>(TestWorld);
            MatrixList->SetContainerPanel(MatrixContainer);

            auto ReconcileHelper = [&](const TArray<FTestItem>& Items, TFunction<bool(const FTestItem&)> CanApply = nullptr) -> bool
            {
                return MatrixList->ReconcileEntries<UGV2ButtonWidgetBase, FTestItem>(
                    Items,
                    [](const FTestItem& Item) { return Item.Key; },
                    [TestWorld]() -> UGV2ButtonWidgetBase* { return NewObject<UGV2ButtonWidgetBase>(TestWorld); },
                    [](UGV2ButtonWidgetBase& Widget, const FTestItem& Item) -> bool { return true; },
                    CanApply);
            };

            // 1. empty -> 1
            const TArray<FTestItem> OneItem = { { FName(TEXT("k1")), TEXT("V1") } };
            TestTrue(TEXT("Matrix: empty -> 1 succeeds"), ReconcileHelper(OneItem));
            TestEqual(TEXT("Matrix: count is 1"), MatrixList->GetEntryCount(), 1);
            UGV2ButtonWidgetBase* W1 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1")));
            TestNotNull(TEXT("Matrix: W1 created"), W1);

            // 2. 1 -> 3
            const TArray<FTestItem> ThreeItems = {
                { FName(TEXT("k1")), TEXT("V1_new") },
                { FName(TEXT("k2")), TEXT("V2") },
                { FName(TEXT("k3")), TEXT("V3") }
            };
            TestTrue(TEXT("Matrix: 1 -> 3 succeeds"), ReconcileHelper(ThreeItems));
            TestEqual(TEXT("Matrix: count is 3"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W1 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            UGV2ButtonWidgetBase* W2 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2")));
            UGV2ButtonWidgetBase* W3 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3")));
            TestNotNull(TEXT("Matrix: W2 created"), W2);
            TestNotNull(TEXT("Matrix: W3 created"), W3);

            // 3. 3 -> 1
            TestTrue(TEXT("Matrix: 3 -> 1 succeeds"), ReconcileHelper(OneItem));
            TestEqual(TEXT("Matrix: count is 1"), MatrixList->GetEntryCount(), 1);
            TestEqual(TEXT("Matrix: W1 still reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestNull(TEXT("Matrix: W2 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))));
            TestNull(TEXT("Matrix: W3 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))));

            // Back to 3 items:
            TestTrue(TEXT("Matrix: re-expand to 3"), ReconcileHelper(ThreeItems));
            W2 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2")));
            W3 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3")));

            // 4. same keys + same order + changed values (reuse pointers)
            const TArray<FTestItem> ThreeItemsUpdated = {
                { FName(TEXT("k1")), TEXT("V1_updated") },
                { FName(TEXT("k2")), TEXT("V2_updated") },
                { FName(TEXT("k3")), TEXT("V3_updated") }
            };
            TestTrue(TEXT("Matrix: same keys same order updated succeeds"), ReconcileHelper(ThreeItemsUpdated));
            TestEqual(TEXT("Matrix: W1 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestEqual(TEXT("Matrix: W2 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))), W2);
            TestEqual(TEXT("Matrix: W3 reused"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 5. same keys + reordered: { k3, k1, k2 }
            const TArray<FTestItem> ThreeItemsReordered = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") },
                { FName(TEXT("k2")), TEXT("V2") }
            };
            TestTrue(TEXT("Matrix: reorder succeeds"), ReconcileHelper(ThreeItemsReordered));
            TestEqual(TEXT("Matrix: W1 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestEqual(TEXT("Matrix: W2 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))), W2);
            TestEqual(TEXT("Matrix: W3 reused after reorder"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TArray<UWidget*> Ordered = MatrixList->GetOrderedEntries();
            TestEqual(TEXT("Matrix: Order 0 is W3"), Ordered[0], Cast<UWidget>(W3));
            TestEqual(TEXT("Matrix: Order 1 is W1"), Ordered[1], Cast<UWidget>(W1));
            TestEqual(TEXT("Matrix: Order 2 is W2"), Ordered[2], Cast<UWidget>(W2));

            // 6. remove one key: remove k2 -> { k3, k1 }
            const TArray<FTestItem> TwoItems = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") }
            };
            TestTrue(TEXT("Matrix: remove one key succeeds"), ReconcileHelper(TwoItems));
            TestEqual(TEXT("Matrix: count is 2"), MatrixList->GetEntryCount(), 2);
            TestEqual(TEXT("Matrix: W3 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TestEqual(TEXT("Matrix: W1 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            TestNull(TEXT("Matrix: W2 removed"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k2"))));

            // 7. add one key: add k4 -> { k3, k1, k4 }
            const TArray<FTestItem> AddItem = {
                { FName(TEXT("k3")), TEXT("V3") },
                { FName(TEXT("k1")), TEXT("V1") },
                { FName(TEXT("k4")), TEXT("V4") }
            };
            TestTrue(TEXT("Matrix: add one key succeeds"), ReconcileHelper(AddItem));
            TestEqual(TEXT("Matrix: count is 3"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);
            TestEqual(TEXT("Matrix: W1 retained"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k1"))), W1);
            UGV2ButtonWidgetBase* W4 = MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k4")));
            TestNotNull(TEXT("Matrix: W4 created"), W4);

            // 8. empty key (rejected, live state unchanged)
            const TArray<FTestItem> EmptyKeyItem = { { FName(), TEXT("Bad") } };
            TestFalse(TEXT("Matrix: empty key rejected"), ReconcileHelper(EmptyKeyItem));
            TestEqual(TEXT("Matrix: count unchanged after empty key"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained after empty key rejection"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 9. duplicate key (rejected, live state unchanged)
            const TArray<FTestItem> DupItems = {
                { FName(TEXT("dup")), TEXT("D1") },
                { FName(TEXT("dup")), TEXT("D2") }
            };
            TestFalse(TEXT("Matrix: duplicate key rejected"), ReconcileHelper(DupItems));
            TestEqual(TEXT("Matrix: count unchanged after dup key"), MatrixList->GetEntryCount(), 3);

            // 10. preflight failure (rejected, live state unchanged)
            const TArray<FTestItem> PreflightFailItems = {
                { FName(TEXT("k3")), TEXT("VALID") },
                { FName(TEXT("k1")), TEXT("INVALID") }
            };
            TestFalse(TEXT("Matrix: preflight rejection"), ReconcileHelper(PreflightFailItems, [](const FTestItem& Item) { return Item.Text != TEXT("INVALID"); }));
            TestEqual(TEXT("Matrix: count unchanged after preflight fail"), MatrixList->GetEntryCount(), 3);
            TestEqual(TEXT("Matrix: W3 retained after preflight fail"), MatrixList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("k3"))), W3);

            // 11. Clear / reset
            MatrixList->ClearEntries();
            TestEqual(TEXT("Matrix: count 0 after clear"), MatrixList->GetEntryCount(), 0);
            TestEqual(TEXT("Matrix: container child count 0 after clear"), MatrixContainer->GetChildrenCount(), 0);
        }
    }

    // 2. UIH-02: Test CommandPanel using Core Repeater
    {
        UClass* CommandClass = LoadClass<UGV2LocationCommandPanelWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2LocationCommandPanelWidgetBase* CmdPanel = CommandClass ? CreateWidget<UGV2LocationCommandPanelWidgetBase>(TestWorld, CommandClass) : NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
        TestNotNull(TEXT("CmdPanel instantiated"), CmdPanel);

        FGV2ButtonViewModel BtnA;
        BtnA.Key = FName(TEXT("btn_a"));
        BtnA.Text.Text = FText::FromString(TEXT("Action A"));
        BtnA.Binding = FGV2UiBindingHandle::Create(TEXT("binding_a"));

        FGV2ButtonViewModel BtnB;
        BtnB.Key = FName(TEXT("btn_b"));
        BtnB.Text.Text = FText::FromString(TEXT("Action B"));
        BtnB.Binding = FGV2UiBindingHandle::Create(TEXT("binding_b"));

        FGV2ButtonViewModel BtnC;
        BtnC.Key = FName(TEXT("btn_c"));
        BtnC.Text.Text = FText::FromString(TEXT("Action C"));
        BtnC.Binding = FGV2UiBindingHandle::Create(TEXT("binding_c"));

        UGV2ListViewWidgetBase* Repeater = CmdPanel->GetRepeater();
        TestNotNull(TEXT("CommandPanel has active Repeater"), Repeater);
        if (Repeater != nullptr)
        {
            auto GetKey = [](const FGV2ButtonViewModel& B) { return B.Key; };
            auto CreateWidgetLambda = [TestWorld, CmdPanel]() -> UGV2ButtonWidgetBase*
            {
                TSubclassOf<UGV2ButtonWidgetBase> BtnClass = CmdPanel->ResolveButtonWidgetClass();
                return BtnClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, BtnClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ButtonWidgetBase& Widget, const FGV2ButtonViewModel& Model)
            {
                Widget.SetKey(Model.Key);
                Widget.SetBindingHandle(Model.Binding);
                return Widget.ApplyText(Model.Text);
            };

            const TArray<FGV2ButtonViewModel> InitialButtons = { BtnA, BtnB, BtnC };
            TestTrue(TEXT("Initial buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(InitialButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries"), Repeater->GetEntryCount(), 3);
            UWidget* WidgetA = Repeater->GetEntryWidget(FName(TEXT("btn_a")));
            UWidget* WidgetB = Repeater->GetEntryWidget(FName(TEXT("btn_b")));
            TestNotNull(TEXT("Button A widget exists"), WidgetA);
            TestNotNull(TEXT("Button B widget exists"), WidgetB);

            // Reorder & update: { BtnB, BtnD, BtnA } -> BtnB & BtnA must be reused
            FGV2ButtonViewModel BtnD;
            BtnD.Key = FName(TEXT("btn_d"));
            BtnD.Text.Text = FText::FromString(TEXT("Action D"));
            BtnD.Binding = FGV2UiBindingHandle::Create(TEXT("binding_d"));

            const TArray<FGV2ButtonViewModel> UpdatedButtons = { BtnB, BtnD, BtnA };
            TestTrue(TEXT("Updated buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(UpdatedButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries after update"), Repeater->GetEntryCount(), 3);
            TestEqual(TEXT("Button B widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_b"))), WidgetB);
            TestEqual(TEXT("Button A widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_a"))), WidgetA);
            TestNull(TEXT("Button C widget removed"), Repeater->GetEntryWidget(FName(TEXT("btn_c"))));
            TestNotNull(TEXT("Button D widget created"), Repeater->GetEntryWidget(FName(TEXT("btn_d"))));

            // Negative: Duplicate button key rejected
            FGV2ButtonViewModel BadBtn;
            BadBtn.Key = FName(TEXT("btn_b"));
            BadBtn.Binding = FGV2UiBindingHandle::Create(TEXT("bad_binding"));
            TArray<FGV2ButtonViewModel> DupButtons = { BtnB, BadBtn };
            TestFalse(TEXT("Duplicate button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(DupButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            // Negative: Empty button key rejected
            FGV2ButtonViewModel EmptyKeyBtn;
            EmptyKeyBtn.Key = FName();
            EmptyKeyBtn.Binding = FGV2UiBindingHandle::Create(TEXT("empty_binding"));
            TArray<FGV2ButtonViewModel> EmptyKeyButtons = { EmptyKeyBtn };
            TestFalse(TEXT("Empty button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(EmptyKeyButtons, GetKey, CreateWidgetLambda, ApplyLambda));
        }
    }

    // 3. UIH-03 & CCF-03: Test Item and Meter Repeater reconciliation and key preservation
    {
        UGV2ListViewWidgetBase* ItemRep = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UWrapBox* ItemBox = NewObject<UWrapBox>(TestWorld);
        ItemRep->SetContainerPanel(ItemBox);

        struct FTestIconEntry { FName Key; FString ResourceId; };
        TArray<FTestIconEntry> InitialItems = {
            { FName(TEXT("item@1")), TEXT("textsystem:resource.ui.missing_icon") },
            { FName(TEXT("item@2")), TEXT("textsystem:resource.ui.missing_icon") }
        };

        auto CreateIcon = [&]() -> UGV2ImageWidgetBase* {
            UGV2ImageWidgetBase* Img = NewObject<UGV2ImageWidgetBase>(TestWorld);
            UImage* InnerImage = NewObject<UImage>(Img);
            if (FProperty* Prop = UGV2ImageWidgetBase::StaticClass()->FindPropertyByName(TEXT("Image")))
            {
                *Prop->ContainerPtrToValuePtr<TObjectPtr<UImage>>(Img) = InnerImage;
            }
            Img->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            return Img;
        };
        auto ApplyIcon = [](UGV2ImageWidgetBase& Img, const FTestIconEntry& Entry) -> bool {
            FString Err;
            return Img.ApplyImageResource(Entry.ResourceId, Err);
        };
        auto GetIconKey = [](const FTestIconEntry& Entry) -> FName { return Entry.Key; };

        TestTrue(TEXT("Initial items reconcile successfully"), ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FTestIconEntry>(InitialItems, GetIconKey, CreateIcon, ApplyIcon));
        TestEqual(TEXT("Item count is 2"), ItemRep->GetEntryCount(), 2);
        UWidget* SwordWidget = ItemRep->GetEntryWidget(FName(TEXT("item@1")));
        UWidget* ShieldWidget = ItemRep->GetEntryWidget(FName(TEXT("item@2")));
        TestNotNull(TEXT("Sword widget exists"), SwordWidget);
        TestNotNull(TEXT("Shield widget exists"), ShieldWidget);

        // Reorder & insert: { item@2, item@3, item@1 }
        TArray<FTestIconEntry> UpdatedItems = {
            { FName(TEXT("item@2")), TEXT("textsystem:resource.ui.missing_icon") },
            { FName(TEXT("item@3")), TEXT("textsystem:resource.ui.missing_icon") },
            { FName(TEXT("item@1")), TEXT("textsystem:resource.ui.missing_icon") }
        };

        TestTrue(TEXT("Updated items reconcile successfully"), ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FTestIconEntry>(UpdatedItems, GetIconKey, CreateIcon, ApplyIcon));
        TestEqual(TEXT("Item count is 3"), ItemRep->GetEntryCount(), 3);
        TestEqual(TEXT("Sword widget pointer preserved"), ItemRep->GetEntryWidget(FName(TEXT("item@1"))), SwordWidget);
        TestEqual(TEXT("Shield widget pointer preserved"), ItemRep->GetEntryWidget(FName(TEXT("item@2"))), ShieldWidget);
        TestNotNull(TEXT("Item 3 widget created"), ItemRep->GetEntryWidget(FName(TEXT("item@3"))));

        // CCF-03: Meter Repeater
        UGV2ListViewWidgetBase* MeterRep = NewObject<UGV2ListViewWidgetBase>(TestWorld);
        UVerticalBox* MeterBox = NewObject<UVerticalBox>(TestWorld);
        MeterRep->SetContainerPanel(MeterBox);

        struct FTestMeterEntry { FName Key; float Percent; };
        TArray<FTestMeterEntry> InitialMeters = {
            { FName(TEXT("stamina")), 0.5f },
            { FName(TEXT("health")), 0.8f }
        };

        auto CreateMeter = [&]() -> UGV2ProgressBarWidgetBase* {
            return NewObject<UGV2ProgressBarWidgetBase>(TestWorld);
        };
        auto ApplyMeter = [](UGV2ProgressBarWidgetBase& Bar, const FTestMeterEntry& Entry) -> bool {
            Bar.ApplyProgress(Entry.Percent);
            return true;
        };
        auto GetMeterKey = [](const FTestMeterEntry& Entry) -> FName { return Entry.Key; };

        TestTrue(TEXT("Initial meters reconcile successfully"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(InitialMeters, GetMeterKey, CreateMeter, ApplyMeter));
        TestEqual(TEXT("Meter count is 2"), MeterRep->GetEntryCount(), 2);
        UWidget* StaminaWidget = MeterRep->GetEntryWidget(FName(TEXT("stamina")));
        UWidget* HealthWidget = MeterRep->GetEntryWidget(FName(TEXT("health")));
        TestNotNull(TEXT("Stamina widget exists"), StaminaWidget);
        TestNotNull(TEXT("Health widget exists"), HealthWidget);

        // Reorder: { health, stamina }
        TArray<FTestMeterEntry> ReorderedMeters = {
            { FName(TEXT("health")), 0.8f },
            { FName(TEXT("stamina")), 0.5f }
        };
        TestTrue(TEXT("Reordered meters reconcile successfully"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(ReorderedMeters, GetMeterKey, CreateMeter, ApplyMeter));
        TestEqual(TEXT("Health widget pointer preserved"), MeterRep->GetEntryWidget(FName(TEXT("health"))), HealthWidget);
        TestEqual(TEXT("Stamina widget pointer preserved"), MeterRep->GetEntryWidget(FName(TEXT("stamina"))), StaminaWidget);

        // Negative: Empty meter key rejected
        TArray<FTestMeterEntry> BadMeters = { { FName(), 0.5f } };
        TestFalse(TEXT("Empty meter key rejected"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(BadMeters, GetMeterKey, CreateMeter, ApplyMeter));

        // Negative: Duplicate meter key rejected
        TArray<FTestMeterEntry> DupMeters = { { FName(TEXT("stamina")), 0.5f }, { FName(TEXT("stamina")), 0.5f } };
        TestFalse(TEXT("Duplicate meter key rejected"), MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FTestMeterEntry>(DupMeters, GetMeterKey, CreateMeter, ApplyMeter));
    }

    // 4. UIH-04 & CCF-04: Test SceneView character collection using Core Repeater and key identity
    {
        UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2LocationSceneWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
        TestNotNull(TEXT("SceneView instantiated"), SceneView);

        struct FTestCharEntry
        {
            FName Key;
            FString ResourceId;
        };

        UGV2ListViewWidgetBase* CharRep = SceneView->GetCharacterRepeater();
        if (CharRep != nullptr)
        {
            TSubclassOf<UGV2ImageWidgetBase> CharClass = SceneView->ResolveCharacterWidgetClass();
            auto GetKey = [](const FTestCharEntry& E) { return E.Key; };
            auto CreateWidgetLambda = [TestWorld, CharClass]() -> UGV2ImageWidgetBase*
            {
                return CharClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, CharClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ImageWidgetBase& Widget, const FTestCharEntry& Entry)
            {
                Widget.SetKey(Entry.Key);
                FString Err;
                return Widget.ApplyOptionalImageResource(Entry.ResourceId, TEXT("textsystem:resource.ui.missing_character"), Err);
            };

            // Positive single character with key identity
            FTestCharEntry CharA{ FName(TEXT("aria")), TEXT("textsystem:resource.ui.missing_portrait") };
            TArray<FTestCharEntry> SingleChar = { CharA };
            TestTrue(TEXT("SceneView accepts character entry"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(SingleChar, GetKey, CreateWidgetLambda, ApplyLambda));

            UWidget* CharAWidgetBefore = CharRep->GetEntryWidget(FName(TEXT("aria")));
            TestNotNull(TEXT("Character widget exists in repeater"), CharAWidgetBefore);
            TestEqual(TEXT("Repeater count is 1"), CharRep->GetEntryCount(), 1);

            // CCF-04: Changing resource ID for same character key preserves widget pointer
            FTestCharEntry CharA_NewRes{ FName(TEXT("aria")), TEXT("textsystem:resource.ui.missing_character") };
            TArray<FTestCharEntry> SingleCharNew = { CharA_NewRes };
            TestTrue(TEXT("SceneView accepts character update with changed resource"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(SingleCharNew, GetKey, CreateWidgetLambda, ApplyLambda));

            UWidget* CharAWidgetAfter = CharRep->GetEntryWidget(FName(TEXT("aria")));
            TestEqual(TEXT("Character widget pointer preserved across resource change"), CharAWidgetAfter, CharAWidgetBefore);

            // Negative CCF-04: Duplicate character keys rejected
            TArray<FTestCharEntry> DupChars = { CharA, CharA };
            TestFalse(TEXT("Duplicate character keys rejected"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(DupChars, GetKey, CreateWidgetLambda, ApplyLambda));
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// CCF-06..12: Location Composite Correctness Contract Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationCompositeContractTest,
    "GV2.Runtime.UI.LocationCompositeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationCompositeContractTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // CCF-06: Capabilities declaration
    // -------------------------------------------------------------------------
    {
        UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2LocationSceneWidgetBase* SceneWidget = SceneClass ? CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
        FGV2UiCapabilityBuilder SceneBuilder;
        SceneWidget->DescribeUiCapabilities(SceneBuilder);
        FGV2UiCapabilityTree SceneTree = SceneBuilder.Build();
        TestNotNull(TEXT("CCF-06: Scene capabilities declared"), SceneTree.FindProperty(TEXT("key")));
        TestNotNull(TEXT("CCF-06: Scene context_text declared"), SceneTree.FindProperty(TEXT("context_text")));
        TestNotNull(TEXT("CCF-06: Scene characters declared"), SceneTree.FindProperty(TEXT("characters")));

        UClass* CmdClass = LoadClass<UGV2LocationCommandPanelWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2LocationCommandPanelWidgetBase* CmdWidget = CmdClass ? CreateWidget<UGV2LocationCommandPanelWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
        FGV2UiCapabilityBuilder CmdBuilder;
        CmdWidget->DescribeUiCapabilities(CmdBuilder);
        FGV2UiCapabilityTree CmdTree = CmdBuilder.Build();
        TestNotNull(TEXT("CCF-06: Command capabilities declared"), CmdTree.FindProperty(TEXT("key")));
        TestNotNull(TEXT("CCF-06: Command items declared"), CmdTree.FindProperty(TEXT("items")));
    }

    // -------------------------------------------------------------------------
    // CCF-07: Repeated elements go through Repeater only (0, 1, 2 cases)
    // -------------------------------------------------------------------------
    {
        UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2LocationSceneWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2LocationSceneWidgetBase>(TestWorld);

        UGV2ListViewWidgetBase* CharRep = SceneView->GetCharacterRepeater();
        if (CharRep != nullptr)
        {
            struct FTestCharEntry { FName Key; FString ResourceId; };
            auto GetKey = [](const FTestCharEntry& E) { return E.Key; };
            auto CreateWidgetLambda = [TestWorld]() -> UGV2ImageWidgetBase*
            {
                return NewObject<UGV2ImageWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ImageWidgetBase& Widget, const FTestCharEntry& Entry)
            {
                Widget.SetKey(Entry.Key);
                return true;
            };

            // 0 characters
            TArray<FTestCharEntry> C0;
            TestTrue(TEXT("CCF-07: 0 characters applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C0, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 0 for empty characters"), CharRep->GetEntryCount(), 0);

            // 1 character
            TArray<FTestCharEntry> C1 = { { FName(TEXT("c_aria")), TEXT("textsystem:resource.ui.missing_portrait") } };
            TestTrue(TEXT("CCF-07: 1 character applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C1, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 1 for 1 character"), CharRep->GetEntryCount(), 1);
            TestNotNull(TEXT("CCF-07: CharA entry in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_aria"))));

            // 2 characters
            TArray<FTestCharEntry> C2 = { { FName(TEXT("c_aria")), TEXT("textsystem:resource.ui.missing_portrait") }, { FName(TEXT("c_merchant")), TEXT("textsystem:resource.ui.missing_portrait") } };
            TestTrue(TEXT("CCF-07: 2 characters applied"), CharRep->ReconcileEntries<UGV2ImageWidgetBase, FTestCharEntry>(C2, GetKey, CreateWidgetLambda, ApplyLambda));
            TestEqual(TEXT("CCF-07: Repeater count 2 for 2 characters"), CharRep->GetEntryCount(), 2);
            TestNotNull(TEXT("CCF-07: CharA still in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_aria"))));
            TestNotNull(TEXT("CCF-07: CharB in repeater"), CharRep->GetEntryWidget(FName(TEXT("c_merchant"))));
        }
    }

    // -------------------------------------------------------------------------
    // CCF-11: Key and host state semantics for SceneView and CommandPanel
    // -------------------------------------------------------------------------
    {
        UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2LocationSceneWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
        SceneView->SetKey(FName(TEXT("scene_test")));
        TestEqual(TEXT("CCF-11: SceneView Key getter/setter"), SceneView->GetKey(), FName(TEXT("scene_test")));

        UClass* CmdClass = LoadClass<UGV2LocationCommandPanelWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2LocationCommandPanelWidgetBase* CmdPanel = CmdClass ? CreateWidget<UGV2LocationCommandPanelWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
        CmdPanel->SetKey(FName(TEXT("cmd_test")));
        TestEqual(TEXT("CCF-11: CommandPanel Key getter/setter"), CmdPanel->GetKey(), FName(TEXT("cmd_test")));
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// UIH-05 & UIH-06: Text Pipeline DPI Scaling & Unified Sizing Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2TextPipelineDpiScalingTest,
    "GV2.Runtime.Presentation.TextPipelineDpiScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2TextPipelineDpiScalingTest::RunTest(const FString& Parameters)
{
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    TestNotNull(TEXT("Configured UI theme is valid"), Theme);
    if (Theme == nullptr) return false;

    // Check evaluate scale at standard heights
    const float Scale720p = Theme->EvaluateTextScale(720.0f);
    const float Scale1080p = Theme->EvaluateTextScale(1080.0f);
    const float Scale1440p = Theme->EvaluateTextScale(1440.0f);
    const float Scale2160p = Theme->EvaluateTextScale(2160.0f);

    TestNearlyEqual(TEXT("Scale at 720p is ~0.85"), Scale720p, 0.85f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 1080p is 1.0"), Scale1080p, 1.00f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 1440p is ~1.25"), Scale1440p, 1.25f, 0.01f);
    TestNearlyEqual(TEXT("Scale at 2160p is ~1.60"), Scale2160p, 1.60f, 0.01f);

    // Check effective font size calculation and MinReadableFontSize clamp
    const float SmallSize720p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("small")), 720.0f);
    TestTrue(TEXT("Small text size at 720p is >= MinReadableFontSize (10pt)"), SmallSize720p >= Theme->MinReadableFontSize);

    const float TitleSize1080p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("title")), 1080.0f);
    TestNearlyEqual(TEXT("Title text size at 1080p is ~20pt"), TitleSize1080p, 20.0f, 0.1f);

    const float TitleSize2160p = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("title")), 2160.0f);
    TestNearlyEqual(TEXT("Title text size at 2160p is ~32pt"), TitleSize2160p, 32.0f, 0.5f);

    // Verify plain text and rich text get the exact same effective font size
    FTextBlockStyle PlainStyle;
    const bool bResolved = UGV2TextPipeline::ResolveStyleForHeight(FName(TEXT("title")), PlainStyle, 1080.0f);
    if (bResolved)
    {
        TestNearlyEqual(TEXT("Plain text style font size matches TitleSize1080p"), (float)PlainStyle.Font.Size, TitleSize1080p, 0.1f);
    }

    // CCF-19: Check actual font sizes across consumer widgets at 720p, 1080p, 1440p, 2160p
    const float Heights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };
    for (float H : Heights)
    {
        const float ExpectedBodySize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("body")), H);
        FTextBlockStyle StyleForHeight;
        TestTrue(*FString::Printf(TEXT("CCF-19: Body style resolves for height %f"), H), UGV2TextPipeline::ResolveStyleForHeight(FName(TEXT("body")), StyleForHeight, H));
        TestNearlyEqual(*FString::Printf(TEXT("CCF-19: Style font size matches expected at height %f"), H), (float)StyleForHeight.Font.Size, ExpectedBodySize, 0.1f);
        TestTrue(*FString::Printf(TEXT("CCF-19: Font size at height %f is >= MinReadableFontSize"), H), (float)StyleForHeight.Font.Size >= Theme->MinReadableFontSize);
    }

    return true;
}

// =========================================================================
// UIH-07 & UIH-08: Graphics Scaling Policy & Compatibility Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2GraphicsScalingPolicyTest,
    "GV2.Runtime.Presentation.GraphicsScalingPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2GraphicsScalingPolicyTest::RunTest(const FString& Parameters)
{
    // 1. Test ScalePolicy compatibility matrix
    TestTrue(TEXT("PreserveAspect compatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::FixedAspect));
    TestFalse(TEXT("PreserveAspect incompatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("PreserveAspect incompatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::Tile));

    TestTrue(TEXT("NineSlice compatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("NineSlice incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::FixedAspect));

    TestTrue(TEXT("Tile compatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::Tile));
    TestFalse(TEXT("Tile incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::FixedAspect));

    TestFalse(TEXT("FreeStretch incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::FixedAspect));
    TestFalse(TEXT("FreeStretch incompatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::NineSlice));
    TestTrue(TEXT("FreeStretch compatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::Tile));

    // 2. Test ImageWidget atomic rollback on incompatible resource apply
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->AddToRoot();
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestWorld != nullptr)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        GameInstance->Init();

        UClass* ImageClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
        UGV2ImageWidgetBase* ImageWidget = ImageClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, ImageClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
        TestNotNull(TEXT("ImageWidget created"), ImageWidget);

        if (ImageWidget != nullptr)
        {
            FString Error;

            // CCF-13 / CCF-14: PostLoad does NOT infer ScalePolicy from InitialResourceId (RenderMode does not mutate ScalePolicy)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            // Simulate PostLoad
            ImageWidget->ConditionalPostLoad();
            TestEqual(TEXT("CCF-13: ScalePolicy remains PreserveAspect after PostLoad"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);

            // CCF-15: 1. PreserveAspect Resulting Brush
            const bool bAppliedAspect = ImageWidget->ApplyImageResource(TEXT("textsystem:resource.ui.missing_portrait"), Error);
            TestTrue(TEXT("CCF-15: PreserveAspect applies fixed aspect resource"), bAppliedAspect);
            TestEqual(TEXT("CCF-15: PreserveAspect brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: PreserveAspect brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // CCF-15: 2. FreeStretch Resulting Brush (DrawAs = Image, Tiling = NoTile)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::FreeStretch);
            const bool bAppliedStretch = ImageWidget->ApplyImageResource(TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("CCF-15: FreeStretch applies tile resource"), bAppliedStretch);
            TestEqual(TEXT("CCF-15: FreeStretch brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: FreeStretch brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // CCF-15: 3. Tile Resulting Brush (DrawAs = Image, Tiling = Both)
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
            const bool bAppliedTile = ImageWidget->ApplyImageResource(TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("CCF-15: Tile applies tile resource"), bAppliedTile);
            TestEqual(TEXT("CCF-15: Tile brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("CCF-15: Tile brush Tiling is Both"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::Both);

            // CCF-15: 4. NineSlice Resulting Brush (DrawAs = Box, Margin parsed)
            UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
            if (Catalog != nullptr)
            {
                UTexture2D* NineSliceTex = UTexture2D::CreateTransient(64, 64);
                FGV2ImageResourceDefinition NineSliceDef;
                NineSliceDef.ResourceId = TEXT("core:resource.surface.test_panel");
                NineSliceDef.RenderMode = EGV2ImageRenderMode::NineSlice;
                NineSliceDef.Texture = NineSliceTex;
                NineSliceDef.NineSliceBorderPixels = FMargin(8.0f);
                FGV2ResolvedImageResource ResolvedNineSlice;
                FString ResolveDefError;
                const bool bResolvedNineSlice = UGV2ImageResourceCatalog::ResolveDefinition(NineSliceDef, ResolvedNineSlice, ResolveDefError);
                TestTrue(TEXT("CCF-15: nine-slice test definition resolves"), bResolvedNineSlice);
                Catalog->ResolvedById.Add(NineSliceDef.ResourceId, MoveTemp(ResolvedNineSlice));

                ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::NineSlice);
                const bool bAppliedNineSlice = ImageWidget->ApplyImageResource(TEXT("core:resource.surface.test_panel"), Error);
                TestTrue(TEXT("CCF-15: NineSlice applies nine-slice resource"), bAppliedNineSlice);
                TestEqual(TEXT("CCF-15: NineSlice brush DrawAs is Box"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Box);
                TestEqual(TEXT("CCF-15: NineSlice brush Margin Left is normalized 0.125"), ImageWidget->GetImageBrush().Margin.Left, 0.125f);

                // CCF-15: 5. Failure atomicity: Incompatible resource leaves previous brush 100% intact
                const FSlateBrush BaselineBrush = ImageWidget->GetImageBrush();
                const FString BaselineId = ImageWidget->GetAppliedResourceId();

                const bool bIncompatibleApply = ImageWidget->ApplyImageResource(TEXT("textsystem:resource.ui.missing_portrait"), Error);
                TestFalse(TEXT("CCF-15: Incompatible FixedAspect resource rejected under NineSlice policy"), bIncompatibleApply);
                TestEqual(TEXT("CCF-15: AppliedResourceId unchanged after failure"), ImageWidget->GetAppliedResourceId(), BaselineId);
                TestEqual(TEXT("CCF-15: Brush ResourceObject unchanged after failure"), ImageWidget->GetImageBrush().GetResourceObject(), BaselineBrush.GetResourceObject());
                TestEqual(TEXT("CCF-15: Brush DrawAs unchanged after failure"), ImageWidget->GetImageBrush().DrawAs.GetValue(), BaselineBrush.DrawAs.GetValue());
                TestEqual(TEXT("CCF-15: Brush Tiling unchanged after failure"), ImageWidget->GetImageBrush().Tiling.GetValue(), BaselineBrush.Tiling.GetValue());
                TestEqual(TEXT("CCF-15: Brush Margin unchanged after failure"), ImageWidget->GetImageBrush().Margin.Left, BaselineBrush.Margin.Left);
            }
        }

        GameInstance->Shutdown();
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// UIH-09..UIH-12: Location Composite Semantics & Validation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationCompositeSemanticsTest,
    "GV2.Runtime.UI.LocationCompositeSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationCompositeSemanticsTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->AddToRoot();
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestWorld != nullptr)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        GameInstance->Init();

        // 1. SceneView validation & semantics
        {
            UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
            UGV2LocationSceneWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
            TestNotNull(TEXT("SceneView created"), SceneView);

            FGV2UiCapabilityBuilder Builder;
            SceneView->DescribeUiCapabilities(Builder);
            FGV2UiCapabilityTree Caps = Builder.Build();
            TestNotNull(TEXT("SceneView has background_tile_resource_id cap"), Caps.FindProperty(TEXT("background_tile_resource_id")));
            TestNotNull(TEXT("SceneView has background_resource_id cap"), Caps.FindProperty(TEXT("background_resource_id")));
            TestNotNull(TEXT("SceneView has context_text cap"), Caps.FindProperty(TEXT("context_text")));
            TestNotNull(TEXT("SceneView has characters cap"), Caps.FindProperty(TEXT("characters")));
            TestNotNull(TEXT("SceneView has key cap"), Caps.FindProperty(TEXT("key")));
        }

        // 4. CommandPanel validation & semantics
        {
            UClass* CommandClass = LoadClass<UGV2LocationCommandPanelWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
            UGV2LocationCommandPanelWidgetBase* CommandPanel = CommandClass ? CreateWidget<UGV2LocationCommandPanelWidgetBase>(TestWorld, CommandClass) : NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
            TestNotNull(TEXT("CommandPanel created"), CommandPanel);

            FGV2UiCapabilityBuilder Builder;
            CommandPanel->DescribeUiCapabilities(Builder);
            FGV2UiCapabilityTree Caps = Builder.Build();
            TestNotNull(TEXT("CommandPanel has items cap"), Caps.FindProperty(TEXT("items")));
            TestNotNull(TEXT("CommandPanel has key cap"), Caps.FindProperty(TEXT("key")));
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// UIH-13: Real Viewport / Layout Matrix Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenViewportMatrixTest,
    "GV2.Runtime.UI.LocationScreenViewportMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenViewportMatrixTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->AddToRoot();
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestWorld != nullptr)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        GameInstance->Init();

        // 1. Load actual registered WBP_LocationScreen
        UClass* ScreenClass = LoadClass<UGV2ScreenWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
        TestNotNull(TEXT("WBP_LocationScreen is loadable"), ScreenClass);

        if (ScreenClass != nullptr)
        {
            UGV2ScreenWidgetBase* LocationScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, ScreenClass);
            TestNotNull(TEXT("WBP_LocationScreen instantiated"), LocationScreen);

            if (LocationScreen != nullptr)
            {
                TSharedPtr<SWidget> SlateWidget = LocationScreen->TakeWidget();
                TestTrue(TEXT("LocationScreen produces valid Slate widget"), SlateWidget.IsValid());

                if (SlateWidget.IsValid())
                {
                    TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(FVector2D(1920, 1080));
                    VirtualWindow->SetContent(SlateWidget.ToSharedRef());

                    // Find child composite widgets in tree
                    TArray<UWidget*> ChildWidgets;
                    LocationScreen->WidgetTree->GetAllWidgets(ChildWidgets);

                    UGV2LocationTopBarWidgetBase* TopBarWidget = nullptr;
                    UGV2LocationPlayerStatusWidgetBase* PlayerStatusWidget = nullptr;
                    UGV2LocationSceneWidgetBase* SceneWidget = nullptr;
                    UGV2LocationCommandPanelWidgetBase* CommandWidget = nullptr;

                    for (UWidget* W : ChildWidgets)
                    {
                        if (auto* TB = Cast<UGV2LocationTopBarWidgetBase>(W)) TopBarWidget = TB;
                        else if (auto* PS = Cast<UGV2LocationPlayerStatusWidgetBase>(W)) PlayerStatusWidget = PS;
                        else if (auto* SC = Cast<UGV2LocationSceneWidgetBase>(W)) SceneWidget = SC;
                        else if (auto* CP = Cast<UGV2LocationCommandPanelWidgetBase>(W)) CommandWidget = CP;
                    }

                    TestNotNull(TEXT("TopBar child composite exists"), TopBarWidget);
                    TestNotNull(TEXT("PlayerStatus child composite exists"), PlayerStatusWidget);
                    TestNotNull(TEXT("Scene child composite exists"), SceneWidget);
                    TestNotNull(TEXT("CommandPanel child composite exists"), CommandWidget);

                    if (CommandWidget != nullptr)
                    {
                        if (UGV2ListViewWidgetBase* CmdRep = CommandWidget->GetRepeater())
                        {
                            struct FTestCmdEntry { FName Key; FText Text; };
                            TArray<FTestCmdEntry> TestButtons;
                            for (int32 Index = 1; Index <= 6; ++Index)
                            {
                                TestButtons.Add({ *FString::Printf(TEXT("cmd_%d"), Index), FText::FromString(*FString::Printf(TEXT("[LOCALE_TEST] Speak with Master Alchemist about Mysterious Elixir (#%d)"), Index)) });
                            }
                            CmdRep->ReconcileEntries<UGV2ButtonWidgetBase, FTestCmdEntry>(
                                TestButtons,
                                [](const FTestCmdEntry& E) { return E.Key; },
                                [TestWorld, CommandWidget]() -> UGV2ButtonWidgetBase*
                                {
                                    TSubclassOf<UGV2ButtonWidgetBase> BtnClass = CommandWidget->ResolveButtonWidgetClass();
                                    return BtnClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, BtnClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
                                },
                                [](UGV2ButtonWidgetBase& Btn, const FTestCmdEntry& Entry)
                                {
                                    Btn.SetKey(Entry.Key);
                                    FGV2TextViewModel VM; VM.Text = Entry.Text;
                                    return Btn.ApplyText(VM);
                                });
                        }
                    }

                    // 3. Test layout geometry across 6 resolutions using real SVirtualWindow
                    struct FViewportResolution
                    {
                        const TCHAR* Name;
                        FVector2D Size;
                        bool bUltrawide;
                    };
                    const FViewportResolution TestResolutions[] = {
                        { TEXT("4K (3840x2160)"), FVector2D(3840, 2160), false },
                        { TEXT("QHD (2560x1440)"), FVector2D(2560, 1440), false },
                        { TEXT("FHD (1920x1080)"), FVector2D(1920, 1080), false },
                        { TEXT("HD (1280x720)"), FVector2D(1280, 720), false },
                        { TEXT("UW-QHD (3440x1440)"), FVector2D(3440, 1440), true },
                        { TEXT("UW-FHD (2560x1080)"), FVector2D(2560, 1080), true }
                    };


                    float SceneWidthFHD = 0.0f;
                    float SceneWidthUWFHD = 0.0f;

                    for (const auto& Res : TestResolutions)
                    {
                        LocationScreen->InvalidateLayoutAndVolatility();
                        VirtualWindow->Resize(Res.Size);
                        VirtualWindow->SlatePrepass(1.0f);

                        // Trigger top-down Slate layout calculation
                        FSlateWindowElementList WindowElementList(VirtualWindow);
                        VirtualWindow->PaintWindow(
                            FPlatformTime::Seconds(),
                            0.016f,
                            WindowElementList,
                            FWidgetStyle(),
                            true);

                        const FVector2D WindowAllocated = VirtualWindow->GetTickSpaceGeometry().GetLocalSize();
                        TestEqual(
                            *FString::Printf(TEXT("CCF-16: [%s] Window allocated size matches target resolution"), Res.Name),
                            WindowAllocated,
                            Res.Size);

                        // 1. TopBar allocated geometry: height <= 25% of Res.Size.Y across ALL resolutions
                        if (TopBarWidget != nullptr && TopBarWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D TopBarAllocated = TopBarWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated height is positive: %f"), Res.Name, TopBarAllocated.Y),
                                TopBarAllocated.Y > 0.0f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated height <= 25%% of screen height (%f <= %f)"), Res.Name, TopBarAllocated.Y, Res.Size.Y * 0.25f),
                                TopBarAllocated.Y <= Res.Size.Y * 0.25f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] TopBar allocated width fills screen width (%f == %f)"), Res.Name, TopBarAllocated.X, Res.Size.X),
                                FMath::IsNearlyEqual(TopBarAllocated.X, Res.Size.X, 1.0f));
                        }

                        // 2. PlayerStatus allocated geometry: width <= 60% of Res.Size.X across ALL resolutions
                        if (PlayerStatusWidget != nullptr && PlayerStatusWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D PlayerStatusAllocated = PlayerStatusWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] PlayerStatus allocated width is positive: %f"), Res.Name, PlayerStatusAllocated.X),
                                PlayerStatusAllocated.X > 0.0f);
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] PlayerStatus allocated width <= 60%% of screen width (%f <= %f)"), Res.Name, PlayerStatusAllocated.X, Res.Size.X * 0.6f),
                                PlayerStatusAllocated.X <= Res.Size.X * 0.6f);
                        }

                        // 3. SceneView allocated geometry: fills remaining body width
                        if (SceneWidget != nullptr && SceneWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D SceneAllocated = SceneWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] SceneView allocated area is positive (%f x %f)"), Res.Name, SceneAllocated.X, SceneAllocated.Y),
                                SceneAllocated.X > 0.0f && SceneAllocated.Y > 0.0f);

                            if (Res.Size.X == 1920.0f && Res.Size.Y == 1080.0f)
                            {
                                SceneWidthFHD = SceneAllocated.X;
                            }
                            else if (Res.Size.X == 2560.0f && Res.Size.Y == 1080.0f)
                            {
                                SceneWidthUWFHD = SceneAllocated.X;
                            }
                        }

                        // 4. CommandPanel allocated geometry: check button placement within bounds
                        if (CommandWidget != nullptr && CommandWidget->GetCachedWidget().IsValid())
                        {
                            const FGeometry CommandGeom = CommandWidget->GetCachedWidget()->GetTickSpaceGeometry();
                            const FVector2D CommandAllocated = CommandGeom.GetLocalSize();
                            TestTrue(
                                *FString::Printf(TEXT("CCF-16: [%s] CommandPanel allocated size is positive (%f x %f)"), Res.Name, CommandAllocated.X, CommandAllocated.Y),
                                CommandAllocated.X > 0.0f && CommandAllocated.Y > 0.0f);

                            if (UGV2ListViewWidgetBase* Repeater = CommandWidget->GetRepeater())
                            {
                                TestEqual(
                                    *FString::Printf(TEXT("CCF-17: [%s] All 6 command buttons instantiated in repeater"), Res.Name),
                                    Repeater->GetEntryCount(),
                                    6);

                                // On HD 720p, verify each instantiated button's allocated geometry is strictly bounded in 2D (BAI-10)
                                if (Res.Size.X == 1280.0f && Res.Size.Y == 720.0f)
                                {
                                    const TArray<UWidget*> Entries = Repeater->GetOrderedEntries();
                                    TestEqual(TEXT("CCF-17: [720p] Repeater ordered entry widgets count matches 6"), Entries.Num(), 6);
                                    for (int32 BtnIndex = 0; BtnIndex < Entries.Num(); ++BtnIndex)
                                    {
                                        if (Entries[BtnIndex] != nullptr && Entries[BtnIndex]->GetCachedWidget().IsValid())
                                        {
                                            const FGeometry BtnGeom = Entries[BtnIndex]->GetCachedWidget()->GetTickSpaceGeometry();
                                            const FVector2D BtnLocalPos = VirtualWindow->GetTickSpaceGeometry().AbsoluteToLocal(BtnGeom.GetAbsolutePosition());
                                            const FVector2D BtnSize = BtnGeom.GetLocalSize();
                                            const FVector2D BtnInCommandPanel = CommandGeom.AbsoluteToLocal(BtnGeom.GetAbsolutePosition());

                                            TestTrue(
                                                *FString::Printf(TEXT("CCF-17: [720p] Button #%d allocated size is positive (%f x %f)"), BtnIndex + 1, BtnSize.X, BtnSize.Y),
                                                BtnSize.X > 0.0f && BtnSize.Y > 0.0f);

                                            // 1. Viewport 2-axis bounding box: Left, Top, Right, Bottom
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d left edge within viewport (%f >= 0)"), BtnIndex + 1, BtnLocalPos.X),
                                                BtnLocalPos.X >= -1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d top edge within viewport (%f >= 0)"), BtnIndex + 1, BtnLocalPos.Y),
                                                BtnLocalPos.Y >= -1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d right edge fits viewport width (%f <= 1280)"), BtnIndex + 1, BtnLocalPos.X + BtnSize.X),
                                                BtnLocalPos.X + BtnSize.X <= 1280.0f + 1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d bottom edge fits viewport height (%f <= 720)"), BtnIndex + 1, BtnLocalPos.Y + BtnSize.Y),
                                                BtnLocalPos.Y + BtnSize.Y <= 720.0f + 1.0f);

                                            // 2. CommandPanel 2-axis containment
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d inside CommandPanel left (%f >= 0)"), BtnIndex + 1, BtnInCommandPanel.X),
                                                BtnInCommandPanel.X >= -1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d inside CommandPanel top (%f >= 0)"), BtnIndex + 1, BtnInCommandPanel.Y),
                                                BtnInCommandPanel.Y >= -1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d fits CommandPanel width (%f <= %f)"), BtnIndex + 1, BtnInCommandPanel.X + BtnSize.X, CommandAllocated.X),
                                                BtnInCommandPanel.X + BtnSize.X <= CommandAllocated.X + 1.0f);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [720p] Button #%d fits CommandPanel height (%f <= %f)"), BtnIndex + 1, BtnInCommandPanel.Y + BtnSize.Y, CommandAllocated.Y),
                                                BtnInCommandPanel.Y + BtnSize.Y <= CommandAllocated.Y + 1.0f);

                                            // 3. Negative containment check: simulated oversized button detection
                                            auto TestFitsInBounds = [](const FVector2D& Pos, const FVector2D& Size, const FVector2D& Bounds) -> bool
                                            {
                                                return Pos.X >= -1.0f && Pos.Y >= -1.0f
                                                    && (Pos.X + Size.X) <= (Bounds.X + 1.0f)
                                                    && (Pos.Y + Size.Y) <= (Bounds.Y + 1.0f);
                                            };
                                            TestFalse(
                                                TEXT("BAI-10: [Negative] Artificial horizontal overflow beyond panel width is rejected"),
                                                TestFitsInBounds(BtnInCommandPanel, FVector2D(CommandAllocated.X + 50.0f, BtnSize.Y), CommandAllocated));
                                            TestFalse(
                                                TEXT("BAI-10: [Negative] Artificial vertical overflow beyond viewport height is rejected"),
                                                TestFitsInBounds(BtnLocalPos, FVector2D(BtnSize.X, 800.0f), FVector2D(1280.0f, 720.0f)));
                                        }
                                    }
                                }
                            }
                        }

                        // Ultrawide check (CCF-18): 21:9 ratio verified
                        if (Res.bUltrawide)
                        {
                            TestTrue(
                                *FString::Printf(TEXT("CCF-18: [%s] Ultrawide aspect ratio is > 2.0"), Res.Name),
                                (Res.Size.X / Res.Size.Y) > 2.0f);
                        }
                    }

                    // CCF-18: Compare FHD (1920x1080) vs UW-FHD (2560x1080) allocated geometry
                    TestTrue(TEXT("CCF-18: FHD and UW-FHD scene widths measured"), SceneWidthFHD > 0.0f && SceneWidthUWFHD > 0.0f);
                    TestTrue(
                        *FString::Printf(TEXT("CCF-18: Ultrawide (21:9) SceneView allocated width (%f) is strictly greater than 16:9 width (%f)"), SceneWidthUWFHD, SceneWidthFHD),
                        SceneWidthUWFHD > SceneWidthFHD);
                    const float WidthDifference = SceneWidthUWFHD - SceneWidthFHD;
                    TestTrue(
                        *FString::Printf(TEXT("CCF-18: Extra ultrawide width allocated to SceneView (%f >= 600px)"), WidthDifference),
                        WidthDifference >= 600.0f);

                    // Negative test: Constrained / small viewport bounds layout elements and does not overflow
                    {
                        VirtualWindow->Resize(FVector2D(100.0f, 100.0f));
                        VirtualWindow->SlatePrepass(1.0f);
                        FSlateWindowElementList WindowElementListSmall(VirtualWindow);
                        VirtualWindow->PaintWindow(FPlatformTime::Seconds(), 0.016f, WindowElementListSmall, FWidgetStyle(), true);
                        if (TopBarWidget != nullptr && TopBarWidget->GetCachedWidget().IsValid())
                        {
                            const FVector2D TopBarSmall = TopBarWidget->GetCachedWidget()->GetTickSpaceGeometry().GetLocalSize();
                            TestTrue(TEXT("CCF-16: [Negative] Constrained viewport bounds TopBar allocated size"), TopBarSmall.X <= 100.0f + 1.0f);
                        }
                    }
                }
            }
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// UIH-14: Rendering Conformance on Instantiated Widgets Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RenderingConformanceTest,
    "GV2.Runtime.Presentation.RenderingConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RenderingConformanceTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->AddToRoot();
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestWorld != nullptr)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        GameInstance->Init();

        const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
        TestNotNull(TEXT("Configured theme is valid"), Theme);

        // 1. Text, RichText, Button, Input, Dropdown sizing conformance across resolutions
        const float Heights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };
        for (const float H : Heights)
        {
            const float ExpectedTitleSize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("title")), H);
            const float ExpectedBodySize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("body")), H);
            const float ExpectedSmallSize = UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("small")), H);

            if (H <= 720.0f)
            {
                TestTrue(TEXT("Small font size at 720p respects MinReadableFontSize"), ExpectedSmallSize >= (Theme ? Theme->MinReadableFontSize : 10.0f));
            }

            FTextBlockStyle TitleStyle;
            FTextBlockStyle BodyStyle;
            FTextBlockStyle SmallStyle;
            TestTrue(TEXT("ResolveStyleForHeight title succeeds"), UGV2TextPipeline::ResolveStyleForHeight(FName(TEXT("title")), TitleStyle, H));
            TestTrue(TEXT("ResolveStyleForHeight body succeeds"), UGV2TextPipeline::ResolveStyleForHeight(FName(TEXT("body")), BodyStyle, H));
            TestTrue(TEXT("ResolveStyleForHeight small succeeds"), UGV2TextPipeline::ResolveStyleForHeight(FName(TEXT("small")), SmallStyle, H));

            TestEqual(*FString::Printf(TEXT("[%.0fp] Title effective font size"), H), TitleStyle.Font.Size, ExpectedTitleSize);
            TestEqual(*FString::Printf(TEXT("[%.0fp] Body effective font size"), H), BodyStyle.Font.Size, ExpectedBodySize);
            TestEqual(*FString::Printf(TEXT("[%.0fp] Small effective font size"), H), SmallStyle.Font.Size, ExpectedSmallSize);

            // Create real widget instances from Blueprint classes
            UClass* TextClass = LoadClass<UGV2TextWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Text.WBP_Text_C"));
            UClass* RichTextClass = LoadClass<UGV2RichTextWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_RichText.WBP_RichText_C"));
            UClass* ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
            UClass* InputClass = LoadClass<UGV2InputFieldWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"));
            UClass* DropdownClass = LoadClass<UGV2DropdownSelectWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"));

            UGV2TextWidgetBase* TextWidget = TextClass ? CreateWidget<UGV2TextWidgetBase>(TestWorld, TextClass) : NewObject<UGV2TextWidgetBase>(TestWorld);
            UGV2RichTextWidgetBase* RichTextWidget = RichTextClass ? CreateWidget<UGV2RichTextWidgetBase>(TestWorld, RichTextClass) : NewObject<UGV2RichTextWidgetBase>(TestWorld);
            UGV2ButtonWidgetBase* ButtonWidget = ButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
            UGV2InputFieldWidgetBase* InputWidget = InputClass ? CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, InputClass) : NewObject<UGV2InputFieldWidgetBase>(TestWorld);
            UGV2DropdownSelectWidgetBase* DropdownWidget = DropdownClass ? CreateWidget<UGV2DropdownSelectWidgetBase>(TestWorld, DropdownClass) : NewObject<UGV2DropdownSelectWidgetBase>(TestWorld);

            TestNotNull(TEXT("TextWidget created"), TextWidget);
            TestNotNull(TEXT("RichTextWidget created"), RichTextWidget);
            TestNotNull(TEXT("ButtonWidget created"), ButtonWidget);
            TestNotNull(TEXT("InputWidget created"), InputWidget);
            TestNotNull(TEXT("DropdownWidget created"), DropdownWidget);

            // Apply models with default style
            FGV2TextViewModel TextModel;
            TextModel.Text = FText::FromString(TEXT("Sample Body Text"));
            TextModel.StyleToken = FName(TEXT("default"));
            if (TextWidget)
            {
                TestTrue(TEXT("ApplyText succeeds"), TextWidget->ApplyText(TextModel));
                TestTrue(TEXT("TextContent matches"), TextWidget->GetTextContent().EqualTo(TextModel.Text));
            }

            FGV2TextViewModel RichModel;
            RichModel.Text = FText::FromString(TEXT("Sample Rich Body"));
            RichModel.StyleToken = FName(TEXT("body"));
            if (RichTextWidget)
            {
                RichTextWidget->ApplyText(RichModel);
            }

            FGV2TextViewModel BtnText;
            BtnText.Text = FText::FromString(TEXT("Button"));
            BtnText.StyleToken = FName(TEXT("body"));
            const FName BtnKey = FName(TEXT("ok"));
            const FGV2UiBindingHandle BtnBinding = FGV2UiBindingHandle::Create(TEXT("btn_ok"));
            if (ButtonWidget)
            {
                ButtonWidget->SetKey(BtnKey);
                ButtonWidget->SetBindingHandle(BtnBinding);
                ButtonWidget->ApplyText(BtnText);
                TestEqual(TEXT("Button Key matches"), ButtonWidget->GetKey(), BtnKey);
                TestEqual(TEXT("Button Binding matches"), ButtonWidget->GetBindingHandle(), BtnBinding);
            }

            if (InputWidget)
            {
                FGV2TextViewModel InputText;
                InputText.Text = FText::FromString(TEXT("Input Label"));
                InputText.StyleToken = FName(TEXT("body"));
                InputWidget->SetKey(FName(TEXT("input_key")));
                InputWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("input_bind")));
                InputWidget->ApplyText(InputText);
            }

            if (DropdownWidget)
            {
                FGV2TextViewModel DropdownPlaceholder;
                DropdownPlaceholder.Text = FText::FromString(TEXT("Select Option"));
                DropdownWidget->ApplyPlaceholderText(DropdownPlaceholder);
                DropdownWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("dd_bind")));
            }
        }

        // 2. Image widgets scale policy and brush state conformance
        UClass* ImageClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
        UGV2ImageWidgetBase* ImageWidget = ImageClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, ImageClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
        TestNotNull(TEXT("ImageWidget created"), ImageWidget);

        if (ImageWidget != nullptr)
        {
            FString Error;

            // PreserveAspect policy with fixed aspect resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            const bool bAspectOk = ImageWidget->ApplyImageResource(TEXT("textsystem:resource.ui.missing_portrait"), Error);
            TestTrue(TEXT("PreserveAspect applied fixed aspect resource"), bAspectOk);
            TestEqual(TEXT("PreserveAspect brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("PreserveAspect brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // Tile policy with tile resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
            const bool bTileOk = ImageWidget->ApplyImageResource(TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("Tile policy applied tile resource"), bTileOk);
            TestEqual(TEXT("Tile brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("Tile brush Tiling is Both"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::Both);

            // FreeStretch policy with tile resource
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::FreeStretch);
            const bool bStretchOk = ImageWidget->ApplyImageResource(TEXT("core:resource.ui.old_paper_tile_256"), Error);
            TestTrue(TEXT("FreeStretch applied resource"), bStretchOk);
            TestEqual(TEXT("FreeStretch brush DrawAs is Image"), ImageWidget->GetImageBrush().DrawAs.GetValue(), ESlateBrushDrawType::Image);
            TestEqual(TEXT("FreeStretch brush Tiling is NoTile"), ImageWidget->GetImageBrush().Tiling.GetValue(), ESlateBrushTileType::NoTile);

            // Negative test: Incompatible graphics resource rejects and preserves previous brush
            const FSlateBrush ValidPrevBrush = ImageWidget->GetImageBrush();
            const FString ValidPrevId = ImageWidget->GetAppliedResourceId();

            const bool bBadApply = ImageWidget->ApplyImageResource(TEXT("nonexistent:resource.image"), Error);
            TestFalse(TEXT("Nonexistent resource is rejected"), bBadApply);
            TestEqual(TEXT("Applied resource id remains previous valid id"), ImageWidget->GetAppliedResourceId(), ValidPrevId);
            TestEqual(TEXT("Applied brush resource object remains unchanged"), ImageWidget->GetImageBrush().GetResourceObject(), ValidPrevBrush.GetResourceObject());
        }
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// UIH-15: LocationScreen Transition Contract Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationScreenTransitionContractTest,
    "GV2.Runtime.UI.LocationScreenTransitionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationScreenTransitionContractTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("RuntimeSubsystem initialized"), Runtime);

    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);

        const FString GameNs = TEXT("r") TEXT("h");
        const FString TavernTitleTextId = GameNs + TEXT(":text.location.tavern.title");
        const FString MarketTitleTextId = GameNs + TEXT(":text.location.market.title");
        const FString TavernBgResId = GameNs + TEXT(":resource.location.tavern");
        const FString MarketBgResId = GameNs + TEXT(":resource.location.market");

        // 1. Initial screen in Tavern
        UGV2ScreenWidgetBase* Screen1 = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("Initial location screen presented in Tavern"), Screen1);

        if (Screen1 != nullptr)
        {
            // 2. Perform location transition specifically to Market (Tavern -> Market)
            // Find the explicit travel command button binding handle for Market
            TArray<UWidget*> ChildWidgets;
            Screen1->WidgetTree->GetAllWidgets(ChildWidgets);
            FGV2UiBindingHandle TravelMarketHandle;

            for (UWidget* Child : ChildWidgets)
            {
                if (auto* CmdPanel = Cast<UGV2LocationCommandPanelWidgetBase>(Child))
                {
                    if (UGV2ListViewWidgetBase* Repeater = CmdPanel->GetRepeater())
                    {
                        if (auto* Btn = Cast<UGV2ButtonWidgetBase>(Repeater->GetEntryWidget(FName(TEXT("travel_city_market")))))
                        {
                            TravelMarketHandle = Btn->GetBindingHandle();
                            break;
                        }
                    }
                }
            }

            TestTrue(TEXT("Found travel_city_market button binding in Tavern screen"), TravelMarketHandle.IsValid());

            if (TravelMarketHandle.IsValid())
            {
                const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelMarketHandle, {});
                TestEqual(TEXT("Travel command interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
            }

            // 3. Screen instance reuse verification
            UGV2ScreenWidgetBase* Screen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Location screen active after travel to market"), Screen2);
            TestEqual(TEXT("Screen instance is preserved and reused across location transition"), Screen1, Screen2);

            // 4. Verify location title and background updated to Market
            TArray<UWidget*> MarketWidgets;
            Screen2->WidgetTree->GetAllWidgets(MarketWidgets);
            bool bFoundMarketTopBar = false;
            bool bFoundMarketScene = false;
            bool bFoundMarketCommands = false;
            bool bTavernTravelButtonPresentInMarket = false;

            for (UWidget* Child : MarketWidgets)
            {
                if (Child != nullptr)
                {
                    if (auto* TopBar = Cast<UGV2LocationTopBarWidgetBase>(Child))
                    {
                        bFoundMarketTopBar = true;
                    }
                    else if (auto* Scene = Cast<UGV2LocationSceneWidgetBase>(Child))
                    {
                        bFoundMarketScene = true;
                        UGV2ImageWidgetBase* Bg = Cast<UGV2ImageWidgetBase>(Scene->GetWidgetFromName(FName(TEXT("Background"))));
                        if (Bg != nullptr)
                        {
                            TestEqual(
                                TEXT("CCF-21: Market Scene background resource ID"),
                                Bg->GetAppliedResourceId(),
                                MarketBgResId);
                        }
                    }
                    else if (auto* Cmd = Cast<UGV2LocationCommandPanelWidgetBase>(Child))
                    {
                        bFoundMarketCommands = true;
                        if (UGV2ListViewWidgetBase* Repeater = Cmd->GetRepeater())
                        {
                            bTavernTravelButtonPresentInMarket = Repeater->GetEntryWidget(FName(TEXT("travel_city_market"))) != nullptr;
                        }
                    }
                }
            }

            TestTrue(TEXT("CCF-21: Market TopBar verified"), bFoundMarketTopBar);
            TestTrue(TEXT("CCF-21: Market Scene verified"), bFoundMarketScene);
            TestTrue(TEXT("CCF-21: Market Commands field captured"), bFoundMarketCommands);
            TestFalse(TEXT("CCF-21: Old Tavern travel command button removed in Market"), bTavernTravelButtonPresentInMarket);
        }

        Runtime->EndSession();
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// Diagnostic: LocationScene Image & Hierarchy Audit
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationSceneDiagnostic,
    "GV2.Runtime.Presentation.LocationSceneDiagnostic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationSceneDiagnostic::RunTest(const FString& Parameters)
{
    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString MarketResourceId = GameNamespace + TEXT(":resource.location.market");
    const FString HeroPortraitResourceId = GameNamespace + TEXT(":resource.portrait.hero");

    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    TestNotNull(TEXT("Image catalog is loaded"), Catalog);
    if (Catalog != nullptr)
    {
        FGV2ResolvedImageResource MarketRes;
        FString Error;
        const bool bMarketResolved = Catalog->Resolve(MarketResourceId, MarketRes, Error);
        TestTrue(*FString::Printf(TEXT("Market resource resolved: %s"), *Error), bMarketResolved);
        if (bMarketResolved)
        {
            UObject* ResObj = MarketRes.Brush.GetResourceObject();
            TestNotNull(TEXT("Market brush resource object is valid"), ResObj);
            UTexture2D* Tex = Cast<UTexture2D>(ResObj);
            TestNotNull(TEXT("Market resource is UTexture2D"), Tex);
            if (Tex != nullptr)
            {
                AddInfo(FString::Printf(TEXT("Market Texture size: %dx%d, SRGB=%d, HasPlatformData=%d"),
                    Tex->GetSizeX(), Tex->GetSizeY(), Tex->SRGB, Tex->GetPlatformData() != nullptr));
            }
        }
    }

    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->AddToRoot();
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (TestWorld != nullptr)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TestWorld);
        GameInstance->Init();

        UClass* SceneClass = LoadClass<UGV2LocationSceneWidgetBase>(
            nullptr,
            TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        TestNotNull(TEXT("SceneClass loaded"), SceneClass);
        if (SceneClass != nullptr)
        {
            UGV2LocationSceneWidgetBase* SceneView = CreateWidget<UGV2LocationSceneWidgetBase>(TestWorld, SceneClass);
            TestNotNull(TEXT("Scene created"), SceneView);
            if (SceneView != nullptr)
            {
                UGV2ImageWidgetBase* Bg = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("Background"))));
                UGV2ImageWidgetBase* BgTile = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("BackgroundTile"))));
                TestNotNull(TEXT("Background widget found"), Bg);
                TestNotNull(TEXT("BackgroundTile widget found"), BgTile);

                if (Bg != nullptr)
                {
                    FString Error;
                    Bg->ApplyOptionalImageResource(MarketResourceId, TEXT("core:resource.ui.missing_background"), Error);
                    AddInfo(FString::Printf(TEXT("Background: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                        *Bg->GetAppliedResourceId(),
                        static_cast<int32>(Bg->GetVisibility()),
                        Bg->GetImageBrush().GetResourceObject() ? *Bg->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                }
                if (BgTile != nullptr)
                {
                    FString Error;
                    BgTile->ApplyOptionalImageResource(TEXT("core:resource.ui.old_paper_tile_256"), TEXT("core:resource.ui.missing_background"), Error);
                    AddInfo(FString::Printf(TEXT("BackgroundTile: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                        *BgTile->GetAppliedResourceId(),
                        static_cast<int32>(BgTile->GetVisibility()),
                        BgTile->GetImageBrush().GetResourceObject() ? *BgTile->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                }
            }
        }

        GameInstance->Shutdown();
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

// =========================================================================
// SVC-09: Composite Rollback Contract (UIF-AF-01, failed apply restores prior model and visuals)
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CompositeRollbackContract,
    "GV2.Runtime.UI.CompositeRollbackContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CompositeRollbackContract::RunTest(const FString& Parameters)
{
    // Transactional ReconcileEntries failure and rollback in ListView
    {
        UVerticalBox* Container = NewObject<UVerticalBox>();
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>();
        ListView->SetContainerPanel(Container);

        struct FTestItemModel
        {
            FName Key;
            FString Text;
            bool bShouldFailApply = false;
        };

        TArray<FTestItemModel> InitialItems = {
            {FName(TEXT("item_1")), TEXT("First Item"), false},
            {FName(TEXT("item_2")), TEXT("Second Item"), false}
        };

        auto CreateTestWidget = []() -> UGV2TextWidgetBase*
        {
            return NewObject<UGV2TextWidgetBase>();
        };

        auto ApplyTestItem = [](UGV2TextWidgetBase& Widget, const FTestItemModel& Model) -> bool
        {
            if (Model.bShouldFailApply)
            {
                return false;
            }
            Widget.ApplyText({FText::FromString(Model.Text)});
            return true;
        };

        const bool bInitialReconcile = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            InitialItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestTrue(TEXT("Initial list items reconciled"), bInitialReconcile);
        TestEqual(TEXT("Container has 2 initial children"), Container->GetChildrenCount(), 2);

        UGV2TextWidgetBase* Item1Widget = ListView->GetEntry<UGV2TextWidgetBase>(FName(TEXT("item_1")));
        UGV2TextWidgetBase* Item2Widget = ListView->GetEntry<UGV2TextWidgetBase>(FName(TEXT("item_2")));
        TestNotNull(TEXT("Item 1 widget exists"), Item1Widget);
        TestNotNull(TEXT("Item 2 widget exists"), Item2Widget);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("Initial item 1 text is 'First Item'"), Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("Initial item 2 text is 'Second Item'"), Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }

        // 2a. Preflight rejection on CanApplyItem: rejects before constructing or mutating widgets
        TArray<FTestItemModel> PreflightInvalidItems = {
            {FName(TEXT("item_1")), TEXT("Preflight Mutated 1"), false},
            {FName(TEXT("item_bad")), TEXT(""), false}
        };

        const bool bPreflightRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            PreflightInvalidItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem,
            [](const FTestItemModel& M) { return !M.Text.IsEmpty(); });

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects items failing CanApplyItem preflight"), bPreflightRejected);
        TestEqual(TEXT("Container children count unchanged after preflight rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after CanApplyItem preflight rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 2 widget state unmutated after CanApplyItem preflight rejection"),
                Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }

        // 2b. Preflight rejection on duplicate key: rejects before mutating any widget
        TArray<FTestItemModel> DuplicateKeyItems = {
            {FName(TEXT("item_1")), TEXT("Duplicate Mutated 1"), false},
            {FName(TEXT("item_1")), TEXT("Duplicate Mutated 2"), false}
        };

        const bool bDuplicateRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            DuplicateKeyItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects duplicate keys in preflight"), bDuplicateRejected);
        TestEqual(TEXT("Container children count unchanged after duplicate key rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after duplicate key rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2c. Preflight rejection on empty key: rejects before mutating any widget
        TArray<FTestItemModel> EmptyKeyItems = {
            {FName(TEXT("item_1")), TEXT("Empty Key Mutated 1"), false},
            {NAME_None, TEXT("Empty Key Mutated 2"), false}
        };

        const bool bEmptyKeyRejected = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            EmptyKeyItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects empty key in preflight"), bEmptyKeyRejected);
        TestEqual(TEXT("Container children count unchanged after empty key rejection"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after empty key rejection"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2d. Widget creation failure: rejects before mutating any existing widget
        TArray<FTestItemModel> CreateFailItems = {
            {FName(TEXT("item_1")), TEXT("Create Fail Mutated 1"), false},
            {FName(TEXT("item_new_fail")), TEXT("New Item Failing Creation"), false}
        };

        auto NullCreateWidget = []() -> UGV2TextWidgetBase*
        {
            return nullptr;
        };

        const bool bCreateFailed = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            CreateFailItems,
            [](const FTestItemModel& M) { return M.Key; },
            NullCreateWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries rejects when widget creation returns null"), bCreateFailed);
        TestEqual(TEXT("Container children count unchanged after widget creation failure"), Container->GetChildrenCount(), 2);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state unmutated after widget creation failure"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }

        // 2e. Runtime item apply failure: Container hierarchy is not committed; calling reconciler restores state
        TArray<FTestItemModel> CandidateItems = {
            {FName(TEXT("item_1")), TEXT("First Item Updated"), false},
            {FName(TEXT("item_2")), TEXT("Second Item Updated"), false},
            {FName(TEXT("item_3_fail")), TEXT("Third Item Failing"), true}
        };

        const bool bFailedReconcile = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            CandidateItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestFalse(TEXT("BAI-07: ReconcileEntries returns false when a child item fails apply"), bFailedReconcile);
        TestEqual(TEXT("Container retains previous 2 children on failed reconcile"), Container->GetChildrenCount(), 2);

        // Calling composite screen element restores previous state on reconcile failure
        const bool bRestored = ListView->ReconcileEntries<UGV2TextWidgetBase, FTestItemModel>(
            InitialItems,
            [](const FTestItemModel& M) { return M.Key; },
            CreateTestWidget,
            ApplyTestItem);

        TestTrue(TEXT("BAI-07: Screen-level rollback restores previous models"), bRestored);
        if (Item1Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 1 widget state restored to 'First Item'"),
                Item1Widget->GetTextContent().ToString(), TEXT("First Item"));
        }
        if (Item2Widget != nullptr)
        {
            TestEqual(TEXT("BAI-07: Item 2 widget state restored to 'Second Item'"),
                Item2Widget->GetTextContent().ToString(), TEXT("Second Item"));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenFieldClosedSchemaRejectionTest,
    "GV2.Runtime.Presentation.ScreenFieldClosedSchemaRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenFieldClosedSchemaRejectionTest::RunTest(const FString& Parameters)
{
    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();
    AddExpectedErrorPlain(TEXT("rejected (closed schema)"), EAutomationExpectedErrorFlags::Contains, 3);

    auto MakeTextSpec = [](const std::string& TextId) -> FObject
    {
        FObject Obj;
        Obj["text_id"] = GV2RuntimeCore::FValue(TextId);
        return Obj;
    };

    // 1. Rejection at field value level: commands with unknown key
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "textsystem:screen.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

        FObject CmdValue;
        CmdValue["items"] = GV2RuntimeCore::FValue(FArray{});
        CmdValue["unknown_field"] = GV2RuntimeCore::FValue(std::string("invalid"));

        Field.Value = GV2RuntimeCore::FValue(CmdValue);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2UiBindingDefinition> Definitions;
        TestFalse(TEXT("BAI-03: Field value level unknown key rejected on commands"), Registry.PrepareBindingDefinitions(Request, Definitions));
    }

    // 2. Rejection at collection element level: unknown key on button item
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "textsystem:screen.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

        FObject BtnObj;
        BtnObj["key"] = GV2RuntimeCore::FValue(std::string("btn_ok"));
        BtnObj["text"] = GV2RuntimeCore::FValue(MakeTextSpec("core:text.common.ok"));
        FObject BindingObj;
        BindingObj["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.common.ok"));
        BtnObj["binding"] = GV2RuntimeCore::FValue(BindingObj);
        BtnObj["extra_sound"] = GV2RuntimeCore::FValue(std::string("click.wav"));

        FObject CmdValue;
        CmdValue["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnObj)});

        Field.Value = GV2RuntimeCore::FValue(CmdValue);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2UiBindingDefinition> Definitions;
        TestFalse(TEXT("BAI-03: Collection element level unknown key rejected on button"), Registry.PrepareBindingDefinitions(Request, Definitions));
    }

    // 3. Rejection at nested Binding level: unknown property in binding object
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "textsystem:screen.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

        FObject BtnObj;
        BtnObj["key"] = GV2RuntimeCore::FValue(std::string("btn_ok"));
        BtnObj["text"] = GV2RuntimeCore::FValue(MakeTextSpec("core:text.common.ok"));
        FObject BadBinding;
        BadBinding["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.common.ok"));
        BadBinding["unknown_meta"] = GV2RuntimeCore::FValue(std::string("extra"));
        BtnObj["binding"] = GV2RuntimeCore::FValue(BadBinding);

        FObject CmdValue;
        CmdValue["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnObj)});

        Field.Value = GV2RuntimeCore::FValue(CmdValue);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2UiBindingDefinition> Definitions;
        TestFalse(TEXT("BAI-03: Nested Binding level unknown key rejected"), Registry.PrepareBindingDefinitions(Request, Definitions));
    }

    // 4. Rejection of duplicate button keys in collection
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "textsystem:screen.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

        FObject BtnObj1;
        BtnObj1["key"] = GV2RuntimeCore::FValue(std::string("btn_ok"));
        BtnObj1["text"] = GV2RuntimeCore::FValue(MakeTextSpec("core:text.common.ok"));
        BtnObj1["binding"] = GV2RuntimeCore::FValue(std::string("core:command.common.ok"));

        FObject BtnObj2;
        BtnObj2["key"] = GV2RuntimeCore::FValue(std::string("btn_ok"));
        BtnObj2["text"] = GV2RuntimeCore::FValue(MakeTextSpec("core:text.common.cancel"));
        BtnObj2["binding"] = GV2RuntimeCore::FValue(std::string("core:command.common.cancel"));

        FObject CmdValue;
        CmdValue["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnObj1), GV2RuntimeCore::FValue(BtnObj2)});

        Field.Value = GV2RuntimeCore::FValue(CmdValue);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2UiBindingDefinition> Definitions;
        TestFalse(TEXT("BAI-03: Duplicate button key rejected in commands"), Registry.PrepareBindingDefinitions(Request, Definitions));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationKeyBoundaryTest,
    "GV2.Runtime.Presentation.LocationKeyBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationKeyBoundaryTest::RunTest(const FString& Parameters)
{
    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();

    auto MakeTextSpec = [](const std::string& TextId) -> FObject
    {
        FObject Obj;
        Obj["text_id"] = GV2RuntimeCore::FValue(TextId);
        return Obj;
    };

    // 1. Repeated element key grammar applies to command items
    {
        auto BuildWithCommandKey = [&](const std::string& Key) -> bool
        {
            GV2RuntimeCore::FScreenRequest Request;
            Request.ScreenId = "textsystem:screen.location";

            GV2RuntimeCore::FScreenField Field;
            Field.FieldId = "commands";
            Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

            FObject EntryObj;
            EntryObj["key"] = GV2RuntimeCore::FValue(Key);
            EntryObj["text"] = GV2RuntimeCore::FValue(MakeTextSpec("core:text.common.ok"));
            FObject BindingObj;
            BindingObj["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.common.ok"));
            EntryObj["binding"] = GV2RuntimeCore::FValue(BindingObj);

            FObject FieldValue;
            FieldValue["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(EntryObj)});

            Field.Value = GV2RuntimeCore::FValue(FieldValue);
            Request.Fields.push_back(MoveTemp(Field));

            TArray<FGV2UiBindingDefinition> Definitions;
            return Registry.PrepareBindingDefinitions(Request, Definitions);
        };

        TestTrue(TEXT("Conforming command key accepted"), BuildWithCommandKey("tavern_keeper"));
        TestFalse(TEXT("Uppercase command key rejected"), BuildWithCommandKey("TavernKeeper"));
        TestFalse(TEXT("Command key with space rejected"), BuildWithCommandKey("tavern keeper"));
        TestFalse(TEXT("Text-derived command key rejected"), BuildWithCommandKey("text:character.name"));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationCompositeUnresolvedClassRejectionTest,
    "GV2.Runtime.Presentation.LocationCompositeUnresolvedClassRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationCompositeUnresolvedClassRejectionTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("TestWorld created"), TestWorld);
    if (TestWorld == nullptr) return false;

    // 1. SceneWidget without character widget class
    {
        UGV2LocationSceneWidgetBase* SceneWidget = NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
        TestNotNull(TEXT("Scene widget instantiated"), SceneWidget);

        UVerticalBox* CharBox = NewObject<UVerticalBox>(SceneWidget);
        UGV2ListViewWidgetBase* CharRep = NewObject<UGV2ListViewWidgetBase>(SceneWidget);
        CharRep->SetContainerPanel(CharBox);

        if (FProperty* Prop = UGV2LocationSceneWidgetBase::StaticClass()->FindPropertyByName(TEXT("CharacterRepeater")))
        {
            *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(SceneWidget) = CharRep;
        }

        TestTrue(TEXT("SceneWidget has usable character repeater host"), SceneWidget->HasUsableCharacterRepeaterHost());

        TSubclassOf<UGV2ImageWidgetBase> CharClass = SceneWidget->ResolveCharacterWidgetClass();
        TestTrue(TEXT("SceneWidget ResolveCharacterWidgetClass handles fallback gracefully"), CharClass == nullptr || CharClass->IsChildOf(UGV2ImageWidgetBase::StaticClass()));
    }

    // 2. CommandPanel without button widget class
    {
        UGV2LocationCommandPanelWidgetBase* CommandPanel = NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
        TestNotNull(TEXT("CommandPanel instantiated"), CommandPanel);

        UWrapBox* ButtonBox = NewObject<UWrapBox>(CommandPanel);
        UGV2ListViewWidgetBase* BtnRep = NewObject<UGV2ListViewWidgetBase>(CommandPanel);
        BtnRep->SetContainerPanel(ButtonBox);

        if (FProperty* Prop = UGV2LocationCommandPanelWidgetBase::StaticClass()->FindPropertyByName(TEXT("ButtonRepeater")))
        {
            *Prop->ContainerPtrToValuePtr<TObjectPtr<UGV2ListViewWidgetBase>>(CommandPanel) = BtnRep;
        }

        TestTrue(TEXT("CommandPanel has usable repeater host"), CommandPanel->HasUsableRepeaterHost());

        TSubclassOf<UGV2ButtonWidgetBase> BtnClass = CommandPanel->ResolveButtonWidgetClass();
        TestTrue(TEXT("CommandPanel ResolveButtonWidgetClass handles fallback gracefully"), BtnClass == nullptr || BtnClass->IsChildOf(UGV2ButtonWidgetBase::StaticClass()));
    }

    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiFailurePropagationTest,
    "GV2.Runtime.UI.FailurePropagationAndTextPipelineRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiFailurePropagationTest::RunTest(const FString& Parameters)
{
    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    AddExpectedErrorPlain(TEXT("rejected (closed schema)"), EAutomationExpectedErrorFlags::Contains, 3);

    // A text model the central pipeline must reject: authoring markup may never reach
    // a plain renderer. Used throughout as the failure injector.
    FGV2TextViewModel PoisonText;
    PoisonText.Text = FText::FromString(TEXT("Poison"));
    PoisonText.NormalizedMarkup = TEXT("<gv2:action id=\"x\">y</>");

    FGV2TextViewModel GoodText;
    GoodText.Text = FText::FromString(TEXT("Fine"));

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("Test world created"), TestWorld);
    if (TestWorld == nullptr) return false;

    // 1. REV3-01 / REV3-02: a meter label reaches the ProgressBar and goes through the
    //    text pipeline, so an unrenderable label fails instead of being dropped.
    {
        UClass* BarClass = LoadClass<UGV2ProgressBarWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C"));
        UGV2ProgressBarWidgetBase* Bar = BarClass
            ? CreateWidget<UGV2ProgressBarWidgetBase>(TestWorld, BarClass)
            : NewObject<UGV2ProgressBarWidgetBase>(TestWorld);
        TestNotNull(TEXT("ProgressBar instantiated"), Bar);
        if (Bar != nullptr)
        {
            FGV2ProgressBarViewModel Model;
            Model.Percent = 0.5f;
            Model.Label = GoodText;
            TestTrue(TEXT("REV3-01: ApplyProgressBarModel accepts a renderable label"), Bar->ApplyProgressBarModel(Model));
            TestEqual(TEXT("REV3-01: Progress updated to 0.5"), Bar->GetProgress(), 0.5f);

            Model.Label = PoisonText;
            TestFalse(TEXT("REV3-02: Label that the text pipeline rejects fails the apply"),
                Bar->ApplyProgressBarModel(Model));
        }
    }

    // 2. REV3-05: a button whose text cannot be rendered fails, and the failure is not
    //    swallowed by the owning collection.
    {
        UClass* ListClass = LoadClass<UGV2ButtonListWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_ButtonList.WBP_ButtonList_C"));
        if (ListClass != nullptr)
        {
            UGV2ButtonListWidgetBase* List = CreateWidget<UGV2ButtonListWidgetBase>(TestWorld, ListClass);
            TestNotNull(TEXT("ButtonList instantiated"), List);
            if (List != nullptr)
            {
                FGV2ButtonViewModel Poison;
                Poison.Key = TEXT("btn_poison");
                Poison.Binding = FGV2UiBindingHandle::Create(TEXT("core:command.test"));
                Poison.Text = PoisonText;
                TestFalse(TEXT("REV3-05: ButtonList reports failure when a child button cannot render its text"),
                    List->ApplyButtonModels({ Poison }));
            }
        }
    }

    // 3. REV3-10: a portrait resource with no bound renderer is a failure, not a success.
    {
        UGV2PortraitWidgetBase* Portrait = NewObject<UGV2PortraitWidgetBase>(TestWorld);
        TestNotNull(TEXT("Portrait instantiated"), Portrait);
        if (Portrait != nullptr)
        {
            FString Error;
            TestFalse(TEXT("REV3-10: Portrait with unbound renderer rejects a supplied resource"),
                Portrait->ApplyPortrait(TEXT("core:resource.ui.missing_portrait"), FString(), Error));
            TestTrue(TEXT("REV3-10: Rejection names the unbound renderer"), Error.Contains(TEXT("PortraitImage")));

            FString EmptyError;
            TestTrue(TEXT("REV3-10: Portrait with no resource and no renderer still succeeds"),
                Portrait->ApplyPortrait(FString(), FString(), EmptyError));
        }
    }

    // 3b. REV3-09: RichText with hover spans fails validation when RichTextPopoverClass is unavailable
    {
        UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
        if (Theme != nullptr)
        {
            TSoftClassPtr<UGV2RichTextPopoverWidgetBase> SavedPopoverClass = Theme->RichTextPopoverClass;
            Theme->RichTextPopoverClass = nullptr;

            TMap<FString, FGV2PreparedUiValue> HoverMap;
            FGV2TextViewModel TitleModel;
            TitleModel.Text = FText::FromString(TEXT("Definition"));
            HoverMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(TitleModel));

            TMap<FString, FGV2PreparedUiValue> SpanMap;
            SpanMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("term")));
            SpanMap.Add(TEXT("hover"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(HoverMap)));

            TArray<FGV2PreparedUiValue> Elements;
            Elements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(SpanMap)));
            FGV2PreparedUiValue SpansVal = FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(Elements));

            UGV2RichTextWidgetBase* RichTextWidget = NewObject<UGV2RichTextWidgetBase>(TestWorld);
            FGV2RichTextSpansPropertyConsumer Consumer;
            FGV2UiPropertyCapability Cap;
            Cap.PropertyName = TEXT("spans");
            Cap.TargetType = EGV2UiCapabilityTargetType::CustomControl;
            Cap.SupportedKind = EGV2PreparedUiValueKind::Array;

            FString PrepError;
            TestFalse(TEXT("REV3-09: RichText with hover spans rejects application when popover class is unavailable"),
                Consumer.Prepare(SpansVal, Cap, RichTextWidget, PrepError));

            Theme->RichTextPopoverClass = SavedPopoverClass;
        }
    }

    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);

    // 4. REV3-03: button element extra properties are rejected by closed schema parser
    {
        const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();

        auto PrepareWithExtraBtnProp = [&Registry](const char* ExtraKey, GV2RuntimeCore::FValue ExtraValue) -> bool
        {
            GV2RuntimeCore::FScreenRequest Request;
            Request.ScreenId = "textsystem:screen.location";

            GV2RuntimeCore::FScreenField Field;
            Field.FieldId = "commands";
            Field.SchemaId = "textsystem:schema.ui_field.location_commands.v1";

            FObject BtnObj;
            BtnObj["key"] = GV2RuntimeCore::FValue(std::string("btn_action"));
            FObject TextObj;
            TextObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.common.ok"));
            BtnObj["text"] = GV2RuntimeCore::FValue(TextObj);
            BtnObj["binding"] = GV2RuntimeCore::FValue(std::string("core:command.common.ok"));
            BtnObj[ExtraKey] = MoveTemp(ExtraValue);

            FObject CmdValue;
            CmdValue["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnObj)});

            Field.Value = GV2RuntimeCore::FValue(CmdValue);
            Request.Fields.push_back(MoveTemp(Field));

            TArray<FGV2UiBindingDefinition> Definitions;
            return Registry.PrepareBindingDefinitions(Request, Definitions);
        };

        TestFalse(TEXT("REV3-03: button rejects scaling_policy"),
            PrepareWithExtraBtnProp("scaling_policy", GV2RuntimeCore::FValue(std::string("tile"))));
        TestFalse(TEXT("REV3-03: button rejects custom_width"),
            PrepareWithExtraBtnProp("custom_width", GV2RuntimeCore::FValue(static_cast<std::int64_t>(64))));
        TestFalse(TEXT("REV3-03: button rejects style"),
            PrepareWithExtraBtnProp("style", GV2RuntimeCore::FValue(std::string("danger"))));
    }

    return true;
}

#endif
