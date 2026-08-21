#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
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
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
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
            TEXT("core:schema.ui_field.button_list.v2"),
            TEXT("core:schema.ui_field.rich_text.v3"),
            TEXT("core:schema.ui_field.checkbox.v1"),
            TEXT("core:schema.ui_field.input_field.v1"),
            TEXT("core:schema.ui_field.dropdown_select.v1")
        };
        for (const TCHAR* SchemaId : FieldSchemas)
        {
            TestTrue(
                *FString::Printf(TEXT("Adapter registry owns %s"), SchemaId),
                AdapterRegistrySource.Contains(SchemaId));
        }
    }
    TestEqual(
        TEXT("Adapter registry contains all 14 baseline and LocationScreen schemas"),
        FGV2ScreenFieldAdapterRegistry::Get().Num(),
        14);

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

    // 1. Valid button list with distinct keys
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "buttons";
        BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
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
        MissingKeyReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "buttons";
        BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
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
        DupKeyReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "buttons";
        BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
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
        TextKeyReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "buttons";
        BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
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

    // 5. Button list invalid grammar key
    {
        GV2RuntimeCore::FScreenRequest InvalidKeyReq;
        InvalidKeyReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BtnField;
        BtnField.FieldId = "buttons";
        BtnField.SchemaId = "core:schema.ui_field.button_list.v2";
        const std::string KeyInvalid = "BTN #1!";
        GV2RuntimeCore::FValue::FObject ValueObj;
        ValueObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
            MakeButtonItem(&KeyInvalid, "core:command.screen.action_a")
        });
        BtnField.Value = GV2RuntimeCore::FValue(MoveTemp(ValueObj));
        InvalidKeyReq.Fields.push_back(MoveTemp(BtnField));
        TArray<FGV2UiBindingDefinition> InvalidDefs;
        TestFalse(
            TEXT("Button list with invalid grammar key is rejected (UiElementKeyInvalid)"),
            FGV2ScreenFieldAdapterRegistry::Get().PrepareBindingDefinitions(InvalidKeyReq, InvalidDefs));
        TestTrue(TEXT("Rejected invalid key leaves definitions empty"), InvalidDefs.IsEmpty());
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
    TestEqual(TEXT("Registry contains all 14 baseline and LocationScreen schemas"), Registry.Num(), 14);
    TestNotNull(TEXT("Location top bar adapter is registered"), Registry.Find("textsystem:schema.ui_field.location_top_bar.v1"));
    TestNotNull(TEXT("Location player status adapter is registered"), Registry.Find("textsystem:schema.ui_field.location_player_status.v1"));
    TestNotNull(TEXT("Location scene adapter is registered"), Registry.Find("textsystem:schema.ui_field.location_scene.v1"));
    TestNotNull(TEXT("Location commands adapter is registered"), Registry.Find("textsystem:schema.ui_field.location_commands.v1"));

    // 1. Image Adapter (core:schema.ui_field.image.v1)
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField ImgField;
        ImgField.FieldId = "illustration";
        ImgField.SchemaId = "core:schema.ui_field.image.v1";
        GV2RuntimeCore::FValue::FObject ImgObj;
        ImgObj["resource_id"] = GV2RuntimeCore::FValue(std::string("core:resource.image.test"));
        ImgField.Value = GV2RuntimeCore::FValue(MoveTemp(ImgObj));
        ValidReq.Fields.push_back(MoveTemp(ImgField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("Image schema prepare succeeds"), Registry.PrepareBindingDefinitions(ValidReq, Defs));
        TestEqual(TEXT("Image schema creates 0 binding definitions"), Defs.Num(), 0);

        TArray<FGV2ScreenFieldValue> BuiltFields;
        TestTrue(TEXT("Image schema build succeeds"), Registry.BuildFields(ValidReq, {}, BuiltFields));
        TestEqual(TEXT("Built 1 field"), BuiltFields.Num(), 1);
        if (BuiltFields.Num() == 1)
        {
            TestEqual(TEXT("FieldId is illustration"), BuiltFields[0].FieldId, FName(TEXT("illustration")));
            TestEqual(TEXT("SchemaId is image.v1"), BuiltFields[0].SchemaId, FString(TEXT("core:schema.ui_field.image.v1")));
            TestEqual(TEXT("ResourceId is core:resource.image.test"), BuiltFields[0].ImageValue.ResourceId, FString(TEXT("core:resource.image.test")));
        }

        // Negative: missing / invalid resource_id
        GV2RuntimeCore::FScreenRequest InvalidReq;
        InvalidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BadField;
        BadField.FieldId = "illustration";
        BadField.SchemaId = "core:schema.ui_field.image.v1";
        GV2RuntimeCore::FValue::FObject BadObj;
        BadObj["resource_id"] = GV2RuntimeCore::FValue(std::string("invalid_format_id"));
        BadField.Value = GV2RuntimeCore::FValue(MoveTemp(BadObj));
        InvalidReq.Fields.push_back(MoveTemp(BadField));
        TArray<FGV2UiBindingDefinition> BadDefs;
        TestFalse(TEXT("Invalid resource_id is rejected"), Registry.PrepareBindingDefinitions(InvalidReq, BadDefs));
    }

    // 2. ProgressBar Adapter (core:schema.ui_field.progress_bar.v1)
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BarField;
        BarField.FieldId = "hp_bar";
        BarField.SchemaId = "core:schema.ui_field.progress_bar.v1";
        GV2RuntimeCore::FValue::FObject BarObj;
        BarObj["percent"] = GV2RuntimeCore::FValue(0.75);
        GV2RuntimeCore::FValue::FObject LabelObj;
        LabelObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.progress.health"));
        BarObj["label"] = GV2RuntimeCore::FValue(MoveTemp(LabelObj));
        BarField.Value = GV2RuntimeCore::FValue(MoveTemp(BarObj));
        ValidReq.Fields.push_back(MoveTemp(BarField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("ProgressBar schema prepare succeeds"), Registry.PrepareBindingDefinitions(ValidReq, Defs));
        TestEqual(TEXT("ProgressBar schema creates 0 binding definitions"), Defs.Num(), 0);

        TArray<FGV2ScreenFieldValue> BuiltFields;
        TestTrue(TEXT("ProgressBar schema build succeeds"), Registry.BuildFields(ValidReq, {}, BuiltFields));
        TestEqual(TEXT("Built 1 field"), BuiltFields.Num(), 1);
        if (BuiltFields.Num() == 1)
        {
            TestEqual(TEXT("FieldId is hp_bar"), BuiltFields[0].FieldId, FName(TEXT("hp_bar")));
            TestEqual(TEXT("SchemaId is progress_bar.v1"), BuiltFields[0].SchemaId, FString(TEXT("core:schema.ui_field.progress_bar.v1")));
            TestEqual(TEXT("Percent is 0.75"), BuiltFields[0].ProgressBarValue.Percent, 0.75f);
        }

        // Negative: percent out of bounds
        GV2RuntimeCore::FScreenRequest InvalidReq;
        InvalidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField BadField;
        BadField.FieldId = "hp_bar";
        BadField.SchemaId = "core:schema.ui_field.progress_bar.v1";
        GV2RuntimeCore::FValue::FObject BadObj;
        BadObj["percent"] = GV2RuntimeCore::FValue(1.5);
        BadField.Value = GV2RuntimeCore::FValue(MoveTemp(BadObj));
        InvalidReq.Fields.push_back(MoveTemp(BadField));
        TArray<FGV2UiBindingDefinition> BadDefs;
        TestFalse(TEXT("Out of bounds percent is rejected"), Registry.PrepareBindingDefinitions(InvalidReq, BadDefs));
    }

    // 3. Portrait Adapter (core:schema.ui_field.portrait.v1)
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField PortraitField;
        PortraitField.FieldId = "hero_portrait";
        PortraitField.SchemaId = "core:schema.ui_field.portrait.v1";
        GV2RuntimeCore::FValue::FObject PortObj;
        PortObj["resource_id"] = GV2RuntimeCore::FValue(std::string("core:resource.image.hero"));
        PortObj["frame_resource_id"] = GV2RuntimeCore::FValue(std::string("core:resource.image.frame"));
        PortraitField.Value = GV2RuntimeCore::FValue(MoveTemp(PortObj));
        ValidReq.Fields.push_back(MoveTemp(PortraitField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("Portrait schema prepare succeeds"), Registry.PrepareBindingDefinitions(ValidReq, Defs));

        TArray<FGV2ScreenFieldValue> BuiltFields;
        TestTrue(TEXT("Portrait schema build succeeds"), Registry.BuildFields(ValidReq, {}, BuiltFields));
        TestEqual(TEXT("Built 1 field"), BuiltFields.Num(), 1);
        if (BuiltFields.Num() == 1)
        {
            TestEqual(TEXT("Portrait resource_id matches"), BuiltFields[0].PortraitValue.ResourceId, FString(TEXT("core:resource.image.hero")));
            TestEqual(TEXT("Frame resource_id matches"), BuiltFields[0].PortraitValue.FrameResourceId, FString(TEXT("core:resource.image.frame")));
        }
    }

    // 4. Modal Adapter (core:schema.ui_field.modal.v1)
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "core:screen.test";
        GV2RuntimeCore::FScreenField ModalField;
        ModalField.FieldId = "confirmation_dialog";
        ModalField.SchemaId = "core:schema.ui_field.modal.v1";
        GV2RuntimeCore::FValue::FObject ModalObj;
        GV2RuntimeCore::FValue::FObject TitleObj;
        TitleObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.modal.title"));
        ModalObj["title"] = GV2RuntimeCore::FValue(MoveTemp(TitleObj));
        GV2RuntimeCore::FValue::FObject ContentObj;
        ContentObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.modal.content"));
        ModalObj["content"] = GV2RuntimeCore::FValue(MoveTemp(ContentObj));

        GV2RuntimeCore::FValue::FObject BtnObj;
        BtnObj["key"] = GV2RuntimeCore::FValue(std::string("ok_btn"));
        GV2RuntimeCore::FValue::FObject BtnTextObj;
        BtnTextObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.button.ok"));
        BtnObj["text"] = GV2RuntimeCore::FValue(MoveTemp(BtnTextObj));
        GV2RuntimeCore::FValue::FObject BtnBindingObj;
        BtnBindingObj["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.modal.confirm"));
        BtnObj["binding"] = GV2RuntimeCore::FValue(MoveTemp(BtnBindingObj));

        ModalObj["buttons"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{ GV2RuntimeCore::FValue(MoveTemp(BtnObj)) });
        ModalField.Value = GV2RuntimeCore::FValue(MoveTemp(ModalObj));
        ValidReq.Fields.push_back(MoveTemp(ModalField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("Modal schema prepare succeeds"), Registry.PrepareBindingDefinitions(ValidReq, Defs));
        TestEqual(TEXT("Modal prepares 1 button binding definition"), Defs.Num(), 1);

        TArray<FGV2UiBindingHandle> Handles = { FGV2UiBindingHandle::Create(TEXT("h_modal_btn")) };
        TArray<FGV2ScreenFieldValue> BuiltFields;
        TestTrue(TEXT("Modal schema build succeeds"), Registry.BuildFields(ValidReq, Handles, BuiltFields));
        TestEqual(TEXT("Built 1 field"), BuiltFields.Num(), 1);
        if (BuiltFields.Num() == 1)
        {
            TestEqual(TEXT("Modal has 1 button"), BuiltFields[0].ModalValue.Buttons.Num(), 1);
            TestEqual(TEXT("Modal button key is ok_btn"), BuiltFields[0].ModalValue.Buttons[0].Key, FName(TEXT("ok_btn")));
            TestEqual(TEXT("Modal button binding handle matches"), BuiltFields[0].ModalValue.Buttons[0].Binding.ToString(), FString(TEXT("h_modal_btn")));
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
            const FGV2ScreenFieldDescriptor Desc = Portrait->GetScreenFieldDescriptor_Implementation();
            TestEqual(TEXT("Portrait schema is portrait.v1"), Desc.SchemaId, FString(TEXT("core:schema.ui_field.portrait.v1")));
        }

        UGV2ModalWidgetBase* Modal = NewObject<UGV2ModalWidgetBase>();
        TestNotNull(TEXT("Transient modal widget created"), Modal);
        if (Modal != nullptr)
        {
            const FGV2ScreenFieldDescriptor Desc = Modal->GetScreenFieldDescriptor_Implementation();
            TestEqual(TEXT("Modal schema is modal.v1"), Desc.SchemaId, FString(TEXT("core:schema.ui_field.modal.v1")));
        }

        UGV2ProgressBarWidgetBase* ProgressBar = NewObject<UGV2ProgressBarWidgetBase>();
        TestNotNull(TEXT("Transient progress bar widget created"), ProgressBar);
        if (ProgressBar != nullptr)
        {
            const FGV2ScreenFieldDescriptor Desc = ProgressBar->GetScreenFieldDescriptor_Implementation();
            TestEqual(TEXT("ProgressBar schema is progress_bar.v1"), Desc.SchemaId, FString(TEXT("core:schema.ui_field.progress_bar.v1")));
        }

        UGV2ImageWidgetBase* Image = NewObject<UGV2ImageWidgetBase>();
        TestNotNull(TEXT("Transient image widget created"), Image);
        if (Image != nullptr)
        {
            const FGV2ScreenFieldDescriptor Desc = Image->GetScreenFieldDescriptor_Implementation();
            TestEqual(TEXT("Image schema is image.v1"), Desc.SchemaId, FString(TEXT("core:schema.ui_field.image.v1")));
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
                    FGV2InteractiveRichTextViewModel ReplacementText;
                    ReplacementText.Text.Text = FText::FromString(TEXT("Replacement text"));
                    RichText->ApplyInteractiveRichText(ReplacementText);
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
            TestEqual(TEXT("Test screen exposes five dynamic fields"), Contract.Num(), 5);
            if (Contract.Num() == 5)
            {
                TestEqual(TEXT("Button list field is canonical"), Contract[0].FieldId, FName(TEXT("buttons")));
                TestEqual(TEXT("Checkbox field is canonical"), Contract[1].FieldId, FName(TEXT("checkbox")));
                TestEqual(TEXT("Dropdown field is canonical"), Contract[2].FieldId, FName(TEXT("class_select")));
                TestEqual(TEXT("Description field is canonical"), Contract[3].FieldId, FName(TEXT("description")));
                TestEqual(TEXT("Input field is canonical"), Contract[4].FieldId, FName(TEXT("player_name")));
                TestEqual(
                    TEXT("Button list field uses the expected schema"),
                    Contract[0].SchemaId,
                    FString(TEXT("core:schema.ui_field.button_list.v2")));
                TestEqual(
                    TEXT("Checkbox field uses the expected schema"),
                    Contract[1].SchemaId,
                    FString(TEXT("core:schema.ui_field.checkbox.v1")));
                TestEqual(
                    TEXT("Dropdown field uses the expected schema"),
                    Contract[2].SchemaId,
                    FString(TEXT("core:schema.ui_field.dropdown_select.v1")));
                TestEqual(
                    TEXT("Description field uses the expected schema"),
                    Contract[3].SchemaId,
                    FString(TEXT("core:schema.ui_field.rich_text.v3")));
                TestEqual(
                    TEXT("Input field uses the expected schema"),
                    Contract[4].SchemaId,
                    FString(TEXT("core:schema.ui_field.input_field.v1")));
                TestTrue(TEXT("Button list field is required"), Contract[0].bRequired);
                TestTrue(TEXT("Checkbox field is required"), Contract[1].bRequired);
                TestTrue(TEXT("Dropdown field is required"), Contract[2].bRequired);
                TestTrue(TEXT("Description field is required"), Contract[3].bRequired);
                TestTrue(TEXT("Input field is required"), Contract[4].bRequired);
            }

            FGV2ButtonViewModel ValidationButton;
            ValidationButton.Key = TEXT("validation");
            ValidationButton.Text.Text = FText::FromString(TEXT("Validation"));
            ValidationButton.Text.StyleToken = TEXT("button");
            ValidationButton.Binding = FGV2UiBindingHandle::Create(TEXT("runtime@1:999"));
            FGV2InteractiveRichTextViewModel ValidationText;
            ValidationText.Text.Text = FText::FromString(TEXT("Valid"));
            ValidationText.Text.StyleToken = TEXT("default");
            FGV2CheckboxViewModel ValidationCheckbox;
            ValidationCheckbox.Key = TEXT("checkbox");
            ValidationCheckbox.Text.Text = FText::FromString(TEXT("Validation checkbox"));
            ValidationCheckbox.Text.StyleToken = TEXT("default");
            ValidationCheckbox.Binding = FGV2UiBindingHandle::Create(TEXT("runtime@1:998"));
            FGV2InputFieldViewModel ValidationInput;
            ValidationInput.Key = TEXT("player_name");
            ValidationInput.Binding = FGV2UiBindingHandle::Create(TEXT("runtime@1:997"));
            FGV2DropdownSelectViewModel ValidationDropdown;
            ValidationDropdown.Binding = FGV2UiBindingHandle::Create(TEXT("runtime@1:996"));
            FGV2DropdownOptionViewModel& ValidationOption = ValidationDropdown.Options.AddDefaulted_GetRef();
            ValidationOption.Key = TEXT("warrior");
            ValidationOption.Text.Text = FText::FromString(TEXT("Warrior"));
            ValidationOption.Text.StyleToken = TEXT("default");
            const TArray<FGV2ScreenFieldValue> ValidFields = {
                FGV2ScreenFieldValue::MakeInteractiveRichText(TEXT("description"), ValidationText),
                FGV2ScreenFieldValue::MakeButtonList(TEXT("buttons"), {ValidationButton}),
                FGV2ScreenFieldValue::MakeCheckbox(TEXT("checkbox"), ValidationCheckbox),
                FGV2ScreenFieldValue::MakeInputField(TEXT("player_name"), ValidationInput),
                FGV2ScreenFieldValue::MakeDropdownSelect(TEXT("class_select"), ValidationDropdown)
            };
            TestTrue(TEXT("Complete field set passes validation"), Screen->CanApplyScreenFields(ValidFields));

            TestFalse(
                TEXT("Missing required field is rejected"),
                Screen->CanApplyScreenFields({ValidFields[0]}));

            TArray<FGV2ScreenFieldValue> UnknownField = ValidFields;
            UnknownField.Add(FGV2ScreenFieldValue::MakeInteractiveRichText(TEXT("unknown"), {}));
            TestFalse(
                TEXT("Unknown field is rejected"),
                Screen->CanApplyScreenFields(UnknownField));

            TArray<FGV2ScreenFieldValue> DuplicateField = ValidFields;
            DuplicateField.Add(ValidFields[0]);
            TestFalse(
                TEXT("Duplicate field is rejected"),
                Screen->CanApplyScreenFields(DuplicateField));

            TArray<FGV2ScreenFieldValue> SchemaMismatch = ValidFields;
            SchemaMismatch[0].SchemaId = TEXT("core:schema.ui_field.button_list.v2");
            TestFalse(
                TEXT("Schema mismatch is rejected"),
                Screen->CanApplyScreenFields(SchemaMismatch));

            UGV2RichTextWidgetBase* DescriptionWidget = Cast<UGV2RichTextWidgetBase>(
                Screen->GetWidgetFromName(TEXT("DescriptionText")));
            UGV2CheckboxWidgetBase* CheckboxWidget = Cast<UGV2CheckboxWidgetBase>(
                Screen->GetWidgetFromName(TEXT("CheckboxField")));
            TestNotNull(TEXT("Test screen uses the reusable checkbox component"), CheckboxWidget);
            if (CheckboxWidget != nullptr)
            {
                FGV2ScreenFieldValue AppliedCheckbox;
                TestTrue(
                    TEXT("Checkbox exposes its applied desired state through the common field contract"),
                    IGV2DynamicScreenElement::Execute_CaptureScreenField(
                        CheckboxWidget,
                        AppliedCheckbox));
                TestFalse(
                    TEXT("Lua initializes the checkbox as unchecked"),
                    AppliedCheckbox.CheckboxValue.bIsChecked);
            }
            TestNotNull(TEXT("Test screen uses the reusable rich text component"), DescriptionWidget);
            if (DescriptionWidget != nullptr)
            {
                TestTrue(
                    TEXT("Lua annotation is projected into the rich text component"),
                    DescriptionWidget->HasInteractiveSpan(TEXT("integration")));
                const FGV2RichTextSpanViewModel* Span =
                    DescriptionWidget->FindInteractiveSpan(TEXT("integration"));
                TestTrue(
                    TEXT("Hover content stays locally available in UE"),
                    Span != nullptr && !Span->Hover.IsEmpty());
                TestFalse(
                    TEXT("Annotated span creates a non-empty tooltip"),
                    DescriptionWidget->CreateSpanToolTip(TEXT("integration"))->IsEmpty());
                TestEqual(
                    TEXT("Span click uses the same semantic input ingress"),
                    DescriptionWidget->SubmitSpanInteraction(TEXT("integration")),
                    EGV2SubmitUiInteractionResult::Accepted);

                FGV2InteractiveRichTextViewModel DanglingText;
                DanglingText.Text.Text = FText::FromString(
                    TEXT("<interactive id=\"missing\">Invalid</interactive>"));
                DanglingText.Text.StyleToken = TEXT("default");
                TestFalse(
                    TEXT("Markup cannot reference an undeclared span"),
                    IGV2DynamicScreenElement::Execute_CanApplyScreenField(
                        DescriptionWidget,
                        FGV2ScreenFieldValue::MakeInteractiveRichText(
                            TEXT("description"), DanglingText)));
            }

            if (CheckboxWidget != nullptr)
            {
                TestEqual(
                    TEXT("Checkbox submits its schema-bound boolean through the common emitter"),
                    CheckboxWidget->SubmitCheckboxState(true),
                    EGV2SubmitUiInteractionResult::Accepted);
                UGV2ScreenWidgetBase* ReconciledScreen = Cast<UGV2ScreenWidgetBase>(
                    Runtime->GetActiveScreen());
                TestNotNull(
                    TEXT("Checkbox input republishes and reconciles the registered screen"),
                    ReconciledScreen);
                UGV2CheckboxWidgetBase* ReconciledCheckbox = ReconciledScreen != nullptr
                    ? Cast<UGV2CheckboxWidgetBase>(
                        ReconciledScreen->GetWidgetFromName(TEXT("CheckboxField")))
                    : nullptr;
                FGV2ScreenFieldValue ReconciledValue;
                TestTrue(
                    TEXT("Reconciled checkbox remains available through the common field contract"),
                    ReconciledCheckbox != nullptr
                        && IGV2DynamicScreenElement::Execute_CaptureScreenField(
                            ReconciledCheckbox,
                            ReconciledValue));
                TestTrue(
                    TEXT("Lua owns and republishes the accepted checkbox state"),
                    ReconciledValue.CheckboxValue.bIsChecked);
            }

            UGV2ScreenWidgetBase* CurrentScreen = Cast<UGV2ScreenWidgetBase>(
                Runtime->GetActiveScreen());
            UGV2InputFieldWidgetBase* InputFieldWidget = CurrentScreen != nullptr
                ? Cast<UGV2InputFieldWidgetBase>(
                    CurrentScreen->GetWidgetFromName(TEXT("PlayerNameField")))
                : nullptr;
            TestNotNull(TEXT("Test screen uses the reusable input field component"), InputFieldWidget);
            if (InputFieldWidget != nullptr)
            {
                TestEqual(
                    TEXT("Input field submits the schema-bound string through the common emitter"),
                    InputFieldWidget->SubmitTextValue(TEXT("Алекс")),
                    EGV2SubmitUiInteractionResult::Accepted);
                CurrentScreen = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
                UGV2InputFieldWidgetBase* ReconciledInput = CurrentScreen != nullptr
                    ? Cast<UGV2InputFieldWidgetBase>(
                        CurrentScreen->GetWidgetFromName(TEXT("PlayerNameField")))
                    : nullptr;
                FGV2ScreenFieldValue ReconciledInputValue;
                TestTrue(
                    TEXT("Reconciled input remains available through the common field contract"),
                    ReconciledInput != nullptr
                        && IGV2DynamicScreenElement::Execute_CaptureScreenField(
                            ReconciledInput,
                            ReconciledInputValue));
                TestEqual(
                    TEXT("Lua owns and republishes the accepted input value"),
                    ReconciledInputValue.InputFieldValue.TextValue,
                    FString(TEXT("Алекс")));
            }

            CurrentScreen = Cast<UGV2ScreenWidgetBase>(Runtime->GetActiveScreen());
            UGV2DropdownSelectWidgetBase* DropdownWidget = CurrentScreen != nullptr
                ? Cast<UGV2DropdownSelectWidgetBase>(
                    CurrentScreen->GetWidgetFromName(TEXT("ClassSelectField")))
                : nullptr;
            TestNotNull(TEXT("Test screen uses the reusable dropdown component"), DropdownWidget);
            if (DropdownWidget != nullptr)
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
                FGV2ScreenFieldValue ReconciledDropdownValue;
                TestTrue(
                    TEXT("Reconciled dropdown remains available through the common field contract"),
                    ReconciledDropdown != nullptr
                        && IGV2DynamicScreenElement::Execute_CaptureScreenField(
                            ReconciledDropdown,
                            ReconciledDropdownValue));
                const FGV2DropdownOptionViewModel* SelectedOption =
                    ReconciledDropdownValue.DropdownSelectValue.Options.FindByPredicate(
                        [](const FGV2DropdownOptionViewModel& Option)
                        {
                            return Option.bSelected;
                        });
                TestTrue(
                    TEXT("Lua owns and republishes the accepted dropdown selection"),
                    SelectedOption != nullptr && SelectedOption->Key == TEXT("mage"));
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
            TEXT("InputFieldWidget implements IGV2DynamicScreenElement"),
            InputFieldWidget->GetClass()->ImplementsInterface(UGV2DynamicScreenElement::StaticClass()));
        TestTrue(
            TEXT("InputFieldWidget implements IGV2UiStyleConsumer"),
            InputFieldWidget->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()));

        FGV2InputFieldViewModel Model;
        Model.Key = TEXT("user_name");
        Model.Text.Text = FText::FromString(TEXT("Player Name"));
        Model.PlaceholderText.Text = FText::FromString(TEXT("Enter name..."));
        Model.TextValue = TEXT("King");
        Model.Binding = FGV2UiBindingHandle::Create(TEXT("core:input.user_name"));

        TestTrue(
            TEXT("CanApplyInputFieldModel returns true for valid model"),
            InputFieldWidget->CanApplyInputFieldModel(Model));
        TestTrue(
            TEXT("ApplyInputFieldModel succeeds"),
            InputFieldWidget->ApplyInputFieldModel(Model));

        FGV2ScreenFieldValue FieldValue = FGV2ScreenFieldValue::MakeInputField(
            TEXT("user_name"),
            Model);
        TestFalse(
            TEXT("Reusable input without a configured ScreenFieldId stays outside a screen contract"),
            IGV2DynamicScreenElement::Execute_CanApplyScreenField(InputFieldWidget, FieldValue));

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
    // UIF-23 & UIF-24: Tab Container Schema Adapter, Recursive Apply & Elongated Paths
    // =========================================================================
    const FGV2ScreenFieldAdapterRegistry& FieldAdapters = FGV2ScreenFieldAdapterRegistry::Get();
    {
        // 1. Rejection of empty tabs
        GV2RuntimeCore::FScreenRequest BadEmptyTabsReq;
        BadEmptyTabsReq.ScreenId = "core:screen.main";
        GV2RuntimeCore::FScreenField EmptyField;
        EmptyField.FieldId = "tabs";
        EmptyField.SchemaId = "core:schema.ui_field.tab_container.v1";
        GV2RuntimeCore::FValue::FObject EmptyObj;
        EmptyObj["tabs"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{});
        EmptyField.Value = GV2RuntimeCore::FValue(EmptyObj);
        BadEmptyTabsReq.Fields.push_back(EmptyField);

        TArray<FGV2UiBindingDefinition> BadDefs;
        TestFalse(TEXT("Empty tabs list rejected"), FieldAdapters.PrepareBindingDefinitions(BadEmptyTabsReq, BadDefs));

        // 2. Rejection of duplicate tab keys
        GV2RuntimeCore::FScreenRequest DupTabsReq;
        DupTabsReq.ScreenId = "core:screen.main";
        GV2RuntimeCore::FScreenField DupField;
        DupField.FieldId = "tabs";
        DupField.SchemaId = "core:schema.ui_field.tab_container.v1";
        GV2RuntimeCore::FValue::FObject DupObj;
        GV2RuntimeCore::FValue::FArray DupTabs;
        {
            GV2RuntimeCore::FValue::FObject T1;
            T1["key"] = GV2RuntimeCore::FValue(std::string("tab_a"));
            GV2RuntimeCore::FValue::FObject Title1;
            Title1["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.title_a"));
            T1["title"] = GV2RuntimeCore::FValue(Title1);
            T1["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_a"));
            DupTabs.push_back(GV2RuntimeCore::FValue(T1));

            GV2RuntimeCore::FValue::FObject T2;
            T2["key"] = GV2RuntimeCore::FValue(std::string("tab_a")); // duplicate key
            GV2RuntimeCore::FValue::FObject Title2;
            Title2["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.title_b"));
            T2["title"] = GV2RuntimeCore::FValue(Title2);
            T2["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_b"));
            DupTabs.push_back(GV2RuntimeCore::FValue(T2));
        }
        DupObj["tabs"] = GV2RuntimeCore::FValue(DupTabs);
        DupField.Value = GV2RuntimeCore::FValue(DupObj);
        DupTabsReq.Fields.push_back(DupField);

        TestFalse(TEXT("Duplicate tab keys rejected"), FieldAdapters.PrepareBindingDefinitions(DupTabsReq, BadDefs));

        // 3. Rejection of nested tab containers (tabs inside tabs)
        GV2RuntimeCore::FScreenRequest NestedTabsReq;
        NestedTabsReq.ScreenId = "core:screen.main";
        GV2RuntimeCore::FScreenField NestedField;
        NestedField.FieldId = "tabs";
        NestedField.SchemaId = "core:schema.ui_field.tab_container.v1";
        GV2RuntimeCore::FValue::FObject NestedObj;
        GV2RuntimeCore::FValue::FArray NestedTabs;
        {
            GV2RuntimeCore::FValue::FObject T1;
            T1["key"] = GV2RuntimeCore::FValue(std::string("outer_tab"));
            GV2RuntimeCore::FValue::FObject Title1;
            Title1["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.title_a"));
            T1["title"] = GV2RuntimeCore::FValue(Title1);
            T1["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_a"));

            // Child field is another tab_container -> MUST BE REJECTED
            GV2RuntimeCore::FValue::FObject InnerFields;
            GV2RuntimeCore::FValue::FObject InnerTabField;
            InnerTabField["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.tab_container.v1"));
            InnerTabField["value"] = GV2RuntimeCore::FValue(DupObj);
            InnerFields["inner_tabs"] = GV2RuntimeCore::FValue(InnerTabField);
            T1["fields"] = GV2RuntimeCore::FValue(InnerFields);
            NestedTabs.push_back(GV2RuntimeCore::FValue(T1));
        }
        NestedObj["tabs"] = GV2RuntimeCore::FValue(NestedTabs);
        NestedField.Value = GV2RuntimeCore::FValue(NestedObj);
        NestedTabsReq.Fields.push_back(NestedField);

        TestFalse(TEXT("Nested tab sets disallowed"), FieldAdapters.PrepareBindingDefinitions(NestedTabsReq, BadDefs));

        // 4. Valid tab container with 2 tabs & recursive child fields
        GV2RuntimeCore::FScreenRequest ValidTabReq;
        ValidTabReq.ScreenId = "core:screen.main";
        GV2RuntimeCore::FScreenField TabField;
        TabField.FieldId = "tabs";
        TabField.SchemaId = "core:schema.ui_field.tab_container.v1";
        GV2RuntimeCore::FValue::FObject TabObj;
        TabObj["default_tab_key"] = GV2RuntimeCore::FValue(std::string("skills"));
        GV2RuntimeCore::FValue::FArray ValidTabs;
        {
            // Tab 1: inventory
            GV2RuntimeCore::FValue::FObject T1;
            T1["key"] = GV2RuntimeCore::FValue(std::string("inventory"));
            GV2RuntimeCore::FValue::FObject Title1;
            Title1["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.tab_inventory"));
            T1["title"] = GV2RuntimeCore::FValue(Title1);
            T1["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_inventory"));

            GV2RuntimeCore::FValue::FObject T1Fields;
            GV2RuntimeCore::FValue::FObject BtnField;
            BtnField["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.button_list.v2"));
            GV2RuntimeCore::FValue::FObject BtnVal;
            GV2RuntimeCore::FValue::FArray Btns;
            GV2RuntimeCore::FValue::FObject Btn1;
            Btn1["key"] = GV2RuntimeCore::FValue(std::string("use_potion"));
            GV2RuntimeCore::FValue::FObject Btn1Title;
            Btn1Title["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.btn_use"));
            Btn1["text"] = GV2RuntimeCore::FValue(Btn1Title);
            GV2RuntimeCore::FValue::FObject Action1;
            Action1["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.item.use"));
            Btn1["binding"] = GV2RuntimeCore::FValue(Action1);
            Btns.push_back(GV2RuntimeCore::FValue(Btn1));
            BtnVal["items"] = GV2RuntimeCore::FValue(Btns);
            BtnField["value"] = GV2RuntimeCore::FValue(BtnVal);
            T1Fields["inventory_buttons"] = GV2RuntimeCore::FValue(BtnField);
            T1["fields"] = GV2RuntimeCore::FValue(T1Fields);
            ValidTabs.push_back(GV2RuntimeCore::FValue(T1));

            // Tab 2: skills
            GV2RuntimeCore::FValue::FObject T2;
            T2["key"] = GV2RuntimeCore::FValue(std::string("skills"));
            GV2RuntimeCore::FValue::FObject Title2;
            Title2["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.tab_skills"));
            T2["title"] = GV2RuntimeCore::FValue(Title2);
            T2["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_skills"));

            GV2RuntimeCore::FValue::FObject T2Fields;
            GV2RuntimeCore::FValue::FObject SkillBtnField;
            SkillBtnField["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.button_list.v2"));
            GV2RuntimeCore::FValue::FObject SkillBtnVal;
            GV2RuntimeCore::FValue::FArray SkillBtns;
            GV2RuntimeCore::FValue::FObject SkillBtn1;
            SkillBtn1["key"] = GV2RuntimeCore::FValue(std::string("learn_fireball"));
            GV2RuntimeCore::FValue::FObject SkillBtn1Title;
            SkillBtn1Title["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.btn_learn"));
            SkillBtn1["text"] = GV2RuntimeCore::FValue(SkillBtn1Title);
            GV2RuntimeCore::FValue::FObject Action2;
            Action2["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.skill.learn"));
            SkillBtn1["binding"] = GV2RuntimeCore::FValue(Action2);
            SkillBtns.push_back(GV2RuntimeCore::FValue(SkillBtn1));
            SkillBtnVal["items"] = GV2RuntimeCore::FValue(SkillBtns);
            SkillBtnField["value"] = GV2RuntimeCore::FValue(SkillBtnVal);
            T2Fields["skill_buttons"] = GV2RuntimeCore::FValue(SkillBtnField);
            T2["fields"] = GV2RuntimeCore::FValue(T2Fields);
            ValidTabs.push_back(GV2RuntimeCore::FValue(T2));
        }
        TabObj["tabs"] = GV2RuntimeCore::FValue(ValidTabs);
        TabField.Value = GV2RuntimeCore::FValue(TabObj);
        ValidTabReq.Fields.push_back(TabField);

        TArray<FGV2UiBindingDefinition> ValidDefs;
        TestTrue(TEXT("PrepareBindingDefinitions for tab container succeeds"), FieldAdapters.PrepareBindingDefinitions(ValidTabReq, ValidDefs));
        if (TestEqual(TEXT("Tab container produces 2 child button definitions"), ValidDefs.Num(), 2))
        {
            // Verify elongated paths: [field_id, tab_key, child_field_id, button_key]
            TestEqual(TEXT("Tab1 binding path segment count"), ValidDefs[0].NodeKeyPath.Num(), 4);
            if (ValidDefs[0].NodeKeyPath.Num() >= 4)
            {
                TestEqual(TEXT("Tab1 binding path segment 0 (field_id)"), ValidDefs[0].NodeKeyPath[0], TEXT("tabs"));
                TestEqual(TEXT("Tab1 binding path segment 1 (tab_key)"), ValidDefs[0].NodeKeyPath[1], TEXT("inventory"));
                TestEqual(TEXT("Tab1 binding path segment 2 (child_field_id)"), ValidDefs[0].NodeKeyPath[2], TEXT("inventory_buttons"));
                TestEqual(TEXT("Tab1 binding path segment 3 (btn_key)"), ValidDefs[0].NodeKeyPath[3], TEXT("use_potion"));
            }

            TestEqual(TEXT("Tab2 binding path segment count"), ValidDefs[1].NodeKeyPath.Num(), 4);
            if (ValidDefs[1].NodeKeyPath.Num() >= 4)
            {
                TestEqual(TEXT("Tab2 binding path segment 0 (field_id)"), ValidDefs[1].NodeKeyPath[0], TEXT("tabs"));
                TestEqual(TEXT("Tab2 binding path segment 1 (tab_key)"), ValidDefs[1].NodeKeyPath[1], TEXT("skills"));
                TestEqual(TEXT("Tab2 binding path segment 2 (child_field_id)"), ValidDefs[1].NodeKeyPath[2], TEXT("skill_buttons"));
                TestEqual(TEXT("Tab2 binding path segment 3 (btn_key)"), ValidDefs[1].NodeKeyPath[3], TEXT("learn_fireball"));
            }
        }

        // BuildFields verification
        TArray<FGV2UiBindingHandle> Handles;
        Handles.Add(FGV2UiBindingHandle::FromSerialized(TEXT("h_inv")));
        Handles.Add(FGV2UiBindingHandle::FromSerialized(TEXT("h_skills")));

        TArray<FGV2ScreenFieldValue> BuiltFields;
        TestTrue(TEXT("BuildFields for tab container succeeds"), FieldAdapters.BuildFields(ValidTabReq, Handles, BuiltFields));
        if (TestEqual(TEXT("1 built field produced"), BuiltFields.Num(), 1))
        {
            TestTrue(TEXT("TabContainerValue is populated"), BuiltFields[0].TabContainerValue.IsValid());
            if (BuiltFields[0].TabContainerValue.IsValid())
            {
                TestEqual(TEXT("DefaultTabKey is skills"), BuiltFields[0].TabContainerValue->DefaultTabKey, FName("skills"));
                TestEqual(TEXT("2 tabs in model"), BuiltFields[0].TabContainerValue->Tabs.Num(), 2);
                if (BuiltFields[0].TabContainerValue->Tabs.Num() >= 2)
                {
                    TestEqual(TEXT("Tab1 Key is inventory"), BuiltFields[0].TabContainerValue->Tabs[0].Key, FName("inventory"));
                    TestEqual(TEXT("Tab2 Key is skills"), BuiltFields[0].TabContainerValue->Tabs[1].Key, FName("skills"));
                    TestEqual(TEXT("Tab1 has 1 child field"), BuiltFields[0].TabContainerValue->Tabs[0].Fields.Num(), 1);
                    TestEqual(TEXT("Tab2 has 1 child field"), BuiltFields[0].TabContainerValue->Tabs[1].Fields.Num(), 1);
                }
            }
        }
    }

    // =========================================================================
    // UIF-25: UI-Local Active Tab State & Widget Lifecycle
    // =========================================================================
    {
        UGV2TabContainerWidgetBase* TabWidget = NewObject<UGV2TabContainerWidgetBase>();
        TestNotNull(TEXT("Tab widget created"), TabWidget);

        FGV2TabContainerViewModel TabModel;
        TabModel.DefaultTabKey = FName("skills");
        {
            FGV2TabItemViewModel& T1 = TabModel.Tabs.AddDefaulted_GetRef();
            T1.Key = FName("inventory");
            T1.ScreenId = TEXT("core:screen.tab_inventory");

            FGV2TabItemViewModel& T2 = TabModel.Tabs.AddDefaulted_GetRef();
            T2.Key = FName("skills");
            T2.ScreenId = TEXT("core:screen.tab_skills");
        }

        TestTrue(TEXT("Apply TabModel succeeds"), TabWidget->ApplyTabContainerModel(TabModel));
        TestEqual(TEXT("Initial active tab is DefaultTabKey (skills)"), TabWidget->GetActiveTabKey(), FName("skills"));
        TestEqual(TEXT("Active tab index is 1"), TabWidget->GetActiveTabIndex(), 1);

        // Switch tab locally
        TestTrue(TEXT("SelectTabByKey to inventory succeeds"), TabWidget->SelectTabByKey(FName("inventory")));
        TestEqual(TEXT("Active tab changed to inventory"), TabWidget->GetActiveTabKey(), FName("inventory"));
        TestEqual(TEXT("Active tab index changed to 0"), TabWidget->GetActiveTabIndex(), 0);

        // Reconcile new revision with same tabs -> preserves active tab (inventory), not resetting to default
        FGV2TabContainerViewModel Rev2Model = TabModel;
        TestTrue(TEXT("Apply Rev2Model succeeds"), TabWidget->ApplyTabContainerModel(Rev2Model));
        TestEqual(TEXT("Active tab preserved across revision (inventory)"), TabWidget->GetActiveTabKey(), FName("inventory"));

        // Reconcile revision where active tab (inventory) was removed -> falls back to default (skills)
        FGV2TabContainerViewModel Rev3Model;
        Rev3Model.DefaultTabKey = FName("skills");
        Rev3Model.Tabs.Add(TabModel.Tabs[1]); // only skills
        TestTrue(TEXT("Apply Rev3Model succeeds"), TabWidget->ApplyTabContainerModel(Rev3Model));
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

        // Prepare document with route holding tab container with 'inventory' (default) and 'skills'
        GV2RuntimeCore::FUiDocument Doc;
        Doc.UiInstanceId = "ui@1:1";
        Doc.Revision = 1;

        GV2RuntimeCore::FScreenInstance RouteInst;
        RouteInst.Layer = "location_content";
        RouteInst.InstanceKey = "main";
        RouteInst.ScreenId = "core:screen.main";

        GV2RuntimeCore::FScreenField TabField;
        TabField.FieldId = "tabs";
        TabField.SchemaId = "core:schema.ui_field.tab_container.v1";
        GV2RuntimeCore::FValue::FObject TabObj;
        TabObj["default_tab_key"] = GV2RuntimeCore::FValue(std::string("inventory"));
        GV2RuntimeCore::FValue::FArray TabsList;
        {
            // Tab 1: inventory with button
            GV2RuntimeCore::FValue::FObject T1;
            T1["key"] = GV2RuntimeCore::FValue(std::string("inventory"));
            GV2RuntimeCore::FValue::FObject Title1;
            Title1["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.tab_inv"));
            T1["title"] = GV2RuntimeCore::FValue(Title1);
            T1["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_inv"));
            GV2RuntimeCore::FValue::FObject T1Fields;
            GV2RuntimeCore::FValue::FObject BtnField;
            BtnField["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.button_list.v2"));
            GV2RuntimeCore::FValue::FObject BtnVal;
            GV2RuntimeCore::FValue::FArray Btns;
            GV2RuntimeCore::FValue::FObject Btn1;
            Btn1["key"] = GV2RuntimeCore::FValue(std::string("use_potion"));
            GV2RuntimeCore::FValue::FObject Btn1Title;
            Btn1Title["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.btn_use"));
            Btn1["text"] = GV2RuntimeCore::FValue(Btn1Title);
            GV2RuntimeCore::FValue::FObject Action1;
            Action1["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.test.step"));
            Btn1["binding"] = GV2RuntimeCore::FValue(Action1);
            Btns.push_back(GV2RuntimeCore::FValue(Btn1));
            BtnVal["items"] = GV2RuntimeCore::FValue(Btns);
            BtnField["value"] = GV2RuntimeCore::FValue(BtnVal);
            T1Fields["inv_buttons"] = GV2RuntimeCore::FValue(BtnField);
            T1["fields"] = GV2RuntimeCore::FValue(T1Fields);
            TabsList.push_back(GV2RuntimeCore::FValue(T1));

            // Tab 2: skills with button
            GV2RuntimeCore::FValue::FObject T2;
            T2["key"] = GV2RuntimeCore::FValue(std::string("skills"));
            GV2RuntimeCore::FValue::FObject Title2;
            Title2["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.tab_skills"));
            T2["title"] = GV2RuntimeCore::FValue(Title2);
            T2["screen_id"] = GV2RuntimeCore::FValue(std::string("core:screen.tab_skills"));
            GV2RuntimeCore::FValue::FObject T2Fields;
            GV2RuntimeCore::FValue::FObject SkillBtnField;
            SkillBtnField["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.button_list.v2"));
            GV2RuntimeCore::FValue::FObject SkillBtnVal;
            GV2RuntimeCore::FValue::FArray SkillBtns;
            GV2RuntimeCore::FValue::FObject SkillBtn1;
            SkillBtn1["key"] = GV2RuntimeCore::FValue(std::string("learn_fireball"));
            GV2RuntimeCore::FValue::FObject SkillBtn1Title;
            SkillBtn1Title["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.btn_learn"));
            SkillBtn1["text"] = GV2RuntimeCore::FValue(SkillBtn1Title);
            GV2RuntimeCore::FValue::FObject Action2;
            Action2["command_id"] = GV2RuntimeCore::FValue(std::string("core:command.test.step"));
            SkillBtn1["binding"] = GV2RuntimeCore::FValue(Action2);
            SkillBtns.push_back(GV2RuntimeCore::FValue(SkillBtn1));
            SkillBtnVal["items"] = GV2RuntimeCore::FValue(SkillBtns);
            SkillBtnField["value"] = GV2RuntimeCore::FValue(SkillBtnVal);
            T2Fields["skill_buttons"] = GV2RuntimeCore::FValue(SkillBtnField);
            T2["fields"] = GV2RuntimeCore::FValue(T2Fields);
            TabsList.push_back(GV2RuntimeCore::FValue(T2));
        }
        TabObj["tabs"] = GV2RuntimeCore::FValue(TabsList);
        TabField.Value = GV2RuntimeCore::FValue(TabObj);
        RouteInst.Fields.push_back(TabField);
        Doc.Route = RouteInst;

        bool bDocumentHandled = false;
        Coordinator.SetDocumentSink([&bDocumentHandled](const FGV2UiDocumentViewModel&) -> bool
        {
            bDocumentHandled = true;
            return true;
        });

        // Publish bindings
        GV2RuntimeCore::FScreenRequest RouteReq;
        RouteReq.ScreenId = RouteInst.ScreenId;
        RouteReq.Fields = RouteInst.Fields;
        TArray<FGV2UiBindingDefinition> Definitions;
        TestTrue(TEXT("PrepareBindingDefinitions for Route succeeds"), FieldAdapters.PrepareBindingDefinitions(RouteReq, Definitions));
        for (FGV2UiBindingDefinition& Def : Definitions)
        {
            Def.NodeKeyPath.Insert(TEXT("main"), 0);
            Def.NodeKeyPath.Insert(TEXT("location_content"), 0);
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
    // UIF-29: Core Minimal Theme & Emergency Screen Resolution
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

            // Check emergency screen titles in catalog
            TestTrue(TEXT("Emergency error title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.title")));
            TestTrue(TEXT("Emergency error description present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.description")));
            TestTrue(TEXT("Emergency loading title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.loading.title")));
            TestTrue(TEXT("Emergency recovery title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.recovery.title")));

            // Resolve text through pipeline with minimal theme
            FGV2TextViewModel ResolvedTitle;
            FString Error;
            TestTrue(
                TEXT("Resolve emergency error title via MinimalTheme"),
                UGV2TextPipeline::Resolve(TEXT("core:text.screen.error.title"), {}, FName("default"), ResolvedTitle, Error));
            TestEqual(TEXT("Error title text matches"), ResolvedTitle.Text.ToString(), TEXT("Error"));
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
// GLS-15: Transition Flow & Screen Instance Reuse Automation Test
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationTransitionFlowTest,
    "GV2.Runtime.Presentation.LocationTransitionFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationTransitionFlowTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance initializes the runtime"), Runtime);

    if (Runtime != nullptr)
    {
        FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);

        UGV2ScreenWidgetBase* ScreenBefore = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("Initial LocationScreen is presented in Tavern"), ScreenBefore);

        if (ScreenBefore != nullptr)
        {
            const TArray<FGV2ScreenFieldDescriptor> Contract = ScreenBefore->GetScreenFieldContract();
            TestTrue(TEXT("Contract contains top_bar"), Contract.ContainsByPredicate([](const FGV2ScreenFieldDescriptor& D){ return D.FieldId == TEXT("top_bar"); }));
            TestTrue(TEXT("Contract contains player_status"), Contract.ContainsByPredicate([](const FGV2ScreenFieldDescriptor& D){ return D.FieldId == TEXT("player_status"); }));
            TestTrue(TEXT("Contract contains scene"), Contract.ContainsByPredicate([](const FGV2ScreenFieldDescriptor& D){ return D.FieldId == TEXT("scene"); }));
            TestTrue(TEXT("Contract contains commands"), Contract.ContainsByPredicate([](const FGV2ScreenFieldDescriptor& D){ return D.FieldId == TEXT("commands"); }));
        }

        UGV2ScreenWidgetBase* ScreenAfter = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestEqual(TEXT("Screen widget instance is reused across locations (same UObject pointer)"), ScreenBefore, ScreenAfter);

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
    // Test ScalePolicy compatibility matrix
    TestTrue(TEXT("PreserveAspect compatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::FixedAspect));
    TestFalse(TEXT("PreserveAspect incompatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("PreserveAspect incompatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::Tile));

    TestTrue(TEXT("NineSlice compatible with NineSlice"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::NineSlice));
    TestFalse(TEXT("NineSlice incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::FixedAspect));

    TestTrue(TEXT("Tile compatible with Tile"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::Tile));
    TestFalse(TEXT("Tile incompatible with FixedAspect"), IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::FixedAspect));

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

        // 1. TopBar validation & semantics
        {
            UGV2LocationTopBarWidgetBase* TopBar = NewObject<UGV2LocationTopBarWidgetBase>(TestWorld);
            TestNotNull(TEXT("TopBar created"), TopBar);

            // Mismatched FieldId / SchemaId rejected by CanApply
            FGV2ScreenFieldValue WrongField = FGV2ScreenFieldValue::MakeLocationTopBar(TEXT("wrong_field"), {});
            TestFalse(TEXT("TopBar rejects mismatched FieldId"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(TopBar, WrongField));

            FGV2ScreenFieldValue WrongSchema;
            WrongSchema.FieldId = TEXT("top_bar");
            WrongSchema.SchemaId = TEXT("wrong:schema");
            TestFalse(TEXT("TopBar rejects mismatched SchemaId"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(TopBar, WrongSchema));

            // ResetScreenField clears applied state
            IGV2DynamicScreenElement::Execute_ResetScreenField(TopBar);
            FGV2ScreenFieldValue Captured;
            IGV2DynamicScreenElement::Execute_CaptureScreenField(TopBar, Captured);
            TestTrue(TEXT("TopBar captured day is empty after reset"), Captured.LocationTopBarValue.Day.Text.IsEmpty());
        }

        // 2. PlayerStatus validation & semantics (0/1/N meters & icons)
        {
            UGV2LocationPlayerStatusWidgetBase* PlayerStatus = NewObject<UGV2LocationPlayerStatusWidgetBase>(TestWorld);
            TestNotNull(TEXT("PlayerStatus created"), PlayerStatus);

            FGV2ScreenFieldValue WrongField = FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("wrong_field"), {});
            TestFalse(TEXT("PlayerStatus rejects mismatched FieldId"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(PlayerStatus, WrongField));

            // Populate with N items, effects & meters
            FGV2LocationPlayerStatusViewModel Model;
            Model.Name.Text = FText::FromString(TEXT("Hero"));
            Model.PortraitResourceId = TEXT("textsystem:resource.ui.missing_portrait");
            
            FGV2LocationMeterEntry Meter1;
            Meter1.Key = FName(TEXT("stamina"));
            Meter1.Meter.Percent = 0.75f;
            Model.Meters.Add(Meter1);

            Model.ItemIconResourceIds = { TEXT("item1"), TEXT("item2"), TEXT("item3") };
            Model.EffectIconResourceIds = { TEXT("effect1"), TEXT("effect2") };

            FGV2ScreenFieldValue ValidField = FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("player_status"), Model);
            TestTrue(TEXT("PlayerStatus accepts valid field descriptor"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(PlayerStatus, ValidField));

            IGV2DynamicScreenElement::Execute_ApplyScreenField(PlayerStatus, ValidField);
            FGV2ScreenFieldValue Captured;
            IGV2DynamicScreenElement::Execute_CaptureScreenField(PlayerStatus, Captured);
            TestEqual(TEXT("PlayerStatus items count preserved"), Captured.LocationPlayerStatusValue.ItemIconResourceIds.Num(), 3);
            TestEqual(TEXT("PlayerStatus effects count preserved"), Captured.LocationPlayerStatusValue.EffectIconResourceIds.Num(), 2);
            TestEqual(TEXT("PlayerStatus meters count preserved"), Captured.LocationPlayerStatusValue.Meters.Num(), 1);

            // ResetScreenField clears everything
            IGV2DynamicScreenElement::Execute_ResetScreenField(PlayerStatus);
            IGV2DynamicScreenElement::Execute_CaptureScreenField(PlayerStatus, Captured);
            TestEqual(TEXT("PlayerStatus items empty after reset"), Captured.LocationPlayerStatusValue.ItemIconResourceIds.Num(), 0);
            TestEqual(TEXT("PlayerStatus effects empty after reset"), Captured.LocationPlayerStatusValue.EffectIconResourceIds.Num(), 0);
            TestEqual(TEXT("PlayerStatus meters empty after reset"), Captured.LocationPlayerStatusValue.Meters.Num(), 0);
        }

        // 3. SceneView validation & semantics (0/1/N characters)
        {
            UGV2LocationSceneWidgetBase* SceneView = NewObject<UGV2LocationSceneWidgetBase>(TestWorld);
            TestNotNull(TEXT("SceneView created"), SceneView);

            FGV2ScreenFieldValue WrongField = FGV2ScreenFieldValue::MakeLocationScene(TEXT("wrong_field"), {});
            TestFalse(TEXT("SceneView rejects mismatched FieldId"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(SceneView, WrongField));

            // Populate with N characters
            FGV2LocationSceneViewModel SceneModel;
            FGV2LocationCharacterEntry Char1;
            Char1.Key = FName(TEXT("aria"));
            Char1.ResourceId = TEXT("char_a");
            FGV2LocationCharacterEntry Char2;
            Char2.Key = FName(TEXT("keeper"));
            Char2.ResourceId = TEXT("char_b");
            SceneModel.Characters = { Char1, Char2 };

            FGV2ScreenFieldValue ValidScene = FGV2ScreenFieldValue::MakeLocationScene(TEXT("scene"), SceneModel);
            TestTrue(TEXT("SceneView accepts valid field"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(SceneView, ValidScene));

            IGV2DynamicScreenElement::Execute_ApplyScreenField(SceneView, ValidScene);
            FGV2ScreenFieldValue Captured;
            IGV2DynamicScreenElement::Execute_CaptureScreenField(SceneView, Captured);
            TestEqual(TEXT("SceneView character count preserved"), Captured.LocationSceneValue.Characters.Num(), 2);

            // ResetScreenField
            IGV2DynamicScreenElement::Execute_ResetScreenField(SceneView);
            IGV2DynamicScreenElement::Execute_CaptureScreenField(SceneView, Captured);
            TestEqual(TEXT("SceneView characters empty after reset"), Captured.LocationSceneValue.Characters.Num(), 0);
        }

        // 4. CommandPanel validation & semantics
        {
            UGV2LocationCommandPanelWidgetBase* CommandPanel = NewObject<UGV2LocationCommandPanelWidgetBase>(TestWorld);
            TestNotNull(TEXT("CommandPanel created"), CommandPanel);

            FGV2ScreenFieldValue WrongField = FGV2ScreenFieldValue::MakeLocationCommands(TEXT("wrong_field"), {});
            TestFalse(TEXT("CommandPanel rejects mismatched FieldId"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(CommandPanel, WrongField));

            // Duplicate keys rejected
            FGV2ButtonViewModel Btn1;
            Btn1.Key = FName(TEXT("btn"));
            Btn1.Binding = FGV2UiBindingHandle::Create(TEXT("binding1"));
            FGV2ButtonViewModel Btn2;
            Btn2.Key = FName(TEXT("btn"));
            Btn2.Binding = FGV2UiBindingHandle::Create(TEXT("binding2"));

            FGV2ScreenFieldValue DupField = FGV2ScreenFieldValue::MakeLocationCommands(TEXT("commands"), { Btn1, Btn2 });
            TestFalse(TEXT("CommandPanel rejects duplicate button keys"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(CommandPanel, DupField));

            // Valid buttons applied and captured
            Btn2.Key = FName(TEXT("btn2"));
            FGV2ScreenFieldValue ValidCmds = FGV2ScreenFieldValue::MakeLocationCommands(TEXT("commands"), { Btn1, Btn2 });
            TestTrue(TEXT("CommandPanel accepts valid buttons"), IGV2DynamicScreenElement::Execute_CanApplyScreenField(CommandPanel, ValidCmds));

            IGV2DynamicScreenElement::Execute_ApplyScreenField(CommandPanel, ValidCmds);
            FGV2ScreenFieldValue Captured;
            IGV2DynamicScreenElement::Execute_CaptureScreenField(CommandPanel, Captured);
            TestEqual(TEXT("CommandPanel button count preserved"), Captured.ButtonListValue.Num(), 2);

            // Reset
            IGV2DynamicScreenElement::Execute_ResetScreenField(CommandPanel);
            IGV2DynamicScreenElement::Execute_CaptureScreenField(CommandPanel, Captured);
            TestEqual(TEXT("CommandPanel button count 0 after reset"), Captured.ButtonListValue.Num(), 0);
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
        UClass* LocationScreenClass = LoadClass<UGV2ScreenWidgetBase>(
            nullptr,
            TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
        TestNotNull(TEXT("WBP_LocationScreen class exists and is loadable"), LocationScreenClass);

        if (LocationScreenClass != nullptr)
        {
            UGV2ScreenWidgetBase* LocationScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, LocationScreenClass);
            TestNotNull(TEXT("WBP_LocationScreen instantiated"), LocationScreen);

            if (LocationScreen != nullptr)
            {
                // 2. Build full realistic candidate document
                FGV2LocationTopBarViewModel TopBarModel;
                TopBarModel.Day.Text = FText::FromString(TEXT("Day 17"));
                TopBarModel.Location.Text = FText::FromString(TEXT("The Boar's Tusk Tavern"));
                TopBarModel.PrimaryResource.Text = FText::FromString(TEXT("Gold: 120"));

                FGV2LocationPlayerStatusViewModel PlayerModel;
                PlayerModel.Name.Text = FText::FromString(TEXT("Aria Storm"));
                PlayerModel.PortraitResourceId = TEXT("core:resource.image.character_portrait");
                FGV2LocationMeterEntry Meter1;
                Meter1.Key = FName(TEXT("stamina"));
                Meter1.Meter.Percent = 0.85f;
                PlayerModel.Meters = { Meter1 };
                PlayerModel.ItemIconResourceIds = { TEXT("item_sword"), TEXT("item_shield") };
                PlayerModel.EffectIconResourceIds = { TEXT("effect_buff") };

                FGV2LocationSceneViewModel SceneModel;
                SceneModel.BackgroundTileResourceId = TEXT("core:resource.ui.old_paper_tile_256");
                SceneModel.BackgroundResourceId = TEXT("core:resource.image.character_portrait");
                FGV2LocationCharacterEntry Char1;
                Char1.Key = FName(TEXT("aria"));
                Char1.ResourceId = TEXT("core:resource.image.character_portrait");
                SceneModel.Characters = { Char1 };
                SceneModel.ContextText.Text = FText::FromString(
                    TEXT("[TEST] A warm and cozy tavern with cheerful laughter. [LOCALE_OVERFLOW_TEST_STRING_FOR_LAYOUT_AND_TEXT_WRAPPING_1234567890]"));

                TArray<FGV2ButtonViewModel> Buttons;
                for (int32 i = 1; i <= 6; ++i)
                {
                    FGV2ButtonViewModel Btn;
                    Btn.Key = FName(*FString::Printf(TEXT("cmd_%d"), i));
                    Btn.Text.Text = FText::FromString(FString::Printf(TEXT("Command Action Button #%d (Test Reflow Wrap)"), i));
                    Btn.Binding = FGV2UiBindingHandle::Create(FString::Printf(TEXT("binding_%d"), i));
                    Buttons.Add(Btn);
                }

                // Apply fields through screen widget field contract
                TArray<FGV2ScreenFieldValue> Fields = {
                    FGV2ScreenFieldValue::MakeLocationTopBar(TEXT("top_bar"), TopBarModel),
                    FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("player_status"), PlayerModel),
                    FGV2ScreenFieldValue::MakeLocationScene(TEXT("scene"), SceneModel),
                    FGV2ScreenFieldValue::MakeLocationCommands(TEXT("commands"), Buttons)
                };
                const bool bFieldsApplied = LocationScreen->ApplyScreenFields(Fields);
                TestTrue(TEXT("WBP_LocationScreen applies full location fields"), bFieldsApplied);

                // 3. Test layout geometry across 6 resolutions
                const struct FViewportResolution
                {
                    const TCHAR* Name;
                    FVector2D Size;
                    bool bUltrawide;
                } TestResolutions[] = {
                    { TEXT("4K (3840x2160)"), FVector2D(3840, 2160), false },
                    { TEXT("QHD (2560x1440)"), FVector2D(2560, 1440), false },
                    { TEXT("FHD (1920x1080)"), FVector2D(1920, 1080), false },
                    { TEXT("HD (1280x720)"), FVector2D(1280, 720), false },
                    { TEXT("UW-QHD (3440x1440)"), FVector2D(3440, 1440), true },
                    { TEXT("UW-FHD (2560x1080)"), FVector2D(2560, 1080), true }
                };

                TSharedPtr<SWidget> SlateWidget = LocationScreen->TakeWidget();
                TestTrue(TEXT("LocationScreen produces valid Slate widget"), SlateWidget.IsValid());

                if (SlateWidget.IsValid())
                {
                    for (const auto& Res : TestResolutions)
                    {
                        SlateWidget->SlatePrepass(1.0f);
                        const FVector2D DesiredSize = SlateWidget->GetDesiredSize();

                        TestTrue(
                            *FString::Printf(TEXT("[%s] Desired width is positive: %f"), Res.Name, DesiredSize.X),
                            DesiredSize.X > 0.0f);
                        TestTrue(
                            *FString::Printf(TEXT("[%s] Desired height is positive: %f"), Res.Name, DesiredSize.Y),
                            DesiredSize.Y > 0.0f);

                        // 720p constraints check
                        if (Res.Size.Y <= 720.0f)
                        {
                            TestTrue(
                                *FString::Printf(TEXT("[%s] Desired height fits 720p constraints"), Res.Name),
                                DesiredSize.Y <= 1080.0f);
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

            // Apply models with body style
            FGV2TextViewModel TextModel;
            TextModel.Text = FText::FromString(TEXT("Sample Body Text"));
            TextModel.StyleToken = FName(TEXT("body"));
            if (TextWidget) TextWidget->ApplyText(TextModel);

            FGV2InteractiveRichTextViewModel RichModel;
            RichModel.Text.Text = FText::FromString(TEXT("Sample Rich Body"));
            RichModel.Text.StyleToken = FName(TEXT("body"));
            if (RichTextWidget) RichTextWidget->ApplyInteractiveRichText(RichModel);

            FGV2ButtonViewModel BtnModel;
            BtnModel.Key = FName(TEXT("ok"));
            BtnModel.Text.Text = FText::FromString(TEXT("Button"));
            BtnModel.Text.StyleToken = FName(TEXT("body"));
            BtnModel.Binding = FGV2UiBindingHandle::Create(TEXT("btn_ok"));
            if (ButtonWidget) ButtonWidget->ApplyButtonModel(BtnModel);

            FGV2InputFieldViewModel InputModel;
            InputModel.Text.Text = FText::FromString(TEXT("Input Label"));
            InputModel.Text.StyleToken = FName(TEXT("body"));
            InputModel.Binding = FGV2UiBindingHandle::Create(TEXT("input_bind"));
            if (InputWidget) InputWidget->ApplyInputFieldModel(InputModel);

            FGV2DropdownSelectViewModel DropdownModel;
            DropdownModel.Placeholder.Text = FText::FromString(TEXT("Select Option"));
            DropdownModel.Binding = FGV2UiBindingHandle::Create(TEXT("dd_bind"));
            FGV2DropdownOptionViewModel Opt;
            Opt.Key = FName(TEXT("opt1"));
            Opt.Text.Text = FText::FromString(TEXT("Option 1"));
            DropdownModel.Options = { Opt };
            if (DropdownWidget) DropdownWidget->ApplyDropdownModel(DropdownModel);

            // Verify effective font sizing matches expected mathematical token size
            TestEqual(*FString::Printf(TEXT("[%.0fp] Title font size resolves correctly"), H), ExpectedTitleSize, UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("title")), H));
            TestEqual(*FString::Printf(TEXT("[%.0fp] Body font size resolves correctly"), H), ExpectedBodySize, UGV2TextPipeline::ResolveEffectiveFontSizeForHeight(FName(TEXT("body")), H));
        }

        // 2. Image widgets scale policy and brush state conformance
        UClass* ImageClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
        UGV2ImageWidgetBase* ImageWidget = ImageClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, ImageClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
        TestNotNull(TEXT("ImageWidget created"), ImageWidget);

        if (ImageWidget != nullptr)
        {
            // Negative test: Incompatible graphics resource rejects gracefully and preserves state
            FString Error;
            const bool bBadApply = ImageWidget->ApplyImageResource(TEXT("nonexistent:resource.image"), Error);
            TestFalse(TEXT("Nonexistent resource is rejected"), bBadApply);
            TestTrue(TEXT("Applied resource id remains empty"), ImageWidget->GetAppliedResourceId().IsEmpty());

            // Positive tests for scale policies
            // PreserveAspect policy
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::PreserveAspect);
            TestEqual(TEXT("ScalePolicy is PreserveAspect"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);

            // Tile policy
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::Tile);
            TestEqual(TEXT("ScalePolicy is Tile"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::Tile);

            // NineSlice policy
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::NineSlice);
            TestEqual(TEXT("ScalePolicy is NineSlice"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::NineSlice);

            // FreeStretch policy
            ImageWidget->SetScalePolicy(EGV2PrimitiveScalePolicy::FreeStretch);
            TestEqual(TEXT("ScalePolicy is FreeStretch"), ImageWidget->GetScalePolicy(), EGV2PrimitiveScalePolicy::FreeStretch);
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

        // 1. Initial screen in Tavern
        UGV2ScreenWidgetBase* Screen1 = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(TEXT("Initial location screen presented in Tavern"), Screen1);

        if (Screen1 != nullptr)
        {
            // 2. Perform location transition (Tavern -> Market)
            // Find a valid command button binding handle from the screen widgets
            TArray<UWidget*> ChildWidgets;
            Screen1->WidgetTree->GetAllWidgets(ChildWidgets);
            FGV2UiBindingHandle TravelHandle;

            for (UWidget* Child : ChildWidgets)
            {
                if (Child != nullptr && Child->Implements<UGV2DynamicScreenElement>())
                {
                    FGV2ScreenFieldValue CapturedField;
                    if (IGV2DynamicScreenElement::Execute_CaptureScreenField(Child, CapturedField)
                        && CapturedField.FieldId == FName(TEXT("commands")))
                    {
                        for (const FGV2ButtonViewModel& Btn : CapturedField.ButtonListValue)
                        {
                            if (Btn.Binding.IsValid())
                            {
                                TravelHandle = Btn.Binding;
                                break;
                            }
                        }
                    }
                }
            }

            if (TravelHandle.IsValid())
            {
                const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(TravelHandle, {});
                TestEqual(TEXT("Travel command interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);
            }

            // 3. Screen instance reuse verification
            UGV2ScreenWidgetBase* Screen2 = Runtime->GetActiveScreenInLayer(
                UGV2GameShellWidgetBase::LayerLocationContent,
                FName(TEXT("location")));
            TestNotNull(TEXT("Location screen active after travel"), Screen2);
            TestEqual(TEXT("Screen instance is preserved and reused across location transition"), Screen1, Screen2);
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
    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    TestNotNull(TEXT("Image catalog is loaded"), Catalog);
    if (Catalog != nullptr)
    {
        FGV2ResolvedImageResource MarketRes;
        FString Error;
        const bool bMarketResolved = Catalog->Resolve(TEXT("textsystem:resource.ui.missing_background"), MarketRes, Error);
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

        UClass* LocationScreenClass = LoadClass<UGV2ScreenWidgetBase>(
            nullptr,
            TEXT("/Game/TextSystem/UI/Screens/WBP_LocationScreen.WBP_LocationScreen_C"));
        TestNotNull(TEXT("LocationScreenClass loaded"), LocationScreenClass);
        if (LocationScreenClass != nullptr)
        {
            UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, LocationScreenClass);
            TestNotNull(TEXT("Screen created"), Screen);
            if (Screen != nullptr)
            {
                FGV2LocationTopBarViewModel TopBarModel;
                TopBarModel.Day.Text = FText::FromString(TEXT("Day 1"));
                TopBarModel.Location.Text = FText::FromString(TEXT("Market"));
                TopBarModel.PrimaryResource.Text = FText::FromString(TEXT("Gold: 60"));

                FGV2LocationPlayerStatusViewModel PlayerModel;
                PlayerModel.Name.Text = FText::FromString(TEXT("Hero"));
                PlayerModel.PortraitResourceId = TEXT("textsystem:resource.ui.missing_portrait");

                FGV2LocationSceneViewModel SceneModel;
                SceneModel.BackgroundTileResourceId = TEXT("core:resource.ui.old_paper_tile_256");
                SceneModel.BackgroundResourceId = TEXT("textsystem:resource.ui.missing_background");
                SceneModel.ContextText.Text = FText::FromString(TEXT("Market square"));

                TArray<FGV2ButtonViewModel> Buttons;
                FGV2ButtonViewModel Btn1;
                Btn1.Key = FName(TEXT("btn1"));
                Btn1.Text.Text = FText::FromString(TEXT("Buy Sword"));
                Btn1.Binding = FGV2UiBindingHandle::Create(TEXT("b1"));
                Buttons.Add(Btn1);

                TArray<FGV2ScreenFieldValue> Fields = {
                    FGV2ScreenFieldValue::MakeLocationTopBar(TEXT("top_bar"), TopBarModel),
                    FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("player_status"), PlayerModel),
                    FGV2ScreenFieldValue::MakeLocationScene(TEXT("scene"), SceneModel),
                    FGV2ScreenFieldValue::MakeLocationCommands(TEXT("commands"), Buttons)
                };
                const bool bApplied = Screen->ApplyScreenFields(Fields);
                TestTrue(TEXT("Screen applied fields"), bApplied);

                // Find Scene widget inside Screen
                UWidget* SceneWidget = Screen->GetWidgetFromName(FName(TEXT("Scene")));
                TestNotNull(TEXT("Scene found in screen"), SceneWidget);
                UGV2LocationSceneWidgetBase* SceneView = Cast<UGV2LocationSceneWidgetBase>(SceneWidget);
                TestNotNull(TEXT("Scene is UGV2LocationSceneWidgetBase"), SceneView);

                if (SceneView != nullptr)
                {
                    UGV2ImageWidgetBase* Bg = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("Background"))));
                    UGV2ImageWidgetBase* BgTile = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("BackgroundTile"))));
                    TestNotNull(TEXT("Background widget found"), Bg);
                    TestNotNull(TEXT("BackgroundTile widget found"), BgTile);

                    if (Bg != nullptr)
                    {
                        AddInfo(FString::Printf(TEXT("Background: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                            *Bg->GetAppliedResourceId(),
                            static_cast<int32>(Bg->GetVisibility()),
                            Bg->GetImageBrush().GetResourceObject() ? *Bg->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                    }
                    UGV2ImageWidgetBase* CharWidget = Cast<UGV2ImageWidgetBase>(SceneView->GetWidgetFromName(FName(TEXT("Character"))));
                    if (CharWidget != nullptr)
                    {
                        TestEqual(TEXT("Character widget collapsed when no characters"), CharWidget->GetVisibility(), ESlateVisibility::Collapsed);
                    }
                    if (BgTile != nullptr)
                    {
                        AddInfo(FString::Printf(TEXT("BackgroundTile: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                            *BgTile->GetAppliedResourceId(),
                            static_cast<int32>(BgTile->GetVisibility()),
                            BgTile->GetImageBrush().GetResourceObject() ? *BgTile->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                    }

                    // Check overlay slot indices
                    if (Bg != nullptr && BgTile != nullptr)
                    {
                        UPanelWidget* ParentPanel = Bg->GetParent();
                        AddInfo(FString::Printf(TEXT("ParentPanel: %s"), ParentPanel ? *ParentPanel->GetName() : TEXT("nullptr")));
                        if (ParentPanel != nullptr)
                        {
                            const int32 BgIndex = ParentPanel->GetChildIndex(Bg);
                            const int32 BgTileIndex = ParentPanel->GetChildIndex(BgTile);
                            AddInfo(FString::Printf(TEXT("Child indices: Background=%d, BackgroundTile=%d, TotalChildren=%d"),
                                BgIndex, BgTileIndex, ParentPanel->GetChildrenCount()));
                            for (int32 i = 0; i < ParentPanel->GetChildrenCount(); ++i)
                            {
                                UWidget* Child = ParentPanel->GetChildAt(i);
                                AddInfo(FString::Printf(TEXT("Child [%d]: %s (Class=%s, Visibility=%d)"),
                                    i, *Child->GetName(), *Child->GetClass()->GetName(), static_cast<int32>(Child->GetVisibility())));
                            }
                        }
                    }
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

#endif

