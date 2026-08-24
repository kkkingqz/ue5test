#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiCapability.h"
#include "GV2ContentCore/UiSchema.h"
#include "Misc/AutomationTest.h"

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
        Schema.ObjectFields.emplace_back("text", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text));
        Schema.ObjectFields.emplace_back("binding", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding));

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
        Schema.ObjectFields.emplace_back("text", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text));
        Schema.ObjectFields.emplace_back("unknown_prop", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::String));

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
            TestTrue(TEXT("Core schema error is fatal"), Diagnostics[0].bFatal);
        }
    }

    // 4. Kind mismatch: schema specifies String instead of Text -> FAIL with core:diagnostic.ui_capability.kind_mismatch
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.ObjectFields.emplace_back("text", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::String));

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
        RefSpec->TargetKind = "item";
        Schema.ObjectFields.emplace_back("icon", RefSpec);

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
            auto NumSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Number);
            NumSpec->NumberMin = -1.0;
            NumSpec->NumberMax = 1.0;
            Schema.ObjectFields.emplace_back("percent", NumSpec);

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
            auto NumSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Number);
            NumSpec->NumberMin = 0.0;
            NumSpec->NumberMax = 0.5;
            Schema.ObjectFields.emplace_back("percent", NumSpec);

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, ButtonCaps, TEXT("core:schema.ui_field.pb.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Narrower numeric range passes"), bCompatible);
        }
    }

    // 7. Mod schema failure policy: bFatal is false (rejects mod, does not block core session)
    {
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.ObjectFields.emplace_back("bad_prop", std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::String));

        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
            Schema, ButtonCaps, TEXT("mymod:schema.ui_field.custom.v1"), TEXT(""), Diagnostics);

        TestTrue(TEXT("Mod schema incompatible fails compatibility"), !bCompatible);
        TestEqual(TEXT("One diagnostic for mod error"), Diagnostics.Num(), 1);
        if (Diagnostics.Num() > 0)
        {
            TestTrue(TEXT("Mod schema diagnostic is NOT fatal to core session"), !Diagnostics[0].bFatal);
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
            ArrSpec->ArrayItems = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
            Schema.ObjectFields.emplace_back("options", ArrSpec);

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, DropdownCaps, TEXT("core:schema.ui_field.dropdown.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Unkeyed array on keyed collection fails"), !bCompatible);
            TestTrue(TEXT("Has collection_identity_mismatch diagnostic"),
                Diagnostics.Num() > 0 && Diagnostics[0].Code == TEXT("core:diagnostic.ui_capability.collection_identity_mismatch"));
        }

        // Schema array with keyed_by -> PASS
        {
            FCompiledUiFieldSpec Schema;
            Schema.Kind = EUiFieldKind::Object;
            auto ArrSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
            ArrSpec->KeyedBy = "key";
            ArrSpec->ArrayItems = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
            Schema.ObjectFields.emplace_back("options", ArrSpec);

            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
                Schema, DropdownCaps, TEXT("core:schema.ui_field.dropdown.v1"), TEXT(""), Diagnostics);

            TestTrue(TEXT("Keyed array on keyed collection passes"), bCompatible);
        }
    }

    return true;
}

#endif
