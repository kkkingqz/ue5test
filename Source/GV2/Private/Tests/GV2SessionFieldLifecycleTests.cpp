#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Bridge/GV2BridgeTypes.h"
#include "Application/GV2PackageClosure.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "Application/GV2SessionCoordinator.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2TextPipelineHost.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiTheme.h"
#include "GV2ContentCore/UiSchema.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"

#include "Tests/GV2PresentationTestFixtures.h"

namespace
{
using GV2PresentationTestFixtures::LoadConfiguredThemeForTest;
using GV2PresentationTestFixtures::MakeResolvedLiteralTextForTest;
using GV2PresentationTestFixtures::MakePreparedResolvedImageForTest;
using GV2PresentationTestFixtures::FGV2ScopedSamplePackageOverride;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenFieldClosedSchemaRejectionTest,
    "GV2.Runtime.Presentation.ScreenFieldClosedSchemaRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenFieldClosedSchemaRejectionTest::RunTest(const FString& Parameters)
{
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
        Request.ScreenId = "core:screen.test.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

        FObject CmdValue;
        CmdValue["items"] = GV2RuntimeCore::FValue(FArray{});
        CmdValue["unknown_field"] = GV2RuntimeCore::FValue(std::string("invalid"));

        Field.Value = GV2RuntimeCore::FValue(CmdValue);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2UiBindingDefinition> Definitions;
        TestFalse(TEXT("BAI-03: Field value level unknown key rejected on commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions));
    }

    // 2. Rejection at collection element level: unknown key on button item
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "core:screen.test.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

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
        TestFalse(TEXT("BAI-03: Collection element level unknown key rejected on button"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions));
    }

    // 3. Rejection at nested Binding level: unknown property in binding object
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "core:screen.test.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

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
        TestFalse(TEXT("BAI-03: Nested Binding level unknown key rejected"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions));
    }

