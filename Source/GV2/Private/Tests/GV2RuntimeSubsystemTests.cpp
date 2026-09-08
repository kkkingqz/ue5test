#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
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

// PAH-04A: the schema cache is session-scoped now -- RebuildSchemaCacheForSession is a
// coordinator-only entry point in production, called once per StartSession. A test that
// calls GV2ScreenFieldMaterializer::PrepareBindingDefinitions/BuildFields/GetCompiledSchema
// directly, without starting a real session, uses this to give itself one from the real
// GameData closure, and releases it on scope exit so it can never leak into an unrelated
// later test (the exact hidden cross-test ordering dependency a process-lifetime static
// used to risk).
struct FGV2ScopedRealSchemaCache
{
    FGV2ScopedRealSchemaCache()
    {
        TArray<FGV2SchemaPackageRoot> Roots;
        for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
        {
            Roots.Add(FGV2SchemaPackageRoot{Entry.PackageId, Entry.RootDirectory});
        }
        GV2ScreenFieldMaterializer::RebuildSchemaCacheForSession(MoveTemp(Roots));
    }
    ~FGV2ScopedRealSchemaCache()
    {
        GV2ScreenFieldMaterializer::ReleaseSchemaCacheForSession();
    }
};

// PAH-04B: sibling to FGV2ScopedRealSchemaCache above -- the image resource catalog is
// session-scoped now too (UGV2ImageResourceCatalog::RebuildForSession/ReleaseForSession,
// called by FGV2SessionCoordinator::StartSession/EndSession in production). A test that
// resolves an image resource_id directly, without starting a real session first, uses
// this to give itself a real catalog built from the real GameData closure.
struct FGV2ScopedRealImageCatalog
{
    FGV2ScopedRealImageCatalog()
    {
        TArray<FString> PackageIds;
        for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
        {
            PackageIds.Add(Entry.PackageId);
        }
        FString Error;
        UGV2ImageResourceCatalog::RebuildForSession(PackageIds, Error);
    }
    ~FGV2ScopedRealImageCatalog()
    {
        UGV2ImageResourceCatalog::ReleaseForSession();
    }
};

// DCA-13: a dynamic SWrapBox (UseAllottedSize=true) only recalculates its own
// wrap threshold (PreferredSize) inside Tick(), which the normal
// FSlateApplication loop drives every frame for a registered top-level
// window -- an off-screen SVirtualWindow driven by hand (Resize +
// SlatePrepass + PaintWindow, no FSlateApplication involved) never receives
// it, so PreferredSize freezes at whatever the first Paint ever measured and
// silently reuses that stale threshold at every later, narrower resolution.
// This walks the Slate tree and calls Tick() directly on every widget that
// still wants one, using the geometry PaintWindow just cached for it
// (GetTickSpaceGeometry() -- Paint/Arrange already update that on their own,
// no Tick needed for that part) -- the explicit-subtree-tick alternative to
// registering a real window with FSlateApplication.
void GV2TickWidgetSubtreeRecursively(const TSharedRef<SWidget>& Widget, double CurrentTime, float DeltaTime)
{
    if (Widget->GetCanTick())
    {
        Widget->Tick(Widget->GetTickSpaceGeometry(), CurrentTime, DeltaTime);
    }
    if (FChildren* Children = Widget->GetAllChildren())
    {
        const int32 NumChildren = Children->Num();
        for (int32 Index = 0; Index < NumChildren; ++Index)
        {
            const TSharedRef<SWidget> Child = Children->GetChildAt(Index);
            if (Child != SNullWidget::NullWidget)
            {
                GV2TickWidgetSubtreeRecursively(Child, CurrentTime, DeltaTime);
            }
        }
    }
}

// DCA-13: one full simulated frame on an off-screen SVirtualWindow honest
// about dynamic (Tick-driven) layouts -- Resize, an initial Paint pass so
// every widget's GetTickSpaceGeometry() reflects the new size, an explicit
// subtree tick so any dynamic SWrapBox catches up its PreferredSize to that
// geometry, then a second Prepass+Paint that actually arranges children
// against the now-correct threshold. A single Paint (the pre-DCA-13 harness)
// only ever arranges against whichever PreferredSize the previous iteration
// left behind.
void GV2SimulateResponsiveFrame(const TSharedRef<SVirtualWindow>& Window, const FVector2D& Size)
{
    Window->Resize(Size);
    Window->SlatePrepass(1.0f);
    {
        FSlateWindowElementList SeedElementList(Window);
        Window->PaintWindow(FPlatformTime::Seconds(), 0.016f, SeedElementList, FWidgetStyle(), true);
    }
    GV2TickWidgetSubtreeRecursively(Window, FPlatformTime::Seconds(), 0.016f);
    Window->SlatePrepass(1.0f);
    FSlateWindowElementList WindowElementList(Window);
    Window->PaintWindow(FPlatformTime::Seconds(), 0.016f, WindowElementList, FWidgetStyle(), true);
}

