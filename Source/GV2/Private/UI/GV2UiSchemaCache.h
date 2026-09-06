#pragma once

#include "CoreMinimal.h"
#include "GV2ContentCore/UiSchema.h"

#include <map>
#include <string>

// DCA-19: a schema root is tagged with the package_id that owns it (from the pinned
// closure, GV2PackageClosure::DiscoverFromGameData), so EnsureDiscovered can reject a
// schema whose own declared `id` namespace doesn't match the package directory it was
// physically found under -- a schema doesn't get to claim someone else's namespace by
// simply not being asked.
struct FGV2SchemaPackageRoot
{
    FString PackageId;
    FString RootDirectory;
};

/**
 * UPP-27: resolves a `ui_field`/`ui_value` schema_id to its compiled
 * FCompiledUiFieldSpec by scanning `*.schema.json5` files under a set of
 * content package roots (each tagged with its owning package_id). Discovery
 * and compilation are both lazy and memoized: the first GetCompiledSchema()
 * call parses every schema file once (self-identifying by their own `id`
 * field, not by a manifest binding -- ui_field/ui_value schemas are addressed
 * by schema_id from content, never bound to a DefinitionType the way
 * RepositoryBuilder's FSchemaRegistry binds them; DCA-19 adds the one check a
 * bare `id` field can't provide on its own -- namespace must match the
 * package root the file was found under), then every call after that is a
 * cache hit or a single CompileUiFieldSpec (cross-schema `schema_ref`
 * resolves through the same in-memory resolver built during discovery).
 * Thread-unsafe by design: used only from the game thread, exactly like the
 * FRuntimeSession/FGV2SessionCoordinator it serves.
 */
class FGV2UiSchemaCache
{
public:
    explicit FGV2UiSchemaCache(TArray<FGV2SchemaPackageRoot> InPackageRoots);

    GV2ContentCore::FCompiledUiFieldSpecPtr GetCompiledSchema(
        const std::string& SchemaId,
        FString& OutError) const;

private:
    void EnsureDiscovered() const;

    TArray<FGV2SchemaPackageRoot> PackageRoots;
    mutable bool bDiscovered = false;
    mutable GV2ContentCore::FInMemoryUiSchemaResolver Resolver;
    mutable std::map<std::string, GV2ContentCore::FCompiledUiFieldSpecPtr, std::less<>> CompiledCache;
};