    // 4. Rejection of duplicate button keys in collection
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "core:screen.test.location";

        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = "commands";
        Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

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
        TestFalse(TEXT("BAI-03: Duplicate button key rejected in commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LocationKeyBoundaryTest,
    "GV2.Runtime.Presentation.LocationKeyBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LocationKeyBoundaryTest::RunTest(const FString& Parameters)
{
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
            Request.ScreenId = "core:screen.test.location";

            GV2RuntimeCore::FScreenField Field;
            Field.FieldId = "commands";
            Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

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
            return GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions);
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

    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    AddExpectedErrorPlain(TEXT("rejected (closed schema)"), EAutomationExpectedErrorFlags::Contains, 3);

    // A text model the central pipeline must reject: authoring markup may never reach
    // a plain renderer. Used throughout as the failure injector.
    const UGV2UiTheme* Theme = PrepareContext->GetTheme().Theme.Get();
    FGV2TextViewModel PoisonText = MakeResolvedLiteralTextForTest(*Theme, TEXT("Poison"));
    PoisonText.NormalizedMarkup = TEXT("<gv2:action id=\"x\">y</>");

    const FGV2TextViewModel GoodText = MakeResolvedLiteralTextForTest(*Theme, TEXT("Fine"));

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
                PrepareUiHostProperties(Bar, Caps, *GoodCandidate, Schema, TEXT("test:schema.progress_bar"), TEXT(""), EmptyPrev, GoodPlan, GoodDiagnostics, nullptr, PrepareContext));
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
                PrepareUiHostProperties(Bar, Caps, *PoisonCandidate, Schema, TEXT("test:schema.progress_bar"), TEXT(""), EmptyPrev, PoisonPlan, PoisonDiagnostics, nullptr, PrepareContext));
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
                    PrepareUiHostProperties(List, Builder.Build(), *Candidate, Schema, TEXT("test:schema.button_list"), TEXT(""), EmptyPrev, Plan, Diagnostics, nullptr, PrepareContext));
            }
        }
    }

    // 3. REV3-10: a portrait resource with no bound renderer is a failure, not a success.
    {
        UGV2PortraitWidgetBase* Portrait = NewObject<UGV2PortraitWidgetBase>(TestWorld);
        TestNotNull(TEXT("Portrait instantiated"), Portrait);
        if (Portrait != nullptr)
        {
            // PSC-10C: ApplyPortrait (resolve + mutate) is gone; the surviving production
            // method takes an already resolved resource. The property under test -- an
            // unbound renderer rejects rather than silently dropping content -- is unchanged.
            FString Error;
            FGV2ResolvedImageResource ResolvedPortrait;
            ResolvedPortrait.ResourceId = TEXT("core:resource.ui.missing_portrait");
            TestFalse(TEXT("REV3-10: Portrait with unbound renderer rejects a supplied resource"),
                Portrait->ApplyResolvedPortrait(MakePreparedResolvedImageForTest(ResolvedPortrait), Error));
            TestTrue(TEXT("REV3-10: Rejection names the unbound renderer"), Error.Contains(TEXT("PortraitImage")));
        }
    }

    // 3b. REV3-09: RichText with hover spans fails validation when RichTextPopoverClass is unavailable
    {
        UGV2UiTheme* MutableTheme = const_cast<UGV2UiTheme*>(Theme);
        if (MutableTheme != nullptr)
        {
            // PSC-10B: the renderer class is resolved once at snapshot build, so the session
            // under test must be built WITHOUT one -- clearing it on the Theme afterwards no
            // longer reaches the value Prepare checks. The guard restores the shared asset on
            // every exit path, not only the successful one.
            GV2PresentationTestFixtures::FPrepareContextFixture::FWithoutRichTextPopoverRenderer
                NoRenderer(MutableTheme);
            GV2PresentationTestFixtures::FPrepareContextFixture RendererlessFixture;
            FString RendererlessError;
            const bool bRendererlessReady = RendererlessFixture.Initialize(RendererlessError);
            TestTrue(
                *FString::Printf(TEXT("REV3-09: rendererless session snapshot builds: %s"), *RendererlessError),
                bRendererlessReady);
            const FGV2PresentationPrepareContext* RendererlessContext = RendererlessFixture.Get();

            TMap<FString, FGV2PreparedUiValue> HoverMap;
            const FGV2TextViewModel TitleModel =
                MakeResolvedLiteralTextForTest(*Theme, TEXT("Definition"));
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
            Consumer.SetPrepareContext(RendererlessContext);
            TestFalse(TEXT("REV3-09: RichText with hover spans rejects application when popover class is unavailable"),
                Consumer.Prepare(SpansVal, Cap, RichTextWidget, PrepError));
            TestTrue(
                *FString::Printf(TEXT("REV3-09: rejection identifies the unavailable popover class [Error: %s]"), *PrepError),
                PrepError.Contains(TEXT("popover"), ESearchCase::IgnoreCase));
        }
    }

    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);

    // 4. REV3-03: button element extra properties are rejected by closed schema parser
    {
        auto PrepareWithExtraBtnProp = [PrepareContext](const char* ExtraKey, GV2RuntimeCore::FValue ExtraValue) -> bool
        {
            GV2RuntimeCore::FScreenRequest Request;
            Request.ScreenId = "core:screen.test.location";

            GV2RuntimeCore::FScreenField Field;
            Field.FieldId = "commands";
            Field.SchemaId = "core:schema.ui_field.synthetic_commands.v1";

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
            return GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, Request, Definitions);
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
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("PCC-04: presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }
    if (UGV2UiTheme* Theme = PrepareContext->GetTheme().Theme.Get())
    {
        Theme->TextCatalog.FindOrAdd(
            TEXT("core:text.character.test_hero.name"),
            FText::FromString(TEXT("Player")));
    }

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

    auto RunBuildFields = [PrepareContext](const std::string& SchemaId, const std::string& FieldId, GV2RuntimeCore::FValue Value) -> bool
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = "core:screen.test.location";
        GV2RuntimeCore::FScreenField Field;
        Field.FieldId = FieldId;
        Field.SchemaId = SchemaId;
        Field.Value = MoveTemp(Value);
        Request.Fields.push_back(MoveTemp(Field));

        TArray<FGV2ScreenFieldValue> Fields;
        return GV2ScreenFieldMaterializer::BuildFields(*PrepareContext, Request, {}, Fields);
    };

    // Valid meters percent within [0.0, 1.0] succeeds
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.character.test_hero.name"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(0.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestTrue(TEXT("PCC-04: Valid percent 0.5 within [0.0, 1.0] passes BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    // Percent below min (e.g. -0.5 < 0.0) rejected by portable validator in BuildFields
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.character.test_hero.name"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(-0.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestFalse(TEXT("PCC-04: Percent -0.5 below min 0.0 rejected by BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    // Percent above max (e.g. 1.5 > 1.0) rejected by portable validator in BuildFields
    {
        FObject StatusObj;
        FObject NameObj;
        NameObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.character.test_hero.name"));
        StatusObj["name"] = GV2RuntimeCore::FValue(NameObj);
        
        FObject MeterObj;
        MeterObj["key"] = GV2RuntimeCore::FValue(std::string("hp"));
        MeterObj["percent"] = GV2RuntimeCore::FValue(1.5);
        StatusObj["meters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(MeterObj)});
        StatusObj["items"] = GV2RuntimeCore::FValue(FArray{});
        StatusObj["effects"] = GV2RuntimeCore::FValue(FArray{});

        TestFalse(TEXT("PCC-04: Percent 1.5 above max 1.0 rejected by BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_player_status.v1", "player_status", GV2RuntimeCore::FValue(StatusObj)));
    }

    // 3. CFC-11: Location scene v2 schema requires characters array; empty object rejected, valid empty array accepted, populated scene accepted
    {
        // 3a. Empty object {} lacks required 'characters' -> rejected by BuildFields
        FObject EmptySceneObj;
        TestFalse(TEXT("CFC-11: Empty scene object {} missing required 'characters' rejected by BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_scene.v1", "scene", GV2RuntimeCore::FValue(EmptySceneObj)));

        // 3b. Scene with valid empty characters [] -> passes BuildFields
        FObject ValidEmptySceneObj;
        ValidEmptySceneObj["characters"] = GV2RuntimeCore::FValue(FArray{});
        TestTrue(TEXT("CFC-11: Scene with valid empty characters array passes BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_scene.v1", "scene", GV2RuntimeCore::FValue(ValidEmptySceneObj)));

        // 3c. Populated scene with character -> passes BuildFields
        FObject PopulatedSceneObj;
        FObject CharObj;
        CharObj["key"] = GV2RuntimeCore::FValue(std::string("guide"));
        CharObj["resource_id"] = GV2RuntimeCore::FValue(std::string("core:resource.character.test_guide"));
        PopulatedSceneObj["characters"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(CharObj)});
        PopulatedSceneObj["background_resource_id"] = GV2RuntimeCore::FValue(std::string("core:resource.location.test_alpha_bg"));
        TestTrue(TEXT("CFC-11: Populated scene with characters passes BuildFields"),
            RunBuildFields("core:schema.ui_field.synthetic_scene.v1", "scene", GV2RuntimeCore::FValue(PopulatedSceneObj)));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2WidgetBlueprintApplyMigrationInventoryTest,
    "GV2.Runtime.Presentation.WidgetBlueprintApplyMigrationInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2WidgetBlueprintApplyMigrationInventoryTest::RunTest(const FString& Parameters)
{
    // Actual moved-class set: every native UUserWidget class physically owned by the lower
    // module. No migrated class name or WBP path is listed here.
    TSet<UClass*> ApplyWidgetClasses;
    TSet<FString> RetiredClassPaths;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (Class != nullptr
            && Class->IsChildOf(UUserWidget::StaticClass())
            && Class->GetOutermost()->GetName() == TEXT("/Script/GV2PresentationApply"))
        {
            ApplyWidgetClasses.Add(Class);
            RetiredClassPaths.Add(FString::Printf(TEXT("/Script/GV2.%s"), *Class->GetName()));
        }
    }
    TestTrue(TEXT("Moved-widget class enumerator produced a non-empty actual set"), !ApplyWidgetClasses.IsEmpty());

    auto ContainsRetiredClassPath = [&RetiredClassPaths](const FString& Value) -> bool
    {
        for (const FString& RetiredPath : RetiredClassPaths)
        {
            if (Value.Contains(RetiredPath, ESearchCase::CaseSensitive))
            {
                return true;
            }
        }
        return false;
    };

    if (!RetiredClassPaths.IsEmpty())
    {
        const FString FirstRetiredPath = *RetiredClassPaths.CreateConstIterator();
        TestTrue(
            TEXT("Synthetic negative: old native-parent metadata is rejected"),
            ContainsRetiredClassPath(FString::Printf(TEXT("Class'%s'"), *FirstRetiredPath)));
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game"));
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 WidgetBlueprintCount = 0;
    int32 AffectedClosureCount = 0;
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.AssetClassPath.GetAssetName() != TEXT("WidgetBlueprint"))
        {
            continue;
        }
        ++WidgetBlueprintCount;

        // Asset Registry metadata is checked before loading, so a stale native-parent or
        // inherited-parent reference cannot be hidden by an already loaded generated class.
        for (const FName Tag : {
                 FBlueprintTags::NativeParentClassPath,
                 FBlueprintTags::ParentClassPath,
                 FBlueprintTags::ImplementedInterfaces})
        {
            FString Value;
            if (Asset.GetTagValue(Tag, Value))
            {
                TestFalse(
                    *FString::Printf(TEXT("%s tag '%s' contains no retired /Script/GV2 class path"),
                        *Asset.PackageName.ToString(), *Tag.ToString()),
                    ContainsRetiredClassPath(Value));
            }
        }

        UObject* BlueprintAsset = Asset.GetAsset();
        TestNotNull(
            *FString::Printf(TEXT("Widget Blueprint asset loads after class migration: %s"), *Asset.PackageName.ToString()),
            BlueprintAsset);
        const FString GeneratedClassPath = FString::Printf(
            TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString());
        UClass* GeneratedClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        TestNotNull(
            *FString::Printf(TEXT("Widget Blueprint generated class loads after class migration: %s"), *GeneratedClassPath),
            GeneratedClass);
        if (GeneratedClass == nullptr)
        {
            continue;
        }

        bool bReferencesMovedClass = false;
        for (UClass* SuperClass = GeneratedClass->GetSuperClass(); SuperClass != nullptr; SuperClass = SuperClass->GetSuperClass())
        {
            if (ApplyWidgetClasses.Contains(SuperClass))
            {
                bReferencesMovedClass = true;
                TestEqual(
                    *FString::Printf(TEXT("%s resolves moved native ancestry to the lower module"), *GeneratedClassPath),
                    SuperClass->GetOutermost()->GetName(),
                    FString(TEXT("/Script/GV2PresentationApply")));
            }
        }
        AffectedClosureCount += bReferencesMovedClass ? 1 : 0;
    }

    TestTrue(TEXT("Asset Registry enumerated Widget Blueprints"), WidgetBlueprintCount > 0);
    TestTrue(TEXT("Asset Registry inheritance closure reaches migrated widget bases"), AffectedClosureCount > 0);
    for (const FString& RetiredPath : RetiredClassPaths)
    {
        TestNull(
            *FString::Printf(TEXT("Retired class object is absent after clean reload: %s"), *RetiredPath),
            FindObject<UClass>(nullptr, *RetiredPath));
    }
    return true;
}