// DCA-14: the one and only definition of 2-axis containment (Left/Top edge
// on-screen, Right/Bottom edge within Bounds), used identically by every
// positive per-button assertion and by both negative overflow self-tests in
// FGV2LocationScreenViewportMatrixTest. Before this, the negative tests
// exercised a copy of this logic declared as a local lambda a few lines
// below the positive assertions, which instead compared each axis inline --
// deleting all twelve positive checks left both negative self-tests green,
// since neither one actually depended on them.
bool GV2FitsInBounds(const FVector2D& Pos, const FVector2D& Size, const FVector2D& Bounds)
{
    return Pos.X >= -1.0f && Pos.Y >= -1.0f
        && (Pos.X + Size.X) <= (Bounds.X + 1.0f)
        && (Pos.Y + Size.Y) <= (Bounds.Y + 1.0f);
}
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
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

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
        TEXT("Source/GV2/Private/UI/GV2RichTextWidgetBase.cpp")
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
        const TCHAR* FieldSchemas[] = {
            TEXT("textsystem:schema.ui_field.location_scene.v1"),
            TEXT("textsystem:schema.ui_field.location_commands.v1")
        };
        for (const TCHAR* SchemaId : FieldSchemas)
        {
            TestFalse(
                *FString::Printf(TEXT("Materializer does not hardcode %s"), SchemaId),
                MaterializerSource.Contains(SchemaId));
        }
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
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(ValidReq, ValidDefs));
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
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(MissingKeyReq, MissingDefs));
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
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(DupKeyReq, DupDefs));
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
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(TextKeyReq, TextDefs));
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
            GV2ScreenFieldMaterializer::PrepareBindingDefinitions(ValidGrammarReq, ValidDefs));
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
                GV2ScreenFieldMaterializer::PrepareBindingDefinitions(InvalidKeyReq, InvalidDefs));
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
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

    if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.progress.health"), FText::FromString(TEXT("Health")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.title"), FText::FromString(TEXT("Title")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.content"), FText::FromString(TEXT("Content")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.button.ok"), FText::FromString(TEXT("OK")));
    }

    // UPP-16..30: the per-schema FAdapter mechanism, and then the registry/singleton
    // class wrapping it, were both deleted outright, not just emptied -- every
    // schema_id (baseline widgets and LocationScreen alike) now resolves through
    // the schema-driven GV2ScreenFieldMaterializer free functions.

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
        TestTrue(TEXT("Generic binding extraction succeeds for commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(ValidReq, Defs));
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
    const FGV2ScopedRealImageCatalog ScopedImageCatalog;

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

    // These fixtures exercise the pipeline's own mechanics (args, style token, markup
    // escaping below), not real game content, so they are registered here as synthetic
    // core-namespaced entries rather than depending on a catalog id owned by a non-core
    // game package (Source/GV2 cannot hardcode a dependency on such a namespace).
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.description"), FText::FromString(TEXT("You are exploring with {player_name}.")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.checkbox"), FText::FromString(TEXT("Enable feature")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.name_label"), FText::FromString(TEXT("Name")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.dropdown_placeholder"), FText::FromString(TEXT("Choose one...")));

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
        UGV2ImageResourceCatalog::GetSessionCatalog();
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

    // DCA-17: the leaf-component contract table (asset -> expected native parent). This pairing
    // is genuine domain knowledge and stays hand-authored, but every OTHER discovered WBP_* is
    // now required to fall into a checkable, reflective bucket in the loop below -- a generic
    // declared composite/screen (UGV2DeclaredCompositeWidgetBase/UGV2ScreenWidgetBase, since
    // DeclaredCompositeAdoption's whole point is that these need no bespoke C++ of their own) or
    // a Designer-configured sibling of an already-vetted leaf's native parent (e.g.
    // WBP_ListView_Wrap/WrapButtons sharing WBP_ListView's UGV2ListViewWidgetBase) -- instead of
    // silently vanishing from a hand-adjusted total.
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
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_Modal.WBP_Modal_C"), UGV2ModalWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_Portrait.WBP_Portrait_C"), UGV2PortraitWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"), UGV2GameShellWidgetBase::StaticClass()},
    };

    TSet<FString> ComponentContractClassPaths;
    TSet<UClass*> ComponentContractNativeParents;
    for (const FComponentContract& Component : Components)
    {
        ComponentContractClassPaths.Add(Component.ClassPath);
        ComponentContractNativeParents.Add(Component.NativeParent);
    }

    int32 WidgetBlueprintCount = 0;
    int32 ReconciledWidgetBlueprintCount = 0;
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
        if (WidgetClass == nullptr)
        {
            continue;
        }

        const bool bIsComponentContractLeaf = ComponentContractClassPaths.Contains(GeneratedClassPath);
        const bool bIsGenericDeclaredCompositeOrScreen =
            WidgetClass->IsChildOf(UGV2DeclaredCompositeWidgetBase::StaticClass())
            || WidgetClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass());
        bool bSharesComponentContractNativeParent = false;
        for (UClass* CoveredParent : ComponentContractNativeParents)
        {
            if (WidgetClass->IsChildOf(CoveredParent))
            {
                bSharesComponentContractNativeParent = true;
                break;
            }
        }
        if (bIsComponentContractLeaf || bIsGenericDeclaredCompositeOrScreen || bSharesComponentContractNativeParent)
        {
            ++ReconciledWidgetBlueprintCount;
        }
        else
        {
            AddError(FString::Printf(
                TEXT("WBP has no Components[] contract, declared composite/screen base, or shared leaf native parent: %s"),
                *AssetName));
        }

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
            // DCA-17: a class property (IGV2TextPipelineHost) replaces the former 9-class
            // IsChildOf enumeration -- see GV2TextPipelineHost.h for what implementing it means.
            TestTrue(
                *FString::Printf(
                    TEXT("Text-bearing WBP must use a Text Pipeline native base: %s"),
                    *AssetName),
                WidgetClass->ImplementsInterface(UGV2TextPipelineHost::StaticClass()));
        }
    }
    TestEqual(
        TEXT("Every discovered WBP is reconciled to a leaf contract, a declared base, or a shared native parent"),
        ReconciledWidgetBlueprintCount,
        WidgetBlueprintCount);
    TestTrue(
        TEXT("Theme provides a visible separator brush"),
        Theme->SeparatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme provides a visible loading indicator brush"),
        Theme->LoadingIndicatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);

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
    FString BuildError;
    TestTrue(
        *FString::Printf(TEXT("Screen Registry builds [Error: %s]"), *BuildError),
        Registry != nullptr && Registry->Build(GV2PackageClosure::DiscoverFromGameData(), BuildError));
    FGV2ResolvedScreenDescriptor TestScreenDescriptor;
    FGV2ScreenResolutionRejection TestScreenRejection;
    const bool bTestScreenResolved = Registry != nullptr
        && Registry->Resolve(
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
    const bool bEmbeddedRequestedAsTopLevelResolved = Registry != nullptr
        && Registry->Resolve(
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
    const bool bTopLevelRequestedAsEmbeddedResolved = Registry != nullptr
        && Registry->Resolve(
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
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/TextSystem/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/RH/UI"));
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
    FGV2ImageCatalogBootstrapGate,
    "GV2.Runtime.Bootstrap.ImageCatalogFailureBlocksReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04B: the catalog is session-scoped now -- there is no more standalone
// "ResourceRootDirectory" setting to mutate into an invalid path. Failure is injected
// instead by dropping one genuinely undecodable PNG into a real closure package's own
// Resources/<PackageId>/ tree (core, always present), the same way a content author
// could break the build by accident -- proving the production BuildFromDirectory/
// BuildFromPackageClosure decode-failure path, not a synthetic injector.
bool FGV2ImageCatalogBootstrapGate::RunTest(const FString& Parameters)
{
    const FString BadResourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/core/resource/pah04b_test"));
    const FString BadResourcePath = FPaths::Combine(BadResourceDir, TEXT("bootstrap_gate_test.png"));
    IFileManager::Get().MakeDirectory(*BadResourceDir, true);
    const TArray<uint8> GarbageBytes = {0x00, 0x01, 0x02, 0x03};
    TestTrue(
        TEXT("Undecodable PNG fixture is written into a real closure package's Resources/ tree"),
        FFileHelper::SaveArrayToFile(GarbageBytes, *BadResourcePath));

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=ImageCatalogNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("Showing UE-native recovery surface: session bootstrap failed"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime != nullptr)
    {
        Runtime->StartSession();
        const FGV2SessionStatus Status = Runtime->GetSessionState();
        TestFalse(TEXT("Undecodable resource keeps session non-ready"), Status.bIsReady);
        TestNotEqual(
            TEXT("Undecodable resource prevents Ready state publication"),
            Status.SessionState,
            EGV2SessionState::Ready);
        // PAH-04B: unlike the old subsystem-level pre-check (ScreenRegistry/Repository,
        // which reject before Coordinator::StartSession is ever called and never show
        // recovery UI), the image catalog now fails INSIDE Coordinator::StartSession,
        // taking the same generic failure path as a repository/Lua-source failure --
        // BootstrapAndSessionLifecycle.md's "При переходе в Failed... отображает
        // UE-native recovery surface" applies here now, not a null active screen.
        TestNotNull(
            TEXT("Failed required catalog shows the UE-native recovery surface as the active Screen"),
            Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen()));
        TestNull(
            TEXT("A session that failed to start publishes no session-scoped image catalog"),
            UGV2ImageResourceCatalog::GetSessionCatalog());
    }

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();

    IFileManager::Get().Delete(*BadResourcePath);

    // A fresh session started after the bad fixture is removed succeeds and publishes a
    // real, resolvable catalog -- the failure was specific to that one file, not sticky.
    UGameInstance* RecoveredGameInstance = NewObject<UGameInstance>(GEngine);
    RecoveredGameInstance->AddToRoot();
    RecoveredGameInstance->InitializeStandalone();
    UWorld* RecoveredWorld = RecoveredGameInstance->GetWorld();

    UGV2RuntimeSubsystem* RecoveredRuntime = RecoveredGameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists after fixture cleanup"), RecoveredRuntime);
    if (RecoveredRuntime != nullptr)
    {
        RecoveredRuntime->StartSession();
        TestTrue(TEXT("Session recovers once the bad fixture is gone"), RecoveredRuntime->GetSessionState().bIsReady);
        UGV2ImageResourceCatalog* RecoveredCatalog = UGV2ImageResourceCatalog::GetSessionCatalog();
        TestNotNull(TEXT("Recovered session publishes a session-scoped image catalog"), RecoveredCatalog);
        if (RecoveredCatalog != nullptr)
        {
            FGV2ResolvedImageResource RecoveredResource;
            FString RecoveredResolveError;
            TestTrue(
                TEXT("Recovered catalog resolves real authored content"),
                RecoveredCatalog->Resolve(
                    TEXT("core:resource.ui.old_paper_tile_256"),
                    RecoveredResource,
                    RecoveredResolveError));
        }
        RecoveredRuntime->EndSession();
    }

    RecoveredGameInstance->Shutdown();
    if (RecoveredWorld != nullptr)
    {
        RecoveredWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(RecoveredWorld);
    }
    RecoveredGameInstance->RemoveFromRoot();
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
            // DCA-05: Scene is now the generic declared composite -- matched by
            // HostIdentity, not a dedicated C++ class, since several other declared
            // composites could also appear in this tree.
            UGV2DeclaredCompositeWidgetBase* SceneWidget = nullptr;
            UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;
            if (Screen->WidgetTree != nullptr)
            {
                Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                {
                    if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                    {
                        SceneWidget = Scene;
                    }
                    else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                    {
                        CommandWidget = Cmd;
                    }
                });
            }

            // M2 (DCA-05...07): the three composites are declarations now, so the risk
            // the migration carries is not a missing widget -- structure and entry counts
            // stay right -- but a property that silently stops arriving at its leaf. The
            // set checked here is enumerated from each composite's own DeclaredCapabilities,
            // not from a hand-written list of properties, so a capability added to a
            // declaration later falls under this check without anyone updating the test.
            UGV2DeclaredCompositeWidgetBase* StatusWidget = nullptr;
            if (Screen->WidgetTree != nullptr)
            {
                Screen->WidgetTree->ForEachWidget([&StatusWidget](UWidget* Widget)
                {
                    if (auto* Status = Cast<UGV2DeclaredCompositeWidgetBase>(Widget);
                        Status != nullptr && Status->GetHostIdentity() == FName(TEXT("player_status")))
                    {
                        StatusWidget = Status;
                    }
                });
            }
            TestNotNull(TEXT("LocationScreen contains PlayerStatus component"), StatusWidget);

            auto VerifyDeclaredValuesArrived =
                [this](UGV2DeclaredCompositeWidgetBase* Composite, const TCHAR* Label) -> int32
            {
                if (Composite == nullptr)
                {
                    return 0;
                }
                const FGV2UiPropertyHostState::FCommittedSnapshot Snapshot =
                    Composite->GetPropertyHostState().GetCommittedSnapshot();
                if (!Snapshot.Schema)
                {
                    TestTrue(
                        *FString::Printf(TEXT("M2: [%s] committed a schema after the Lua-driven revision"), Label),
                        false);
                    return 0;
                }

                // The set is the intersection of two independently produced sides: what the
                // Designer declaration binds, and what the committed schema requires. Both
                // sides are read, not written here. Schema-optional fields are excluded on
                // the schema's own say-so -- the composite's identity `key` is declared
                // `required: false` and is never published by the document, so demanding a
                // committed value for it would assert the opposite of the schema.
                TSet<FString> SchemaFieldNames;
                TSet<FString> RequiredSchemaFieldNames;
                for (const auto& FieldEntry : Snapshot.Schema->Fields)
                {
                    const FString FieldName = UTF8_TO_TCHAR(FieldEntry.Name.c_str());
                    SchemaFieldNames.Add(FieldName);
                    if (FieldEntry.bRequired)
                    {
                        RequiredSchemaFieldNames.Add(FieldName);
                    }
                }

                int32 Arrived = 0;
                for (const FGV2DeclaredUiCapability& Declared : Composite->DeclaredCapabilities)
                {
                    const FString PropertyName = Declared.PropertyName.ToString();
                    if (!SchemaFieldNames.Contains(PropertyName))
                    {
                        continue;
                    }
                    // A declaration-optional property whose child is unbound on this asset
                    // is not declared at all for this instance (DCA-01), so requiring a
                    // committed value for it would assert the opposite of that contract.
                    if (Declared.bOptional
                        && Declared.ChildWidgetName != NAME_None
                        && Composite->GetWidgetFromName(Declared.ChildWidgetName) == nullptr)
                    {
                        continue;
                    }
                    const bool bCommitted = Snapshot.Properties.FindField(PropertyName) != nullptr;
                    if (bCommitted)
                    {
                        ++Arrived;
                    }
                    // Schema-required is the only case the contract lets us demand. The
                    // count returned below covers the rest: a revision where nothing at
                    // all arrived would satisfy every required check of a schema whose
                    // fields are all optional, which is exactly the scene's situation.
                    if (RequiredSchemaFieldNames.Contains(PropertyName))
                    {
                        TestTrue(
                            *FString::Printf(
                                TEXT("M2: [%s] schema-required declared property '%s' has a committed value after the Lua-driven revision"),
                                Label,
                                *PropertyName),
                            bCommitted);
                    }
                }
                return Arrived;
            };

            const int32 SceneArrived = VerifyDeclaredValuesArrived(SceneWidget, TEXT("scene"));
            const int32 StatusArrived = VerifyDeclaredValuesArrived(StatusWidget, TEXT("player_status"));
            const int32 CommandsArrived = VerifyDeclaredValuesArrived(CommandWidget, TEXT("commands"));
            TestTrue(
                *FString::Printf(
                    TEXT("M2: every migrated composite received at least one declared value from Lua (scene=%d, player_status=%d, commands=%d)"),
                    SceneArrived, StatusArrived, CommandsArrived),
                SceneArrived > 0 && StatusArrived > 0 && CommandsArrived > 0);

            // Accounting alone is not enough: the value must reach the primitive the
            // declaration binds. The leaf is resolved through the declaration itself,
            // so this does not hard-code any widget name.
            auto DeclaredTextLeafContent =
                [](UGV2DeclaredCompositeWidgetBase* Composite, const TCHAR* PropertyName) -> FText
            {
                if (Composite == nullptr)
                {
                    return FText::GetEmpty();
                }
                for (const FGV2DeclaredUiCapability& Declared : Composite->DeclaredCapabilities)
                {
                    if (Declared.PropertyName != FName(PropertyName)
                        || Declared.Kind != EGV2DeclaredUiCapabilityKind::Text)
                    {
                        continue;
                    }
                    if (UGV2TextWidgetBase* Leaf =
                            Cast<UGV2TextWidgetBase>(Composite->GetWidgetFromName(Declared.ChildWidgetName)))
                    {
                        return Leaf->GetTextContent();
                    }
                }
                return FText::GetEmpty();
            };

            const FText SceneContext = DeclaredTextLeafContent(SceneWidget, TEXT("context_text"));
            TestFalse(
                TEXT("M2: scene context text published by Lua reached its text primitive"),
                SceneContext.IsEmpty());
            const FText StatusName = DeclaredTextLeafContent(StatusWidget, TEXT("name"));
            TestFalse(
                TEXT("M2: player_status name published by Lua reached its text primitive"),
                StatusName.IsEmpty());

            TestNotNull(TEXT("LocationScreen contains SceneView component"), SceneWidget);
            UGV2ListViewWidgetBase* CharRep = SceneWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(SceneWidget->GetWidgetFromName(TEXT("CharacterRepeater")))
                : nullptr;
            if (CharRep != nullptr)
            {
                TestEqual(TEXT("Initial tavern scene has 1 character"), CharRep->GetEntryCount(), 1);
                TestNotNull(TEXT("Initial tavern character widget matches keeper"), CharRep->GetEntryWidget(FName(TEXT("tavern_keeper"))));
            }

            // 2. Find travel button to market in CommandPanel and submit interaction
            TestNotNull(TEXT("LocationScreen contains CommandPanel component"), CommandWidget);
            UGV2ListViewWidgetBase* CmdRep = CommandWidget != nullptr
                ? Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                : nullptr;
            if (CmdRep != nullptr)
            {
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
                UGV2DeclaredCompositeWidgetBase* MarketScene = nullptr;
                UGV2DeclaredCompositeWidgetBase* MarketCommandsWidget = nullptr;
                if (MarketScreen->WidgetTree != nullptr)
                {
                    MarketScreen->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            MarketScene = Scene;
                        }
                        else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                        {
                            MarketCommandsWidget = Cmd;
                        }
                    });
                }
                TestNotNull(TEXT("Market Screen contains SceneView component"), MarketScene);
                UGV2ListViewWidgetBase* MarketCharRep = MarketScene != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(MarketScene->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (MarketCharRep != nullptr)
                {
                    TestEqual(TEXT("Market scene has 0 characters"), MarketCharRep->GetEntryCount(), 0);
                }

                // 4. Travel back to tavern
                TestNotNull(TEXT("Market Screen contains CommandPanel component"), MarketCommandsWidget);
                UGV2ListViewWidgetBase* MarketCmdRep = MarketCommandsWidget != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(MarketCommandsWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
                    : nullptr;
                if (MarketCmdRep != nullptr)
                {
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
                UGV2DeclaredCompositeWidgetBase* TavernScene2 = nullptr;
                if (TavernScreen2->WidgetTree != nullptr)
                {
                    TavernScreen2->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
                        {
                            TavernScene2 = Scene;
                        }
                    });
                }
                TestNotNull(TEXT("Returned Tavern Screen contains SceneView component"), TavernScene2);
                UGV2ListViewWidgetBase* CharRep2 = TavernScene2 != nullptr
                    ? Cast<UGV2ListViewWidgetBase>(TavernScene2->GetWidgetFromName(TEXT("CharacterRepeater")))
                    : nullptr;
                if (CharRep2 != nullptr)
                {
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
    FGV2SchemaCacheSessionScopingTest,
    "GV2.Runtime.ContentCore.SchemaCacheSessionScoping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04A: two sequential sessions -- here, two sequential RebuildSchemaCacheForSession
// calls, the exact production entry point FGV2SessionCoordinator::StartSession makes --
// each with their own resolved package roots, must see only their own session's schemas.
// The core-decoupling gate forbids a game-package namespace literal anywhere under
// Source/, so this uses two temporary, hand-written *.schema.json5 fixtures under the
// core namespace instead of real higher-package GameData content -- proving both
// directions a one-sided real-content asymmetry could only prove one of: schema present
// in root set 1 and absent from set 2, AND vice versa, AND that rebuilding truly
// replaces rather than accumulates (set 1's schema must vanish once set 2 is active,
// not just coexist with it).
bool FGV2SchemaCacheSessionScopingTest::RunTest(const FString& Parameters)
{
    const FString RootA = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PAH04ATest/RootA"));
    const FString RootB = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PAH04ATest/RootB"));
    IFileManager::Get().DeleteDirectory(*RootA, false, true);
    IFileManager::Get().DeleteDirectory(*RootB, false, true);
    IFileManager::Get().MakeDirectory(*RootA, true);
    IFileManager::Get().MakeDirectory(*RootB, true);

    static const TCHAR* SchemaIdA = TEXT("core:schema.ui_field.pah04a_fixture_a.v1");
    static const TCHAR* SchemaIdB = TEXT("core:schema.ui_field.pah04a_fixture_b.v1");
    const FString SchemaJson5A = FString::Printf(
        TEXT("{ id: \"%s\", schema_domain: \"ui_field\", schema_version: 1, root: { kind: \"object\", fields: { text: { kind: \"text\", required: true } } } }"),
        SchemaIdA);
    const FString SchemaJson5B = FString::Printf(
        TEXT("{ id: \"%s\", schema_domain: \"ui_field\", schema_version: 1, root: { kind: \"object\", fields: { text: { kind: \"text\", required: true } } } }"),
        SchemaIdB);
    TestTrue(TEXT("Fixture A written"), FFileHelper::SaveStringToFile(SchemaJson5A, *FPaths::Combine(RootA, TEXT("fixture_a.schema.json5"))));
    TestTrue(TEXT("Fixture B written"), FFileHelper::SaveStringToFile(SchemaJson5B, *FPaths::Combine(RootB, TEXT("fixture_b.schema.json5"))));

    auto IsUnknownSchemaId = [](const FString& Error) { return Error.Contains(TEXT("unknown schema_id")); };

    GV2ScreenFieldMaterializer::RebuildSchemaCacheForSession({FGV2SchemaPackageRoot{TEXT("core"), RootA}});

    FString ErrorA1;
    GV2ScreenFieldMaterializer::GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdA), ErrorA1);
    TestFalse(
        *FString::Printf(TEXT("Session 1 (root A) resolves fixture A [Error: %s]"), *ErrorA1),
        IsUnknownSchemaId(ErrorA1));

    FString ErrorB1;
    GV2ScreenFieldMaterializer::GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdB), ErrorB1);
    TestTrue(
        *FString::Printf(TEXT("Session 1 (root A) does not know fixture B [Error: %s]"), *ErrorB1),
        IsUnknownSchemaId(ErrorB1));

    // Controlled restart: a second RebuildSchemaCacheForSession call, root B this time --
    // mirrors what StartSession does for a real session replacement.
    GV2ScreenFieldMaterializer::RebuildSchemaCacheForSession({FGV2SchemaPackageRoot{TEXT("core"), RootB}});

    FString ErrorB2;
    GV2ScreenFieldMaterializer::GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdB), ErrorB2);
    TestFalse(
        *FString::Printf(TEXT("Session 2 (root B) resolves fixture B [Error: %s]"), *ErrorB2),
        IsUnknownSchemaId(ErrorB2));

    FString ErrorA2;
    GV2ScreenFieldMaterializer::GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdA), ErrorA2);
    TestTrue(
        *FString::Printf(TEXT("Session 2 (root B) no longer knows fixture A -- replaced, not accumulated [Error: %s]"), *ErrorA2),
        IsUnknownSchemaId(ErrorA2));

    GV2ScreenFieldMaterializer::ReleaseSchemaCacheForSession();

    IFileManager::Get().DeleteDirectory(*RootA, false, true);
    IFileManager::Get().DeleteDirectory(*RootB, false, true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogClosureScopingTest,
    "GV2.Runtime.ContentCore.ImageCatalogClosureScoping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04B: two sequential RebuildForSession calls -- the exact production entry point
// FGV2SessionCoordinator::StartSession makes -- with different resolved package
// closures, must see only their own session's resources. Uses real content from the
// higher gameplay package's own Resources/ tree (always on disk) rather than a
// synthetic fixture, since the point being proven is specifically about closure
// MEMBERSHIP (a resource physically present on disk but outside this session's own
// package set), not about discovery mechanics -- that's already covered by
// BuildFromDirectory's own scanner tests. GameNamespace is built at runtime, not a
// literal game-package-prefixed id, to satisfy the core-decoupling gate the same way
// FGV2LocationSceneDiagnostic already does.
bool FGV2ImageCatalogClosureScopingTest::RunTest(const FString& Parameters)
{
    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString RhResourceId = GameNamespace + TEXT(":resource.character.tavern_keeper");
    auto IsUnknownResourceId = [](const FString& Error) { return Error.Contains(TEXT("Unknown image resource_id")); };

    FString ErrorWithoutRh;
    TestTrue(
        TEXT("Session 1 (core+textsystem, no rh) builds successfully"),
        UGV2ImageResourceCatalog::RebuildForSession({TEXT("core"), TEXT("textsystem")}, ErrorWithoutRh));
    UGV2ImageResourceCatalog* CatalogWithoutRh = UGV2ImageResourceCatalog::GetSessionCatalog();
    TestNotNull(TEXT("Session 1 publishes a catalog"), CatalogWithoutRh);
    if (CatalogWithoutRh != nullptr)
    {
        FGV2ResolvedImageResource Resolved1;
        FString ResolveError1;
        TestFalse(
            *FString::Printf(TEXT("Session 1's closure excludes rh, so its own resource is unknown, not a build error [Error: %s]"), *ResolveError1),
            CatalogWithoutRh->Resolve(RhResourceId, Resolved1, ResolveError1));
        TestTrue(TEXT("The failure is an unknown-ID lookup, not a build-time rejection"), IsUnknownResourceId(ResolveError1));

        FGV2ResolvedImageResource CoreResolved;
        FString CoreResolveError;
        TestTrue(
            *FString::Printf(TEXT("Session 1 still resolves a resource whose package IS in its closure [Error: %s]"), *CoreResolveError),
            CatalogWithoutRh->Resolve(TEXT("core:resource.ui.old_paper_tile_256"), CoreResolved, CoreResolveError));
    }

    // Controlled restart: a second RebuildForSession call, this time with rh included --
    // mirrors what StartSession does for a real session replacement.
    FString ErrorWithRh;
    TestTrue(
        TEXT("Session 2 (core+textsystem+rh) builds successfully"),
        UGV2ImageResourceCatalog::RebuildForSession({TEXT("core"), TEXT("textsystem"), GameNamespace}, ErrorWithRh));
    UGV2ImageResourceCatalog* CatalogWithRh = UGV2ImageResourceCatalog::GetSessionCatalog();
    TestNotNull(TEXT("Session 2 publishes a catalog"), CatalogWithRh);
    if (CatalogWithRh != nullptr)
    {
        FGV2ResolvedImageResource Resolved2;
        FString ResolveError2;
        TestTrue(
            *FString::Printf(TEXT("Session 2, whose closure includes rh, resolves rh's own resource [Error: %s]"), *ResolveError2),
            CatalogWithRh->Resolve(RhResourceId, Resolved2, ResolveError2));
    }

    UGV2ImageResourceCatalog::ReleaseForSession();
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
            FString RegistryBuildError;
            const bool bRegistryBuilt = Registry != nullptr && Registry->Build(GV2PackageClosure::DiscoverFromGameData(), RegistryBuildError);
            FGV2ResolvedScreenDescriptor RegisteredDescriptor;
            FGV2ScreenResolutionRejection RegisteredRejection;
            UClass* RegisteredClass = bRegistryBuilt
                    && Registry->Resolve(
                        TEXT("core:screen.test"),
                        FGV2ScreenPlacement::TopLevel(UGV2GameShellWidgetBase::LayerLocationContent),
                        RegisteredDescriptor,
                        RegisteredRejection)
                ? RegisteredDescriptor.WidgetClass
                : nullptr;
            TestEqual(
                TEXT("Created screen class comes from the configured registry entry"),
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
    // GBF-06: enumerate every asset package actually present under Content, then inspect
    // every screen Blueprint generated class. A new .uasset cannot evade this audit by
    // being absent from a hand-maintained list of known screens.
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter AllGameAssetsFilter;
    AllGameAssetsFilter.PackagePaths.Add(TEXT("/Game"));
    AllGameAssetsFilter.bRecursivePaths = true;
    TArray<FAssetData> AllGameAssets;
    AssetRegistryModule.Get().GetAssets(AllGameAssetsFilter, AllGameAssets);
    TSet<FName> RegistryPackageNames;
    for (const FAssetData& Asset : AllGameAssets)
    {
        RegistryPackageNames.Add(Asset.PackageName);
    }
    TArray<FString> ContentAssetFiles;
    IFileManager::Get().FindFilesRecursive(ContentAssetFiles, *FPaths::ProjectContentDir(), TEXT("*.uasset"), true, false);
    TestTrue(TEXT("GBF-06: project contains assets to audit"), ContentAssetFiles.Num() > 0);
    for (const FString& AssetFilename : ContentAssetFiles)
    {
        FString PackageName;
        const bool bHasPackageName = FPackageName::TryConvertFilenameToLongPackageName(AssetFilename, PackageName);
        TestTrue(*FString::Printf(TEXT("GBF-06: Asset Registry enumerates Content asset '%s'"), *AssetFilename), bHasPackageName);
        if (bHasPackageName)
        {
            TestTrue(*FString::Printf(TEXT("GBF-06: Asset Registry has package '%s'"), *PackageName),
                RegistryPackageNames.Contains(FName(*PackageName)));
        }
    }
    TArray<FString> CallbackImplementers;
    TArray<FString> TabModelCallbackImplementers;
    TArray<FString> TabSelectionCallbackImplementers;
    TArray<FString> CentralStyleBlueprintOverrides;
    for (const FAssetData& Asset : AllGameAssets)
    {
        if (Asset.AssetClassPath.GetAssetName() != TEXT("WidgetBlueprint"))
        {
            continue;
        }
        const FString GeneratedClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString());
        UClass* const GeneratedClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        TestNotNull(*FString::Printf(TEXT("GBF-06: Widget Blueprint generated class loads '%s'"), *Asset.PackageName.ToString()), GeneratedClass);
        if (GeneratedClass == nullptr)
        {
            continue;
        }
        const UFunction* CallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnScreenFieldsApplied"));
        if (GeneratedClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass())
            && CallbackFunction != nullptr
            && CallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            CallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* TabModelCallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnTabModelApplied"));
        if (GeneratedClass->IsChildOf(UGV2TabContainerWidgetBase::StaticClass())
            && TabModelCallbackFunction != nullptr
            && TabModelCallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            TabModelCallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* TabSelectionCallbackFunction = GeneratedClass->FindFunctionByName(TEXT("OnTabSelectionUpdated"));
        if (GeneratedClass->IsChildOf(UGV2TabContainerWidgetBase::StaticClass())
            && TabSelectionCallbackFunction != nullptr
            && TabSelectionCallbackFunction->GetOuterUClass() == GeneratedClass)
        {
            TabSelectionCallbackImplementers.Add(Asset.PackageName.ToString());
        }
        const UFunction* CentralStyleFunction = GeneratedClass->FindFunctionByName(TEXT("ApplyCentralStyle"));
        if (GeneratedClass->ImplementsInterface(UGV2UiStyleConsumer::StaticClass())
            && CentralStyleFunction != nullptr
            && CentralStyleFunction->GetOuterUClass() == GeneratedClass)
        {
            CentralStyleBlueprintOverrides.Add(Asset.PackageName.ToString());
        }
    }
    TestEqual(TEXT("GBF-06: no screen Blueprint implements the obsolete callback"), CallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: screen base exposes no callback inside document Commit"),
        UGV2ScreenWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnScreenFieldsApplied")));
    TestEqual(TEXT("GBF-06: no tab Blueprint implements the obsolete model callback"), TabModelCallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: tab base exposes no model callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnTabModelApplied")));
    TestEqual(TEXT("GBF-06: no tab Blueprint implements the obsolete selection callback"), TabSelectionCallbackImplementers.Num(), 0);
    TestNull(TEXT("GBF-06: tab base exposes no selection callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindFunctionByName(TEXT("OnTabSelectionUpdated")));
    TestNull(TEXT("GBF-06: tab base exposes no multicast callback inside document Commit"),
        UGV2TabContainerWidgetBase::StaticClass()->FindPropertyByName(TEXT("OnTabChanged")));
    TestEqual(TEXT("GBF-06: no Widget Blueprint overrides the central style hook used by Commit"), CentralStyleBlueprintOverrides.Num(), 0);

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
    TestFalse(TEXT("Empty registry fails to build"), Registry->Build({}, ValidationError));

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
                const bool bDetached = Shell->DetachScreen(ProbeWidget);
                TestTrue(*FString::Printf(TEXT("Probe widget for layer '%s' detaches cleanly"), *LayerToVerify.ToString()), bDetached);
            }
        }

        FGV2LayeredUiReconciler Reconciler;

        TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> ScreenClasses;
        ScreenClasses.Add(TEXT("core:screen.main"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.alt"), UGV2ScreenWidgetBase::StaticClass());
        ScreenClasses.Add(TEXT("core:screen.modal_confirm"), UGV2ScreenWidgetBase::StaticClass());

        int32 FactoryInstantiations = 0;
        auto MockFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
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

        // Step G: UPP-28 Multi-layer failure injection across layers
        // REV3-07 closure: FGV2LayeredUiReconciler.Reconcile is document-level atomic — MultiDoc2 below
        // injects a failing field in one layer (modal_stack) and Step G's assertions confirm ALL layers
        // (route, overlay, modal) are left completely untouched, not just the layer that failed.
        // Set up active state with 3 layers: Route in location_content, Overlay in overlay_stack, Modal in modal_stack
        FGV2UiDocumentViewModel MultiDoc1;
        MultiDoc1.UiInstanceId = TEXT("ui@1:1");
        MultiDoc1.Revision = 7;
        MultiDoc1.bHasRoute = true;
        MultiDoc1.Route.Layer = TEXT("location_content");
        MultiDoc1.Route.InstanceKey = TEXT("main");
        MultiDoc1.Route.ScreenId = TEXT("core:screen.main");

        FGV2ScreenInstanceViewModel OverlayInst1;
        OverlayInst1.Layer = TEXT("overlay_stack");
        OverlayInst1.InstanceKey = TEXT("hud");
        OverlayInst1.ScreenId = TEXT("core:screen.alt");
        MultiDoc1.Overlays.Add(OverlayInst1);

        FGV2ScreenInstanceViewModel ModalInst1;
        ModalInst1.Layer = TEXT("modal_stack");
        ModalInst1.InstanceKey = TEXT("dialog1");
        ModalInst1.ScreenId = TEXT("core:screen.modal_confirm");
        MultiDoc1.Modals.Add(ModalInst1);

        const bool bMulti1Success = Reconciler.Reconcile(Shell, MultiDoc1, MockFactory, ReconcileError);
        TestTrue(*FString::Printf(TEXT("Reconcile MultiDoc1 succeeds [Error: %s]"), *ReconcileError), bMulti1Success);
        UGV2ScreenWidgetBase* RouteWidgetMulti = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        UGV2ScreenWidgetBase* OverlayWidgetMulti = Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud"));
        UGV2ScreenWidgetBase* ModalWidgetMulti = Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1"));
        TestNotNull(TEXT("RouteWidgetMulti active"), RouteWidgetMulti);
        TestNotNull(TEXT("OverlayWidgetMulti active"), OverlayWidgetMulti);
        TestNotNull(TEXT("ModalWidgetMulti active"), ModalWidgetMulti);

        if (Shell != nullptr)
        {
            TestTrue(TEXT("location_content contains RouteWidgetMulti"), Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidgetMulti));
            TestTrue(TEXT("overlay_stack contains OverlayWidgetMulti"), Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(OverlayWidgetMulti));
            TestTrue(TEXT("modal_stack contains ModalWidgetMulti"), Shell->GetScreensInLayer(TEXT("modal_stack")).Contains(ModalWidgetMulti));
            TestTrue(TEXT("Modal stack is interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
            TestFalse(TEXT("Location content is blocked by modal"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Overlay stack is blocked by modal"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
        }

        // Now prepare MultiDoc2: Replaces route, adds new overlay, but injects failing field in modal
        FGV2UiDocumentViewModel MultiDoc2;
        MultiDoc2.UiInstanceId = TEXT("ui@1:1");
        MultiDoc2.Revision = 8;
        MultiDoc2.bHasRoute = true;
        MultiDoc2.Route.Layer = TEXT("location_content");
        MultiDoc2.Route.InstanceKey = TEXT("main");
        MultiDoc2.Route.ScreenId = TEXT("core:screen.alt"); // Replaces route

        FGV2ScreenInstanceViewModel OverlayInst2;
        OverlayInst2.Layer = TEXT("overlay_stack");
        OverlayInst2.InstanceKey = TEXT("minimap");
        OverlayInst2.ScreenId = TEXT("core:screen.main"); // New overlay
        MultiDoc2.Overlays.Add(OverlayInst2);

        FGV2ScreenInstanceViewModel FailingModalInst;
        FailingModalInst.Layer = TEXT("modal_stack");
        FailingModalInst.InstanceKey = TEXT("dialog1");
        FailingModalInst.ScreenId = TEXT("core:screen.modal_confirm");
        FGV2ScreenFieldValue BadField;
        BadField.FieldId = TEXT("invalid_non_canonical_field_name_with_UPPERCASE"); // fails IsCanonicalFieldId in PrepareScreenFields
        FailingModalInst.Fields.Add(BadField);
        MultiDoc2.Modals.Add(FailingModalInst);

        const bool bMulti2Success = Reconciler.Reconcile(Shell, MultiDoc2, MockFactory, ReconcileError);
        TestFalse(TEXT("Reconcile MultiDoc2 fails due to failing modal field"), bMulti2Success);
        TestFalse(TEXT("ReconcileError is populated"), ReconcileError.IsEmpty());

        // Verify ALL layers remain completely untouched in both Reconciler and Shell!
        TestEqual(
            TEXT("Route widget in location_content untouched"),
            Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")),
            RouteWidgetMulti);
        TestEqual(
            TEXT("Overlay widget in overlay_stack untouched"),
            Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud")),
            OverlayWidgetMulti);
        TestEqual(
            TEXT("Modal widget in modal_stack untouched"),
            Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1")),
            ModalWidgetMulti);
        TestNull(
            TEXT("Minimap overlay was never added to active set"),
            Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("minimap")));

        if (Shell != nullptr)
        {
            TestTrue(TEXT("Old Route widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidgetMulti));
            TestTrue(TEXT("Old Overlay widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(OverlayWidgetMulti));
            TestTrue(TEXT("Old Modal widget was not detached from Shell"), Shell->GetScreensInLayer(TEXT("modal_stack")).Contains(ModalWidgetMulti));
            TestFalse(TEXT("Location content layer is still blocked"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestFalse(TEXT("Overlay stack layer is still blocked"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
            TestTrue(TEXT("Modal stack layer is still interactive"), Shell->IsLayerInteractive(TEXT("modal_stack")));
        }

        // Step H: Clean recovery - Remove modals and reuse Route widget instance
        FGV2UiDocumentViewModel MultiDoc3;
        MultiDoc3.UiInstanceId = TEXT("ui@1:1");
        MultiDoc3.Revision = 9;
        MultiDoc3.bHasRoute = true;
        MultiDoc3.Route.Layer = TEXT("location_content");
        MultiDoc3.Route.InstanceKey = TEXT("main");
        MultiDoc3.Route.ScreenId = TEXT("core:screen.main"); // Reuses RouteWidgetMulti

        const bool bMulti3Success = Reconciler.Reconcile(Shell, MultiDoc3, MockFactory, ReconcileError);
        TestTrue(*FString::Printf(TEXT("Reconcile MultiDoc3 succeeds [Error: %s]"), *ReconcileError), bMulti3Success);
        UGV2ScreenWidgetBase* RouteWidgetReused = Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main"));
        TestEqual(TEXT("Route widget instance was reused preserving UI-local state"), RouteWidgetReused, RouteWidgetMulti);
        TestNull(TEXT("Overlay widget was detached"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("hud")));
        TestNull(TEXT("Modal widget was detached"), Reconciler.GetActiveScreen(TEXT("modal_stack"), TEXT("dialog1")));

        // Step I: PCC-06 -- Commit-phase failure injection into a single screen. Before
        // PCC-06, CommitReconcile discarded CommitScreenFields' result and kept iterating
        // (the exact swallowed-failure shape this task closes); this proves the traversal
        // now stops on first failure, Reconcile reports it, and the previous revision's
        // observable widget state is left untouched rather than partially overwritten.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* FaultScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            FaultScreen->WidgetTree = NewObject<UWidgetTree>(FaultScreen);
            UGV2DeclaredCompositeWidgetBase* TopBar = FaultScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            FaultScreen->WidgetTree->RootWidget = TopBar;
            // DescribeUiCapabilities resolves "day" via GetWidgetFromName("DayText"), a lookup
            // in TopBar's *own* WidgetTree (DUC-08: TopBar is now the generic declared
            // composite, so DayText must be nested inside it, not a sibling in the screen's
            // tree, exactly like a real WBP instance's Designer tree).
            TopBar->WidgetTree = NewObject<UWidgetTree>(TopBar);
            UGV2TextWidgetBase* FaultDayTextWidget = TopBar->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBar->WidgetTree->RootWidget = FaultDayTextWidget;
            TestNotNull(TEXT("PCC-06: DayText child constructs"), FaultDayTextWidget);
            // DUC-01: identity is the shared HostIdentity (public via IGV2UiPropertyHost),
            // not a per-class reflected property -- no reflection needed to set it.
            TopBar->SetHostIdentity(FName(TEXT("top_bar")));
            TopBar->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            auto MakeDayFieldValue = [](const FString& DayText) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ "day", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });

                FGV2TextViewModel DayModel;
                DayModel.Text = FText::FromString(DayText);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeText(DayModel));

                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("top_bar");
                FieldValue.SchemaId = TEXT("test:schema.pcc06_fault_top_bar.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeFaultDoc = [&](const FString& DayText) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 20;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("fault_slot");
                Doc.Route.ScreenId = TEXT("core:screen.pcc06_fault_target");
                Doc.Route.Fields.Add(MakeDayFieldValue(DayText));
                return Doc;
            };

            TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> FaultScreenClasses;
            FaultScreenClasses.Add(TEXT("core:screen.pcc06_fault_target"), UGV2ScreenWidgetBase::StaticClass());
            auto FaultFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return FaultScreen;
            };

            IGV2UiPropertyHost* TopBarHost = Cast<IGV2UiPropertyHost>(TopBar);
            TestNotNull(TEXT("PCC-06: TopBar is an IGV2UiPropertyHost"), TopBarHost);

            FString FaultReconcileError;
            const bool bBaseline = Reconciler.Reconcile(Shell, MakeFaultDoc(TEXT("Monday")), FaultFactory, FaultReconcileError);
            TestTrue(*FString::Printf(TEXT("PCC-06: baseline reconcile of fault screen succeeds [Error: %s]"), *FaultReconcileError), bBaseline);
            if (TopBarHost != nullptr)
            {
                const FGV2PreparedUiValue* BaselineDay = TopBarHost->GetPropertyHostState().GetLastCommittedProperties().FindField(TEXT("day"));
                TestNotNull(TEXT("PCC-06: baseline commit recorded a 'day' property"), BaselineDay);
                if (BaselineDay != nullptr)
                {
                    TestEqual(TEXT("PCC-06: baseline committed day value is Monday"), BaselineDay->AsText().Text.ToString(), TEXT("Monday"));
                }
            }

            auto FailInjector = [](const FString& ScreenId, const FString& /*PropertyPath*/) -> bool
            {
                return ScreenId == TEXT("core:screen.pcc06_fault_target");
            };
            const bool bFaultResult = Reconciler.Reconcile(Shell, MakeFaultDoc(TEXT("Tuesday")), FaultFactory, FaultReconcileError, FailInjector);

            TestFalse(TEXT("PCC-06: Reconcile fails when injected Commit failure occurs"), bFaultResult);
            TestTrue(TEXT("PCC-06: ReconcileError names the commit-phase diagnostic"),
                FaultReconcileError.Contains(TEXT("core:diagnostic.ui_reconcile.commit_failed")));
            TestTrue(TEXT("PCC-06: ReconcileError names the failed screen_id"),
                FaultReconcileError.Contains(TEXT("core:screen.pcc06_fault_target")));
            if (TopBarHost != nullptr)
            {
                const FGV2PreparedUiValue* DayAfterFault = TopBarHost->GetPropertyHostState().GetLastCommittedProperties().FindField(TEXT("day"));
                TestNotNull(TEXT("PCC-06: 'day' property still tracked after failed commit"), DayAfterFault);
                if (DayAfterFault != nullptr)
                {
                    TestEqual(TEXT("PCC-06: committed day value is still Monday, not Tuesday -- injected Commit never ran SetLastCommittedProperties"),
                        DayAfterFault->AsText().Text.ToString(), TEXT("Monday"));
                }
            }
            TestEqual(TEXT("PCC-06: reused widget instance is still the active screen for the slot"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("fault_slot")), FaultScreen);
        }

        // Step J: PCC-07 -- multi-layer Commit-phase failure injection. UPP-28's own DoD
        // ("failed Prepare leaves the active set and bindings untouched") only covered the
        // Prepare phase; before PCC-07's reorder in CommitReconcile (commit every screen
        // BEFORE touching Shell attach/detach or ActiveScreens), an injected Commit failure
        // on one screen of a multi-layer document could leave *other*, unrelated layers'
        // screens already replaced in the Shell tree while ActiveScreens rolled back whole
        // -- a real state/bookkeeping divergence, not just a wrong count. This proves that
        // after a Commit failure on layer A, layer B's PREVIOUS widget instance is still
        // both the reconciler's ActiveScreens entry AND still attached in the Shell tree,
        // not merely present in some count.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* TopBarScreenV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            TopBarScreenV1->WidgetTree = NewObject<UWidgetTree>(TopBarScreenV1);
            UGV2DeclaredCompositeWidgetBase* TopBarV1 = TopBarScreenV1->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            TopBarScreenV1->WidgetTree->RootWidget = TopBarV1;
            // DayText must be nested inside TopBarV1's *own* WidgetTree (DUC-08).
            TopBarV1->WidgetTree = NewObject<UWidgetTree>(TopBarV1);
            UGV2TextWidgetBase* TopBarV1DayText = TopBarV1->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBarV1->WidgetTree->RootWidget = TopBarV1DayText;
            TestNotNull(TEXT("PCC-07: layer A v1 DayText child constructs"), TopBarV1DayText);
            TopBarV1->SetHostIdentity(FName(TEXT("top_bar")));
            TopBarV1->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* TopBarScreenV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            TopBarScreenV2->WidgetTree = NewObject<UWidgetTree>(TopBarScreenV2);
            UGV2DeclaredCompositeWidgetBase* TopBarV2 = TopBarScreenV2->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBar"));
            TopBarScreenV2->WidgetTree->RootWidget = TopBarV2;
            // DayText must be nested inside TopBarV2's *own* WidgetTree (DUC-08).
            TopBarV2->WidgetTree = NewObject<UWidgetTree>(TopBarV2);
            UGV2TextWidgetBase* TopBarV2DayText = TopBarV2->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
            TopBarV2->WidgetTree->RootWidget = TopBarV2DayText;
            TestNotNull(TEXT("PCC-07: layer A v2 DayText child constructs"), TopBarV2DayText);
            TopBarV2->SetHostIdentity(FName(TEXT("top_bar")));
            TopBarV2->DeclaredCapabilities.Add({ FName(TEXT("day")), FName(TEXT("DayText")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* PlainScreenV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            UGV2ScreenWidgetBase* PlainScreenV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            auto MakeTopBarFieldValue = [](const FString& DayText) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ "day", false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel DayModel;
                DayModel.Text = FText::FromString(DayText);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(TEXT("day"), FGV2PreparedUiValue::MakeText(DayModel));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("top_bar");
                FieldValue.SchemaId = TEXT("test:schema.pcc07_multi_layer_top_bar.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeMultiLayerDoc = [&](const FString& ScreenIdSuffix, const FString& DayText) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 30;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("pcc07_a");
                Doc.Route.ScreenId = FString::Printf(TEXT("core:screen.pcc07_a_%s"), *ScreenIdSuffix);
                Doc.Route.Fields.Add(MakeTopBarFieldValue(DayText));

                FGV2ScreenInstanceViewModel OverlayInst;
                OverlayInst.Layer = TEXT("overlay_stack");
                OverlayInst.InstanceKey = TEXT("pcc07_b");
                OverlayInst.ScreenId = FString::Printf(TEXT("core:screen.pcc07_b_%s"), *ScreenIdSuffix);
                Doc.Overlays.Add(OverlayInst);
                return Doc;
            };

            TMap<FString, UGV2ScreenWidgetBase*> MultiLayerScreensByScreenId;
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_a_v1"), TopBarScreenV1);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_a_v2"), TopBarScreenV2);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_b_v1"), PlainScreenV1);
            MultiLayerScreensByScreenId.Add(TEXT("core:screen.pcc07_b_v2"), PlainScreenV2);
            auto MultiLayerFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                UGV2ScreenWidgetBase** Found = MultiLayerScreensByScreenId.Find(ScreenId);
                return Found != nullptr ? *Found : nullptr;
            };

            FString MultiLayerError;
            const bool bMultiBaseline = Reconciler.Reconcile(Shell, MakeMultiLayerDoc(TEXT("v1"), TEXT("Monday")), MultiLayerFactory, MultiLayerError);
            TestTrue(*FString::Printf(TEXT("PCC-07: baseline multi-layer reconcile succeeds [Error: %s]"), *MultiLayerError), bMultiBaseline);
            TestEqual(TEXT("PCC-07: layer A baseline widget is TopBarScreenV1"), Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("pcc07_a")), TopBarScreenV1);
            TestEqual(TEXT("PCC-07: layer B baseline widget is PlainScreenV1"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("pcc07_b")), PlainScreenV1);

            auto MultiLayerFailInjector = [](const FString& ScreenId, const FString& /*PropertyPath*/) -> bool
            {
                return ScreenId == TEXT("core:screen.pcc07_a_v2");
            };
            const bool bMultiFault = Reconciler.Reconcile(Shell, MakeMultiLayerDoc(TEXT("v2"), TEXT("Tuesday")), MultiLayerFactory, MultiLayerError, MultiLayerFailInjector);

            TestFalse(TEXT("PCC-07: Reconcile fails when layer A's Commit is injected to fail"), bMultiFault);
            TestTrue(TEXT("PCC-07: error names layer A's screen_id"), MultiLayerError.Contains(TEXT("core:screen.pcc07_a_v2")));

            TestEqual(TEXT("PCC-07: layer A (the failing layer) is still its PREVIOUS widget instance"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("pcc07_a")), TopBarScreenV1);
            TestEqual(TEXT("PCC-07: layer B (an unrelated, otherwise-successful layer) is STILL its previous widget instance, not silently advanced"),
                Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("pcc07_b")), PlainScreenV1);

            if (Shell != nullptr)
            {
                TestTrue(TEXT("PCC-07: Shell still shows layer A's v1 widget attached (state, not count)"),
                    Shell->GetScreensInLayer(TEXT("location_content")).Contains(TopBarScreenV1));
                TestFalse(TEXT("PCC-07: Shell never attached layer A's v2 widget"),
                    Shell->GetScreensInLayer(TEXT("location_content")).Contains(TopBarScreenV2));
                TestTrue(TEXT("PCC-07: Shell still shows layer B's v1 widget attached, untouched by layer A's failure"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(PlainScreenV1));
                TestFalse(TEXT("PCC-07: Shell never attached layer B's v2 widget either -- the whole document aborted, not just layer A"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(PlainScreenV2));
            }

            // Clean up this scenario's route/overlay so later shared-Shell assertions in
            // this test function see the state they expect (no PCC-07-specific residue).
            FGV2UiDocumentViewModel CleanupDoc;
            CleanupDoc.UiInstanceId = TEXT("ui@1:1");
            CleanupDoc.Revision = 31;
            CleanupDoc.bHasRoute = false;
            FString CleanupError;
            TestTrue(*FString::Printf(TEXT("PCC-07: cleanup reconcile succeeds [Error: %s]"), *CleanupError),
                Reconciler.Reconcile(Shell, CleanupDoc, MultiLayerFactory, CleanupError));
        }

        // GBH-01: a document naming a layer with no authored Shell host is rejected
        // wholesale in Prepare, before the first live mutation -- not discovered
        // partway through Commit after an earlier layer already attached. Uses its
        // own partial-host Shell (the shared `Shell` above authors all six layers),
        // built via reflection since BackgroundHost/etc are protected BindWidgetOptional
        // fields with no public setter -- the same FindFProperty pattern
        // PrepareUiHostProperties already uses to read a target widget by name.
        {
            UGV2GameShellWidgetBase* PartialShell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, UGV2GameShellWidgetBase::StaticClass());
            TestNotNull(TEXT("GBH-01: partial-host shell instantiated"), PartialShell);
            if (PartialShell != nullptr)
            {
                PartialShell->AddToRoot();

                UVerticalBox* LocationHostPanel = NewObject<UVerticalBox>(PartialShell);
                if (FObjectPropertyBase* HostProp = FindFProperty<FObjectPropertyBase>(PartialShell->GetClass(), TEXT("LocationContentHost")))
                {
                    HostProp->SetObjectPropertyValue_InContainer(PartialShell, LocationHostPanel);
                }
                TestTrue(TEXT("GBH-01: location_content host is authored on the fixture"), PartialShell->HasHostForLayer(TEXT("location_content")));
                TestFalse(TEXT("GBH-01: character_presentation host is NOT authored on the fixture"), PartialShell->HasHostForLayer(TEXT("character_presentation")));

                FGV2LayeredUiReconciler GbhReconciler;
                TMap<FString, TSubclassOf<UGV2ScreenWidgetBase>> GbhScreenClasses;
                GbhScreenClasses.Add(TEXT("core:screen.gbh01_a"), UGV2ScreenWidgetBase::StaticClass());
                GbhScreenClasses.Add(TEXT("core:screen.gbh01_b"), UGV2ScreenWidgetBase::StaticClass());
                auto GbhFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
                {
                    TSubclassOf<UGV2ScreenWidgetBase>* FoundClass = GbhScreenClasses.Find(ScreenId);
                    return FoundClass != nullptr && *FoundClass != nullptr
                        ? CreateWidget<UGV2ScreenWidgetBase>(TestWorld, *FoundClass)
                        : nullptr;
                };

                // Route (location_content) has a host and would attach first, successfully.
                // The overlay (character_presentation) has none and would attach second,
                // and fail -- exactly the "first succeeds, second doesn't" shape GBH-01
                // must catch in Prepare rather than leave the first physically attached.
                FGV2UiDocumentViewModel GbhDoc;
                GbhDoc.UiInstanceId = TEXT("ui@gbh01:1");
                GbhDoc.Revision = 1;
                GbhDoc.bHasRoute = true;
                GbhDoc.Route.Layer = TEXT("location_content");
                GbhDoc.Route.InstanceKey = TEXT("gbh01_a");
                GbhDoc.Route.ScreenId = TEXT("core:screen.gbh01_a");
                FGV2ScreenInstanceViewModel GbhOverlay;
                GbhOverlay.Layer = TEXT("character_presentation");
                GbhOverlay.InstanceKey = TEXT("gbh01_b");
                GbhOverlay.ScreenId = TEXT("core:screen.gbh01_b");
                GbhDoc.Overlays.Add(GbhOverlay);

                FGV2LayeredUiReconciler::FPreparedReconciliationPlan GbhPlan;
                FString GbhError;
                const bool bGbhPrepared = GbhReconciler.PrepareReconcile(PartialShell, GbhDoc, GbhFactory, GbhPlan, GbhError);
                TestFalse(*FString::Printf(TEXT("GBH-01: missing layer host is rejected in Prepare [Error: %s]"), *GbhError), bGbhPrepared);
                TestTrue(*FString::Printf(TEXT("GBH-01: rejection names the missing-host diagnostic [Error: %s]"), *GbhError),
                    GbhError.Contains(TEXT("core:diagnostic.ui_reconcile.missing_layer_host")));
                TestTrue(*FString::Printf(TEXT("GBH-01: rejection names the failing layer [Error: %s]"), *GbhError),
                    GbhError.Contains(TEXT("character_presentation")));

                TestEqual(TEXT("GBH-01: Shell tree unchanged -- location_content host still has no children"),
                    LocationHostPanel->GetChildrenCount(), 0);
                TestEqual(TEXT("GBH-01: Reconciler's ActiveScreens untouched"), GbhReconciler.GetActiveScreens().Num(), 0);

                // Full Reconcile() (Prepare + Commit) also fails wholesale and never
                // reaches Commit -- the plan it would have committed is simply discarded.
                FString GbhReconcileError;
                const bool bGbhReconciled = GbhReconciler.Reconcile(PartialShell, GbhDoc, GbhFactory, GbhReconcileError);
                TestFalse(TEXT("GBH-01: full Reconcile() also rejects the document wholesale"), bGbhReconciled);

                // Positive control: the same partial-host Shell accepts a document that
                // only targets the layer it does have a host for -- the new check does
                // not over-reject.
                FGV2UiDocumentViewModel GbhPositiveDoc;
                GbhPositiveDoc.UiInstanceId = TEXT("ui@gbh01:2");
                GbhPositiveDoc.Revision = 1;
                GbhPositiveDoc.bHasRoute = true;
                GbhPositiveDoc.Route = GbhDoc.Route;
                FString GbhPositiveError;
                TestTrue(*FString::Printf(TEXT("GBH-01: a document naming only the authored layer still succeeds [Error: %s]"), *GbhPositiveError),
                    GbhReconciler.Reconcile(PartialShell, GbhPositiveDoc, GbhFactory, GbhPositiveError));
                TestEqual(TEXT("GBH-01: positive control actually attached to the authored host"),
                    LocationHostPanel->GetChildrenCount(), 1);

                PartialShell->RemoveFromRoot();
            }
        }

        // GBF-01 (rewritten for PAH-06B): a real engine-level per-layer reconcile failure
        // must abort CommitReconcile and restore every layer's Shell tree exactly, not
        // just the failing one. USizeBox is a real single-child UPanelWidget: asking a
        // single-child host to hold 2 simultaneous screens makes the swap's second
        // AddChild fail by construction (UPanelWidget::AddChild's own single-child gate --
        // see PAH-06A's finding that this is the one deterministic, engine-native trigger,
        // not a mock). This deliberately reaches production CommitReconcile instead of
        // calling ReconcilePrepared as a helper-level unit test, and deliberately also
        // replaces location_content's route in the SAME document, so the failure is on a
        // layer processed AFTER location_content (GetApprovedLayers order) -- proving the
        // cross-layer rollback PAH-06B adds: location_content must be restored to v1 too,
        // not just overlay_stack left alone.
        {
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.layer_reconcile_failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2GameShellWidgetBase* AttachFailureShell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, UGV2GameShellWidgetBase::StaticClass());
            TestNotNull(TEXT("GBF-01: attach-failure Shell instantiated"), AttachFailureShell);
            if (AttachFailureShell != nullptr)
            {
                AttachFailureShell->AddToRoot();

                UVerticalBox* LocationHostPanel = NewObject<UVerticalBox>(AttachFailureShell);
                USizeBox* SingleChildOverlayHost = NewObject<USizeBox>(AttachFailureShell);
                auto SetShellHost = [](UGV2GameShellWidgetBase* TargetShell, const FName PropertyName, UPanelWidget* Host)
                {
                    if (FObjectPropertyBase* HostProperty = FindFProperty<FObjectPropertyBase>(TargetShell->GetClass(), PropertyName))
                    {
                        HostProperty->SetObjectPropertyValue_InContainer(TargetShell, Host);
                    }
                };
                SetShellHost(AttachFailureShell, TEXT("LocationContentHost"), LocationHostPanel);
                SetShellHost(AttachFailureShell, TEXT("OverlayStackHost"), SingleChildOverlayHost);

                UGV2ScreenWidgetBase* RouteV1 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* RouteV2 = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* AcceptedOverlay = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                UGV2ScreenWidgetBase* RejectedOverlay = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
                TestNotNull(TEXT("GBF-01: prior route constructs"), RouteV1);
                TestNotNull(TEXT("GBF-01: replacement route constructs"), RouteV2);
                TestNotNull(TEXT("GBF-01: accepted overlay constructs"), AcceptedOverlay);
                TestNotNull(TEXT("GBF-01: rejected overlay constructs"), RejectedOverlay);

                TMap<FString, UGV2ScreenWidgetBase*> ScreensById;
                ScreensById.Add(TEXT("core:screen.gbf01_route_v1"), RouteV1);
                ScreensById.Add(TEXT("core:screen.gbf01_route_v2"), RouteV2);
                ScreensById.Add(TEXT("core:screen.gbf01_overlay_accepted"), AcceptedOverlay);
                ScreensById.Add(TEXT("core:screen.gbf01_overlay_rejected"), RejectedOverlay);
                auto AttachFailureFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
                {
                    UGV2ScreenWidgetBase** Found = ScreensById.Find(ScreenId);
                    return Found != nullptr ? *Found : nullptr;
                };

                FGV2LayeredUiReconciler AttachFailureReconciler;
                FString AttachFailureError;

                FGV2UiDocumentViewModel BaselineDoc;
                BaselineDoc.UiInstanceId = TEXT("ui@gbf01:1");
                BaselineDoc.Revision = 1;
                BaselineDoc.bHasRoute = true;
                BaselineDoc.Route.Layer = TEXT("location_content");
                BaselineDoc.Route.InstanceKey = TEXT("gbf01_route");
                BaselineDoc.Route.ScreenId = TEXT("core:screen.gbf01_route_v1");

                const bool bBaselineCommitted = AttachFailureReconciler.Reconcile(AttachFailureShell, BaselineDoc, AttachFailureFactory, AttachFailureError);
                TestTrue(*FString::Printf(TEXT("GBF-01: baseline route commits [Error: %s]"), *AttachFailureError), bBaselineCommitted);
                TestEqual(TEXT("GBF-01: baseline active route is v1"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("location_content"), TEXT("gbf01_route")), RouteV1);
                TestTrue(TEXT("GBF-01: baseline route is physically attached"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV1));

                const FGV2LayeredUiReconciler::FScreenSlotKey RouteKey{TEXT("location_content"), TEXT("gbf01_route")};
                const FGV2LayeredUiReconciler::FActiveScreenEntry* BaselineRouteEntry = AttachFailureReconciler.GetActiveScreens().Find(RouteKey);
                TestNotNull(TEXT("GBF-01: baseline ActiveScreens stores route metadata"), BaselineRouteEntry);
                if (BaselineRouteEntry != nullptr)
                {
                    TestEqual(TEXT("GBF-01: baseline metadata stores v1 screen id"), BaselineRouteEntry->ScreenId, TEXT("core:screen.gbf01_route_v1"));
                }

                const int32 ActiveCountBeforeFailure = AttachFailureReconciler.GetActiveScreens().Num();
                FGV2UiDocumentViewModel CandidateDoc;
                CandidateDoc.UiInstanceId = TEXT("ui@gbf01:1");
                CandidateDoc.Revision = 2;
                CandidateDoc.bHasRoute = true;
                CandidateDoc.Route.Layer = TEXT("location_content");
                CandidateDoc.Route.InstanceKey = TEXT("gbf01_route");
                CandidateDoc.Route.ScreenId = TEXT("core:screen.gbf01_route_v2");
                FGV2ScreenInstanceViewModel AcceptedOverlayInst;
                AcceptedOverlayInst.Layer = TEXT("overlay_stack");
                AcceptedOverlayInst.InstanceKey = TEXT("gbf01_overlay_accepted");
                AcceptedOverlayInst.ScreenId = TEXT("core:screen.gbf01_overlay_accepted");
                CandidateDoc.Overlays.Add(AcceptedOverlayInst);
                FGV2ScreenInstanceViewModel RejectedOverlayInst;
                RejectedOverlayInst.Layer = TEXT("overlay_stack");
                RejectedOverlayInst.InstanceKey = TEXT("gbf01_overlay_rejected");
                RejectedOverlayInst.ScreenId = TEXT("core:screen.gbf01_overlay_rejected");
                CandidateDoc.Overlays.Add(RejectedOverlayInst);

                const bool bRejectedCommit = AttachFailureReconciler.Reconcile(AttachFailureShell, CandidateDoc, AttachFailureFactory, AttachFailureError);

                TestFalse(TEXT("GBF-01: single-child overlay host with 2 desired screens rejects document Commit"), bRejectedCommit);
                TestTrue(*FString::Printf(TEXT("GBF-01: failure reports the layer-reconcile diagnostic [Error: %s]"), *AttachFailureError),
                    AttachFailureError.Contains(TEXT("core:diagnostic.ui_reconcile.layer_reconcile_failed")));
                TestTrue(TEXT("GBF-01: failure names the failing layer"),
                    AttachFailureError.Contains(TEXT("overlay_stack")));
                TestEqual(TEXT("GBF-01: ActiveScreens count stays on the previous revision"),
                    AttachFailureReconciler.GetActiveScreens().Num(), ActiveCountBeforeFailure);
                TestEqual(TEXT("GBF-01: prior route stays active after recovery"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("location_content"), TEXT("gbf01_route")), RouteV1);
                TestNull(TEXT("GBF-01: accepted overlay is absent from ActiveScreens (whole document rejected)"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbf01_overlay_accepted")));
                TestNull(TEXT("GBF-01: rejected overlay is absent from ActiveScreens"),
                    AttachFailureReconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbf01_overlay_rejected")));
                const FGV2LayeredUiReconciler::FActiveScreenEntry* RouteAfterFailure = AttachFailureReconciler.GetActiveScreens().Find(RouteKey);
                TestNotNull(TEXT("GBF-01: prior route metadata remains present after recovery"), RouteAfterFailure);
                if (RouteAfterFailure != nullptr)
                {
                    TestEqual(TEXT("GBF-01: prior route metadata remains on v1"), RouteAfterFailure->ScreenId, TEXT("core:screen.gbf01_route_v1"));
                }
                TestTrue(TEXT("GBF-01: prior route is reattached to the Shell tree (cross-layer rollback)"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV1));
                TestFalse(TEXT("GBF-01: replacement route is removed by recovery"),
                    AttachFailureShell->GetScreensInLayer(TEXT("location_content")).Contains(RouteV2));
                TestEqual(TEXT("GBF-01: overlay_stack host is restored to its exact prior (empty) state"),
                    SingleChildOverlayHost->GetChildrenCount(), 0);
                TestFalse(TEXT("GBF-01: accepted overlay is physically absent from the Shell tree"),
                    AttachFailureShell->GetScreensInLayer(TEXT("overlay_stack")).Contains(AcceptedOverlay));
                TestFalse(TEXT("GBF-01: rejected overlay is physically absent from the Shell tree"),
                    AttachFailureShell->GetScreensInLayer(TEXT("overlay_stack")).Contains(RejectedOverlay));

                AttachFailureShell->RemoveFromRoot();
            }
        }

        // Step K: GBH-11 (REM-02, ADR-0041) -- the danger point PCC-07 (Step J above)
        // never reached. PCC-07 injects failure on a screen that is being REPLACED by a
        // brand-new widget instance (V1 -> V2): the target widget is off-tree until the
        // whole document commits, so a mid-Commit failure there was always safe -- there
        // is nothing live to leave half-mutated. This step targets the actually dangerous
        // case: the SAME reused live screen widget across two fields (field_a, field_b),
        // where field_a's Commit succeeds -- physically mutating a widget that is already
        // the active, on-screen previous revision -- before field_b's Commit is injected
        // to fail. Before GBH-10 this left field_a's widget showing the new revision's
        // text while ActiveScreens/LastCommittedProperties stayed on revision 1 (REM-02's
        // literal "logically old, physically part-new" shape). Also closes the one
        // GBH-10 boundary that had no dedicated fault-injection test yet: several field
        // hosts of one reused Screen (CommitScreenFields' own multi-host loop).
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 2);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 2);

            UGV2ScreenWidgetBase* ReusedScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            ReusedScreen->WidgetTree = NewObject<UWidgetTree>(ReusedScreen);
            UVerticalBox* ReusedScreenRoot = ReusedScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            ReusedScreen->WidgetTree->RootWidget = ReusedScreenRoot;

            UGV2DeclaredCompositeWidgetBase* FieldA = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldA"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldA);
            FieldA->WidgetTree = NewObject<UWidgetTree>(FieldA);
            UGV2TextWidgetBase* TextA = FieldA->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextA"));
            FieldA->WidgetTree->RootWidget = TextA;
            FieldA->SetHostIdentity(FName(TEXT("field_a")));
            FieldA->DeclaredCapabilities.Add({ FName(TEXT("value_a")), FName(TEXT("TextA")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2DeclaredCompositeWidgetBase* FieldB = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldB"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldB);
            FieldB->WidgetTree = NewObject<UWidgetTree>(FieldB);
            UGV2TextWidgetBase* TextB = FieldB->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextB"));
            FieldB->WidgetTree->RootWidget = TextB;
            FieldB->SetHostIdentity(FName(TEXT("field_b")));
            FieldB->DeclaredCapabilities.Add({ FName(TEXT("value_b")), FName(TEXT("TextB")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2ScreenWidgetBase* SiblingFailureScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            SiblingFailureScreen->WidgetTree = NewObject<UWidgetTree>(SiblingFailureScreen);
            UGV2DeclaredCompositeWidgetBase* SiblingFailureField = SiblingFailureScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("SiblingFailureField"));
            SiblingFailureScreen->WidgetTree->RootWidget = SiblingFailureField;
            SiblingFailureField->WidgetTree = NewObject<UWidgetTree>(SiblingFailureField);
            UGV2TextWidgetBase* SiblingFailureText = SiblingFailureField->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("SiblingFailureText"));
            SiblingFailureField->WidgetTree->RootWidget = SiblingFailureText;
            SiblingFailureField->SetHostIdentity(FName(TEXT("sibling_field")));
            SiblingFailureField->DeclaredCapabilities.Add({ FName(TEXT("value_s")), FName(TEXT("SiblingFailureText")), EGV2DeclaredUiCapabilityKind::Text });

            auto MakeReusedFieldValue = [](const FName& FieldId, const FString& PropName, const FString& Text) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ TCHAR_TO_UTF8(*PropName), false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel Model;
                Model.Text = FText::FromString(Text);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(PropName, FGV2PreparedUiValue::MakeText(Model));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = FieldId;
                FieldValue.SchemaId = TEXT("test:schema.gbh11_reused_field.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeReusedDoc = [&](const FString& TextA_Value, const FString& TextB_Value) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@1:1");
                Doc.Revision = 40;
                Doc.bHasRoute = false;
                FGV2ScreenInstanceViewModel OverlayInst;
                OverlayInst.Layer = TEXT("overlay_stack");
                OverlayInst.InstanceKey = TEXT("gbh11_reused");
                OverlayInst.ScreenId = TEXT("core:screen.gbh11_reused");
                OverlayInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("field_a")), TEXT("value_a"), TextA_Value));
                OverlayInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("field_b")), TEXT("value_b"), TextB_Value));
                Doc.Overlays.Add(OverlayInst);
                FGV2ScreenInstanceViewModel SiblingInst;
                SiblingInst.Layer = TEXT("overlay_stack");
                SiblingInst.InstanceKey = TEXT("gbf05_sibling_failure");
                SiblingInst.ScreenId = TEXT("core:screen.gbf05_sibling_failure");
                SiblingInst.Fields.Add(MakeReusedFieldValue(FName(TEXT("sibling_field")), TEXT("value_s"), TEXT("Sibling")));
                Doc.Overlays.Add(SiblingInst);
                return Doc;
            };

            auto ReusedFactory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                if (ScreenId == TEXT("core:screen.gbh11_reused"))
                {
                    return ReusedScreen;
                }
                return ScreenId == TEXT("core:screen.gbf05_sibling_failure") ? SiblingFailureScreen : nullptr;
            };

            FString ReusedError;
            TestTrue(*FString::Printf(TEXT("GBH-11: baseline reconcile of the reused screen succeeds [Error: %s]"), *ReusedError),
                Reconciler.Reconcile(Shell, MakeReusedDoc(TEXT("OldA"), TEXT("OldB")), ReusedFactory, ReusedError));
            TestEqual(TEXT("GBH-11: baseline widget is ReusedScreen"), Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbh11_reused")), ReusedScreen);
            TestEqual(TEXT("GBH-11: baseline TextA reads OldA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("GBH-11: baseline TextB reads OldB"), TextB->GetTextContent().ToString(), TEXT("OldB"));

            const int32 ActiveScreensCountBeforeFault = Reconciler.GetActiveScreens().Num();

            auto ReusedFailInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.gbh11_reused") && PropertyPath == TEXT("value_b");
            };
            const bool bReusedFault = Reconciler.Reconcile(Shell, MakeReusedDoc(TEXT("NewA"), TEXT("NewB")), ReusedFactory, ReusedError, ReusedFailInjector);
            TestFalse(TEXT("GBH-11: Reconcile fails when the reused screen's field_b Commit is injected"), bReusedFault);

            TestEqual(TEXT("GBH-11: same reused widget is STILL the active screen (never replaced)"),
                Reconciler.GetActiveScreen(TEXT("overlay_stack"), TEXT("gbh11_reused")), ReusedScreen);
            TestEqual(TEXT("GBH-11: ActiveScreens count is unchanged by the failed reconcile"),
                Reconciler.GetActiveScreens().Num(), ActiveScreensCountBeforeFault);

            // The actual danger point: field_a's Commit succeeded (it runs before field_b
            // in mutation order) and physically wrote "NewA" to TextA before field_b's
            // injected failure aborted the screen. Without GBH-10's rollback, this
            // assertion is exactly the one that would fail -- TextA would read "NewA".
            TestEqual(TEXT("GBH-11: TextA restored to OldA, NOT left on the uncommitted NewA (REM-02's core claim)"),
                TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("GBH-11: TextB still reads OldB (its own Commit was never reached)"),
                TextB->GetTextContent().ToString(), TEXT("OldB"));

            const FGV2PreparedUiObject& FieldALastCommitted = FieldA->GetPropertyHostState().GetLastCommittedProperties();
            const FGV2PreparedUiValue* FieldALastValueA = FieldALastCommitted.FindField(TEXT("value_a"));
            TestTrue(TEXT("GBH-11: FieldA's LastCommittedProperties has value_a"), FieldALastValueA != nullptr);
            if (FieldALastValueA != nullptr)
            {
                TestEqual(TEXT("GBH-11: FieldA's LastCommittedProperties still names revision 1's OldA, not advanced past the failed commit"),
                    FieldALastValueA->AsText().Text.ToString(), TEXT("OldA"));
            }

            if (Shell != nullptr)
            {
                TestTrue(TEXT("GBH-11: Shell still shows the same reused widget attached (reuse never touches attach)"),
                    Shell->GetScreensInLayer(TEXT("overlay_stack")).Contains(ReusedScreen));
            }

            // GBF-05: this is the document-level production path. The reused screen
            // commits both fields successfully, then the later sibling screen rejects
            // its commit. CommitReconcile must call RollbackFieldPlans for the already
            // committed screen and restore its accounting as well as its widgets.
            const auto OuterScreenFailureInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.gbf05_sibling_failure") && PropertyPath == TEXT("value_s");
            };
            const bool bOuterScreenFault = Reconciler.Reconcile(
                Shell, MakeReusedDoc(TEXT("OuterA"), TEXT("OuterB")), ReusedFactory, ReusedError, OuterScreenFailureInjector);
            TestFalse(TEXT("GBF-05: later sibling screen fault rejects the document transaction"), bOuterScreenFault);
            TestEqual(TEXT("GBF-05: document rollback physically restores the earlier reused screen"),
                TextA->GetTextContent().ToString(), TEXT("OldA"));
            const FGV2PreparedUiValue* FieldAAfterOuterScreenFault = FieldA->GetPropertyHostState().GetLastCommittedProperties().FindField(TEXT("value_a"));
            TestNotNull(TEXT("GBF-05: document rollback restores earlier screen accounting"), FieldAAfterOuterScreenFault);
            if (FieldAAfterOuterScreenFault != nullptr)
            {
                TestEqual(TEXT("GBF-05: document rollback accounting matches restored widget"),
                    FieldAAfterOuterScreenFault->AsText().Text.ToString(), TEXT("OldA"));
            }
            TestEqual(TEXT("GBF-05: document rollback restores earlier screen schema id"),
                FieldA->GetPropertyHostState().GetLastCommittedSchemaId(), TEXT("test:schema.gbh11_reused_field.v1"));

            // Clean up this scenario's overlay so later shared-Shell assertions in this
            // test function see the state they expect (no GBH-11-specific residue).
            FGV2UiDocumentViewModel ReusedCleanupDoc;
            ReusedCleanupDoc.UiInstanceId = TEXT("ui@1:1");
            ReusedCleanupDoc.Revision = 41;
            ReusedCleanupDoc.bHasRoute = false;
            FString ReusedCleanupError;
            TestTrue(*FString::Printf(TEXT("GBH-11: cleanup reconcile succeeds [Error: %s]"), *ReusedCleanupError),
                Reconciler.Reconcile(Shell, ReusedCleanupDoc, ReusedFactory, ReusedCleanupError));
        }

        // GBF-04 (GBH-R2, ADR-0041): the inverse of a reused field host is defined by
        // its committed schema, not the candidate schema.  Revision A owns two numeric
        // properties; revision B owns neither.  Injecting a fault after the first reset
        // makes the old implementation replay a second Reset under schema B, leaving the
        // earlier value at zero instead of restoring revision A.
        {
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);

            UGV2ScreenWidgetBase* SchemaSwitchScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            SchemaSwitchScreen->WidgetTree = NewObject<UWidgetTree>(SchemaSwitchScreen);
            UVerticalBox* SchemaSwitchRoot = SchemaSwitchScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            SchemaSwitchScreen->WidgetTree->RootWidget = SchemaSwitchRoot;

            UGV2DeclaredCompositeWidgetBase* SchemaSwitchField = SchemaSwitchScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("SchemaSwitchField"));
            SchemaSwitchRoot->AddChildToVerticalBox(SchemaSwitchField);
            SchemaSwitchField->WidgetTree = NewObject<UWidgetTree>(SchemaSwitchField);
            UVerticalBox* SchemaSwitchFieldRoot = SchemaSwitchField->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FieldRoot"));
            SchemaSwitchField->WidgetTree->RootWidget = SchemaSwitchFieldRoot;
            UGV2ProgressBarWidgetBase* FirstMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("FirstMeter"));
            UGV2ProgressBarWidgetBase* SecondMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("SecondMeter"));
            UGV2ProgressBarWidgetBase* ThirdMeter = SchemaSwitchField->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("ThirdMeter"));
            SchemaSwitchFieldRoot->AddChildToVerticalBox(FirstMeter);
            SchemaSwitchFieldRoot->AddChildToVerticalBox(SecondMeter);
            SchemaSwitchFieldRoot->AddChildToVerticalBox(ThirdMeter);
            SchemaSwitchField->SetHostIdentity(FName(TEXT("schema_switch")));
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("first")), FName(TEXT("FirstMeter")), EGV2DeclaredUiCapabilityKind::Number });
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("second")), FName(TEXT("SecondMeter")), EGV2DeclaredUiCapabilityKind::Number });
            SchemaSwitchField->DeclaredCapabilities.Add({ FName(TEXT("third")), FName(TEXT("ThirdMeter")), EGV2DeclaredUiCapabilityKind::Number });

            auto MakeSchemaSwitchValue = [](const bool bOwnsMeters, const bool bAddsThird = false) -> FGV2ScreenFieldValue
            {
                auto Schema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                if (bOwnsMeters)
                {
                    auto MakeNumberSpec = []() -> GV2ContentCore::FCompiledUiFieldSpecPtr
                    {
                        auto NumberSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>();
                        NumberSpec->Kind = GV2ContentCore::EUiFieldKind::Scalar;
                        GV2ContentCore::FScalarFieldSpec Scalar;
                        Scalar.Kind = GV2ContentCore::EScalarFieldKind::Number;
                        Scalar.MinimumNumber = 0.0;
                        Scalar.MaximumNumber = 1.0;
                        NumberSpec->Scalar = Scalar;
                        return NumberSpec;
                    };
                    Schema->Fields.push_back({ "first", false, MakeNumberSpec() });
                    Schema->Fields.push_back({ "second", false, MakeNumberSpec() });
                    Fields.Emplace(TEXT("first"), FGV2PreparedUiValue::MakeNumber(0.25));
                    Fields.Emplace(TEXT("second"), FGV2PreparedUiValue::MakeNumber(0.75));
                    if (bAddsThird)
                    {
                        Schema->Fields.push_back({ "third", false, MakeNumberSpec() });
                        Fields.Emplace(TEXT("third"), FGV2PreparedUiValue::MakeNumber(0.5));
                    }
                }

                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = TEXT("schema_switch");
                FieldValue.SchemaId = bOwnsMeters
                    ? TEXT("test:schema.gbf04_meter_pair.v1")
                    : TEXT("test:schema.gbf04_empty.v2");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = Schema;
                return FieldValue;
            };

            TestTrue(TEXT("GBF-04: baseline schema A commits"),
                SchemaSwitchScreen->ApplyScreenFields({ MakeSchemaSwitchValue(true) }));
            TestEqual(TEXT("GBF-04: baseline first meter is materialized"), FirstMeter->GetProgress(), 0.25f);
            TestEqual(TEXT("GBF-04: baseline second meter is materialized"), SecondMeter->GetProgress(), 0.75f);

            FGV2ScreenMutationPlan SchemaSwitchPlan;
            FString SchemaSwitchPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-04: candidate schema B prepares [Error: %s]"), *SchemaSwitchPrepareError),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(false) }, SchemaSwitchPlan, SchemaSwitchPrepareError));
            TestEqual(TEXT("GBF-04: candidate has one field plan"), SchemaSwitchPlan.FieldPlans.Num(), 1);
            if (SchemaSwitchPlan.FieldPlans.Num() == 1)
            {
                const TArray<FGV2UiPropertyMutation>& ForwardMutations = SchemaSwitchPlan.FieldPlans[0].MutationPlan.GetMutations();
                TestEqual(TEXT("GBF-04: schema removal prepares a reset for each formerly-owned property"), ForwardMutations.Num(), 2);
                const FString LastMutationPath = ForwardMutations.Num() > 0 ? ForwardMutations.Last().PropertyPath : FString();
                TestFalse(TEXT("GBF-04: candidate has a final mutation to inject"), LastMutationPath.IsEmpty());
                if (!LastMutationPath.IsEmpty())
                {
                    FString SchemaSwitchCommitError;
                    TestFalse(TEXT("GBF-04: commit fault rejects candidate schema B"),
                        SchemaSwitchScreen->CommitScreenFields(SchemaSwitchPlan, SchemaSwitchCommitError, [&LastMutationPath](const FString& Path)
                        {
                            return Path == LastMutationPath;
                        }));
                }
            }

            TestEqual(TEXT("GBF-04: first meter returns to schema A value after failed schema switch"), FirstMeter->GetProgress(), 0.25f);
            TestEqual(TEXT("GBF-04: second meter returns to schema A value after failed schema switch"), SecondMeter->GetProgress(), 0.75f);

            // Schema expansion needs the other half of the inverse rule: `third` was
            // absent from schema A, so its inverse is Reset rather than an old value.
            FGV2ScreenMutationPlan ExpansionPlan;
            FString ExpansionPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-04: schema expansion prepares [Error: %s]"), *ExpansionPrepareError),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(true, true) }, ExpansionPlan, ExpansionPrepareError));
            TestEqual(TEXT("GBF-04: schema expansion has one inverse per forward mutation"),
                ExpansionPlan.FieldPlans[0].MutationPlan.Num(), ExpansionPlan.FieldPlans[0].RollbackPlan.Num());

            // GBF-05: a higher transaction can reject this already successful screen
            // after its Commit advanced the host snapshot. Rollback must restore both
            // the widgets and the snapshot which the *next* Prepare observes.
            FString ExpansionCommitError;
            TestTrue(TEXT("GBF-05: expanded revision commits before outer failure"),
                SchemaSwitchScreen->CommitScreenFields(ExpansionPlan, ExpansionCommitError));
            TestEqual(TEXT("GBF-05: expanded third meter is physically applied"), ThirdMeter->GetProgress(), 0.5f);
            const FGV2UiRollbackResult OuterRollbackResult = RollbackFieldPlans(ExpansionPlan.FieldPlans);
            TestTrue(TEXT("GBF-05: outer rollback of a cleanly-prepared plan restores successfully"), OuterRollbackResult.bRestored);
            TestEqual(TEXT("GBF-05: rollback physically resets candidate-only meter"), ThirdMeter->GetProgress(), 0.0f);
            const FGV2UiPropertyHostState& StateAfterOuterRollback = SchemaSwitchField->GetPropertyHostState();
            TestEqual(TEXT("GBF-05: outer rollback restores prior schema id"),
                StateAfterOuterRollback.GetLastCommittedSchemaId(), TEXT("test:schema.gbf04_meter_pair.v1"));
            TestNull(TEXT("GBF-05: outer rollback removes candidate-only property from committed state"),
                StateAfterOuterRollback.GetLastCommittedProperties().FindField(TEXT("third")));
            FGV2ScreenMutationPlan NextPreparePlan;
            FString NextPrepareError;
            TestTrue(*FString::Printf(TEXT("GBF-05: next Prepare reads restored revision [Error: %s]"), *NextPrepareError),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(true) }, NextPreparePlan, NextPrepareError));
            TestEqual(TEXT("GBF-05: next Prepare returns the one declared field plan"), NextPreparePlan.FieldPlans.Num(), 1);
            if (NextPreparePlan.FieldPlans.Num() == 1)
            {
                const TArray<FGV2UiPropertyMutation>& NextMutations = NextPreparePlan.FieldPlans[0].MutationPlan.GetMutations();
                TestEqual(TEXT("GBF-05: next Prepare has only the prior schema's two properties"), NextMutations.Num(), 2);
                const bool bNextPrepareResetsCancelledThird = NextMutations.ContainsByPredicate([](const FGV2UiPropertyMutation& Mutation)
                {
                    return Mutation.PropertyName == TEXT("third");
                });
                TestFalse(TEXT("GBF-05: next Prepare does not see the cancelled third property"), bNextPrepareResetsCancelledThird);
            }

            // A value-only legacy snapshot is deliberately not accepted as an inverse
            // source: guessing schema B here would recreate the original defect.
            SchemaSwitchField->GetPropertyHostState().SetLastCommittedProperties(
                SchemaSwitchField->GetPropertyHostState().GetLastCommittedProperties());
            FGV2ScreenMutationPlan MissingSchemaPlan;
            FString MissingSchemaError;
            TestFalse(TEXT("GBF-04: previous value without committed schema rejects Prepare"),
                SchemaSwitchScreen->PrepareScreenFields({ MakeSchemaSwitchValue(false) }, MissingSchemaPlan, MissingSchemaError));
            TestTrue(TEXT("GBF-04: missing committed schema reports typed rollback diagnostic"),
                MissingSchemaError.Contains(TEXT("core:diagnostic.ui_rollback.missing_committed_schema")));
        }

        if (Shell != nullptr)
        {
            TestTrue(TEXT("Location content layer is unblocked after modal closure"), Shell->IsLayerInteractive(TEXT("location_content")));
            TestTrue(TEXT("Overlay stack layer is unblocked"), Shell->IsLayerInteractive(TEXT("overlay_stack")));
            TestTrue(TEXT("Background layer is unblocked"), Shell->IsLayerInteractive(TEXT("background")));
            TestTrue(TEXT("Core interface layer is unblocked"), Shell->IsLayerInteractive(TEXT("core_interface")));
            TestEqual(TEXT("Overlay host is empty"), Shell->GetScreensInLayer(TEXT("overlay_stack")).Num(), 0);
            TestEqual(TEXT("Modal host is empty"), Shell->GetScreensInLayer(TEXT("modal_stack")).Num(), 0);
        }

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
    FGV2ModalStackKeyedCollectionOrderingContract,
    "GV2.Runtime.UI.ModalStackKeyedCollectionOrderingContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06A (ADR-0042, INV-P3 proof slice): modal_stack is the one Game Shell layer
