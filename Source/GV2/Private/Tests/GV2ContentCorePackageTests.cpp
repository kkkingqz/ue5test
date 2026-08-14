#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/PackageDescriptor.h"
#include "Misc/AutomationTest.h"
#include <type_traits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCorePackageDescriptorTest,
    "GV2.Runtime.ContentCore.PackageDescriptorValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCorePackageDescriptorTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;
    static_assert(!std::is_assignable_v<FPackageDescriptor&, const FPackageDescriptor&>);

    // 1. Valid core + test_mod descriptor set
    const FPackageDescriptor CorePkg(
        "core",
        "core",
        0,
        { "definitions/items.json5", "definitions/screens.json5" },
        { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item_v1.json5") });
    const FPackageDescriptor ModPkg(
        "test_mod",
        "test_mod",
        1,
        { "definitions/screens.json5" });

    std::vector<FDiagnostic> ValidDiags = ValidatePackageDescriptors({ CorePkg, ModPkg });
    TestTrue(TEXT("Valid package descriptors produce 0 diagnostics"), ValidDiags.empty());

    // 2. Missing core package
    std::vector<FDiagnostic> MissingCoreDiags = ValidatePackageDescriptors({ ModPkg });
    TestEqual(TEXT("Missing core produces 1 diagnostic"), MissingCoreDiags.size(), static_cast<size_t>(1));
    if (!MissingCoreDiags.empty())
    {
        TestEqual(TEXT("Code is missing_core"), MissingCoreDiags[0].Code, std::string("core:diagnostic.repository.package_set.missing_core"));
    }

    // 3. Duplicate package ID
    FPackageDescriptor DupCore = CorePkg;
    std::vector<FDiagnostic> DupIdDiags = ValidatePackageDescriptors({ CorePkg, DupCore });
    TestFalse(TEXT("Duplicate ID produces diagnostics"), DupIdDiags.empty());
    bool bFoundDupId = false;
    for (const auto& D : DupIdDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.duplicate_id") bFoundDupId = true;
    }
    TestTrue(TEXT("Contains duplicate_id diagnostic"), bFoundDupId);

    // 4. Duplicate load_index
    const FPackageDescriptor DupIndexMod("mod_2", "mod_2", 1);
    std::vector<FDiagnostic> DupIndexDiags = ValidatePackageDescriptors({ CorePkg, ModPkg, DupIndexMod });
    bool bFoundDupIndex = false;
    for (const auto& D : DupIndexDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.duplicate_load_index") bFoundDupIndex = true;
    }
    TestTrue(TEXT("Contains duplicate_load_index diagnostic"), bFoundDupIndex);

    // 5. Invalid core index
    const FPackageDescriptor BadCore("core", "core", 5);
    std::vector<FDiagnostic> BadCoreDiags = ValidatePackageDescriptors({ BadCore });
    bool bFoundBadCoreIndex = false;
    for (const auto& D : BadCoreDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.invalid_core_index") bFoundBadCoreIndex = true;
    }
    TestTrue(TEXT("Contains invalid_core_index diagnostic"), bFoundBadCoreIndex);

    const FPackageDescriptor InvalidIdentity("Bad-Mod", "foreign", 2);
    const std::vector<FDiagnostic> InvalidIdentityDiags = ValidatePackageDescriptors({ CorePkg, InvalidIdentity });
    bool bFoundInvalidPackageId = false;
    bool bFoundNamespaceMismatch = false;
    for (const FDiagnostic& Diagnostic : InvalidIdentityDiags)
    {
        bFoundInvalidPackageId |= Diagnostic.Code == "core:diagnostic.repository.package_set.invalid_package_id";
        bFoundNamespaceMismatch |= Diagnostic.Code == "core:diagnostic.repository.package_set.namespace_mismatch";
    }
    TestTrue(TEXT("Invalid package_id is rejected"), bFoundInvalidPackageId);
    TestTrue(TEXT("Foreign namespace is rejected"), bFoundNamespaceMismatch);

    const FPackageDescriptor VersionedSchemas(
        "core",
        "core",
        0,
        {},
        {
            FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item_v1.json5"),
            FSchemaBinding("item", 2, "core:schema.item.v2", "schemas/item_v2.json5"),
        });
    TestTrue(
        TEXT("Different exact versions of one definition type are allowed"),
        ValidatePackageDescriptors({ VersionedSchemas }).empty());

    const FPackageDescriptor DuplicateBinding(
        "core",
        "core",
        0,
        {},
        {
            FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item_v1.json5"),
            FSchemaBinding("item", 1, "core:schema.item.alternate", "schemas/item_alternate.json5"),
        });
    const std::vector<FDiagnostic> DuplicateBindingDiagnostics = ValidatePackageDescriptors({ DuplicateBinding });
    bool bFoundDuplicateBinding = false;
    for (const FDiagnostic& Diagnostic : DuplicateBindingDiagnostics)
    {
        bFoundDuplicateBinding |= Diagnostic.Code == "core:diagnostic.repository.package_set.duplicate_schema_binding";
    }
    TestTrue(TEXT("Duplicate exact binding is rejected in descriptor"), bFoundDuplicateBinding);

    const FPackageDescriptor InvalidVersion(
        "core",
        "core",
        0,
        {},
        { FSchemaBinding("item", 0, "core:schema.item.v0", "schemas/item_v0.json5") });
    TestFalse(TEXT("Non-positive schema version is rejected"), ValidatePackageDescriptors({ InvalidVersion }).empty());

    const FPackageDescriptor ExtensionPackage(
        "core", "core", 0, {}, {},
        { FExtensionSchemaBinding(
            "item", 1, "definition_entry", "core",
            "core:schema.extension.item.entry.v1",
            "schemas/item_entry_extension_v1.json5") });
    TestTrue(TEXT("Package-owned exact extension binding is valid"),
        ValidatePackageDescriptors({ ExtensionPackage }).empty());

    const FPackageDescriptor ForeignExtensionPackage(
        "core", "core", 0, {}, {},
        { FExtensionSchemaBinding(
            "item", 1, "definition_entry", "weather_mod",
            "weather_mod:schema.extension.item.entry.v1",
            "schemas/foreign_extension_v1.json5") });
    const std::vector<FDiagnostic> ForeignExtensionDiagnostics =
        ValidatePackageDescriptors({ ForeignExtensionPackage });
    bool bFoundInvalidExtensionBinding = false;
    for (const FDiagnostic& Diagnostic : ForeignExtensionDiagnostics)
    {
        bFoundInvalidExtensionBinding |= Diagnostic.Code
            == "core:diagnostic.repository.package_set.invalid_extension_schema_binding";
    }
    TestTrue(TEXT("Foreign extension schema binding is rejected"), bFoundInvalidExtensionBinding);

    const FPackageDescriptor ValidRetirement(
        "core", "core", 0, {}, {}, {},
        { FRedirectDescriptor("core:item.old", "test_mod:item.new") },
        { "core:item.removed" });
    TestTrue(TEXT("Owner may redirect to a same-kind foreign target"),
        ValidatePackageDescriptors({ ValidRetirement }).empty());

    const FPackageDescriptor InvalidRetirement(
        "core", "core", 0, {}, {}, {},
        {
            FRedirectDescriptor("test_mod:item.foreign", "core:item.target"),
            FRedirectDescriptor("core:item.old", "core:screen.wrong_kind"),
            FRedirectDescriptor("core:item.conflict", "core:item.target"),
        },
        { "core:item.conflict", "test_mod:item.foreign_tombstone" });
    const std::vector<FDiagnostic> RetirementDiagnostics =
        ValidatePackageDescriptors({ InvalidRetirement });
    bool bForeignRedirect = false;
    bool bKindMismatch = false;
    bool bConflict = false;
    bool bForeignTombstone = false;
    for (const FDiagnostic& Diagnostic : RetirementDiagnostics)
    {
        bForeignRedirect |= Diagnostic.Code == "core:diagnostic.repository.redirect.foreign_source";
        bKindMismatch |= Diagnostic.Code == "core:diagnostic.repository.redirect.kind_mismatch";
        bConflict |= Diagnostic.Code == "core:diagnostic.repository.redirect.conflict";
        bForeignTombstone |= Diagnostic.Code == "core:diagnostic.repository.tombstone.foreign_id";
    }
    TestTrue(TEXT("Foreign redirect source is rejected"), bForeignRedirect);
    TestTrue(TEXT("Redirect kind mismatch is rejected"), bKindMismatch);
    TestTrue(TEXT("Redirect/tombstone conflict is rejected"), bConflict);
    TestTrue(TEXT("Foreign tombstone is rejected"), bForeignTombstone);

    return true;
}

#endif