// CFC-06 (ADR-0044 D1/D2, PSC-AF-05, STATUS-015): Candidate build failure before BeginReplace
// preserves session A's UI, viewport attachment, bindings, and status on UGV2RuntimeSubsystem.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionPreservesProjectionWhenCandidateFailsTest,
    "GV2.Runtime.Session.PreservesProjectionWhenCandidateFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionPreservesProjectionWhenCandidateFailsTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // 1. Start initial session A
    FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
    const FGV2SessionStatus StatusA = Runtime->GetSessionState();
    TestTrue(TEXT("Initial session A is ready"), StatusA.bIsReady);
    TestEqual(TEXT("Initial session A is in Ready state"), StatusA.SessionState, EGV2SessionState::Ready);

    UGV2GameShellWidgetBase* ShellA = Runtime->GetActiveGameShell();
    UUserWidget* ScreenA = Runtime->GetActiveScreen();
    TestNotNull(TEXT("Session A created an active GameShell"), ShellA);
    TestNotNull(TEXT("Session A created an active Screen"), ScreenA);

    // 2. Inject candidate B failure before BeginReplace (undecodable resource)
    const FString BadResourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/core/resource/cfc06_test"));
    const FString BadResourcePath = FPaths::Combine(BadResourceDir, TEXT("candidate_fail_test.png"));
    IFileManager::Get().MakeDirectory(*BadResourceDir, true);
    const TArray<uint8> GarbageBytes = {0x00, 0x01, 0x02, 0x03};
    TestTrue(
        TEXT("Undecodable PNG fixture is written to trigger candidate build failure"),
        FFileHelper::SaveArrayToFile(GarbageBytes, *BadResourcePath));

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=ImageCatalogNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    // 3. Attempt replacement session B -> must fail candidate build and preserve session A
    Runtime->StartSession();

    // 4. Assert session A is completely preserved
    const FGV2SessionStatus StatusAfterFail = Runtime->GetSessionState();
    TestTrue(TEXT("Session A remains ready after candidate B failure"), StatusAfterFail.bIsReady);
    TestEqual(TEXT("Session A remains in Ready state"), StatusAfterFail.SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Active GameShell is still the exact session A instance"), Runtime->GetActiveGameShell(), ShellA);
    TestEqual(TEXT("Active Screen is still the exact session A instance"), Runtime->GetActiveScreen(), ScreenA);
    TestNull(
        TEXT("No recovery screen was shown since session A was preserved"),
        Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen()));

    // Cleanup bad file
    IFileManager::Get().Delete(*BadResourcePath);
    IFileManager::Get().DeleteDirectory(*BadResourceDir, false, true);

    // End session and clean up
    Runtime->EndSession();
    return true;
}

