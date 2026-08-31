#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "UI/GV2PreparedUiValue.h"

#include <string_view>

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
bool PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions);

// Materializes every field's raw Lua value into a FGV2PreparedUiObject candidate
// (plus its compiled schema) using the resolved Handles from a prior
// PrepareBindingDefinitions() + FGV2UiBindingRegistry::PrepareBindings() pass --
// Handles must be in the same order PrepareBindingDefinitions produced them in,
// since both passes walk the same schema/value tree in the same deterministic order.
bool BuildFields(
    const GV2RuntimeCore::FScreenRequest& Request,
    const TArray<FGV2UiBindingHandle>& Handles,
    TArray<FGV2ScreenFieldValue>& OutFields);

bool IsKnownSchema(const std::string& SchemaId);

// Exposed for PCC-03 verification: the same raw-materialized-value -> prepared-value
// projection BuildFields() uses internally, callable directly against a hand-built
// schema/value so a test can drive the real production projection end to end
// without requiring a file-backed schema under GameData/.
struct FMaterializeContext
{
    const TArray<FGV2UiBindingHandle>* Handles = nullptr;
    int32* HandleCursor = nullptr;
};

bool ProjectMaterializedValue(
    FMaterializeContext& Ctx,
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    const GV2ContentCore::FValue& MaterializedValue,
    FGV2PreparedUiValue& OutValue);
}
