#pragma once

#include "CoreMinimal.h"
#include "GV2ContentCore/UiSchema.h"

#include <map>
#include <string>

/**
 * UPP-27: resolves a `ui_field`/`ui_value` schema_id to its compiled
 * FCompiledUiFieldSpec by scanning `*.schema.json5` files under a fixed set of
 * content package roots. Discovery and compilation are both lazy and
 * memoized: the first GetCompiledSchema() call parses every schema file once
 * (self-identifying by their own `id` field, not by a manifest binding -- ui_field/
 * ui_value schemas are addressed by schema_id from content, never bound to a
 * DefinitionType the way RepositoryBuilder's FSchemaRegistry binds them), then
 * every call after that is a cache hit or a single CompileUiFieldSpec (cross-
 * schema `schema_ref` resolves through the same in-memory resolver built during
 * discovery). Thread-unsafe by design: used only from the game thread, exactly
 * like the FRuntimeSession/FGV2SessionCoordinator it serves.
 */
class FGV2UiSchemaCache
{
public:
    explicit FGV2UiSchemaCache(TArray<FString> InPackageRoots);

    GV2ContentCore::FCompiledUiFieldSpecPtr GetCompiledSchema(
        const std::string& SchemaId,
        FString& OutError) const;

private:
    void EnsureDiscovered() const;

    TArray<FString> PackageRoots;
    mutable bool bDiscovered = false;
    mutable GV2ContentCore::FInMemoryUiSchemaResolver Resolver;
    mutable std::map<std::string, GV2ContentCore::FCompiledUiFieldSpecPtr, std::less<>> CompiledCache;
};