// CFC-06 (ADR-0044 D1/D2): Initial document apply failure after BeginReplace transitions
// coordinator to Failed and displays native recovery widget without false Ready state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionNativeRecoveryOnInitialApplyFailureTest,
    "GV2.Runtime.Session.NativeRecoveryOnInitialApplyFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionNativeRecoveryOnInitialApplyFailureTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // 1. Force document sink failure after BeginReplace
    UGV2RuntimeSubsystem::bTestForceDocumentSinkFailure = true;

    AddExpectedErrorPlain(
        TEXT("UI Document reconciliation failed (forced by automation test)"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedErrorPlain(
        TEXT("GV2 Lua runtime fault: code=InitialPresentationApplyFailed"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedErrorPlain(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedErrorPlain(
        TEXT("Showing UE-native recovery surface: session bootstrap failed"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    // 2. Start session with forced apply failure
    FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);

    // 3. Assert failure semantics: not ready, state Failed, recovery screen active
    const FGV2SessionStatus FailedStatus = Runtime->GetSessionState();
    TestFalse(TEXT("Session is not ready after initial apply failure"), FailedStatus.bIsReady);
    TestEqual(TEXT("Session state is Failed"), FailedStatus.SessionState, EGV2SessionState::Failed);
    TestNull(TEXT("Active GameShell is null after apply failure"), Runtime->GetActiveGameShell());

    UGV2RecoveryScreenWidget* RecoveryScreen = Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen());
    TestNotNull(TEXT("Native recovery widget is shown as active screen"), RecoveryScreen);

    // Reset test flag
    UGV2RuntimeSubsystem::bTestForceDocumentSinkFailure = false;

    // Clean up
    Runtime->EndSession();
    return true;
}

// SAC-01 (ADR-0044, CFC-AF-19): Ready session A survives a subsystem RequestSession call with an
// invalid descriptor: generation, snapshot identity, active screen, working binding, and bIsReady
// are preserved, and a non-zero operation id with Failed outcome is returned.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionPreservesReadySessionOnInvalidDescriptorRequestTest,
    "GV2.Runtime.Session.PreservesReadySessionOnInvalidDescriptorRequest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionPreservesReadySessionOnInvalidDescriptorRequestTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // 1. Start initial session A
    FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
    const FGV2SessionStatus StatusA = Runtime->GetSessionState();
    TestTrue(TEXT("Initial session A is ready"), StatusA.bIsReady);
    TestEqual(TEXT("Initial session A is in Ready state"), StatusA.SessionState, EGV2SessionState::Ready);
    const int32 GenA = StatusA.SessionGeneration;

    const FGV2SessionContentSnapshot* SnapshotA = Runtime->GetContentSnapshotForAutomationTest();
    TestNotNull(TEXT("Session A snapshot exists"), SnapshotA);
    const FString SnapshotHashA = SnapshotA ? SnapshotA->GetPresentationHash() : TEXT("");
    TestFalse(TEXT("Session A snapshot hash is non-empty"), SnapshotHashA.IsEmpty());

    UGV2GameShellWidgetBase* ShellA = Runtime->GetActiveGameShell();
    UUserWidget* ScreenA = Runtime->GetActiveScreen();
    TestNotNull(TEXT("Session A created an active GameShell"), ShellA);
    TestNotNull(TEXT("Session A created an active Screen"), ScreenA);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    TestTrue(TEXT("Lua VM is started for session A"), Coordinator && Coordinator->IsLuaVmStarted());

    // Publish a test binding on session A to verify working binding preservation
    TArray<FGV2UiBindingDefinition> Definitions;
    FGV2UiBindingDefinition Def;
    Def.CommandId = TEXT("core:command.test.step");
    Def.NodeKeyPath = { TEXT("main"), TEXT("test_action") };
    Definitions.Add(Def);
    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(TEXT("PublishScreenBindings succeeds"), Coordinator && Coordinator->PublishScreenBindings(Definitions, Handles));
    TestEqual(TEXT("1 handle published"), Handles.Num(), 1);
    const FGV2UiBindingHandle WorkingHandle = Handles.Num() > 0 ? Handles[0] : FGV2UiBindingHandle();
    TestTrue(TEXT("Working handle is valid"), WorkingHandle.IsValid());
    const int32 BindingsCountBefore = Coordinator ? Coordinator->GetBindingRegistry().Num() : 0;
    TestTrue(TEXT("Session A has published bindings"), BindingsCountBefore > 0);
    TestEqual(
        TEXT("Working binding before request is Accepted"),
        Runtime->SubmitUiInteraction(WorkingHandle, {}),
        EGV2SubmitUiInteractionResult::Accepted);

    // 2. Call subsystem RequestSession with invalid descriptor
    FSessionStartDescriptor BadDesc;
    BadDesc.Mode = ESessionStartMode::NewGame;
    BadDesc.SeedHex = TEXT("not_a_valid_hex");

    AddExpectedErrorPlain(
        TEXT("GV2 Lua runtime fault: code=InvalidSessionDescriptor"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedErrorPlain(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    const int64 OpId = Runtime->RequestSession(BadDesc);

    // 3. Assert non-zero operation id and Failed outcome
    TestTrue(TEXT("RequestSession returns a non-zero operation id"), OpId > 0);
    ESessionOperationOutcome Outcome;
    TestTrue(TEXT("Outcome is recorded for OpId"), Runtime->GetSessionOperationOutcome(OpId, Outcome));
    TestEqual(TEXT("Operation outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);

    // 4. Assert session A is completely preserved
    const FGV2SessionStatus StatusAfter = Runtime->GetSessionState();
    TestTrue(TEXT("Session A remains ready after invalid descriptor request"), StatusAfter.bIsReady);
    TestEqual(TEXT("Session A remains in Ready state"), StatusAfter.SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Session A generation is unchanged"), StatusAfter.SessionGeneration, GenA);

    const FGV2SessionContentSnapshot* SnapshotAfter = Runtime->GetContentSnapshotForAutomationTest();
    TestNotNull(TEXT("Session A snapshot still exists"), SnapshotAfter);
    if (SnapshotAfter != nullptr)
    {
        TestEqual(TEXT("Snapshot identity is preserved"), SnapshotAfter->GetPresentationHash(), SnapshotHashA);
    }

    TestEqual(TEXT("Active GameShell is still the exact session A instance"), Runtime->GetActiveGameShell(), ShellA);
    TestEqual(TEXT("Active Screen is still the exact session A instance"), Runtime->GetActiveScreen(), ScreenA);
    TestNull(
        TEXT("No recovery screen was shown since session A was preserved"),
        Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen()));

    const int32 BindingsCountAfter = Coordinator ? Coordinator->GetBindingRegistry().Num() : 0;
    TestEqual(TEXT("Binding registry count is unchanged"), BindingsCountAfter, BindingsCountBefore);
    TestEqual(
        TEXT("Working binding after request remains Accepted"),
        Runtime->SubmitUiInteraction(WorkingHandle, {}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestTrue(TEXT("Lua VM remains started"), Coordinator && Coordinator->IsLuaVmStarted());

    // Clean up
    Runtime->EndSession();
    return true;
}

// SAC-01 (ADR-0044, CFC-AF-19): Ready session A survives a subsystem RequestSession call when the
// repository is not ready / unpublishable: generation, snapshot identity, active screen, working
// binding, and bIsReady are preserved, and a non-zero operation id with Failed outcome is returned.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionPreservesReadySessionOnUnreadyRepositoryRequestTest,
    "GV2.Runtime.Session.PreservesReadySessionOnUnreadyRepositoryRequest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionPreservesReadySessionOnUnreadyRepositoryRequestTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // 1. Start initial session A
    FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
    const FGV2SessionStatus StatusA = Runtime->GetSessionState();
    TestTrue(TEXT("Initial session A is ready"), StatusA.bIsReady);
    TestEqual(TEXT("Initial session A is in Ready state"), StatusA.SessionState, EGV2SessionState::Ready);
    const int32 GenA = StatusA.SessionGeneration;

    const FGV2SessionContentSnapshot* SnapshotA = Runtime->GetContentSnapshotForAutomationTest();
    TestNotNull(TEXT("Session A snapshot exists"), SnapshotA);
    const FString SnapshotHashA = SnapshotA ? SnapshotA->GetPresentationHash() : TEXT("");
    TestFalse(TEXT("Session A snapshot hash is non-empty"), SnapshotHashA.IsEmpty());

    UGV2GameShellWidgetBase* ShellA = Runtime->GetActiveGameShell();
    UUserWidget* ScreenA = Runtime->GetActiveScreen();
    TestNotNull(TEXT("Session A created an active GameShell"), ShellA);
    TestNotNull(TEXT("Session A created an active Screen"), ScreenA);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    TestTrue(TEXT("Lua VM is started for session A"), Coordinator && Coordinator->IsLuaVmStarted());

    // Publish a test binding on session A to verify working binding preservation
    TArray<FGV2UiBindingDefinition> Definitions;
    FGV2UiBindingDefinition Def;
    Def.CommandId = TEXT("core:command.test.step");
    Def.NodeKeyPath = { TEXT("main"), TEXT("test_action") };
    Definitions.Add(Def);
    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(TEXT("PublishScreenBindings succeeds"), Coordinator && Coordinator->PublishScreenBindings(Definitions, Handles));
    TestEqual(TEXT("1 handle published"), Handles.Num(), 1);
    const FGV2UiBindingHandle WorkingHandle = Handles.Num() > 0 ? Handles[0] : FGV2UiBindingHandle();
    TestTrue(TEXT("Working handle is valid"), WorkingHandle.IsValid());
    const int32 BindingsCountBefore = Coordinator ? Coordinator->GetBindingRegistry().Num() : 0;
    TestTrue(TEXT("Session A has published bindings"), BindingsCountBefore > 0);
    TestEqual(
        TEXT("Working binding before request is Accepted"),
        Runtime->SubmitUiInteraction(WorkingHandle, {}),
        EGV2SubmitUiInteractionResult::Accepted);

    // 2. Force repository not ready and call RequestSession with valid descriptor
    UGV2RuntimeSubsystem::bTestForceRepositoryNotReady = true;

    FSessionStartDescriptor NextDesc;
    NextDesc.Mode = ESessionStartMode::NewGame;
    NextDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    AddExpectedErrorPlain(
        TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedErrorPlain(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    const int64 OpId = Runtime->RequestSession(NextDesc);

    UGV2RuntimeSubsystem::bTestForceRepositoryNotReady = false;

    // 3. Assert non-zero operation id and Failed outcome
    TestTrue(TEXT("RequestSession returns a non-zero operation id"), OpId > 0);
    ESessionOperationOutcome Outcome;
    TestTrue(TEXT("Outcome is recorded for OpId"), Runtime->GetSessionOperationOutcome(OpId, Outcome));
    TestEqual(TEXT("Operation outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);

    // 4. Assert session A is completely preserved
    const FGV2SessionStatus StatusAfter = Runtime->GetSessionState();
    TestTrue(TEXT("Session A remains ready after unready repository request"), StatusAfter.bIsReady);
    TestEqual(TEXT("Session A remains in Ready state"), StatusAfter.SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Session A generation is unchanged"), StatusAfter.SessionGeneration, GenA);

    const FGV2SessionContentSnapshot* SnapshotAfter = Runtime->GetContentSnapshotForAutomationTest();
    TestNotNull(TEXT("Session A snapshot still exists"), SnapshotAfter);
    if (SnapshotAfter != nullptr)
    {
        TestEqual(TEXT("Snapshot identity is preserved"), SnapshotAfter->GetPresentationHash(), SnapshotHashA);
    }

    TestEqual(TEXT("Active GameShell is still the exact session A instance"), Runtime->GetActiveGameShell(), ShellA);
    TestEqual(TEXT("Active Screen is still the exact session A instance"), Runtime->GetActiveScreen(), ScreenA);
    TestNull(
        TEXT("No recovery screen was shown since session A was preserved"),
        Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen()));

    const int32 BindingsCountAfter = Coordinator ? Coordinator->GetBindingRegistry().Num() : 0;
    TestEqual(TEXT("Binding registry count is unchanged"), BindingsCountAfter, BindingsCountBefore);
    TestEqual(
        TEXT("Working binding after request remains Accepted"),
        Runtime->SubmitUiInteraction(WorkingHandle, {}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestTrue(TEXT("Lua VM remains started"), Coordinator && Coordinator->IsLuaVmStarted());

    // Clean up
    Runtime->EndSession();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ScreenRegistryContentRootsCapturedInResolvedPackageSetTest,
    "GV2.Runtime.ScreenRegistry.ContentRootsCapturedInResolvedPackageSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ScreenRegistryContentRootsCapturedInResolvedPackageSetTest::RunTest(const FString& Parameters)
{
    // SAC-02: ue_content_roots is captured into FResolvedPackageSource during ResolvePackageSet*,
    // and passed to UGV2ScreenRegistry through ClosureEntries without reading package.json5 from disk.
    // Modifying package.json5 after ResolvePackageSet* does not affect the captured roots or ownership;
    // only a newly resolved package set sees disk changes.
    const FString TempPackageDir = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        TEXT("SAC02_ContentRootsCapture_") + FGuid::NewGuid().ToString());

    IFileManager& FileManager = IFileManager::Get();
    FileManager.MakeDirectory(*TempPackageDir, true);

    const FString ManifestPath = FPaths::Combine(TempPackageDir, TEXT("package.json5"));
    const FString ManifestA = TEXT("{\n")
        TEXT("  package_id: \"core\",\n")
        TEXT("  namespace: \"core\",\n")
        TEXT("  version: \"1.0.0\",\n")
        TEXT("  ue_content_roots: [\"/Game/TestA\"]\n")
        TEXT("}\n");

    TestTrue(TEXT("Write manifest A"), FFileHelper::SaveStringToFile(ManifestA, *ManifestPath));

    std::vector<GV2ContentCore::FDiagnostic> DiagsA;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> SetA =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(
            {std::filesystem::path(TCHAR_TO_UTF8(*TempPackageDir))},
            DiagsA);

    TestTrue(TEXT("SetA resolved successfully"), SetA.has_value() && SetA->OrderedSources.size() == 1);
    if (!SetA.has_value() || SetA->OrderedSources.size() != 1)
    {
        FileManager.DeleteDirectory(*TempPackageDir, false, true);
        return false;
    }

    const TArray<GV2PackageClosure::FEntry> ClosureEntriesA =
        GV2PackageClosure::FromResolvedPackageSet(*SetA);
    TestEqual(TEXT("ClosureEntriesA has 1 entry"), ClosureEntriesA.Num(), 1);
    TestEqual(TEXT("ClosureEntriesA captured 1 root"), ClosureEntriesA[0].UeContentRoots.Num(), 1);
    if (ClosureEntriesA[0].UeContentRoots.Num() > 0)
    {
        TestEqual(TEXT("Captured root is /Game/TestA"), ClosureEntriesA[0].UeContentRoots[0], TEXT("/Game/TestA"));
    }

    // Now overwrite package.json5 on disk with /Game/TestB
    const FString ManifestB = TEXT("{\n")
        TEXT("  package_id: \"core\",\n")
        TEXT("  namespace: \"core\",\n")
        TEXT("  version: \"1.0.0\",\n")
        TEXT("  ue_content_roots: [\"/Game/TestB\"]\n")
        TEXT("}\n");
    TestTrue(TEXT("Overwrite manifest on disk with /Game/TestB"), FFileHelper::SaveStringToFile(ManifestB, *ManifestPath));

    // ResolveContentRootOwnershipFromGameData using captured ClosureEntriesA must STILL yield /game/testa/
    TArray<FGV2ContentRootOwnership> OwnershipA;
    FString ErrorA;
    TestTrue(
        TEXT("Resolve ownership from captured ClosureEntriesA succeeds"),
        UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(ClosureEntriesA, OwnershipA, ErrorA));
    TestEqual(TEXT("OwnershipA has 1 root"), OwnershipA.Num(), 1);
    if (OwnershipA.Num() > 0)
    {
        TestEqual(
            TEXT("OwnershipA normalized root is /game/testa/ (not mutated by disk file change)"),
            OwnershipA[0].NormalizedRoot,
            TEXT("/game/testa/"));
        TestEqual(TEXT("OwnershipA package_id is core"), OwnershipA[0].PackageId, TEXT("core"));
    }

    // Only a freshly resolved package set sees /Game/TestB
    std::vector<GV2ContentCore::FDiagnostic> DiagsB;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> SetB =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(
            {std::filesystem::path(TCHAR_TO_UTF8(*TempPackageDir))},
            DiagsB);
    TestTrue(TEXT("SetB resolved successfully"), SetB.has_value() && SetB->OrderedSources.size() == 1);
    if (SetB.has_value() && SetB->OrderedSources.size() == 1)
    {
        const TArray<GV2PackageClosure::FEntry> ClosureEntriesB =
            GV2PackageClosure::FromResolvedPackageSet(*SetB);
        TArray<FGV2ContentRootOwnership> OwnershipB;
        FString ErrorB;
        TestTrue(
            TEXT("Resolve ownership from newly resolved ClosureEntriesB succeeds"),
            UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(ClosureEntriesB, OwnershipB, ErrorB));
        TestEqual(TEXT("OwnershipB has 1 root"), OwnershipB.Num(), 1);
        if (OwnershipB.Num() > 0)
        {
            TestEqual(
                TEXT("OwnershipB normalized root is /game/testb/"),
                OwnershipB[0].NormalizedRoot,
                TEXT("/game/testb/"));
        }
    }

    // Check package without ue_content_roots: produces empty roots without error
    const FString ManifestNoRoots = TEXT("{\n")
        TEXT("  package_id: \"core\",\n")
        TEXT("  namespace: \"core\",\n")
        TEXT("  version: \"1.0.0\"\n")
        TEXT("}\n");
    TestTrue(TEXT("Write manifest without roots"), FFileHelper::SaveStringToFile(ManifestNoRoots, *ManifestPath));

    std::vector<GV2ContentCore::FDiagnostic> DiagsNoRoots;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> SetNoRoots =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(
            {std::filesystem::path(TCHAR_TO_UTF8(*TempPackageDir))},
            DiagsNoRoots);
    TestTrue(TEXT("SetNoRoots resolved successfully"), SetNoRoots.has_value() && SetNoRoots->OrderedSources.size() == 1);
    if (SetNoRoots.has_value() && SetNoRoots->OrderedSources.size() == 1)
    {
        const TArray<GV2PackageClosure::FEntry> ClosureEntriesNoRoots =
            GV2PackageClosure::FromResolvedPackageSet(*SetNoRoots);
        TestEqual(TEXT("ClosureEntriesNoRoots has 0 roots"), ClosureEntriesNoRoots[0].UeContentRoots.Num(), 0);
        TArray<FGV2ContentRootOwnership> OwnershipNoRoots;
        FString ErrorNoRoots;
        TestTrue(
            TEXT("Resolve ownership from ClosureEntriesNoRoots succeeds"),
            UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(ClosureEntriesNoRoots, OwnershipNoRoots, ErrorNoRoots));
        TestEqual(TEXT("OwnershipNoRoots has 0 roots"), OwnershipNoRoots.Num(), 0);
    }

    // Clean up
    FileManager.DeleteDirectory(*TempPackageDir, false, true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
