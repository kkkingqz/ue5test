#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiSchemaCache.h"

#include <string_view>

class FGV2PresentationPrepareContext;

// UPP-30: schema-driven materialization for Screen Fields, as free functions --
// replaces the deleted FGV2ScreenFieldAdapterRegistry class. That class had held
// zero per-schema adapters since UPP-25 (the last one was deleted); the "Registry"
// framing itself was the only thing still standing in for a scheme that no longer
// exists, so UPP-30 removes the class/singleton wrapper along with it. Both
// functions below walk a field's *compiled* schema (resolved on demand, cached
// internally) against its raw Lua value, so a field is known iff some
// *.schema.json5 file declares that schema_id -- nothing here hardcodes a list.
namespace GV2ScreenFieldMaterializer
{
// CFC-04 (ADR-0043 D1, BootstrapAndSessionLifecycle.md): the snapshot is the sole
// UI schema authority. All entry points below require an explicit pinned
// FGV2PresentationPrepareContext reference and resolve schemas through its cache;
// no process-global or ambient schema cache exists.
bool PrepareBindingDefinitions(
    const FGV2PresentationPrepareContext& PrepareContext,
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions);

// Materializes every field's raw Lua value into a FGV2PreparedUiObject candidate
// (plus its compiled schema) using the resolved Handles from a prior
// PrepareBindingDefinitions() + FGV2UiBindingRegistry::PrepareBindings() pass --
// Handles must be in the same order PrepareBindingDefinitions produced them in,
// since both passes walk the same schema/value tree in the same deterministic order.
// PrepareContext routes semantic values and schemas through the pinned session snapshot.
bool BuildFields(
    const FGV2PresentationPrepareContext& PrepareContext,
    const GV2RuntimeCore::FScreenRequest& Request,
    const TArray<FGV2UiBindingHandle>& Handles,
    TArray<FGV2ScreenFieldValue>& OutFields);

// DUC-09 / CFC-04: lets a nested-screen-fields consumer re-resolve one envelope's compiled
// schema by schema_id after ProjectMaterializedValue already validated it -- a cache
// hit against the snapshot-scoped cache, needed to fill FGV2ScreenFieldValue::CompiledSchema.
std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> GetCompiledSchema(
    const FGV2PresentationPrepareContext& PrepareContext,
    const std::string& SchemaId,
    FString& OutError);

// Exposed for PCC-03 verification: the same raw-materialized-value -> prepared-value
// projection BuildFields() uses internally, callable directly against a hand-built
// schema/value so a test can drive the real production projection end to end
// without requiring a file-backed schema under GameData/.
struct FMaterializeContext
{
    const TArray<FGV2UiBindingHandle>* Handles = nullptr;
    int32* HandleCursor = nullptr;
    // PSC-10A: optional, threaded through to ResolveText -> UGV2TextPipeline::Resolve().
    const FGV2PresentationPrepareContext* PrepareContext = nullptr;
};

bool ProjectMaterializedValue(
    FMaterializeContext& Ctx,
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    const GV2ContentCore::FValue& MaterializedValue,
    FGV2PreparedUiValue& OutValue);
}
