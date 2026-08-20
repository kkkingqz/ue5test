#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Application/GV2ScreenFieldAdapterRegistry.h"
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

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
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
        TEXT("Adapter registry contains the five published Screen Field schemas"),
        FGV2ScreenFieldAdapterRegistry::Get().Num(),
        5);

    FString UiDocumentContract;
    if (ReadSource(
            TEXT("Docs/UI/UIDocumentAndReconciliation.md"),
            UiDocumentContract))
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
                *FString::Printf(TEXT("UI Document contract names supported schema %s"), SchemaId),
                UiDocumentContract.Contains(SchemaId));
        }
        TestFalse(
            TEXT("UI Document contract does not claim only two field adapters"),
            UiDocumentContract.Contains(TEXT("только RichText и ButtonList")));
        TestTrue(
            TEXT("UI Document contract labels unimplemented layered tests as planned acceptance"),
            UiDocumentContract.Contains(
                TEXT("planned acceptance criteria полноценного layered reconciler")));
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
                || WidgetClass->IsChildOf(UGV2RichTextPopoverWidgetBase::StaticClass());
            TestTrue(
                *FString::Printf(
                    TEXT("Text-bearing WBP must use a Text Pipeline native base: %s"),
                    *AssetName),
                bUsesTextPipelineBase);
        }
    }
    TestEqual(TEXT("UI contract audits every current WBP asset"), WidgetBlueprintCount, 17);
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
        {TEXT("/Game/UI/Widgets/WBP_RichText.WBP_RichText_C"), UGV2RichTextWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"), UGV2ImageWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"), UGV2ButtonWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Checkbox.WBP_Checkbox_C"), UGV2CheckboxWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"), UGV2InputFieldWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"), UGV2DropdownSelectWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ButtonList.WBP_ButtonList_C"), UGV2ButtonListWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C"), UGV2ProgressBarWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Separator.WBP_Separator_C"), UGV2SeparatorWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_LoadingIndicator.WBP_LoadingIndicator_C"), UGV2LoadingIndicatorWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_RichTextPopover.WBP_RichTextPopover_C"), UGV2RichTextPopoverWidgetBase::StaticClass()},
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
    TestEqual(
        TEXT("Screen Registry contains the test screen entry"),
        Registry != nullptr ? Registry->GetEntries().Num() : 0,
        1);
    if (Registry != nullptr && Registry->GetEntries().Num() == 1)
    {
        TestEqual(
            TEXT("Screen Registry maps the canonical test screen_id"),
            Registry->GetEntries()[0].ScreenId,
            FString(TEXT("core:screen.test")));
    }
    UClass* TestScreenClass = Registry != nullptr && Registry->GetEntries().Num() == 1
        ? Registry->GetEntries()[0].WidgetClass.LoadSynchronous()
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
             TEXT("/Game/UI/Styles/BP_UIStyle_Text_Default.BP_UIStyle_Text_Default_C"),
             TEXT("/Game/UI/Styles/BP_UIStyle_ButtonLabel_Default.BP_UIStyle_ButtonLabel_Default_C")})
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
    FGV2LuaTestScreenWidgetCreation,
    "GV2.Runtime.Presentation.LuaCreatesRegisteredScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaTestScreenWidgetCreation::RunTest(const FString& Parameters)
{
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
            const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
            UGV2ScreenRegistry* Registry = RegistrySettings != nullptr
                ? RegistrySettings->RegistryAsset.LoadSynchronous()
                : nullptr;
            UClass* RegisteredClass = Registry != nullptr && Registry->GetEntries().Num() == 1
                ? Registry->GetEntries()[0].WidgetClass.LoadSynchronous()
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

#endif