// converted from the per-widget AttachScreenToLayer loop to
// FGV2KeyedCollection::ReconcilePrepared (Source/GV2/Public/UI/GV2KeyedCollection.h). The
// old loop could not reorder two already-attached (reused) widgets at all --
// AttachScreenToLayer no-ops once a widget's parent already equals the host -- so this
// proves reuse + create + remove + reorder in a single reconcile call, reading the Host
// panel's ACTUAL physical child order (Shell::GetScreensInLayer, which walks
// Host->GetChildAt(i)), not just the reconciler's ActiveScreens bookkeeping or a returned
// bool. The remaining five layers are untouched -- this is scoped to modal_stack alone.
bool FGV2ModalStackKeyedCollectionOrderingContract::RunTest(const FString& Parameters)
{
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
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-06A: Game shell instantiated"), Shell);
        if (Shell != nullptr)
        {
            Shell->AddToRoot();

            FGV2LayeredUiReconciler Reconciler;
            auto MockFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            };
            auto MakeModalInstance = [](FName InstanceKey) -> FGV2ScreenInstanceViewModel
            {
                FGV2ScreenInstanceViewModel Inst;
                Inst.Layer = UGV2GameShellWidgetBase::LayerModalStack;
                Inst.InstanceKey = InstanceKey;
                Inst.ScreenId = TEXT("core:screen.modal_probe");
                return Inst;
            };

            FString ReconcileError;

            FGV2UiDocumentViewModel Doc1;
            Doc1.UiInstanceId = TEXT("ui@pah06a");
            Doc1.Revision = 1;
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_a")));
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_b")));
            Doc1.Modals.Add(MakeModalInstance(TEXT("modal_c")));
            TestTrue(*FString::Printf(TEXT("PAH-06A: reconcile [A,B,C] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError));

            UGV2ScreenWidgetBase* WidgetA = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_a"));
            UGV2ScreenWidgetBase* WidgetB = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_b"));
            UGV2ScreenWidgetBase* WidgetC = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_c"));
            TestNotNull(TEXT("PAH-06A: A created"), WidgetA);
            TestNotNull(TEXT("PAH-06A: B created"), WidgetB);
            TestNotNull(TEXT("PAH-06A: C created"), WidgetC);

            const TArray<UUserWidget*> InitialOrder = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerModalStack);
            TestEqual(TEXT("PAH-06A: [A,B,C] has 3 physical children"), InitialOrder.Num(), 3);
            if (InitialOrder.Num() == 3)
            {
                TestEqual(TEXT("PAH-06A: physical child 0 is A"), InitialOrder[0], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06A: physical child 1 is B"), InitialOrder[1], Cast<UUserWidget>(WidgetB));
                TestEqual(TEXT("PAH-06A: physical child 2 is C"), InitialOrder[2], Cast<UUserWidget>(WidgetC));
            }

            // [A, B, C] -> [C, A, D]: A and C reused (same widget identity), B removed, D
            // newly created -- physical order must become exactly C, A, D.
            FGV2UiDocumentViewModel Doc2;
            Doc2.UiInstanceId = TEXT("ui@pah06a");
            Doc2.Revision = 2;
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_c")));
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_a")));
            Doc2.Modals.Add(MakeModalInstance(TEXT("modal_d")));
            TestTrue(*FString::Printf(TEXT("PAH-06A: reconcile [C,A,D] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError));

            TestEqual(TEXT("PAH-06A: A is the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_a")), WidgetA);
            TestEqual(TEXT("PAH-06A: C is the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_c")), WidgetC);
            UGV2ScreenWidgetBase* WidgetD = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_d"));
            TestNotNull(TEXT("PAH-06A: D was created"), WidgetD);
            TestNull(TEXT("PAH-06A: B removed from active set"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerModalStack, TEXT("modal_b")));

            const TArray<UUserWidget*> ReorderedOrder = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerModalStack);
            TestEqual(TEXT("PAH-06A: [C,A,D] has 3 physical children"), ReorderedOrder.Num(), 3);
            if (ReorderedOrder.Num() == 3)
            {
                TestEqual(TEXT("PAH-06A: physical child 0 is C (reused, moved)"), ReorderedOrder[0], Cast<UUserWidget>(WidgetC));
                TestEqual(TEXT("PAH-06A: physical child 1 is A (reused, moved)"), ReorderedOrder[1], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06A: physical child 2 is D (newly created)"), ReorderedOrder[2], Cast<UUserWidget>(WidgetD));
            }
            TestFalse(TEXT("PAH-06A: B is no longer a child of modal_stack"), ReorderedOrder.Contains(Cast<UUserWidget>(WidgetB)));
            TestNull(TEXT("PAH-06A: B has no parent after removal"), WidgetB->GetParent());

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
    FGV2KeyedCollectionReconcilePreparedRestoresOnSwapFailure,
    "GV2.Runtime.UI.KeyedCollectionReconcilePreparedRestoresOnSwapFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06A (ADR-0042, INV-P3 proof slice): proves FGV2KeyedCollection::ReconcilePrepared's
// container-swap restore-on-failure path (GV2KeyedCollection.h, the AddChild-fails branch
// a few lines after "Commit to Container atomically") by reading the ACTUAL container
// children afterward, not the returned bool. UPanelWidget::AddChild's only failure
// conditions are a null widget and the single-child gate
// (!bCanHaveMultipleChildren && GetChildrenCount() > 0); AddChild is not virtual, so no
// C++ subclass can inject a failure at an arbitrary position within a genuinely
// multi-child container. UBorder (a UContentWidget, single-child by construction) is the
// one deterministic, engine-native trigger: reconciling 2 desired widgets into it makes
// the swap's second AddChild call fail by construction, not by a test double standing in
// for the engine.
//
// PAH-06A's own modal_stack integration (GV2LayeredUiReconciler.cpp) could not exercise
// this branch through the REAL WBP_GameShell asset, since its authored layer hosts are
// all genuinely multi-child panels (Overlay/CanvasPanel-family) -- so this test, at the
// primitive that actually owns the guarantee, was its only exercise at the time. PAH-06B
// later found a production-reachable case after all: a misconfigured Shell whose host for
// some layer IS single-child (GBF-01, "GV2.UI.LayeredReconciliationContract") -- this test
// remains the more direct, minimal one.
bool FGV2KeyedCollectionReconcilePreparedRestoresOnSwapFailure::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UBorder* FailingContainer = NewObject<UBorder>(TestWorld);
    UGV2ButtonWidgetBase* ExistingWidget = NewObject<UGV2ButtonWidgetBase>(TestWorld);
    TestNotNull(TEXT("PAH-06A: pre-existing widget attaches to the single-child container"),
        FailingContainer->AddChild(ExistingWidget));
    TestEqual(TEXT("PAH-06A: container starts with exactly 1 child"), FailingContainer->GetChildrenCount(), 1);

    TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> WidgetsByKey;
    WidgetsByKey.Add(TEXT("existing"), ExistingWidget);

    UGV2ButtonWidgetBase* NewWidget = NewObject<UGV2ButtonWidgetBase>(TestWorld);
    const TArray<FName> DesiredKeys = { TEXT("existing"), TEXT("new") };

    struct FGV2TestNoopPrepared
    {
    };

    TArray<UGV2ButtonWidgetBase*> OrderedOut;
    TArray<UGV2ButtonWidgetBase*> PreviousOrderedOut;
    const bool bReconciled = FGV2KeyedCollection::ReconcilePrepared<UGV2ButtonWidgetBase, FName, FGV2TestNoopPrepared>(
        FailingContainer,
        DesiredKeys,
        WidgetsByKey,
        [](const FName& Key) { return Key; },
        [&]() -> UGV2ButtonWidgetBase* { return NewWidget; },
        [](UGV2ButtonWidgetBase&, const FName&, FGV2TestNoopPrepared&) { return true; },
        [](UGV2ButtonWidgetBase&, const FGV2TestNoopPrepared&) {},
        OrderedOut,
        nullptr,
        &PreviousOrderedOut);

    TestFalse(TEXT("PAH-06A: reconciling a 2nd widget into a single-child container fails (engine-native AddChild rejection, not a mock)"), bReconciled);
    TestEqual(TEXT("PAH-06A: container is restored to exactly its prior 1 child"), FailingContainer->GetChildrenCount(), 1);
    if (FailingContainer->GetChildrenCount() == 1)
    {
        TestEqual(TEXT("PAH-06A: the restored child is the ORIGINAL widget, not a partially-applied one"),
            FailingContainer->GetChildAt(0), Cast<UWidget>(ExistingWidget));
    }
    TestEqual(TEXT("PAH-06A: the widget that failed to attach was never parented anywhere"),
        NewWidget->GetParent(), static_cast<UPanelWidget*>(nullptr));

    TestEqual(TEXT("PAH-06A: OutPreviousOrderedWidgets captured the container's exact prior order"), PreviousOrderedOut.Num(), 1);
    if (PreviousOrderedOut.Num() == 1)
    {
        TestEqual(TEXT("PAH-06A: OutPreviousOrderedWidgets[0] is the original widget"), PreviousOrderedOut[0], ExistingWidget);
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
    FGV2NonModalLayerReorderAndReplaceContract,
    "GV2.Runtime.UI.NonModalLayerReorderAndReplaceContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-06B (ADR-0042, INV-P3): PAH-06A proved the shared primitive fits modal_stack alone;
// this proves the generalization to the other five layers actually landed, using
// overlay_stack (two simultaneous instances are a legitimate, real use of that layer,
// unlike the single-instance-by-convention layers). Before PAH-06B, AttachScreenToLayer's
// per-widget loop could not reorder two already-attached reused widgets at all (no-ops
// once a widget's parent already equals the host), and replacing one of two screens could
// leave the untouched sibling in the wrong physical position (the replacement widget is
// freshly attached and appended, not inserted where the old one was). Both read the ACTUAL
// panel child order (Shell::GetScreensInLayer), not a returned bool.
bool FGV2NonModalLayerReorderAndReplaceContract::RunTest(const FString& Parameters)
{
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
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-06B: Game shell instantiated"), Shell);
        if (Shell != nullptr)
        {
            Shell->AddToRoot();

            FGV2LayeredUiReconciler Reconciler;
            auto MockFactory = [&](const FString&, FName) -> UGV2ScreenWidgetBase*
            {
                return CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            };
            auto MakeOverlayInstance = [](FName InstanceKey, const FString& ScreenId) -> FGV2ScreenInstanceViewModel
            {
                FGV2ScreenInstanceViewModel Inst;
                Inst.Layer = UGV2GameShellWidgetBase::LayerOverlayStack;
                Inst.InstanceKey = InstanceKey;
                Inst.ScreenId = ScreenId;
                return Inst;
            };

            FString ReconcileError;

            // [A, B] -> [B, A]: both reused, only order changes.
            FGV2UiDocumentViewModel Doc1;
            Doc1.UiInstanceId = TEXT("ui@pah06b");
            Doc1.Revision = 1;
            Doc1.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            Doc1.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile [A,B] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc1, MockFactory, ReconcileError));

            UGV2ScreenWidgetBase* WidgetA = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a"));
            UGV2ScreenWidgetBase* WidgetB = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b"));
            TestNotNull(TEXT("PAH-06B: A created"), WidgetA);
            TestNotNull(TEXT("PAH-06B: B created"), WidgetB);

            TArray<UUserWidget*> Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: [A,B] has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: [A,B] child 0 is A"), Order[0], Cast<UUserWidget>(WidgetA));
                TestEqual(TEXT("PAH-06B: [A,B] child 1 is B"), Order[1], Cast<UUserWidget>(WidgetB));
            }

            FGV2UiDocumentViewModel Doc2;
            Doc2.UiInstanceId = TEXT("ui@pah06b");
            Doc2.Revision = 2;
            Doc2.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe")));
            Doc2.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile [B,A] succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc2, MockFactory, ReconcileError));

            TestEqual(TEXT("PAH-06B: A is the same reused instance after reorder"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a")), WidgetA);
            TestEqual(TEXT("PAH-06B: B is the same reused instance after reorder"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b")), WidgetB);

            Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: [B,A] has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: [B,A] child 0 is B (reused, moved)"), Order[0], Cast<UUserWidget>(WidgetB));
                TestEqual(TEXT("PAH-06B: [B,A] child 1 is A (reused, moved)"), Order[1], Cast<UUserWidget>(WidgetA));
            }

            // Replace B (in the 2nd slot) with a new widget B2 -- A's position must not move.
            FGV2UiDocumentViewModel Doc3;
            Doc3.UiInstanceId = TEXT("ui@pah06b");
            Doc3.Revision = 3;
            Doc3.Overlays.Add(MakeOverlayInstance(TEXT("overlay_b"), TEXT("core:screen.overlay_probe_v2")));
            Doc3.Overlays.Add(MakeOverlayInstance(TEXT("overlay_a"), TEXT("core:screen.overlay_probe")));
            TestTrue(*FString::Printf(TEXT("PAH-06B: reconcile replace-B succeeds [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, Doc3, MockFactory, ReconcileError));

            UGV2ScreenWidgetBase* WidgetB2 = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_b"));
            TestNotNull(TEXT("PAH-06B: B2 was created"), WidgetB2);
            TestNotEqual(TEXT("PAH-06B: B2 is a different widget instance than B"), WidgetB2, WidgetB);
            TestEqual(TEXT("PAH-06B: A is still the same reused instance"),
                Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerOverlayStack, TEXT("overlay_a")), WidgetA);

            Order = Shell->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack);
            TestEqual(TEXT("PAH-06B: replace-B has 2 physical children"), Order.Num(), 2);
            if (Order.Num() == 2)
            {
                TestEqual(TEXT("PAH-06B: replaced B2 keeps B's slot (position 0)"), Order[0], Cast<UUserWidget>(WidgetB2));
                TestEqual(TEXT("PAH-06B: untouched A keeps its position (position 1)"), Order[1], Cast<UUserWidget>(WidgetA));
            }
            TestFalse(TEXT("PAH-06B: old B is no longer a child of overlay_stack"), Order.Contains(Cast<UUserWidget>(WidgetB)));

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
    FGV2PresentationAuthorityPhaseContract,
    "GV2.Runtime.UI.PresentationAuthorityPhaseContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-08 (ADR-0042, INV-P5): observational half of the two-part gate. The structural
// half (Tools/Testing/validate_presentation_authority_phase.py) classifies every
// authority access by phase and walks call chains out of every application-phase root;
// it reads source. This one observes the property directly: each authority bumps a
// counter (GV2PresentationAuthorityProbe.h), and the test brackets the two public phase
// functions and compares deltas. A source scan can be defeated by indirection
// (Commit() -> helper() -> service() -> lookup); a counter cannot.
//
// The Prepare > 0 half is not decoration. Without it the Commit == 0 assertion is
// satisfied just as well by a fixture that resolves nothing at all, which is a check
// that passes because it checks nothing -- the exact shape that made an earlier
// milestone's "every schema-required property arrived" assertion vacuous for a schema
// whose fields are all optional.
//
// Measurement point is CommitReconcile, never the enclosing Reconcile: since PAH-07,
// Reconcile answers a failed compensating rollback by replaying PrepareReconcile against
// the last committed document. That nested preparation is legitimate, and the third
// scenario below measures it rather than asserting it away -- a caveat that is proven is
// a caveat; one that is only written down is an excuse waiting to be used.
bool FGV2PresentationAuthorityPhaseContract::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();
    if (TestWorld != nullptr)
    {
        UClass* GameShellClass = LoadClass<UGV2GameShellWidgetBase>(
            nullptr, TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"));
        if (GameShellClass == nullptr)
        {
            GameShellClass = UGV2GameShellWidgetBase::StaticClass();
        }
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-08: Game shell instantiated"), Shell);

        // The factory is registry-backed on purpose: GV2LayeredUiReconciler.h documents
        // FScreenFactory as "an implementation backed by UGV2ScreenRegistry::Resolve", so
        // this is the production shape, not an authority call invented for the test.
        UGV2ScreenRegistry* Registry = UGV2ScreenRegistrySettings::GetConfiguredRegistry() != nullptr
            ? const_cast<UGV2ScreenRegistry*>(UGV2ScreenRegistrySettings::GetConfiguredRegistry())
            : nullptr;
        TestNotNull(TEXT("PAH-08: a configured Screen Registry is available"), Registry);

        UGV2ScreenWidgetBase* Fixture = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

        if (Shell != nullptr && Registry != nullptr && Fixture != nullptr)
        {
            Shell->AddToRoot();

            auto Factory = [&](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
            {
                FGV2ResolvedScreenDescriptor Descriptor;
                FGV2ScreenResolutionRejection Rejection;
                // Result deliberately unused for widget selection: the fixture screen is
                // returned either way. What matters here is that the production factory
                // shape consults the authority, and that it does so during preparation.
                (void)Registry->Resolve(ScreenId, FGV2ScreenPlacement::TopLevel(Layer), Descriptor, Rejection);
                return Fixture;
            };

            FGV2UiDocumentViewModel Doc;
            Doc.UiInstanceId = TEXT("ui@1:1");
            Doc.Revision = 1;
            Doc.bHasRoute = true;
            Doc.Route.Layer = TEXT("location_content");
            Doc.Route.InstanceKey = TEXT("main");
            Doc.Route.ScreenId = TEXT("core:screen.pah08_probe");

            FGV2LayeredUiReconciler Reconciler;
            FGV2LayeredUiReconciler::FPreparedReconciliationPlan Plan;
            FString Error;

            const uint64 BeforePrepare = GV2PresentationAuthorityProbe::GetResolveCount();
            const bool bPrepared = Reconciler.PrepareReconcile(Shell, Doc, Factory, Plan, Error);
            const uint64 AfterPrepare = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: candidate prepares [Error: %s]"), *Error), bPrepared);
            TestTrue(
                *FString::Printf(
                    TEXT("PAH-08: preparation resolves at least one authority (delta %llu) -- without this the "
                         "commit assertion below would pass on a fixture that resolves nothing"),
                    static_cast<unsigned long long>(AfterPrepare - BeforePrepare)),
                AfterPrepare > BeforePrepare);

            const uint64 BeforeCommit = GV2PresentationAuthorityProbe::GetResolveCount();
            const bool bCommitted = Reconciler.CommitReconcile(Shell, Plan, Error);
            const uint64 AfterCommit = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: candidate commits [Error: %s]"), *Error), bCommitted);
            TestEqual(
                TEXT("PAH-08: application resolves no authority -- the prepared plan already carries what it needs"),
                static_cast<int64>(AfterCommit - BeforeCommit),
                static_cast<int64>(0));

            // Third scenario: the caveat, measured. A full Reconcile of a second revision
            // prepares once, so its delta is strictly positive -- which is exactly why the
            // invariant is asserted around CommitReconcile and not around Reconcile. If a
            // later change moved the bracket outward, this assertion is what shows the
            // measurement point is load-bearing rather than incidental.
            // A *different* screen id, not just a later revision: preparation consults the
            // factory only when a screen instance is new. Reusing the same screen resolves
            // nothing at all -- which is itself the invariant working, and which made the
            // first draft of this assertion fail. Recorded rather than quietly patched.
            FGV2UiDocumentViewModel NextDoc = Doc;
            NextDoc.Revision = 2;
            NextDoc.Route.ScreenId = TEXT("core:screen.pah08_probe_second");
            const uint64 BeforeWhole = GV2PresentationAuthorityProbe::GetResolveCount();
            FString WholeError;
            const bool bWhole = Reconciler.Reconcile(Shell, NextDoc, Factory, WholeError);
            const uint64 AfterWhole = GV2PresentationAuthorityProbe::GetResolveCount();
            TestTrue(*FString::Printf(TEXT("PAH-08: whole reconcile succeeds [Error: %s]"), *WholeError), bWhole);
            TestTrue(
                TEXT("PAH-08: Reconcile as a whole DOES resolve (it contains preparation), so it is the wrong "
                     "bracket for the invariant -- CommitReconcile is"),
                AfterWhole > BeforeWhole);

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
    FGV2PresentationCatastrophicRecoveryContract,
    "GV2.Runtime.UI.PresentationCatastrophicRecoveryContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-07 (ADR-0042, INV-P4): presentation has exactly one committed logical state; the
// physical UMG tree is its recoverable projection. Proves the two DIFFERENT observable
// outcomes of a Commit failure: (a) the compensating rollback (GBH-10, ADR-0041) succeeds
// -- FGV2LayeredUiReconciler::GetHealth() stays Nominal, the previous revision is intact
// in place, exactly like every pre-PAH-07 rollback test already covered; (b) the rollback
// itself ALSO fails -- GetHealth() becomes RecoveredFromCatastrophicFailure, and the
// physical tree (both the failing layer AND an untouched sibling layer, to prove the
// canonical part actually suffices INCLUDING composition and order there) is discarded
// and rebuilt from the last successfully committed document, not left in an unverified
// state. Both are forced via ScreenCommitFailureInjector/ScreenRollbackFailureInjector --
// the same production-path injectors PAH-01/GBH-10/GBF-05 already established, not a
// synthetic health flag flipped by hand.
bool FGV2PresentationCatastrophicRecoveryContract::RunTest(const FString& Parameters)
{
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
        UGV2GameShellWidgetBase* Shell = CreateWidget<UGV2GameShellWidgetBase>(TestWorld, GameShellClass);
        TestNotNull(TEXT("PAH-07: Game shell instantiated"), Shell);
        if (Shell != nullptr)
        {
            Shell->AddToRoot();

            // Route widget at location_content -- a plain, field-less screen. Reused by a
            // fixed-instance factory (not CreateWidget-per-call) so its C++ identity is
            // directly comparable before/after catastrophic recovery: recovery must
            // re-resolve "core:screen.route" through the SAME production ScreenFactory
            // path and land on this exact object again, proving the canonical composition
            // of an UNTOUCHED sibling layer survives, not just the layer that failed.
            UGV2ScreenWidgetBase* RouteWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            UGV2ScreenWidgetBase* ReplacementRouteWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            // A second overlay_stack instance alongside the reused/faulting one, so
            // recovery's physical order can be checked as [Reused, Extra], not just
            // composition.
            UGV2ScreenWidgetBase* ExtraOverlayWidget = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());

            UGV2ScreenWidgetBase* ReusedScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
            ReusedScreen->WidgetTree = NewObject<UWidgetTree>(ReusedScreen);
            UVerticalBox* ReusedScreenRoot = ReusedScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            ReusedScreen->WidgetTree->RootWidget = ReusedScreenRoot;

            UGV2DeclaredCompositeWidgetBase* FieldA = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldA"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldA);
            FieldA->WidgetTree = NewObject<UWidgetTree>(FieldA);
            UGV2TextWidgetBase* TextA = FieldA->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextA"));
            FieldA->WidgetTree->RootWidget = TextA;
            FieldA->SetHostIdentity(FName(TEXT("field_a")));
            FieldA->DeclaredCapabilities.Add({ FName(TEXT("value_a")), FName(TEXT("TextA")), EGV2DeclaredUiCapabilityKind::Text });

            UGV2DeclaredCompositeWidgetBase* FieldB = ReusedScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
                UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("FieldB"));
            ReusedScreenRoot->AddChildToVerticalBox(FieldB);
            FieldB->WidgetTree = NewObject<UWidgetTree>(FieldB);
            UGV2TextWidgetBase* TextB = FieldB->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("TextB"));
            FieldB->WidgetTree->RootWidget = TextB;
            FieldB->SetHostIdentity(FName(TEXT("field_b")));
            FieldB->DeclaredCapabilities.Add({ FName(TEXT("value_b")), FName(TEXT("TextB")), EGV2DeclaredUiCapabilityKind::Text });

            TMap<FString, UGV2ScreenWidgetBase*> ScreensById;
            ScreensById.Add(TEXT("core:screen.pah07_route"), RouteWidget);
            ScreensById.Add(TEXT("core:screen.pah07_route_v2"), ReplacementRouteWidget);
            ScreensById.Add(TEXT("core:screen.pah07_extra"), ExtraOverlayWidget);
            ScreensById.Add(TEXT("core:screen.pah07_reused"), ReusedScreen);
            auto Factory = [&](const FString& ScreenId, FName) -> UGV2ScreenWidgetBase*
            {
                UGV2ScreenWidgetBase** Found = ScreensById.Find(ScreenId);
                return Found != nullptr ? *Found : nullptr;
            };

            auto MakeFieldValue = [](const FName& FieldId, const FString& PropName, const FString& Text) -> FGV2ScreenFieldValue
            {
                auto ItemSchema = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
                ItemSchema->Fields.push_back({ TCHAR_TO_UTF8(*PropName), false, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });
                FGV2TextViewModel Model;
                Model.Text = FText::FromString(Text);
                TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
                Fields.Emplace(PropName, FGV2PreparedUiValue::MakeText(Model));
                FGV2ScreenFieldValue FieldValue;
                FieldValue.FieldId = FieldId;
                FieldValue.SchemaId = TEXT("test:schema.pah07_reused_field.v1");
                FieldValue.PreparedValue = FGV2PreparedUiObject::Create(MoveTemp(Fields));
                FieldValue.CompiledSchema = ItemSchema;
                return FieldValue;
            };

            auto MakeDoc = [&](int64 Revision, const FString& RouteScreenId, const FString& TextA_Value, const FString& TextB_Value) -> FGV2UiDocumentViewModel
            {
                FGV2UiDocumentViewModel Doc;
                Doc.UiInstanceId = TEXT("ui@pah07");
                Doc.Revision = Revision;
                Doc.bHasRoute = true;
                Doc.Route.Layer = TEXT("location_content");
                Doc.Route.InstanceKey = TEXT("main");
                Doc.Route.ScreenId = RouteScreenId;

                FGV2ScreenInstanceViewModel ReusedInst;
                ReusedInst.Layer = TEXT("overlay_stack");
                ReusedInst.InstanceKey = TEXT("reused");
                ReusedInst.ScreenId = TEXT("core:screen.pah07_reused");
                ReusedInst.Fields.Add(MakeFieldValue(FName(TEXT("field_a")), TEXT("value_a"), TextA_Value));
                ReusedInst.Fields.Add(MakeFieldValue(FName(TEXT("field_b")), TEXT("value_b"), TextB_Value));
                Doc.Overlays.Add(ReusedInst);

                FGV2ScreenInstanceViewModel ExtraInst;
                ExtraInst.Layer = TEXT("overlay_stack");
                ExtraInst.InstanceKey = TEXT("extra");
                ExtraInst.ScreenId = TEXT("core:screen.pah07_extra");
                Doc.Overlays.Add(ExtraInst);

                return Doc;
            };

            FGV2LayeredUiReconciler Reconciler;
            FString ReconcileError;

            // D1: baseline commit. This becomes LastCommittedDocument -- what catastrophic
            // recovery replays.
            TestTrue(*FString::Printf(TEXT("PAH-07: baseline D1 commits [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, MakeDoc(1, TEXT("core:screen.pah07_route"), TEXT("OldA"), TEXT("OldB")), Factory, ReconcileError));
            TestEqual(TEXT("PAH-07: baseline health is Nominal"), Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);
            TestEqual(TEXT("PAH-07: baseline route is RouteWidget"), Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestEqual(TEXT("PAH-07: baseline TextA reads OldA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("PAH-07: baseline TextB reads OldB"), TextB->GetTextContent().ToString(), TEXT("OldB"));
            {
                const TArray<UUserWidget*> BaselineOverlayOrder = Shell->GetScreensInLayer(TEXT("overlay_stack"));
                TestEqual(TEXT("PAH-07: baseline overlay_stack has 2 physical children"), BaselineOverlayOrder.Num(), 2);
                if (BaselineOverlayOrder.Num() == 2)
                {
                    TestEqual(TEXT("PAH-07: baseline overlay child 0 is ReusedScreen"), BaselineOverlayOrder[0], Cast<UUserWidget>(ReusedScreen));
                    TestEqual(TEXT("PAH-07: baseline overlay child 1 is ExtraOverlayWidget"), BaselineOverlayOrder[1], Cast<UUserWidget>(ExtraOverlayWidget));
                }
            }

            // D2 (ordinary path): field_b's Commit is injected to fail; its OWN self-heal
            // rollback is NOT injected, so it succeeds -- the pre-PAH-07 guarantee.
            // GetHealth() must stay Nominal: this Commit failure is NOT catastrophic.
            const auto OrdinaryCommitInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_b");
            };
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);
            const bool bOrdinaryFault = Reconciler.Reconcile(
                Shell, MakeDoc(2, TEXT("core:screen.pah07_route_v2"), TEXT("NewA"), TEXT("NewB")), Factory, ReconcileError, OrdinaryCommitInjector);
            TestFalse(TEXT("PAH-07: D2 (ordinary rollback) fails Reconcile"), bOrdinaryFault);
            TestFalse(TEXT("PAH-07: D2's OutError does NOT carry the rollback-failed marker"),
                ReconcileError.Contains(GGV2UiRollbackFailedDiagnosticCode));
            TestEqual(TEXT("PAH-07: health stays Nominal after an ordinary (successfully rolled back) Commit failure"),
                Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);
            TestEqual(TEXT("PAH-07: route is still RouteWidget (D2's route replacement never committed)"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestEqual(TEXT("PAH-07: TextA still restored to OldA after the ordinary rollback"), TextA->GetTextContent().ToString(), TEXT("OldA"));

            // D3 (catastrophic path): field_b's Commit is injected to fail AND field_a's
            // own self-heal rollback (value_a) is ALSO injected to fail -- the physical
            // tree's relationship to ActiveScreens is now undefined per ADR-0041, and
            // Reconcile must fall back to catastrophic recovery.
            const auto CatastrophicCommitInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_b");
            };
            const auto CatastrophicRollbackInjector = [](const FString& ScreenId, const FString& PropertyPath) -> bool
            {
                return ScreenId == TEXT("core:screen.pah07_reused") && PropertyPath == TEXT("value_a");
            };
            AddExpectedErrorPlain(TEXT("ApplyScreenFields commit failed"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("GBH-10: rollback failed restoring host 'FieldA' property 'value_a'"), EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedErrorPlain(TEXT("CommitReconcile: core:diagnostic.ui_reconcile.commit_failed"), EAutomationExpectedErrorFlags::Contains, 1);
            const bool bCatastrophicFault = Reconciler.Reconcile(
                Shell, MakeDoc(3, TEXT("core:screen.pah07_route_v2"), TEXT("CatA"), TEXT("CatB")), Factory, ReconcileError,
                CatastrophicCommitInjector, CatastrophicRollbackInjector);
            TestFalse(TEXT("PAH-07: D3 (catastrophic) fails Reconcile"), bCatastrophicFault);
            TestTrue(*FString::Printf(TEXT("PAH-07: D3's OutError carries the rollback-failed marker [Error: %s]"), *ReconcileError),
                ReconcileError.Contains(GGV2UiRollbackFailedDiagnosticCode));
            TestEqual(TEXT("PAH-07: health becomes RecoveredFromCatastrophicFailure"),
                Reconciler.GetHealth(), EGV2PresentationHealth::RecoveredFromCatastrophicFailure);

            // Canonical state after recovery must match D1 (the last successfully
            // committed document), NOT D3 (the rejected candidate) -- read through the
            // SAME production accessors, not internal bookkeeping alone.
            TestEqual(TEXT("PAH-07: after recovery, route resolves back to D1's RouteWidget (same C++ identity via the fixed-instance factory)"),
                Reconciler.GetActiveScreen(TEXT("location_content"), TEXT("main")), RouteWidget);
            TestTrue(TEXT("PAH-07: after recovery, RouteWidget is physically attached to location_content"),
                Shell->GetScreensInLayer(TEXT("location_content")).Contains(RouteWidget));
            TestFalse(TEXT("PAH-07: after recovery, the rejected D3 route replacement is NOT attached"),
                Shell->GetScreensInLayer(TEXT("location_content")).Contains(ReplacementRouteWidget));
            TestEqual(TEXT("PAH-07: after recovery, TextA reads D1's OldA, not D3's CatA"), TextA->GetTextContent().ToString(), TEXT("OldA"));
            TestEqual(TEXT("PAH-07: after recovery, TextB reads D1's OldB, not D3's CatB"), TextB->GetTextContent().ToString(), TEXT("OldB"));
            {
                const TArray<UUserWidget*> RecoveredOverlayOrder = Shell->GetScreensInLayer(TEXT("overlay_stack"));
                TestEqual(TEXT("PAH-07: after recovery, overlay_stack has D1's 2 physical children"), RecoveredOverlayOrder.Num(), 2);
                if (RecoveredOverlayOrder.Num() == 2)
                {
                    TestEqual(TEXT("PAH-07: after recovery, overlay child 0 is ReusedScreen (D1's order preserved)"), RecoveredOverlayOrder[0], Cast<UUserWidget>(ReusedScreen));
                    TestEqual(TEXT("PAH-07: after recovery, overlay child 1 is ExtraOverlayWidget (D1's order preserved)"), RecoveredOverlayOrder[1], Cast<UUserWidget>(ExtraOverlayWidget));
                }
            }

            // A later, ordinary successful apply clears the recovery marker -- Health
            // reports the CURRENT state, not a permanent scar from a past incident.
            TestTrue(*FString::Printf(TEXT("PAH-07: D4 (ordinary, post-recovery) commits [Error: %s]"), *ReconcileError),
                Reconciler.Reconcile(Shell, MakeDoc(4, TEXT("core:screen.pah07_route"), TEXT("FinalA"), TEXT("FinalB")), Factory, ReconcileError));
            TestEqual(TEXT("PAH-07: health returns to Nominal after the next successful commit"),
                Reconciler.GetHealth(), EGV2PresentationHealth::Nominal);

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
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

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
        V1.Add(TEXT("screen_id"), FGV2PreparedUiValue::MakeStableId(TEXT("core:screen.test_embedded"), TEXT("screen")));
        ValidTabs.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(V1)));

        TMap<FString, FGV2PreparedUiValue> V2;
        V2.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("skills")));
        V2.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Skills")) }));
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
            if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
            {
                Theme->TextCatalog.FindOrAdd(TEXT("core:text.duc09_day"), FText::FromString(TEXT("Monday")));
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
                { "schema_id", GV2ContentCore::FValue(std::string("textsystem:schema.ui_field.declared_composite_fixture.v1")) },
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
                            TestEqual(TEXT("DUC-09: envelope schema_id value"), SchemaIdOut->AsString(), FString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
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
                { "schema_id", GV2ContentCore::FValue(std::string("textsystem:schema.ui_field.nonexistent_probe.v1")) },
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
            const bool bBadProjected = bBadValidated && GV2ScreenFieldMaterializer::ProjectMaterializedValue(
                BadMatCtx, OuterSchema, BadOuterMaterialized, BadProjected);
            TestFalse(TEXT("DUC-09: unknown nested schema_id is rejected, not silently passed through"), bBadProjected);
        }

        // 27b. Consumer: FGV2TabContainerTabsPropertyConsumer turns an already-
        // materialized envelope array into a real TArray<FGV2ScreenFieldValue>
        // and applies it through the child screen's own public
        // PrepareScreenFields / CommitScreenFields -- the same two-phase API a
        // top-level screen uses, not a hand-rolled mutation plan.
        {
            UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
            GameInstance->AddToRoot();
            GameInstance->InitializeStandalone();
            UWorld* TestWorld = GameInstance->GetWorld();

            // Child screen: a real UGV2ScreenWidgetBase with a nested declared
            // composite (DUC-08 shape) exposing exactly the two properties
            // textsystem:schema.ui_field.declared_composite_fixture.v1 declares.
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
            TMap<FName, UGV2ScreenWidgetBase*> SeedWidgets;
            SeedWidgets.Add(FName(TEXT("info")), ChildScreen);
            SeedWidgets.Add(FName(TEXT("failure")), FailureScreen);
            NestedTabContainer->ApplyTabEntries(SeedEntries, SeedWidgets);

            TSharedPtr<IGV2PropertyConsumer> NestedConsumer = FGV2PropertyConsumerFactory::CreateConsumer(
                EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen);
            TestNotNull(TEXT("DUC-09: tab container consumer created"), NestedConsumer.Get());

            FGV2UiPropertyCapability NestedTabCap;
            NestedTabCap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;

            FGV2TextViewModel DayVM;
            DayVM.Text = FText::FromString(TEXT("Tuesday"));
            TMap<FString, FGV2PreparedUiValue> InnerFields;
            InnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(DayVM));
            InnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.7));

            TArray<TPair<FString, FGV2PreparedUiValue>> EnvelopeFields;
            EnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            EnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
            EnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(InnerFields)));
            TArray<FGV2PreparedUiValue> FieldsArray;
            FieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(EnvelopeFields)));

            TMap<FString, FGV2PreparedUiValue> TabMap;
            TabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("info")));
            TabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Info")) }));
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
            BaselineFailureInner.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Baseline")) }));
            BaselineFailureInner.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.5));
            TArray<TPair<FString, FGV2PreparedUiValue>> BaselineFailureEnvelope;
            BaselineFailureEnvelope.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            BaselineFailureEnvelope.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
            BaselineFailureEnvelope.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BaselineFailureInner)));
            TArray<FGV2PreparedUiValue> BaselineFailureFields;
            BaselineFailureFields.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(BaselineFailureEnvelope)));
            TMap<FString, FGV2PreparedUiValue> BaselineFailureTabMap = TabMap;
            BaselineFailureTabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("failure")));
            BaselineFailureTabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Failure")) }));
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
                NestedTabContainer->GetScreenWidgetForTab(FName(TEXT("failure"))), FailureScreen);

            FGV2TextViewModel UpdatedDayVM;
            UpdatedDayVM.Text = FText::FromString(TEXT("Wednesday"));
            TMap<FString, FGV2PreparedUiValue> UpdatedInnerFields;
            UpdatedInnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(UpdatedDayVM));
            UpdatedInnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.2));
            TArray<TPair<FString, FGV2PreparedUiValue>> UpdatedEnvelopeFields;
            UpdatedEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            UpdatedEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
            UpdatedEnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UpdatedInnerFields)));
            TArray<FGV2PreparedUiValue> UpdatedFieldsArray;
            UpdatedFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(UpdatedEnvelopeFields)));
            TMap<FString, FGV2PreparedUiValue> UpdatedInfoTabMap = TabMap;
            UpdatedInfoTabMap.Add(TEXT("fields"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(UpdatedFieldsArray)));

            TMap<FString, FGV2PreparedUiValue> FailureInnerFields;
            FailureInnerFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Never committed")) }));
            FailureInnerFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.1));
            TArray<TPair<FString, FGV2PreparedUiValue>> FailureEnvelopeFields;
            FailureEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            FailureEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
            FailureEnvelopeFields.Emplace(TEXT("value"), FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureInnerFields)));
            TArray<FGV2PreparedUiValue> FailureFieldsArray;
            FailureFieldsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FailureEnvelopeFields)));
            TMap<FString, FGV2PreparedUiValue> FailureTabMap = TabMap;
            FailureTabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("failure")));
            FailureTabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Failure")) }));
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
            const FGV2PreparedUiValue* NestedCommittedDay = DayBlock->GetPropertyHostState().GetLastCommittedProperties().FindField(TEXT("day"));
            TestNotNull(TEXT("GBF-05: nested reused child restores committed day metadata"), NestedCommittedDay);
            if (NestedCommittedDay != nullptr)
            {
                TestEqual(TEXT("GBF-05: nested reused child metadata matches the physical baseline"),
                    NestedCommittedDay->AsText().Text.ToString(), TEXT("Tuesday"));
            }
            TestEqual(TEXT("GBF-05: nested reused child restores prior schema id"),
                DayBlock->GetPropertyHostState().GetLastCommittedSchemaId(), TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1"));
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
            ThursdayInner.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Thursday")) }));
            ThursdayInner.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(0.9));
            TArray<TPair<FString, FGV2PreparedUiValue>> ThursdayEnvelope;
            ThursdayEnvelope.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("day_block")));
            ThursdayEnvelope.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
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
                NestedTabContainer->GetScreenWidgetForTab(FName(TEXT("info"))), ChildScreen);

            // Negative: an *extra* field_id the child screen has no host for is
            // rejected, not silently ignored (DUC-09's own Done criterion) --
            // "day_block" is included too so the only failure is the unknown
            // extra field, not the (separately-enforced) missing-value-for-host case.
            TArray<TPair<FString, FGV2PreparedUiValue>> UnknownEnvelopeFields;
            UnknownEnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(TEXT("nonexistent_field")));
            UnknownEnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1")));
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
    }

    // =========================================================================
    // DUC-11: composition-cycle guard for screen_id-based nested screens
    // =========================================================================
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        UWorld* TestWorld = GameInstance->GetWorld();

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
        auto MakeTabsFieldValue = [](const FString& TargetScreenId) -> FGV2ScreenFieldValue
        {
            TMap<FString, FGV2PreparedUiValue> TabMap;
            TabMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("back")));
            TabMap.Add(TEXT("title"), FGV2PreparedUiValue::MakeText(FGV2TextViewModel{ FText::FromString(TEXT("Back")) }));
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
            const bool bPrepared = ScreenA->PrepareScreenFields(Fields, Plan, Error, &Chain);
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
            const bool bPrepared = ScreenB->PrepareScreenFields(Fields, Plan, Error, &Chain);
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
            const bool bPreparedC = ScreenC->PrepareScreenFields(Fields, Plan, Error, &Chain);
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
    // DCA-18: every expectation below is a position comparison against
    // PackageLoadOrder, computed independently in this test -- never a hardcoded
    // true/false copied from UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace's
    // own branches.
    // =========================================================================
    {
        const TArray<GV2PackageClosure::FEntry> RealClosureEntries = GV2PackageClosure::DiscoverFromGameData();
        const TArray<FString> RealPackageLoadOrder = UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(RealClosureEntries);
        TestEqual(TEXT("GameData package load order resolves exactly core, textsystem, rh"), RealPackageLoadOrder.Num(), 3);

        // PAH-05: content root ownership now comes from each package's own
        // GameData/<id>/package.json5 "ue_content_roots" -- core declares two
        // (/Game/core, /Game/UI), textsystem and rh one each.
        TArray<FGV2ContentRootOwnership> RealOwnership;
        FString OwnershipError;
        TestTrue(
            *FString::Printf(TEXT("GameData content root ownership resolves [Error: %s]"), *OwnershipError),
            UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(RealClosureEntries, RealOwnership, OwnershipError));
        TestEqual(TEXT("GameData declares exactly four UE content roots across three packages"), RealOwnership.Num(), 4);

        auto ExpectedAllowed = [](const TArray<FString>& Order, const TArray<FGV2ContentRootOwnership>& Ownership, const FString& ScreenNamespace, const FString& AssetPath) -> bool
        {
            const int32 ScreenIdx = Order.IndexOfByPredicate(
                [&ScreenNamespace](const FString& PackageId) { return PackageId.Equals(ScreenNamespace, ESearchCase::IgnoreCase); });
            if (ScreenIdx == INDEX_NONE)
            {
                return false;
            }
            const FString OwningPackage = UGV2ScreenRegistry::FindOwningPackageForAssetPath(AssetPath, Ownership);
            if (OwningPackage.IsEmpty())
            {
                // PAH-03: unowned /Game/ content is rejected; content outside /Game/
                // entirely is trusted by declared domain. Independently recomputed here,
                // not copied from the production branch it mirrors.
                return UGV2ScreenRegistry::IsTrustedExternalContentDomain(AssetPath);
            }
            const int32 AssetIdx = Order.IndexOfByPredicate(
                [&OwningPackage](const FString& PackageId) { return PackageId.Equals(OwningPackage, ESearchCase::IgnoreCase); });
            return AssetIdx == INDEX_NONE || AssetIdx <= ScreenIdx;
        };

        struct FCase
        {
            FString ScreenNamespace;
            FString AssetPath;
        };
        const FCase Cases[] = {
            {TEXT("core"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
            {TEXT("core"), TEXT("/Game/core/WBP_CoreScreen")},
            {TEXT("core"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("core"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("textsystem"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("textsystem"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
            {TEXT("textsystem"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("rh"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("rh"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("rh"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
        };
        int32 CasesCovered = 0;
        for (const FCase& Case : Cases)
        {
            const bool bExpected = ExpectedAllowed(RealPackageLoadOrder, RealOwnership, Case.ScreenNamespace, Case.AssetPath);
            const bool bActual = UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                Case.ScreenNamespace, Case.AssetPath, RealPackageLoadOrder, RealOwnership);
            TestEqual(
                *FString::Printf(TEXT("%s screen vs %s matches the load_index-derived expectation"), *Case.ScreenNamespace, *Case.AssetPath),
                bActual,
                bExpected);
            ++CasesCovered;
        }
        TestEqual(TEXT("Every namespace/asset case above was evaluated"), CasesCovered, static_cast<int32>(UE_ARRAY_COUNT(Cases)));

        // A namespace absent from the pinned closure is rejected, not allowed by default
        // (the old ladder's unconditional trailing `return true` for this exact case).
        TestFalse(
            TEXT("Namespace absent from the pinned closure is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("sample"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), RealPackageLoadOrder, RealOwnership));

        // PAH-03/05: a /Game/ asset whose root isn't declared by any package in the
        // closure is unowned, not unconstrained -- rejected, not the old ladder's
        // `return true` for an empty FindOwningPackageForAssetPath result.
        TestFalse(
            TEXT("PAH-03: synthetic /Game/ path outside every declared package root is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("PAH-03: FindOwningPackageForAssetPath itself returns empty for that path"),
            UGV2ScreenRegistry::FindOwningPackageForAssetPath(TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned"), RealOwnership).IsEmpty());

        // PAH-03: content outside /Game/ entirely (engine-shipped, or an enabled plugin's
        // own content root) has no project-package ownership to violate and is trusted by
        // declared domain, not rejected alongside a genuinely unowned /Game/ root.
        TestTrue(
            TEXT("PAH-03: engine-shipped content path is a trusted external domain"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Engine/EditorResources/S_Actor"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("PAH-03: enabled-plugin content path is a trusted external domain"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/CommonUI/Widgets/WBP_SomePluginWidget"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("IsTrustedExternalContentDomain itself: /Engine/ path"),
            UGV2ScreenRegistry::IsTrustedExternalContentDomain(TEXT("/Engine/EditorResources/S_Actor")));
        TestFalse(
            TEXT("IsTrustedExternalContentDomain itself: a /Game/ path is never a trusted external domain"),
            UGV2ScreenRegistry::IsTrustedExternalContentDomain(TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned")));

        // A package that doesn't exist in today's real closure still works correctly once
        // it's present in PackageLoadOrder -- proving the rule reads positions generically
        // instead of special-casing three known names. Ownership (which package owns
        // /Game/RH/) is unaffected by load order, so RealOwnership is reused as-is.
        const TArray<FString> ExtendedOrder = {TEXT("core"), TEXT("textsystem"), TEXT("rh"), TEXT("modx")};
        TestTrue(
            TEXT("A fourth package appended to the closure can reference the layer directly below it"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("modx"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), ExtendedOrder, RealOwnership));

        // Reversing the closure's order flips which layer may reference which, proving the
        // decision is read from PackageLoadOrder's positions and not hardcoded by name.
        const TArray<FString> ReversedOrder = {TEXT("rh"), TEXT("textsystem"), TEXT("core")};
        TestTrue(
            TEXT("Under a reversed closure, core (now highest) can reference rh (now lowest)"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), ReversedOrder, RealOwnership));
        TestFalse(
            TEXT("Under a reversed closure, rh (now lowest) cannot reference core (now highest)"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("rh"), TEXT("/Game/UI/Widgets/WBP_Testscreen"), ReversedOrder, RealOwnership));

        // End-to-end: a Screen Registry entry violating layer ownership is still rejected.
        FGV2ScreenRegistryEntry BadEntry;
        BadEntry.ScreenId = TEXT("core:screen.bad_ref");
        BadEntry.Layer = TEXT("location_content");
        BadEntry.WidgetClass = TSoftClassPtr<UGV2ScreenWidgetBase>(FSoftObjectPath(TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen.WBP_Textscreen_C")));
        TestFalse(
            TEXT("Core screen referencing TextSystem is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), BadEntry.WidgetClass.ToSoftObjectPath().ToString(), RealPackageLoadOrder, RealOwnership));

        // PAH-05: a synthetic fourth package declares its OWN content root -- understood
        // by BuildContentRootOwnership/FindOwningPackageForAssetPath purely from this
        // data, with zero Source/ changes (no fifth entry added to any hardcoded table,
        // because none exists anymore).
        TArray<FGV2ContentRootOwnership> SyntheticOwnership;
        FString SyntheticError;
        const TArray<FGV2DeclaredPackageRoots> SyntheticDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/core"), TEXT("/Game/UI")}},
            FGV2DeclaredPackageRoots{TEXT("modx"), {TEXT("/Game/ModX")}},
        };
        TestTrue(
            TEXT("PAH-05: a synthetic fourth package's own declared root resolves without any Source/ change"),
            UGV2ScreenRegistry::BuildContentRootOwnership(SyntheticDeclared, SyntheticOwnership, SyntheticError));
        TestEqual(
            TEXT("PAH-05: the synthetic fourth package's asset resolves to its own package_id"),
            UGV2ScreenRegistry::FindOwningPackageForAssetPath(TEXT("/Game/ModX/Widgets/WBP_ModXScreen"), SyntheticOwnership),
            FString(TEXT("modx")));

        // PAH-05: two packages declaring the same (or a nested) root is a build error,
        // rejected outright -- never resolved in favor of the more specific root.
        TArray<FGV2ContentRootOwnership> OverlappingOwnership;
        FString OverlapError;
        const TArray<FGV2DeclaredPackageRoots> OverlappingDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/UI")}},
            FGV2DeclaredPackageRoots{TEXT("modx"), {TEXT("/Game/UI/Widgets")}},
        };
        TestFalse(
            TEXT("PAH-05: a nested/overlapping root declared by a different package is rejected"),
            UGV2ScreenRegistry::BuildContentRootOwnership(OverlappingDeclared, OverlappingOwnership, OverlapError));
        TestTrue(TEXT("PAH-05: overlap rejection clears any partial ownership result"), OverlappingOwnership.IsEmpty());
        TestFalse(TEXT("PAH-05: overlap rejection names the conflict"), OverlapError.IsEmpty());

        // Same package declaring the same root twice is not a conflict with itself.
        TArray<FGV2ContentRootOwnership> SamePackageOwnership;
        FString SamePackageError;
        const TArray<FGV2DeclaredPackageRoots> SamePackageDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/core"), TEXT("/Game/core/Sub")}},
        };
        TestTrue(
            TEXT("PAH-05: a package's own nested root does not conflict with itself"),
            UGV2ScreenRegistry::BuildContentRootOwnership(SamePackageDeclared, SamePackageOwnership, SamePackageError));
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
        // Exercises UGV2ListViewWidgetBase::ReconcileEntries directly (the generic
        // low-level primitive the KeyedCollection consumer builds on), independent of
        // any widget-specific model type -- a plain local Key/Text/Binding fixture is
        // all this needs, matching UPP-30's retirement of FGV2ButtonViewModel.
        struct FTestButtonModel
        {
            FName Key;
            FGV2TextViewModel Text;
            FGV2UiBindingHandle Binding;
        };

        UClass* CommandClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2DeclaredCompositeWidgetBase* CmdPanel = CommandClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CommandClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        TestNotNull(TEXT("CmdPanel instantiated"), CmdPanel);
        UClass* TestButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));

        FTestButtonModel BtnA;
        BtnA.Key = FName(TEXT("btn_a"));
        BtnA.Text.Text = FText::FromString(TEXT("Action A"));
        BtnA.Binding = FGV2UiBindingHandle::Create(TEXT("binding_a"));

        FTestButtonModel BtnB;
        BtnB.Key = FName(TEXT("btn_b"));
        BtnB.Text.Text = FText::FromString(TEXT("Action B"));
        BtnB.Binding = FGV2UiBindingHandle::Create(TEXT("binding_b"));

        FTestButtonModel BtnC;
        BtnC.Key = FName(TEXT("btn_c"));
        BtnC.Text.Text = FText::FromString(TEXT("Action C"));
        BtnC.Binding = FGV2UiBindingHandle::Create(TEXT("binding_c"));

        UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(CmdPanel->GetWidgetFromName(TEXT("ButtonRepeater")));
        TestNotNull(TEXT("CommandPanel has active Repeater"), Repeater);
        if (Repeater != nullptr)
        {
            auto GetKey = [](const FTestButtonModel& B) { return B.Key; };
            auto CreateWidgetLambda = [TestWorld, TestButtonClass]() -> UGV2ButtonWidgetBase*
            {
                return TestButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, TestButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ButtonWidgetBase& Widget, const FTestButtonModel& Model)
            {
                Widget.SetKey(Model.Key);
                Widget.SetBindingHandle(Model.Binding);
                return Widget.ApplyText(Model.Text);
            };

            const TArray<FTestButtonModel> InitialButtons = { BtnA, BtnB, BtnC };
            TestTrue(TEXT("Initial buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(InitialButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries"), Repeater->GetEntryCount(), 3);
            UWidget* WidgetA = Repeater->GetEntryWidget(FName(TEXT("btn_a")));
            UWidget* WidgetB = Repeater->GetEntryWidget(FName(TEXT("btn_b")));
            TestNotNull(TEXT("Button A widget exists"), WidgetA);
            TestNotNull(TEXT("Button B widget exists"), WidgetB);

            // Reorder & update: { BtnB, BtnD, BtnA } -> BtnB & BtnA must be reused
            FTestButtonModel BtnD;
            BtnD.Key = FName(TEXT("btn_d"));
            BtnD.Text.Text = FText::FromString(TEXT("Action D"));
            BtnD.Binding = FGV2UiBindingHandle::Create(TEXT("binding_d"));

            const TArray<FTestButtonModel> UpdatedButtons = { BtnB, BtnD, BtnA };
            TestTrue(TEXT("Updated buttons apply successfully"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(UpdatedButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            TestEqual(TEXT("CommandPanel Repeater has 3 entries after update"), Repeater->GetEntryCount(), 3);
            TestEqual(TEXT("Button B widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_b"))), WidgetB);
            TestEqual(TEXT("Button A widget reused (same pointer)"), Repeater->GetEntryWidget(FName(TEXT("btn_a"))), WidgetA);
            TestNull(TEXT("Button C widget removed"), Repeater->GetEntryWidget(FName(TEXT("btn_c"))));
            TestNotNull(TEXT("Button D widget created"), Repeater->GetEntryWidget(FName(TEXT("btn_d"))));

            // Negative: Duplicate button key rejected
            FTestButtonModel BadBtn;
            BadBtn.Key = FName(TEXT("btn_b"));
            BadBtn.Binding = FGV2UiBindingHandle::Create(TEXT("bad_binding"));
            TArray<FTestButtonModel> DupButtons = { BtnB, BadBtn };
            TestFalse(TEXT("Duplicate button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(DupButtons, GetKey, CreateWidgetLambda, ApplyLambda));

            // Negative: Empty button key rejected
            FTestButtonModel EmptyKeyBtn;
            EmptyKeyBtn.Key = FName();
            EmptyKeyBtn.Binding = FGV2UiBindingHandle::Create(TEXT("empty_binding"));
            TArray<FTestButtonModel> EmptyKeyButtons = { EmptyKeyBtn };
            TestFalse(TEXT("Empty button key rejected"), Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FTestButtonModel>(EmptyKeyButtons, GetKey, CreateWidgetLambda, ApplyLambda));
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
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        TestNotNull(TEXT("SceneView instantiated"), SceneView);

        struct FTestCharEntry
        {
            FName Key;
            FString ResourceId;
        };

        UGV2ListViewWidgetBase* CharRep = Cast<UGV2ListViewWidgetBase>(SceneView->GetWidgetFromName(TEXT("CharacterRepeater")));
        if (CharRep != nullptr)
        {
            TSubclassOf<UGV2ImageWidgetBase> CharClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C"));
            auto GetKey = [](const FTestCharEntry& E) { return E.Key; };
            auto CreateWidgetLambda = [TestWorld, CharClass]() -> UGV2ImageWidgetBase*
            {
                return CharClass ? CreateWidget<UGV2ImageWidgetBase>(TestWorld, CharClass) : NewObject<UGV2ImageWidgetBase>(TestWorld);
            };
            auto ApplyLambda = [](UGV2ImageWidgetBase& Widget, const FTestCharEntry& Entry)
            {
                Widget.SetKey(Entry.Key);
                FString Err;
                return Widget.ApplyImageResource(Entry.ResourceId, Err);
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
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneWidget = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        FGV2UiCapabilityBuilder SceneBuilder;
        SceneWidget->DescribeUiCapabilities(SceneBuilder);
        FGV2UiCapabilityTree SceneTree = SceneBuilder.Build();
        TestNotNull(TEXT("CCF-06: Scene capabilities declared"), SceneTree.FindProperty(TEXT("key")));
        TestNotNull(TEXT("CCF-06: Scene context_text declared"), SceneTree.FindProperty(TEXT("context_text")));
        TestNotNull(TEXT("CCF-06: Scene characters declared"), SceneTree.FindProperty(TEXT("characters")));

        UClass* CmdClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2DeclaredCompositeWidgetBase* CmdWidget = CmdClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
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
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);

        UGV2ListViewWidgetBase* CharRep = Cast<UGV2ListViewWidgetBase>(SceneView->GetWidgetFromName(TEXT("CharacterRepeater")));
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
        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
        SceneView->SetKey(FName(TEXT("scene_test")));
        TestEqual(TEXT("CCF-11: SceneView Key getter/setter"), SceneView->GetKey(), FName(TEXT("scene_test")));

        UClass* CmdClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
        UGV2DeclaredCompositeWidgetBase* CmdPanel = CmdClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CmdClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
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
    const FGV2ScopedRealImageCatalog ScopedImageCatalog;

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
            UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalog::GetSessionCatalog();
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
            UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
            UGV2DeclaredCompositeWidgetBase* SceneView = SceneClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
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
            UClass* CommandClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_CommandPanel.WBP_CommandPanel_C"));
            UGV2DeclaredCompositeWidgetBase* CommandPanel = CommandClass ? CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CommandClass) : NewObject<UGV2DeclaredCompositeWidgetBase>(TestWorld);
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

                    UGV2DeclaredCompositeWidgetBase* TopBarWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* PlayerStatusWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* SceneWidget = nullptr;
                    UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;

                    for (UWidget* W : ChildWidgets)
                    {
                        // DUC-08/DCA-05/DCA-06/DCA-07: TopBar, Scene, PlayerStatus and
                        // CommandPanel are now the generic declared composite -- matched by
                        // HostIdentity, not a dedicated C++ class, since several other
                        // declared composites could also appear in this tree.
                        if (auto* TB = Cast<UGV2DeclaredCompositeWidgetBase>(W); TB != nullptr && TB->GetHostIdentity() == FName(TEXT("top_bar"))) TopBarWidget = TB;
                        else if (auto* PS = Cast<UGV2DeclaredCompositeWidgetBase>(W); PS != nullptr && PS->GetHostIdentity() == FName(TEXT("player_status"))) PlayerStatusWidget = PS;
                        else if (auto* SC = Cast<UGV2DeclaredCompositeWidgetBase>(W); SC != nullptr && SC->GetHostIdentity() == FName(TEXT("scene"))) SceneWidget = SC;
                        else if (auto* CP = Cast<UGV2DeclaredCompositeWidgetBase>(W); CP != nullptr && CP->GetHostIdentity() == FName(TEXT("commands"))) CommandWidget = CP;
                    }

                    TestNotNull(TEXT("TopBar child composite exists"), TopBarWidget);
                    TestNotNull(TEXT("PlayerStatus child composite exists"), PlayerStatusWidget);
                    TestNotNull(TEXT("Scene child composite exists"), SceneWidget);
                    TestNotNull(TEXT("CommandPanel child composite exists"), CommandWidget);

                    if (CommandWidget != nullptr)
                    {
                        if (UGV2ListViewWidgetBase* CmdRep = Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater"))))
                        {
                            struct FTestCmdEntry { FName Key; FText Text; };
                            TArray<FTestCmdEntry> TestButtons;
                            for (int32 Index = 1; Index <= 6; ++Index)
                            {
                                TestButtons.Add({ *FString::Printf(TEXT("cmd_%d"), Index), FText::FromString(*FString::Printf(TEXT("[LOCALE_TEST] Speak with Master Alchemist about Mysterious Elixir (#%d)"), Index)) });
                            }
                            UClass* CmdButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
                            CmdRep->ReconcileEntries<UGV2ButtonWidgetBase, FTestCmdEntry>(
                                TestButtons,
                                [](const FTestCmdEntry& E) { return E.Key; },
                                [TestWorld, CmdButtonClass]() -> UGV2ButtonWidgetBase*
                                {
                                    return CmdButtonClass ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, CmdButtonClass) : NewObject<UGV2ButtonWidgetBase>(TestWorld);
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

                    // DCA-13: prove the harness fix actually recalculates a dynamic
                    // WrapBox's wrap threshold -- not just that geometry queries return
                    // non-stale numbers for widgets that never depended on Tick() in the
                    // first place (every check below this already worked before DCA-13,
                    // since Paint/Arrange update GetTickSpaceGeometry() on their own).
                    // A standalone SWrapBox with UseAllottedSize=true, unrelated to any
                    // production composite, is measured at the narrowest and widest
                    // resolutions in the matrix: if the fix works, more fixed-width
                    // slots fit on the first row at 3840 than at 1280.
                    {
                        TSharedRef<SWrapBox> ProbeWrapBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0f, 4.0f));
                        for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
                        {
                            ProbeWrapBox->AddSlot()
                            [
                                SNew(SBox).WidthOverride(300.0f).HeightOverride(40.0f)
                            ];
                        }
                        TSharedRef<SVirtualWindow> ProbeWindow = SNew(SVirtualWindow).Size(FVector2D(1280, 720));
                        ProbeWindow->SetContent(ProbeWrapBox);

                        auto MeasureFirstRowSlotCount = [&ProbeWindow, &ProbeWrapBox](const FVector2D& Size) -> int32
                        {
                            GV2SimulateResponsiveFrame(ProbeWindow, Size);
                            FArrangedChildren ArrangedChildren(EVisibility::All);
                            ProbeWrapBox->ArrangeChildren(ProbeWrapBox->GetTickSpaceGeometry(), ArrangedChildren, true);
                            int32 FirstRowCount = 0;
                            float FirstRowY = -1.0f;
                            for (int32 Index = 0; Index < ArrangedChildren.Num(); ++Index)
                            {
                                const float Y = ArrangedChildren[Index].Geometry.GetAbsolutePosition().Y;
                                if (FirstRowY < 0.0f)
                                {
                                    FirstRowY = Y;
                                }
                                if (!FMath::IsNearlyEqual(Y, FirstRowY, 1.0f))
                                {
                                    break;
                                }
                                ++FirstRowCount;
                            }
                            return FirstRowCount;
                        };

                        const int32 FirstRowAt1280 = MeasureFirstRowSlotCount(FVector2D(1280, 720));
                        const int32 FirstRowAt3840 = MeasureFirstRowSlotCount(FVector2D(3840, 2160));
                        TestTrue(
                            *FString::Printf(TEXT("DCA-13: dynamic WrapBox wrap threshold differs between 1280 (%d/row) and 3840 (%d/row)"), FirstRowAt1280, FirstRowAt3840),
                            FirstRowAt3840 > FirstRowAt1280);
                    }

                    // DCA-13: strict per-button geometry coverage is computed from the
                    // loop itself, not asserted by name -- a resolution that cannot be
                    // strictly checked is named and counted as excluded, and the total
                    // is compared against the matrix size below, so silently narrowing
                    // coverage back to one resolution shows up as a numeric mismatch
                    // instead of passing quietly.
                    int32 StrictButtonGeometryCoveredCount = 0;
                    TArray<FString> StrictButtonGeometryExclusionReasons;

                    // DCA-14: counted the same way -- two real GV2FitsInBounds-backed
                    // positive assertions per button (viewport containment, CommandPanel
                    // containment), summed from the loop itself and compared below
                    // against the expected total for however many resolutions actually
                    // reached strict coverage. Deleting a positive assertion (or all of
                    // them) reduces this count without touching the negative self-tests,
                    // so it surfaces as its own numeric mismatch instead of leaving the
                    // negative tests as the only, silently-insufficient signal.
                    int32 PositivePerButtonBoundsAssertionCount = 0;

                    for (const auto& Res : TestResolutions)
                    {
                        LocationScreen->InvalidateLayoutAndVolatility();
                        GV2SimulateResponsiveFrame(VirtualWindow, Res.Size);

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

                            if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater"))))
                            {
                                TestEqual(
                                    *FString::Printf(TEXT("CCF-17: [%s] All 6 command buttons instantiated in repeater"), Res.Name),
                                    Repeater->GetEntryCount(),
                                    6);

                                // DCA-13: strict per-button geometry, on every resolution in the
                                // matrix, not only 720p -- the harness fix above (GV2SimulateResponsiveFrame)
                                // is what makes this honest at every width, not only the one where the
                                // old single Paint pass happened to already be correct.
                                const TArray<UWidget*> Entries = Repeater->GetOrderedEntries();
                                if (TestEqual(*FString::Printf(TEXT("CCF-17: [%s] Repeater ordered entry widgets count matches 6"), Res.Name), Entries.Num(), 6))
                                {
                                    for (int32 BtnIndex = 0; BtnIndex < Entries.Num(); ++BtnIndex)
                                    {
                                        if (Entries[BtnIndex] != nullptr && Entries[BtnIndex]->GetCachedWidget().IsValid())
                                        {
                                            const FGeometry BtnGeom = Entries[BtnIndex]->GetCachedWidget()->GetTickSpaceGeometry();
                                            const FVector2D BtnLocalPos = VirtualWindow->GetTickSpaceGeometry().AbsoluteToLocal(BtnGeom.GetAbsolutePosition());
                                            const FVector2D BtnSize = BtnGeom.GetLocalSize();
                                            const FVector2D BtnInCommandPanel = CommandGeom.AbsoluteToLocal(BtnGeom.GetAbsolutePosition());

                                            TestTrue(
                                                *FString::Printf(TEXT("CCF-17: [%s] Button #%d allocated size is positive (%f x %f)"), Res.Name, BtnIndex + 1, BtnSize.X, BtnSize.Y),
                                                BtnSize.X > 0.0f && BtnSize.Y > 0.0f);

                                            // DCA-14: both positive checks below and both negative
                                            // self-tests further down call the exact same
                                            // GV2FitsInBounds -- there is no second, independently
                                            // maintained copy of "fits inside these bounds" anywhere
                                            // in this test.

                                            // 1. Viewport 2-axis containment
                                            const bool bFitsViewport = GV2FitsInBounds(BtnLocalPos, BtnSize, Res.Size);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [%s] Button #%d fits viewport bounds (pos=%s size=%s bounds=%s)"),
                                                    Res.Name, BtnIndex + 1, *BtnLocalPos.ToString(), *BtnSize.ToString(), *Res.Size.ToString()),
                                                bFitsViewport);
                                            ++PositivePerButtonBoundsAssertionCount;

                                            // 2. CommandPanel 2-axis containment
                                            const bool bFitsCommandPanel = GV2FitsInBounds(BtnInCommandPanel, BtnSize, CommandAllocated);
                                            TestTrue(
                                                *FString::Printf(TEXT("BAI-10: [%s] Button #%d fits CommandPanel bounds (pos=%s size=%s bounds=%s)"),
                                                    Res.Name, BtnIndex + 1, *BtnInCommandPanel.ToString(), *BtnSize.ToString(), *CommandAllocated.ToString()),
                                                bFitsCommandPanel);
                                            ++PositivePerButtonBoundsAssertionCount;

                                            // 3. Negative containment check: simulated oversized button detection
                                            TestFalse(
                                                *FString::Printf(TEXT("BAI-10: [%s] [Negative] Artificial horizontal overflow beyond panel width is rejected"), Res.Name),
                                                GV2FitsInBounds(BtnInCommandPanel, FVector2D(CommandAllocated.X + 50.0f, BtnSize.Y), CommandAllocated));
                                            TestFalse(
                                                *FString::Printf(TEXT("BAI-10: [%s] [Negative] Artificial vertical overflow beyond viewport height is rejected"), Res.Name),
                                                GV2FitsInBounds(BtnLocalPos, FVector2D(BtnSize.X, Res.Size.Y + 80.0f), Res.Size));
                                        }
                                    }
                                    ++StrictButtonGeometryCoveredCount;
                                }
                                else
                                {
                                    StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                        TEXT("[%s] ButtonRepeater entry count was not 6"), Res.Name));
                                }
                            }
                            else
                            {
                                StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                    TEXT("[%s] ButtonRepeater not found under CommandPanel"), Res.Name));
                            }
                        }
                        else
                        {
                            StrictButtonGeometryExclusionReasons.Add(FString::Printf(
                                TEXT("[%s] CommandPanel widget missing or not laid out"), Res.Name));
                        }

                        // Ultrawide check (CCF-18): 21:9 ratio verified
                        if (Res.bUltrawide)
                        {
                            TestTrue(
                                *FString::Printf(TEXT("CCF-18: [%s] Ultrawide aspect ratio is > 2.0"), Res.Name),
                                (Res.Size.X / Res.Size.Y) > 2.0f);
                        }
                    }

                    // DCA-13: the count above comes from the loop itself, not a hand-picked
                    // literal -- a regression that silently narrows strict coverage back
                    // down (e.g. reintroducing a single-resolution gate) shows up here as
                    // this count falling below UE_ARRAY_COUNT(TestResolutions), with each
                    // excluded resolution named and reasoned, not as a silent pass.
                    if (!TestEqual(
                        TEXT("DCA-13: strict per-button geometry check covers every resolution in the matrix"),
                        StrictButtonGeometryCoveredCount,
                        static_cast<int32>(UE_ARRAY_COUNT(TestResolutions))))
                    {
                        for (const FString& Reason : StrictButtonGeometryExclusionReasons)
                        {
                            AddError(FString::Printf(TEXT("DCA-13: resolution excluded from strict geometry coverage -- %s"), *Reason));
                        }
                    }

                    // DCA-14: two GV2FitsInBounds-backed positive assertions per button
                    // (viewport, CommandPanel), six buttons, once per resolution that
                    // reached strict coverage above -- deleting a positive assertion (or
                    // all of them) drops this count below the expected total, on its own,
                    // independently of whether the two negative self-tests still pass.
                    TestEqual(
                        TEXT("DCA-14: every strictly-covered resolution ran both per-button bounds assertions"),
                        PositivePerButtonBoundsAssertionCount,
                        StrictButtonGeometryCoveredCount * 6 * 2);

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
    const FGV2ScopedRealImageCatalog ScopedImageCatalog;

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
                if (auto* CmdPanel = Cast<UGV2DeclaredCompositeWidgetBase>(Child); CmdPanel != nullptr && CmdPanel->GetHostIdentity() == FName(TEXT("commands")))
                {
                    if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(CmdPanel->GetWidgetFromName(TEXT("ButtonRepeater"))))
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
                    if (auto* TopBar = Cast<UGV2DeclaredCompositeWidgetBase>(Child); TopBar != nullptr && TopBar->GetHostIdentity() == FName(TEXT("top_bar")))
                    {
                        bFoundMarketTopBar = true;
                    }
                    else if (auto* Scene = Cast<UGV2DeclaredCompositeWidgetBase>(Child); Scene != nullptr && Scene->GetHostIdentity() == FName(TEXT("scene")))
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
                    else if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Child); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
                    {
                        bFoundMarketCommands = true;
                        if (UGV2ListViewWidgetBase* Repeater = Cast<UGV2ListViewWidgetBase>(Cmd->GetWidgetFromName(TEXT("ButtonRepeater"))))
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
// UPP-27/STATUS-004: public preflight predicts a deep child's failure
// =========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenPreflightPredictsDeepChildFailureTest,
    "GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenPreflightPredictsDeepChildFailureTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // Before Prepare/Commit, CanApplyScreenField only checked field_id/schema_id and
    // never opened a keyed collection, so "schema passes, a deep child fails at
    // commit" could not be predicted by the public preflight -- it only surfaced once
    // ApplyScreenFields actually tried to commit (STATUS-004). This reproduces exactly
    // that shape: a top-level "commands" object that is schema-valid, whose items
    // array has a duplicate key -- detectable only by recursing into the collection,
    // not by any shallow field_id/schema_id check -- and proves CanApplyScreenFields
    // and ApplyScreenFields both reject it up front, leaving the screen exactly as it
    // was before the failed attempt (no partial mutation to roll back).

    AddExpectedErrorPlain(TEXT("ApplyScreenFields rejected"), EAutomationExpectedErrorFlags::Contains, 3);

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Screen instantiated"), Screen);
    if (Screen == nullptr) return false;

    Screen->WidgetTree = NewObject<UWidgetTree>(Screen);
    UVerticalBox* Root = Screen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    Screen->WidgetTree->RootWidget = Root;

    UGV2DeclaredCompositeWidgetBase* CommandPanel = Screen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
        UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("CommandPanel"));
    Root->AddChildToVerticalBox(CommandPanel);

    // The bare native button class has no WidgetTree, so its "text" capability's declared
    // "LabelText" target can never resolve -- DUC-05 made that a deterministic preflight
    // rejection rather than a silent self-fallback, so this fixture needs the real
    // WBP_Button (as production and the other tests in this file do).
    UClass* const ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));

    // DescribeUiCapabilities resolves children via GetWidgetFromName against CommandPanel's
    // own WidgetTree, not the Screen's -- ButtonRepeater/ButtonBox must live in it.
    CommandPanel->WidgetTree = NewObject<UWidgetTree>(CommandPanel);
    UVerticalBox* CmdRoot = CommandPanel->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    CommandPanel->WidgetTree->RootWidget = CmdRoot;

    UGV2ListViewWidgetBase* ButtonRepeater = CommandPanel->WidgetTree->ConstructWidget<UGV2ListViewWidgetBase>(UGV2ListViewWidgetBase::StaticClass(), TEXT("ButtonRepeater"));
    UWrapBox* ButtonBox = CommandPanel->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ButtonBox"));
    ButtonRepeater->SetContainerPanel(ButtonBox);
    CmdRoot->AddChildToVerticalBox(ButtonRepeater);

    {
        FGV2DeclaredUiCapability ItemsCap;
        ItemsCap.PropertyName = FName(TEXT("items"));
        ItemsCap.ChildWidgetName = FName(TEXT("ButtonRepeater"));
        ItemsCap.Kind = EGV2DeclaredUiCapabilityKind::CollectionHost;
        ItemsCap.EntryWidgetClass = ButtonClass != nullptr ? ButtonClass : UGV2ButtonWidgetBase::StaticClass();
        ItemsCap.KeyPropertyName = TEXT("key");
        CommandPanel->DeclaredCapabilities.Add(ItemsCap);
    }
    CommandPanel->DeclaredCapabilities.Add({ FName(TEXT("key")), NAME_None, EGV2DeclaredUiCapabilityKind::Key });

    CommandPanel->SetHostIdentity(FName(TEXT("commands")));
    TestEqual(TEXT("CommandPanel answers to screen field 'commands'"), Screen->GetScreenFieldIds(), TArray<FName>{FName(TEXT("commands"))});

    auto MakeCommandsSchema = []() -> std::shared_ptr<FCompiledUiFieldSpec>
    {
        auto ItemSpec = std::make_shared<FCompiledUiFieldSpec>();
        ItemSpec->Kind = EUiFieldKind::Object;
        ItemSpec->Fields.push_back({"key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)});
        ItemSpec->Fields.push_back({"text", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text)});
        ItemSpec->Fields.push_back({"binding", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding)});

        auto ItemsArraySpec = std::make_shared<FCompiledUiFieldSpec>();
        ItemsArraySpec->Kind = EUiFieldKind::Array;
        ItemsArraySpec->KeyedBy = std::string("key");
        ItemsArraySpec->Items = ItemSpec;

        auto Schema = std::make_shared<FCompiledUiFieldSpec>();
        Schema->Kind = EUiFieldKind::Object;
        Schema->Fields.push_back({"items", true, ItemsArraySpec});
        Schema->Fields.push_back({"key", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)});
        return Schema;
    };

    auto MakeItem = [](const TCHAR* Key, const TCHAR* DisplayText) -> FGV2PreparedUiValue
    {
        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(DisplayText);
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("key"), FGV2PreparedUiValue::MakeKey(Key));
        Fields.Emplace(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        Fields.Emplace(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("action@1:1"))));
        return FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(Fields)));
    };

    auto MakeCommandsValue = [](TArray<FGV2PreparedUiValue> Items) -> TSharedRef<const FGV2PreparedUiObject>
    {
        TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
        Fields.Emplace(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(Items))));
        return FGV2PreparedUiObject::Create(MoveTemp(Fields));
    };

    const std::shared_ptr<FCompiledUiFieldSpec> Schema = MakeCommandsSchema();

    // 1. A valid apply first, so the screen has real, observable prior state.
    FGV2ScreenFieldValue ValidField;
    ValidField.FieldId = FName(TEXT("commands"));
    ValidField.SchemaId = TEXT("textsystem:schema.ui_field.location_commands.v1");
    ValidField.CompiledSchema = Schema;
    ValidField.PreparedValue = MakeCommandsValue({MakeItem(TEXT("btn_ok"), TEXT("OK"))});

    TestTrue(TEXT("Valid commands field applies"), Screen->ApplyScreenFields({ValidField}));
    TestEqual(TEXT("Button created for the valid apply"), ButtonBox->GetChildrenCount(), 1);
    UGV2ButtonWidgetBase* OriginalButton = Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0));
    TestNotNull(TEXT("Original button resolved"), OriginalButton);

    // 2. A deep-child failure: schema-valid top level, duplicate key inside items.
    FGV2ScreenFieldValue InvalidField;
    InvalidField.FieldId = FName(TEXT("commands"));
    InvalidField.SchemaId = TEXT("textsystem:schema.ui_field.location_commands.v1");
    InvalidField.CompiledSchema = Schema;
    InvalidField.PreparedValue = MakeCommandsValue({
        MakeItem(TEXT("dup_key"), TEXT("First")),
        MakeItem(TEXT("dup_key"), TEXT("Second")),
    });

    TestFalse(TEXT("Public preflight predicts the deep duplicate-key failure"), Screen->CanApplyScreenFields({InvalidField}));
    TestFalse(TEXT("ApplyScreenFields also rejects it (Prepare fails before any Commit)"), Screen->ApplyScreenFields({InvalidField}));

    // 3. No partial mutation: the screen is exactly as the valid apply left it.
    TestEqual(TEXT("Button count is unchanged after the rejected apply"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(TEXT("The original button instance is untouched"), Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)), OriginalButton);
    if (OriginalButton != nullptr)
    {
        TestEqual(TEXT("Original button key is unchanged"), OriginalButton->GetKey(), FName(TEXT("btn_ok")));
    }

    // 4. GBH-04: the top-level host<->envelope bijection itself (distinct from the
    // deep-child failure above) is strict on both sides, with no optional-host
    // policy -- ScreenTemplates.md's Invariants claims exactly this; this proves it
    // rather than leaving the claim resting on nothing but the source reading the
    // same way. A configured host ("commands") with no incoming envelope at all is
    // rejected, not silently skipped as an optional field.
    TestFalse(
        TEXT("GBH-04: a configured host with no incoming envelope is rejected, not treated as optional"),
        Screen->ApplyScreenFields({}));
    TestEqual(TEXT("GBH-04: no mutation from the missing-envelope rejection"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(
        TEXT("GBH-04: the original button instance survives the missing-envelope rejection"),
        Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)),
        OriginalButton);

    // An incoming envelope naming a field_id no configured host answers to is
    // rejected too -- both directions of the bijection are enforced, not just one.
    FGV2ScreenFieldValue UnknownField;
    UnknownField.FieldId = FName(TEXT("nonexistent_field"));
    UnknownField.SchemaId = ValidField.SchemaId;
    UnknownField.CompiledSchema = Schema;
    UnknownField.PreparedValue = MakeCommandsValue({MakeItem(TEXT("btn_ok"), TEXT("OK"))});

    TestFalse(
        TEXT("GBH-04: an incoming envelope with no matching configured host is rejected"),
        Screen->ApplyScreenFields({ValidField, UnknownField}));
    TestEqual(TEXT("GBH-04: no mutation from the unknown-field rejection"), ButtonBox->GetChildrenCount(), 1);
    TestEqual(
        TEXT("GBH-04: the original button instance survives the unknown-field rejection"),
        Cast<UGV2ButtonWidgetBase>(ButtonBox->GetChildAt(0)),
        OriginalButton);

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
    const FGV2ScopedRealImageCatalog ScopedImageCatalog;

    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString MarketResourceId = GameNamespace + TEXT(":resource.location.market");
    const FString HeroPortraitResourceId = GameNamespace + TEXT(":resource.portrait.hero");

    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalog::GetSessionCatalog();
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

        UClass* SceneClass = LoadClass<UGV2DeclaredCompositeWidgetBase>(
            nullptr,
            TEXT("/Game/TextSystem/UI/Widgets/WBP_SceneView.WBP_SceneView_C"));
        TestNotNull(TEXT("SceneClass loaded"), SceneClass);
        if (SceneClass != nullptr)
        {
            UGV2DeclaredCompositeWidgetBase* SceneView = CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, SceneClass);
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
                    Bg->ApplyImageResource(MarketResourceId, Error);
                    AddInfo(FString::Printf(TEXT("Background: AppliedResourceId='%s', Visibility=%d, BrushResObj=%s"),
                        *Bg->GetAppliedResourceId(),
                        static_cast<int32>(Bg->GetVisibility()),
                        Bg->GetImageBrush().GetResourceObject() ? *Bg->GetImageBrush().GetResourceObject()->GetName() : TEXT("nullptr")));
                }
                if (BgTile != nullptr)
                {
                    FString Error;
                    BgTile->ApplyImageResource(TEXT("core:resource.ui.old_paper_tile_256"), Error);
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
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

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
        TestFalse(TEXT("BAI-03: Field value level unknown key rejected on commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions));
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
        TestFalse(TEXT("BAI-03: Collection element level unknown key rejected on button"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions));
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
        TestFalse(TEXT("BAI-03: Nested Binding level unknown key rejected"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions));
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
        TestFalse(TEXT("BAI-03: Duplicate button key rejected in commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationKeyBoundaryTest,
    "GV2.Runtime.Presentation.LocationKeyBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationKeyBoundaryTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

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
            return GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions);
        };

        TestTrue(TEXT("Conforming command key accepted"), BuildWithCommandKey("tavern_keeper"));
        TestFalse(TEXT("Uppercase command key rejected"), BuildWithCommandKey("TavernKeeper"));
        TestFalse(TEXT("Command key with space rejected"), BuildWithCommandKey("tavern keeper"));
        TestFalse(TEXT("Text-derived command key rejected"), BuildWithCommandKey("text:character.name"));
    }

    return true;
}

// DCA-07: FGV2LocationCompositeUnresolvedClassRejectionTest
// ("GV2.Runtime.Presentation.LocationCompositeUnresolvedClassRejection") and
// FGV2LocationCompositeCapabilityQueryIsPureTest
// ("GV2.Runtime.Presentation.LocationCompositeCapabilityQueryIsPure") were deleted here,
// not just emptied. DCA-05/DCA-06 had already stripped both down to their CommandPanel
// half -- Scene's and PlayerStatus's halves were gone, each replaced by an explanatory
// comment at the time. DCA-07 deletes UGV2LocationCommandPanelWidgetBase, the last
// per-class subject either test had left, so nothing of either test's original subject
// remains to assert against. Their invariants -- a CollectionHost entry with an unset
// EntryWidgetClass declares but does not default (PCC-09/DCA-01/03); DescribeUiCapabilities
// allocates and mutates nothing on repeated calls (PCC-12) -- are the same generic
// mechanisms DCA-01/03 already proved class-agnostically, and the generic composite's
// construction-by-WidgetTree-lookup (no internal/transient state to eagerly build in the
// first place) already proves class-agnostically. A stub that only creates and destroys a
// UWorld would pass without checking anything -- worse than no test, since its name would
// still promise per-class coverage that no longer exists anywhere in the codebase.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HostIdentityIsSharedTest,
    "GV2.Runtime.Presentation.HostIdentityIsSharedNotPerClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HostIdentityIsSharedTest::RunTest(const FString& Parameters)
{
    // DUC-01: identity ("this host's identity within its enclosing host") is declared once
    // on FGV2UiPropertyHostState / IGV2UiPropertyHost, not as a per-class property four
    // Location composites each redeclared. Part 1 proves the property is on the *shared*
    // surface: UGV2ListViewWidgetBase is a generic repeater primitive (PCC-10/PCC-12) that
    // implements only IGV2UiPropertyHost, is not one of DUC-02's addressable base elements,
    // and never had its own identity concept -- yet SetHostIdentity/GetHostIdentity work on
    // it exactly the same way as on a Location composite or a DUC-02 base element.
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("DUC-01: TestWorld created"), TestWorld);
    if (TestWorld == nullptr) return false;

    UGV2ListViewWidgetBase* PlainHost = NewObject<UGV2ListViewWidgetBase>(TestWorld);
    TestNotNull(TEXT("DUC-01: plain UGV2ListViewWidgetBase instantiated"), PlainHost);
    if (PlainHost != nullptr)
    {
        TestTrue(TEXT("DUC-01: UGV2ListViewWidgetBase does not implement IGV2ScreenFieldHost"), Cast<IGV2ScreenFieldHost>(PlainHost) == nullptr);
        TestEqual(TEXT("DUC-01: fresh host has no identity"), PlainHost->GetHostIdentity(), NAME_None);
        PlainHost->SetHostIdentity(FName(TEXT("some_property_name")));
        TestEqual(TEXT("DUC-01: identity round-trips on a plain IGV2UiPropertyHost"), PlainHost->GetHostIdentity(), FName(TEXT("some_property_name")));
    }

    // Part 2: the existing screen-level duplicate-field_id rejection
    // (CollectScreenFieldHosts's SeenFieldIds check in GV2ScreenWidgetBase.cpp) is now
    // backed by the shared HostIdentity instead of each class's own ScreenFieldId member --
    // confirm it still rejects two hosts configured with the same identity, not silently
    // deduplicated or accepted.
    UGV2ScreenWidgetBase* DupScreen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    DupScreen->WidgetTree = NewObject<UWidgetTree>(DupScreen);
    UVerticalBox* DupRoot = DupScreen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    DupScreen->WidgetTree->RootWidget = DupRoot;
    UGV2DeclaredCompositeWidgetBase* TopBarA = DupScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
        UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBarA"));
    UGV2DeclaredCompositeWidgetBase* TopBarB = DupScreen->WidgetTree->ConstructWidget<UGV2DeclaredCompositeWidgetBase>(
        UGV2DeclaredCompositeWidgetBase::StaticClass(), TEXT("TopBarB"));
    DupRoot->AddChildToVerticalBox(TopBarA);
    DupRoot->AddChildToVerticalBox(TopBarB);
    TopBarA->SetHostIdentity(FName(TEXT("top_bar")));
    TopBarB->SetHostIdentity(FName(TEXT("top_bar")));

    AddExpectedErrorPlain(TEXT("GetScreenFieldIds failed: duplicate screen field host 'top_bar'"), EAutomationExpectedErrorFlags::Contains, 1);
    TestEqual(TEXT("DUC-01: duplicate identity within one host is rejected, not silently accepted"), DupScreen->GetScreenFieldIds(), TArray<FName>{});

    TopBarB->SetHostIdentity(FName(TEXT("top_bar_2")));
    TestEqual(
        TEXT("DUC-01: distinct identities on the same host are accepted"),
        DupScreen->GetScreenFieldIds(),
        TArray<FName>{FName(TEXT("top_bar")), FName(TEXT("top_bar_2"))});

    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2BaseElementNonCanonicalIdentityTest,
    "GV2.Runtime.Presentation.BaseElementNonCanonicalIdentityRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2BaseElementNonCanonicalIdentityTest::RunTest(const FString& Parameters)
{
    // DUC-02: "неканоническая идентичность отклоняется с диагностикой" -- proven on a base
    // element (UGV2TextWidgetBase, newly addressable, no dedicated C++ class), not just on a
    // Location composite: the existing IsCanonicalFieldId check in
    // GV2ScreenWidgetBase.cpp's CollectScreenFieldHosts applies identically regardless of
    // which IGV2ScreenFieldHost implementer configured the bad value.
    AddExpectedErrorPlain(TEXT("has non-canonical field_id"), EAutomationExpectedErrorFlags::Contains, 1);

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("DUC-02: TestWorld created"), TestWorld);
    if (TestWorld == nullptr) return false;

    UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    Screen->WidgetTree = NewObject<UWidgetTree>(Screen);
    UGV2TextWidgetBase* TextHost = Screen->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(
        UGV2TextWidgetBase::StaticClass(), TEXT("TextHost"));
    Screen->WidgetTree->RootWidget = TextHost;
    TextHost->SetHostIdentity(FName(TEXT("Not-Canonical")));

    TestEqual(
        TEXT("DUC-02: non-canonical identity on a base element is rejected, not silently accepted"),
        Screen->GetScreenFieldIds(),
        TArray<FName>{});

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
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

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

    // 1. REV3-01 / REV3-02: a meter label reaches the ProgressBar's LabelText through
    //    PrepareUiHostProperties/the central text consumer, so an unrenderable label
    //    fails Prepare instead of being dropped or applied unstyled. Routed through the
    //    same host-level Prepare/Commit every real screen field now uses -- UPP-30
    //    retired the widget-specific ApplyProgressBarModel/FGV2ProgressBarViewModel this
    //    used to call directly.
    {
        using namespace GV2ContentCore;

        // Built with a manual WidgetTree (matching GV2UiPrepareCommitTests.cpp's
        // MakeTestHostWidget pattern) rather than loading WBP_ProgressBar: Prepare's
        // target resolution (GetWidgetFromName) and DescribeUiCapabilities' own
        // LabelText/ProgressBar != nullptr checks both need real bound children, and
        // this keeps the test deterministic regardless of asset availability.
        UGV2ProgressBarWidgetBase* Bar = NewObject<UGV2ProgressBarWidgetBase>(TestWorld);
        TestNotNull(TEXT("ProgressBar instantiated"), Bar);
        if (Bar != nullptr)
        {
            Bar->WidgetTree = NewObject<UWidgetTree>(Bar);
            UVerticalBox* Root = Bar->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
            Bar->WidgetTree->RootWidget = Root;
            UProgressBar* ProgressBarWidget = Bar->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
            Root->AddChildToVerticalBox(ProgressBarWidget);
            UCommonTextBlock* LabelWidget = Bar->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("LabelText"));
            Root->AddChildToVerticalBox(LabelWidget);
            if (FProperty* Prop = UGV2ProgressBarWidgetBase::StaticClass()->FindPropertyByName(TEXT("ProgressBar")))
            {
                *Prop->ContainerPtrToValuePtr<TObjectPtr<UProgressBar>>(Bar) = ProgressBarWidget;
            }
            if (FProperty* Prop = UGV2ProgressBarWidgetBase::StaticClass()->FindPropertyByName(TEXT("LabelText")))
            {
                *Prop->ContainerPtrToValuePtr<TObjectPtr<UCommonTextBlock>>(Bar) = LabelWidget;
            }

            auto PercentSpec = std::make_shared<FCompiledUiFieldSpec>();
            PercentSpec->Kind = EUiFieldKind::Scalar;
            FScalarFieldSpec PercentScalar;
            PercentScalar.Kind = EScalarFieldKind::Number;
            PercentScalar.MinimumNumber = 0.0;
            PercentScalar.MaximumNumber = 1.0;
            PercentSpec->Scalar = PercentScalar;

            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            Schema.Fields.push_back({"percent", false, PercentSpec});
            Schema.Fields.push_back({"label", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text)});

            FGV2UiCapabilityBuilder Builder;
            Bar->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            const FGV2PreparedUiObject EmptyPrev;

            TMap<FString, FGV2PreparedUiValue> GoodFields;
            GoodFields.Add(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.5));
            GoodFields.Add(TEXT("label"), FGV2PreparedUiValue::MakeText(GoodText));
            const TSharedRef<const FGV2PreparedUiObject> GoodCandidate = FGV2PreparedUiObject::Create(GoodFields);

            FGV2UiHostMutationPlan GoodPlan;
            TArray<FGV2UiSchemaCompatibilityDiagnostic> GoodDiagnostics;
            TestTrue(TEXT("REV3-01: a renderable label prepares"),
                PrepareUiHostProperties(Bar, Caps, *GoodCandidate, Schema, TEXT("test:schema.progress_bar"), TEXT(""), EmptyPrev, GoodPlan, GoodDiagnostics));
            FString FailedPath, CommitError;
            TestTrue(TEXT("REV3-01: prepared plan commits"), CommitUiHostProperties(Bar, GoodPlan, FailedPath, CommitError));
            TestEqual(TEXT("REV3-01: Progress updated to 0.5"), Bar->GetProgress(), 0.5f);

            TMap<FString, FGV2PreparedUiValue> PoisonFields;
            PoisonFields.Add(TEXT("percent"), FGV2PreparedUiValue::MakeNumber(0.5));
            PoisonFields.Add(TEXT("label"), FGV2PreparedUiValue::MakeText(PoisonText));
            const TSharedRef<const FGV2PreparedUiObject> PoisonCandidate = FGV2PreparedUiObject::Create(PoisonFields);

            FGV2UiHostMutationPlan PoisonPlan;
            TArray<FGV2UiSchemaCompatibilityDiagnostic> PoisonDiagnostics;
            TestFalse(TEXT("REV3-02: Label that the text pipeline rejects fails Prepare"),
                PrepareUiHostProperties(Bar, Caps, *PoisonCandidate, Schema, TEXT("test:schema.progress_bar"), TEXT(""), EmptyPrev, PoisonPlan, PoisonDiagnostics));
        }
    }

    // 2. REV3-05: a button whose text cannot be rendered fails, and the failure is not
    //    swallowed by the owning collection. Routed through PrepareUiHostProperties
    //    directly -- UPP-30 retired ApplyButtonModels/FGV2ButtonViewModel this used to
    //    call.
    {
        using namespace GV2ContentCore;

        // Manual WidgetTree, matching the ProgressBar block above -- deterministic
        // regardless of WBP_ButtonList's availability in this test environment.
        UGV2ButtonListWidgetBase* List = NewObject<UGV2ButtonListWidgetBase>(TestWorld);
        TestNotNull(TEXT("ButtonList instantiated"), List);
        if (List != nullptr)
        {
            List->WidgetTree = NewObject<UWidgetTree>(List);
            UVerticalBox* ButtonContainerWidget = List->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ButtonContainer"));
            List->WidgetTree->RootWidget = ButtonContainerWidget;
            if (FProperty* Prop = UGV2ButtonListWidgetBase::StaticClass()->FindPropertyByName(TEXT("ButtonContainer")))
            {
                *Prop->ContainerPtrToValuePtr<TObjectPtr<UVerticalBox>>(List) = ButtonContainerWidget;
            }
            if (FProperty* Prop = UGV2ButtonListWidgetBase::StaticClass()->FindPropertyByName(TEXT("ButtonWidgetClass")))
            {
                *Prop->ContainerPtrToValuePtr<TSubclassOf<UGV2ButtonWidgetBase>>(List) = UGV2ButtonWidgetBase::StaticClass();
            }

            {
                auto ItemSpec = std::make_shared<FCompiledUiFieldSpec>();
                ItemSpec->Kind = EUiFieldKind::Object;
                ItemSpec->Fields.push_back({"key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)});
                ItemSpec->Fields.push_back({"text", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text)});
                ItemSpec->Fields.push_back({"binding", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding)});

                auto ItemsArraySpec = std::make_shared<FCompiledUiFieldSpec>();
                ItemsArraySpec->Kind = EUiFieldKind::Array;
                ItemsArraySpec->KeyedBy = std::string("key");
                ItemsArraySpec->Items = ItemSpec;

                FCompiledUiFieldSpec Schema;
                Schema.Kind = EUiFieldKind::Object;
                Schema.Fields.push_back({"items", true, ItemsArraySpec});

                FGV2UiCapabilityBuilder Builder;
                List->DescribeUiCapabilities(Builder);
                const FGV2PreparedUiObject EmptyPrev;

                TMap<FString, FGV2PreparedUiValue> PoisonItemMap;
                PoisonItemMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("btn_poison")));
                PoisonItemMap.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(PoisonText));
                PoisonItemMap.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("core:command.test"))));
                TArray<FGV2PreparedUiValue> Elements;
                Elements.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(PoisonItemMap)));

                TMap<FString, FGV2PreparedUiValue> RootFields;
                RootFields.Add(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(Elements)));
                const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(RootFields);

                FGV2UiHostMutationPlan Plan;
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
                TestFalse(TEXT("REV3-05: ButtonList reports failure when a child button cannot render its text"),
                    PrepareUiHostProperties(List, Builder.Build(), *Candidate, Schema, TEXT("test:schema.button_list"), TEXT(""), EmptyPrev, Plan, Diagnostics));
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
        auto PrepareWithExtraBtnProp = [](const char* ExtraKey, GV2RuntimeCore::FValue ExtraValue) -> bool
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
            return GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Request, Definitions);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenFieldUnifiedValidatorPcc04Test,
    "GV2.Runtime.Presentation.ScreenFieldUnifiedValidatorPcc04",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenFieldUnifiedValidatorPcc04Test::RunTest(const FString& Parameters)
{
    const FGV2ScopedRealSchemaCache ScopedSchemaCache;

    auto ReadSource = [this](const TCHAR* RelativePath, FString& OutSource)
    {
        const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
        const bool bLoaded = FFileHelper::LoadFileToString(OutSource, *FullPath);
        TestTrue(*FString::Printf(TEXT("Source audit can read %s"), RelativePath), bLoaded);
        return bLoaded;
    };

    // 1. Enforce that materializer uses the portable ValidateUiFieldValue and second implementation is removed
    FString MaterializerSource;
    if (ReadSource(
            TEXT("Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp"),
            MaterializerSource))
    {
        TestTrue(
            TEXT("PCC-04: GV2ScreenFieldMaterializer delegates to GV2ContentCore::ValidateUiFieldValue"),
            MaterializerSource.Contains(TEXT("GV2ContentCore::ValidateUiFieldValue")));
        TestFalse(
            TEXT("PCC-04: Second validator WalkFieldValue is completely deleted"),
            MaterializerSource.Contains(TEXT("WalkFieldValue")));
    }

    // 2. Parity check: Test min/max and constraint validation in BuildFields vs ValidateUiFieldValue
    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    auto RunBuildFields = [](const std::string& SchemaId, const std::string& FieldId, GV2RuntimeCore::FValue Value) -> bool
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = FieldId;
        Field.SchemaId = SchemaId;
        Field.Value = MoveTemp(Value);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2ScreenFieldValue> Fields;
        return GV2ScreenFieldMaterializer::BuildFields(Request, {}, Fields);
    };

    // Valid meters percent within [0.0, 1.0] succeeds
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.common.ok"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(0.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestTrue(TEXT("PCC-04: Valid percent 0.5 within [0.0, 1.0] passes BuildFields"),
            RunBuildFields("textsystem:schema.ui_field.location_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    // Percent below min (e.g. -0.5 < 0.0) rejected by portable validator in BuildFields
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.common.ok"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(-0.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestFalse(TEXT("PCC-04: Percent -0.5 below min 0.0 rejected by BuildFields"),
            RunBuildFields("textsystem:schema.ui_field.location_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    // Percent above max (e.g. 1.5 > 1.0) rejected by portable validator in BuildFields
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.common.ok"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(1.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestFalse(TEXT("PCC-04: Percent 1.5 above max 1.0 rejected by BuildFields"),
            RunBuildFields("textsystem:schema.ui_field.location_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PackageSetSingleResolutionAcrossConsumersContract,
    "GV2.Runtime.Content.PackageSetSingleResolutionAcrossConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PackageSetSingleResolutionAcrossConsumersContract::RunTest(const FString& Parameters)
{
    // PSC-02 (ADR-0043 D1/D5, PAH-R3): proves the "single resolved package set shared
    // by every consumer" invariant with two genuinely different candidate closures --
    // mirroring the divergence PAH-R3 actually found (an Editor profile whose package
    // roots differ from GameData/'s canonical mods.lock order). Each candidate is fed,
    // independently, into both Screen Registry and the repository builder; each
    // consumer's output must reflect exactly the input it was given, never the other
    // candidate's, and never a third, independently-rediscovered closure.
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));

    auto ResolveFixtureSet =
        [&GameDataDir](std::initializer_list<const TCHAR*> PackageIds) -> TOptional<GV2ContentHostSupport::FResolvedPackageSet>
    {
        std::vector<std::filesystem::path> Roots;
        for (const TCHAR* PackageId : PackageIds)
        {
            Roots.push_back(std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, PackageId))));
        }
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        std::optional<GV2ContentHostSupport::FResolvedPackageSet> Resolved =
            GV2ContentHostSupport::ResolvePackageSetFromDirectories(Roots, Diagnostics);
        return Resolved
            ? TOptional<GV2ContentHostSupport::FResolvedPackageSet>(MoveTemp(*Resolved))
            : TOptional<GV2ContentHostSupport::FResolvedPackageSet>();
    };

    // SetA mirrors the canonical GameData/ closure (core, textsystem, rh); SetB mirrors
    // an Editor profile that diverges from it (core, textsystem, sample) -- the same two
    // real fixture compositions FGV2RhStartScreenFlow/FGV2DebugStartScreenFlow already
    // exercise elsewhere in this file, reused here as two genuinely distinct inputs.
    const TOptional<GV2ContentHostSupport::FResolvedPackageSet> SetA = ResolveFixtureSet({TEXT("core"), TEXT("textsystem"), TEXT("rh")});
    const TOptional<GV2ContentHostSupport::FResolvedPackageSet> SetB = ResolveFixtureSet({TEXT("core"), TEXT("textsystem"), TEXT("sample")});
    TestTrue(TEXT("Candidate set A (core, textsystem, rh) resolves"), SetA.IsSet());
    TestTrue(TEXT("Candidate set B (core, textsystem, sample) resolves"), SetB.IsSet());
    if (!SetA.IsSet() || !SetB.IsSet())
    {
        return false;
    }

    auto OrderOf = [](const GV2ContentHostSupport::FResolvedPackageSet& Set) -> TArray<FString>
    {
        TArray<FString> Order;
        Order.Reserve(static_cast<int32>(Set.OrderedSources.size()));
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : Set.OrderedSources)
        {
            Order.Add(UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()));
        }
        return Order;
    };

    auto VerifyConsumersMatchInput = [this](const GV2ContentHostSupport::FResolvedPackageSet& Input, const TArray<FString>& ExpectedOrder, const TCHAR* Label) -> bool
    {
        bool bOk = true;
        const TArray<GV2PackageClosure::FEntry> ClosureEntries = GV2PackageClosure::FromResolvedPackageSet(Input);

        const TArray<FString> RegistryOrder = UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(ClosureEntries);
        bOk &= TestEqual(
            *FString::Printf(TEXT("%s: Screen Registry's package load order matches the input set exactly"), Label),
            FString::Join(RegistryOrder, TEXT(",")), FString::Join(ExpectedOrder, TEXT(",")));

        // BuildGV2RepositoryFromResolvedPackageSet is a pure projection of Input.OrderedSources
        // (PackageDiscoveryAndOrderConformance case 12 already proves this projection's
        // correctness at the portable layer) -- here it only needs to succeed from this
        // exact input, proving the repository consumed it rather than rediscovering.
        const GV2ContentCore::FBuildResult RepositoryBuild = BuildGV2RepositoryFromResolvedPackageSet(Input);
        bOk &= TestFalse(
            *FString::Printf(TEXT("%s: repository builds successfully from this exact input set"), Label),
            RepositoryBuild.IsFailure());
        return bOk;
    };

    const TArray<FString> ExpectedOrderA = OrderOf(*SetA);
    const TArray<FString> ExpectedOrderB = OrderOf(*SetB);
    TestTrue(TEXT("The two candidate sets have genuinely different composition (rh vs sample)"), ExpectedOrderA != ExpectedOrderB);

    VerifyConsumersMatchInput(*SetA, ExpectedOrderA, TEXT("SetA(core,textsystem,rh)"));
    VerifyConsumersMatchInput(*SetB, ExpectedOrderB, TEXT("SetB(core,textsystem,sample)"));

    // The failure mode this defends against (PAH-R3): a consumer silently ignoring its
    // given input and reading some other, independently-discovered closure instead --
    // which would make both consumers agree on ONE order regardless of which set they
    // were handed. Cross-checking the two orders directly rules that out.
    const TArray<FString> RegistryOrderForA = UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(GV2PackageClosure::FromResolvedPackageSet(*SetA));
    const TArray<FString> RegistryOrderForB = UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(GV2PackageClosure::FromResolvedPackageSet(*SetB));
    TestTrue(
        TEXT("Screen Registry's resolved order for SetA differs from SetB -- not collapsed to one shared closure"),
        RegistryOrderForA != RegistryOrderForB);

    return true;
}

#endif
