#if WITH_DEV_AUTOMATION_TESTS

#include "Application/GV2PackageClosure.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiSchemaCache.h"
#include "Blueprint/WidgetTree.h"
#include "GV2ContentCore/UiSchema.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"

namespace
{
using namespace GV2ContentCore;

FCompiledUiFieldSpecPtr MakePropertyHostScalarSpec(
    const EScalarFieldKind Kind,
    const TOptional<double> MinNumber = {},
    const TOptional<double> MaxNumber = {})
{
    auto Spec = std::make_shared<FCompiledUiFieldSpec>();
    Spec->Kind = EUiFieldKind::Scalar;
    FScalarFieldSpec Scalar;
    Scalar.Kind = Kind;
    if (MinNumber.IsSet()) { Scalar.MinimumNumber = MinNumber.GetValue(); }
    if (MaxNumber.IsSet()) { Scalar.MaximumNumber = MaxNumber.GetValue(); }
    Spec->Scalar = MoveTemp(Scalar);
    return Spec;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiPropertyHostTest,
    "GV2.UI.PropertyHostAndCapabilities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiPropertyHostTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // 1. Build a Button capability tree: text, binding, enabled, icon (ref:resource), tooltip
    const FGV2UiCapabilityTree ButtonCaps = FGV2UiCapabilityBuilder()
        .AddText(TEXT("text"), FName(TEXT("ButtonLabel")))
        .AddBinding(TEXT("binding"), FName(TEXT("ClickBinding")))
        .AddBoolean(TEXT("enabled"), FName(TEXT("ButtonRoot")))
        .AddImage(TEXT("icon"), FName(TEXT("IconImage")), TEXT("resource"))
        .AddText(TEXT("tooltip"), FName(TEXT("TooltipText")))
        .AddNumber(TEXT("percent"), FName(TEXT("ProgressBar")), 0.0, 1.0)
        .Build();

    TestEqual(TEXT("Button capabilities count"), ButtonCaps.Num(), 6);
    TestTrue(TEXT("Has text cap"), ButtonCaps.HasProperty(TEXT("text")));
    TestTrue(TEXT("Has binding cap"), ButtonCaps.HasProperty(TEXT("binding")));

    // 2. Subset rule: Narrower valid schema { text, binding } is a subset of ButtonCaps -> PASS
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "text", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });
        Schema.Fields.push_back({ "binding", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding) });

        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
            Schema, ButtonCaps, TEXT("core:schema.ui_field.btn.v1"), TEXT(""), Diagnostics);

        TestTrue(TEXT("Subset schema is compatible"), bCompatible);
        TestEqual(TEXT("No diagnostics on valid subset"), Diagnostics.Num(), 0);
    }

    // 3. Extra property in schema -> FAIL with core:diagnostic.ui_capability.unknown_schema_property
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "text", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });
        Schema.Fields.push_back({ "unknown_prop", false, MakePropertyHostScalarSpec(EScalarFieldKind::String) });

        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
            Schema, ButtonCaps, TEXT("core:schema.ui_field.btn.v1"), TEXT(""), Diagnostics);

        TestTrue(TEXT("Extra schema prop fails"), !bCompatible);
        TestEqual(TEXT("One diagnostic for unknown prop"), Diagnostics.Num(), 1);
        if (Diagnostics.Num() > 0)
        {
            TestEqual(TEXT("Code is unknown_schema_property"),
                Diagnostics[0].Code, TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
            TestEqual(TEXT("Property path is unknown_prop"), Diagnostics[0].PropertyPath, TEXT("unknown_prop"));
        }
    }

    // 4. Kind mismatch: schema specifies String instead of Text -> FAIL with core:diagnostic.ui_capability.kind_mismatch
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "text", false, MakePropertyHostScalarSpec(EScalarFieldKind::String) });

        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
            Schema, ButtonCaps, TEXT("core:schema.ui_field.btn.v1"), TEXT(""), Diagnostics);

        TestTrue(TEXT("Kind mismatch fails"), !bCompatible);
        TestEqual(TEXT("One diagnostic for kind mismatch"), Diagnostics.Num(), 1);
        if (Diagnostics.Num() > 0)
        {
            TestEqual(TEXT("Code is kind_mismatch"),
                Diagnostics[0].Code, TEXT("core:diagnostic.ui_capability.kind_mismatch"));
        }
    }

    // 5. Target kind mismatch for Ref: schema requires "item", widget supports "resource"
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        auto RefSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Ref);
        RefSpec->RefTargetKind = "item";
        Schema.Fields.push_back({ "icon", false, RefSpec });

        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
            Schema, ButtonCaps, TEXT("core:schema.ui_field.btn.v1"), TEXT(""), Diagnostics);

        TestTrue(TEXT("Target kind mismatch fails"), !bCompatible);
        TestEqual(TEXT("One diagnostic for target_kind mismatch"), Diagnostics.Num(), 1);
        if (Diagnostics.Num() > 0)
        {
            TestEqual(TEXT("Code is target_kind_mismatch"),
                Diagnostics[0].Code, TEXT("core:diagnostic.ui_capability.target_kind_mismatch"));
        }
    }

    // 6. Range bounds check: broader range in schema rejected, narrower range accepted
    {
        // Broader range [-1.0, 1.0] vs capability [0.0, 1.0] -> FAIL
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto NumSpec = MakePropertyHostScalarSpec(EScalarFieldKind::Number, -1.0, 1.0);
            Schema.Fields.push_back({ "percent", false, NumSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, ButtonCaps, TEXT("core:schema.ui_field.pb.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Broader numeric range fails"), !bCompatible);
            TestTrue(TEXT("Has range_unsupported diagnostic"),
                Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_capability.range_unsupported"));
        }

        // Narrower range [0.0, 0.5] vs capability [0.0, 1.0] -> PASS
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto NumSpec = MakePropertyHostScalarSpec(EScalarFieldKind::Number, 0.0, 0.5);
            Schema.Fields.push_back({ "percent", false, NumSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, ButtonCaps, TEXT("core:schema.ui_field.pb.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Narrower numeric range passes"), bCompatible);
        }
    }

    // 8. Keyed collection policy: array capability with bRequiresKeyedIdentity requires schema keyed_by
    {
        FGV2UiPropertyCapability ItemCap;
        ItemCap.SupportedKind = EGV2PreparedUiValueKind::Text;
        ItemCap.TargetName = FName(TEXT("OptionText"));

        const FGV2UiCapabilityTree DropdownCaps = FGV2UiCapabilityBuilder()
            .AddKeyedCollection(TEXT("options"), FName(TEXT("OptionsList")), ItemCap, TEXT("key"))
            .Build();

        // Schema array without keyed_by -> FAIL
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->Items = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
            Schema.Fields.push_back({ "options", false, ArrSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, DropdownCaps, TEXT("core:schema.ui_field.dropdown.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Unkeyed array on keyed collection fails"), !bCompatible);
            TestTrue(TEXT("Has collection_identity_mismatch diagnostic"),
                Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_capability.collection_identity_mismatch"));
        }

        // GBF-02: `keyed_by` is a name-bearing schema constraint, not merely a
        // boolean. Before this task the schema projection retained only the boolean,
        // so this preflight comparison falsely accepted `id` against the declaration's
        // `key` default without needing any runtime items to expose the disagreement.
        // Restoring that flag-only projection must make this regression fail again.
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->KeyedBy = "id";
            ArrSpec->Items = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
            Schema.Fields.push_back({ "options", false, ArrSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, DropdownCaps, TEXT("core:schema.ui_field.dropdown.v1"), TEXT(""), Diagnostics);

            TestFalse(TEXT("GBF-02: keyed_by name mismatch is rejected before Ready"), bCompatible);
            TestEqual(TEXT("GBF-02: keyed_by name mismatch emits one diagnostic"), Diagnostics.Num(), 1);
            if (Diagnostics.Num() > 0)
            {
                TestEqual(TEXT("GBF-02: keyed_by name mismatch reports KeyPropertyMismatch"),
                    Diagnostics[0].Code, TEXT("core:diagnostic.ui_capability.key_property_mismatch"));
                TestTrue(TEXT("GBF-02: diagnostic names both schema and capability keys"),
                    Diagnostics[0].Message.Contains(TEXT("id")) && Diagnostics[0].Message.Contains(TEXT("key")));
            }
        }

        // Schema array with matching keyed_by -> PASS
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->KeyedBy = "key";
            ArrSpec->Items = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
            Schema.Fields.push_back({ "options", false, ArrSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, DropdownCaps, TEXT("core:schema.ui_field.dropdown.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Matching keyed_by name on keyed collection passes"), bCompatible);
            TestEqual(TEXT("Matching keyed_by name has no diagnostics"), Diagnostics.Num(), 0);
        }
    }

    // 9. PCC-02: Keyed collection element compatibility participates in Schema ⊆ Capabilities recursively
    {
        // 9a. Extra property in collection item schema is rejected with distinguishable code
        {
            FGV2UiCapabilityTree ItemCaps = FGV2UiCapabilityBuilder()
                .AddKey(TEXT("key"), NAME_None)
                .AddText(TEXT("text"), FName(TEXT("LabelText")))
                .Build();

            const FGV2UiCapabilityTree ListCaps = FGV2UiCapabilityBuilder()
                .AddKeyedCollection(TEXT("items"), FName(TEXT("Container")), ItemCaps, TEXT("key"))
                .Build();

            // Schema element has extra property 'extra_field' (scalar number)
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto ItemSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Object);
            ItemSpec->Fields.push_back({ "key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key) });
            ItemSpec->Fields.push_back({ "text", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });
            ItemSpec->Fields.push_back({ "extra_field", false, MakePropertyHostScalarSpec(EScalarFieldKind::Number) });

            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->KeyedBy = "key";
            ArrSpec->Items = ItemSpec;
            Schema.Fields.push_back({ "items", false, ArrSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, ListCaps, TEXT("test:schema.extra_elem_item"), TEXT(""), Diagnostics);

            TestFalse(TEXT("PCC-02: Extra property in collection element schema is rejected before Ready"), bCompatible);
            TestTrue(TEXT("PCC-02: Has unknown_schema_property diagnostic for element property"),
                Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
            if (Diagnostics.Num() > 0)
            {
                TestEqual(TEXT("PCC-02: Diagnostic property_path reaches into element"),
                    Diagnostics[0].PropertyPath, TEXT("items[].extra_field"));
            }
        }

        // 9b. More narrow schema for collection item is accepted (Schema ⊆ Capabilities)
        {
            // Capability supports: key, text, binding, icon (ref)
            FGV2UiCapabilityTree RichItemCaps = FGV2UiCapabilityBuilder()
                .AddKey(TEXT("key"), NAME_None)
                .AddText(TEXT("text"), FName(TEXT("LabelText")))
                .AddBinding(TEXT("binding"), NAME_None)
                .AddImage(TEXT("icon"), FName(TEXT("IconImage")), TEXT("resource"))
                .Build();

            const FGV2UiCapabilityTree ListCaps = FGV2UiCapabilityBuilder()
                .AddKeyedCollection(TEXT("items"), FName(TEXT("Container")), RichItemCaps, TEXT("key"))
                .Build();

            // Schema only provides subset: key, text
            FCompiledUiFieldSpec NarrowSchema;
            NarrowSchema.Kind = EUiFieldKind::Object;
            auto NarrowItemSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Object);
            NarrowItemSpec->Fields.push_back({ "key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key) });
            NarrowItemSpec->Fields.push_back({ "text", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->KeyedBy = "key";
            ArrSpec->Items = NarrowItemSpec;
            NarrowSchema.Fields.push_back({ "items", false, ArrSpec });

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                NarrowSchema, ListCaps, TEXT("test:schema.narrow_elem_item"), TEXT(""), Diagnostics);

            TestTrue(TEXT("PCC-02: More narrow schema for collection item is accepted"), bCompatible);
            TestEqual(TEXT("PCC-02: Zero diagnostics for subset schema"), Diagnostics.Num(), 0);
        }

        // 9c. Separate test confirming both sides originate from DIFFERENT sources:
        // Schema loaded from repository content, Capabilities queried from live widget
        {
            UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
            TestNotNull(TEXT("TestWorld created"), TestWorld);

            // Source 1: Schema loaded directly from repository package
            FGV2UiSchemaCache RepoSchemaCache({ FGV2SchemaPackageRoot{TEXT("textsystem"), FPaths::ProjectDir() / TEXT("GameData/textsystem")} });
            FString SchemaErr;
            GV2ContentCore::FCompiledUiFieldSpecPtr RepoSchema = RepoSchemaCache.GetCompiledSchema(
                "textsystem:schema.ui_field.location_commands.v1",
                SchemaErr);
            TestNotNull(TEXT("PCC-02 Source 1: Schema loaded from repository"), RepoSchema.get());

            // Source 2: Capability tree queried directly from widget
            UGV2ButtonListWidgetBase* ButtonListWidget = CreateWidget<UGV2ButtonListWidgetBase>(
                TestWorld, UGV2ButtonListWidgetBase::StaticClass());
            TestNotNull(TEXT("PCC-02 Source 2: Widget created"), ButtonListWidget);
            // DCA-03: ButtonWidgetClass has no fallback -- without it the "items" nested
            // item capability tree stays empty, and the S ⊆ C check below sees the
            // schema's item fields (key/text/binding) as unsupported.
            if (FProperty* Prop = UGV2ButtonListWidgetBase::StaticClass()->FindPropertyByName(TEXT("ButtonWidgetClass")))
            {
                UClass* RealButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
                *Prop->ContainerPtrToValuePtr<TSubclassOf<UGV2ButtonWidgetBase>>(ButtonListWidget) =
                    RealButtonClass != nullptr ? RealButtonClass : UGV2ButtonWidgetBase::StaticClass();
            }

            FGV2UiCapabilityBuilder WidgetBuilder;
            ButtonListWidget->DescribeUiCapabilities(WidgetBuilder);
            const FGV2UiCapabilityTree WidgetCaps = WidgetBuilder.Build();

            // S ⊆ C check with different sources: valid repository schema matches widget capabilities
            if (RepoSchema != nullptr)
            {
                TArray<FGV2UiSchemaCompatibilityDiagnostic> MatchDiags;
                const bool bMatch = CheckUiSchemaCapabilityCompatibility(
                    *RepoSchema, WidgetCaps, TEXT("textsystem:schema.ui_field.location_commands.v1"), TEXT(""), MatchDiags);
                for (const auto& D : MatchDiags)
                {
                    UE_LOG(LogTemp, Error, TEXT("PCC-02 MatchDiag: code=%s path=%s msg=%s"), *D.Code, *D.PropertyPath, *D.Message);
                }
                TestTrue(TEXT("PCC-02: Repo schema matches widget capabilities"), bMatch);
                TestEqual(TEXT("PCC-02: Zero diagnostics for matching repo schema"), MatchDiags.Num(), 0);

                // Clone repo schema and add an unsupported property to items elements
                FCompiledUiFieldSpec ModifiedRepoSchema = *RepoSchema;
                for (auto& Field : ModifiedRepoSchema.Fields)
                {
                    if (Field.Name == "items" && Field.Spec != nullptr && Field.Spec->Items != nullptr)
                    {
                        auto ClonedItemSpec = std::make_shared<FCompiledUiFieldSpec>(*Field.Spec->Items);
                        ClonedItemSpec->Fields.push_back({
                            "unsupported_extra_action",
                            false,
                            std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key)
                        });
                        auto ClonedArrSpec = std::make_shared<FCompiledUiFieldSpec>(*Field.Spec);
                        ClonedArrSpec->Items = ClonedItemSpec;
                        Field.Spec = ClonedArrSpec;
                        break;
                    }
                }

                TArray<FGV2UiSchemaCompatibilityDiagnostic> MismatchDiags;
                const bool bMismatch = CheckUiSchemaCapabilityCompatibility(
                    ModifiedRepoSchema, WidgetCaps, TEXT("textsystem:schema.ui_field.location_commands.v1"), TEXT(""), MismatchDiags);
                TestFalse(TEXT("PCC-02: Repo schema with extra item property rejected"), bMismatch);
                TestTrue(TEXT("PCC-02: Rejection diagnostic has unknown_schema_property"),
                    MismatchDiags.Num() > 0 && MismatchDiags[0].Code == TEXT("core:diagnostic.ui_capability.unknown_schema_property"));
                if (MismatchDiags.Num() > 0)
                {
                    TestEqual(TEXT("PCC-02: Diagnostic property_path is items[].unsupported_extra_action"),
                        MismatchDiags[0].PropertyPath, TEXT("items[].unsupported_extra_action"));
                }
            }

            TestWorld->DestroyWorld(false);
        }
    }

    // 10. PCC-08: Reset mutations obey the same invariants as apply mutations -- a reset
    // whose target/consumer cannot be resolved must reject Prepare with a typed
    // diagnostic, not silently add an unresolvable mutation that Commit later no-ops
    // without a trace. Before PCC-08, both reset branches in PrepareUiHostProperties
    // (absent-optional-property, and property-removed-from-a-reused-instance's-schema)
    // skipped the missing_target/unsupported_kind checks the apply branch already had.
    {
        UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
        TestNotNull(TEXT("PCC-08: TestWorld created"), TestWorld);
        if (TestWorld != nullptr)
        {
            // 10a. Negative: optional Text property whose capability TargetName does not
            // resolve on the host -- absent value in the candidate must REJECT Prepare,
            // not silently add an unresolvable reset mutation.
            {
                UGV2PanelWidgetBase* Host = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
                TestNotNull(TEXT("PCC-08: unresolvable-target host widget instantiates"), Host);

                const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
                    .AddText(TEXT("subtitle"), FName(TEXT("NonexistentSubtitleWidget")))
                    .Build();

                FCompiledUiFieldSpec Schema;
                Schema.Kind = EUiFieldKind::Object;
                Schema.Fields.push_back({ "subtitle", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

                const FGV2PreparedUiObject EmptyCandidate;
                const FGV2PreparedUiObject EmptyLastCommitted;
                FGV2UiHostMutationPlan Plan;
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
                const bool bPrepared = PrepareUiHostProperties(
                    Host, Caps, EmptyCandidate, Schema, TEXT("test:schema.pcc08_unresolvable_reset"),
                    TEXT(""), EmptyLastCommitted, Plan, Diagnostics);

                TestFalse(TEXT("PCC-08: Prepare rejects a reset whose target cannot be resolved"), bPrepared);
                TestTrue(TEXT("PCC-08: Diagnostic is missing_target"),
                    Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_consumer.missing_target"));
                TestTrue(TEXT("PCC-08: Plan is empty, not populated with an unresolvable mutation"), Plan.IsEmpty());
            }

            // 10b. Positive: same shape, but the capability's target genuinely resolves --
            // the reset mutation is still accepted and added to the plan (the fix must not
            // over-reject a resolvable reset).
            {
                UGV2PanelWidgetBase* Host = CreateWidget<UGV2PanelWidgetBase>(TestWorld, UGV2PanelWidgetBase::StaticClass());
                TestNotNull(TEXT("PCC-08: resolvable-target host widget instantiates"), Host);
                Host->WidgetTree = NewObject<UWidgetTree>(Host);
                UGV2TextWidgetBase* SubtitleWidget = Host->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("SubtitleWidget"));
                TestNotNull(TEXT("PCC-08: SubtitleWidget child constructs"), SubtitleWidget);
                Host->WidgetTree->RootWidget = SubtitleWidget;

                const FGV2UiCapabilityTree Caps = FGV2UiCapabilityBuilder()
                    .AddText(TEXT("subtitle"), FName(TEXT("SubtitleWidget")))
                    .Build();

                FCompiledUiFieldSpec Schema;
                Schema.Kind = EUiFieldKind::Object;
                Schema.Fields.push_back({ "subtitle", false, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

                const FGV2PreparedUiObject EmptyCandidate;
                const FGV2PreparedUiObject EmptyLastCommitted;
                FGV2UiHostMutationPlan Plan;
                TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
                const bool bPrepared = PrepareUiHostProperties(
                    Host, Caps, EmptyCandidate, Schema, TEXT("test:schema.pcc08_resolvable_reset"),
                    TEXT(""), EmptyLastCommitted, Plan, Diagnostics);

                TestTrue(TEXT("PCC-08: Prepare accepts a reset whose target resolves"), bPrepared);
                TestEqual(TEXT("PCC-08: Plan has exactly one (reset) mutation"), Plan.Num(), 1);
                if (Plan.Num() == 1)
                {
                    TestTrue(TEXT("PCC-08: The mutation is marked as a reset"), Plan.GetMutations()[0].bIsReset);
                }
            }

            TestWorld->DestroyWorld(false);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SchemaRootsFromClosureTest,
    "GV2.Runtime.ContentAuthoring.SchemaRootsFromClosure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SchemaRootsFromClosureTest::RunTest(const FString& Parameters)
{
    // DCA-19: schema roots come from the pinned closure (GameData/mods.lock.json5),
    // not a hand-written list -- a package physically present on disk but absent
    // from the closure (GameData/sample) must never be scanned for schemas.
    const TArray<GV2PackageClosure::FEntry> Closure = GV2PackageClosure::DiscoverFromGameData();
    TestEqual(TEXT("Pinned closure resolves exactly the three mods.lock.json5 packages"), Closure.Num(), 3);
    bool bClosureIncludesSample = false;
    for (const GV2PackageClosure::FEntry& Entry : Closure)
    {
        bClosureIncludesSample |= Entry.PackageId.Equals(TEXT("sample"), ESearchCase::IgnoreCase);
    }
    TestFalse(
        TEXT("GameData/sample is physically present but absent from mods.lock.json5, so it's not in the closure"),
        bClosureIncludesSample);

    // A schema whose declared `id` namespace doesn't match the package root it was
    // discovered under is rejected -- proven with real content (textsystem's own
    // location_commands schema) deliberately tagged as belonging to "core".
    FGV2UiSchemaCache MismatchedCache({
        FGV2SchemaPackageRoot{TEXT("core"), FPaths::ProjectDir() / TEXT("GameData/textsystem")}
    });
    FString MismatchError;
    TestNull(
        TEXT("A schema whose id namespace doesn't match its tagged package root is not registered"),
        MismatchedCache.GetCompiledSchema("textsystem:schema.ui_field.location_commands.v1", MismatchError).get());

    FGV2UiSchemaCache MatchedCache({
        FGV2SchemaPackageRoot{TEXT("textsystem"), FPaths::ProjectDir() / TEXT("GameData/textsystem")}
    });
    FString MatchedError;
    TestNotNull(
        TEXT("The same schema resolves normally when its package root is correctly tagged"),
        MatchedCache.GetCompiledSchema("textsystem:schema.ui_field.location_commands.v1", MatchedError).get());

    return true;
}

#endif
