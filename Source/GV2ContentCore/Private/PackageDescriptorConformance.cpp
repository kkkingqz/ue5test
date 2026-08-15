#include "GV2ContentCore/Testing/PackageDescriptorConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"

#include <string>
#include <type_traits>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunPackageDescriptorConformance()
{
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
    if (!ValidDiags.empty())
    {
        return "package_descriptor.valid_set_produces_zero_diagnostics";
    }

    // 2. Missing core package
    std::vector<FDiagnostic> MissingCoreDiags = ValidatePackageDescriptors({ ModPkg });
    if (MissingCoreDiags.empty()
        || MissingCoreDiags[0].Code != "core:diagnostic.repository.package_set.missing_core")
    {
        return "package_descriptor.missing_core_rejected";
    }

    // 3. Duplicate package ID
    FPackageDescriptor DupCore = CorePkg;
    std::vector<FDiagnostic> DupIdDiags = ValidatePackageDescriptors({ CorePkg, DupCore });
    bool bFoundDupId = false;
    for (const auto& D : DupIdDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.duplicate_id")
        {
            bFoundDupId = true;
        }
    }
    if (!bFoundDupId)
    {
        return "package_descriptor.duplicate_package_id_rejected";
    }

    // 4. Duplicate load_index
    const FPackageDescriptor DupIndexMod("mod_2", "mod_2", 1);
    std::vector<FDiagnostic> DupIndexDiags = ValidatePackageDescriptors({ CorePkg, ModPkg, DupIndexMod });
    bool bFoundDupIndex = false;
    for (const auto& D : DupIndexDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.duplicate_load_index")
        {
            bFoundDupIndex = true;
        }
    }
    if (!bFoundDupIndex)
    {
        return "package_descriptor.duplicate_load_index_rejected";
    }

    // 5. Invalid core index
    const FPackageDescriptor BadCore("core", "core", 5);
    std::vector<FDiagnostic> BadCoreDiags = ValidatePackageDescriptors({ BadCore });
    bool bFoundBadCoreIndex = false;
    for (const auto& D : BadCoreDiags)
    {
        if (D.Code == "core:diagnostic.repository.package_set.invalid_core_index")
        {
            bFoundBadCoreIndex = true;
        }
    }
    if (!bFoundBadCoreIndex)
    {
        return "package_descriptor.invalid_core_index_rejected";
    }

    // 6. Invalid identity and namespace mismatch
    const FPackageDescriptor InvalidIdentity("Bad-Mod", "foreign", 2);
    const std::vector<FDiagnostic> InvalidIdentityDiags = ValidatePackageDescriptors({ CorePkg, InvalidIdentity });
    bool bFoundInvalidPackageId = false;
    bool bFoundNamespaceMismatch = false;
    for (const FDiagnostic& Diagnostic : InvalidIdentityDiags)
    {
        bFoundInvalidPackageId |= Diagnostic.Code == "core:diagnostic.repository.package_set.invalid_package_id";
        bFoundNamespaceMismatch |= Diagnostic.Code == "core:diagnostic.repository.package_set.namespace_mismatch";
    }
    if (!bFoundInvalidPackageId || !bFoundNamespaceMismatch)
    {
        return "package_descriptor.invalid_package_id_and_namespace_mismatch_rejected";
    }

    // 7. Versioned schemas of one definition type
    const FPackageDescriptor VersionedSchemas(
        "core",
        "core",
        0,
        {},
        {
            FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item_v1.json5"),
            FSchemaBinding("item", 2, "core:schema.item.v2", "schemas/item_v2.json5"),
        });
    if (!ValidatePackageDescriptors({ VersionedSchemas }).empty())
    {
        return "package_descriptor.versioned_schemas_of_same_type_allowed";
    }

    // 8. Duplicate exact schema binding
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
    if (!bFoundDuplicateBinding)
    {
        return "package_descriptor.duplicate_schema_binding_rejected";
    }

    // 9. Non-positive schema version
    const FPackageDescriptor InvalidVersion(
        "core",
        "core",
        0,
        {},
        { FSchemaBinding("item", 0, "core:schema.item.v0", "schemas/item_v0.json5") });
    if (ValidatePackageDescriptors({ InvalidVersion }).empty())
    {
        return "package_descriptor.non_positive_schema_version_rejected";
    }

    // 10. Package-owned extension schema binding
    const FPackageDescriptor ExtensionPackage(
        "core", "core", 0, {}, {},
        { FExtensionSchemaBinding(
            "item", 1, "definition_entry", "core",
            "core:schema.extension.item.entry.v1",
            "schemas/item_entry_extension_v1.json5") });
    if (!ValidatePackageDescriptors({ ExtensionPackage }).empty())
    {
        return "package_descriptor.package_owned_extension_binding_valid";
    }

    // 11. Foreign extension schema binding
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
    if (!bFoundInvalidExtensionBinding)
    {
        return "package_descriptor.foreign_extension_schema_binding_rejected";
    }

    // 12. Valid retirement
    const FPackageDescriptor ValidRetirement(
        "core", "core", 0, {}, {}, {},
        { FRedirectDescriptor("core:item.old", "test_mod:item.new") },
        { "core:item.removed" });
    if (!ValidatePackageDescriptors({ ValidRetirement }).empty())
    {
        return "package_descriptor.valid_retirement_redirects_and_tombstones";
    }

    // 13. Invalid retirement
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
    if (!bForeignRedirect)
    {
        return "package_descriptor.foreign_redirect_source_rejected";
    }
    if (!bKindMismatch)
    {
        return "package_descriptor.redirect_kind_mismatch_rejected";
    }
    if (!bConflict)
    {
        return "package_descriptor.redirect_tombstone_conflict_rejected";
    }
    if (!bForeignTombstone)
    {
        return "package_descriptor.foreign_tombstone_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
