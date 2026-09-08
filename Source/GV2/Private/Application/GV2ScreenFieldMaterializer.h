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
// PAH-04A (ADR-0006, ADR-0042 INV-P1/INV-P2): the schema cache used by every
// function below is session-scoped, not process-lifetime. FGV2SessionCoordinator
// calls this once per StartSession, with the exact same resolved package roots
// (BootstrapAndSessionLifecycle.md: "Одни и те же resolved package roots обязаны
// использоваться и для repository build, и для загрузки package Lua sources" --
// this extends that invariant to schemas) used to build this session's repository
// and load its Lua sources, before Ready. Discovery happens synchronously inside
// this call (FGV2UiSchemaCache's constructor); nothing here reads the filesystem
// again afterward.
void RebuildSchemaCacheForSession(TArray<FGV2SchemaPackageRoot> PackageRoots);

// Releases the session-scoped schema cache: called on EndSession and on a failed
// StartSession, so no compiled schema or parsed document survives past the
// session that owns it. Every function below reports "no schema cache" until
// RebuildSchemaCacheForSession runs again.
void ReleaseSchemaCacheForSession();

bool PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions);

// Materializes every field's raw Lua value into a FGV2PreparedUiObject candidate
// (plus its compiled schema) using the resolved Handles from a prior
// PrepareBindingDefinitions() + FGV2UiBindingRegistry::PrepareBindings() pass --
// Handles must be in the same order PrepareBindingDefinitions produced them in,
// since both passes walk the same schema/value tree in the same deterministic order.
// PSC-10A: PrepareContext is optional so the one existing production call site (and
// every test call site) keeps compiling unchanged; passing one routes Text field
// resolution through PrepareContext->GetTheme() instead of the legacy
// GetConfiguredTheme() static accessor (see UGV2TextPipeline::Resolve()'s own doc
// comment) and populates the resolved presentation UGV2TextPipeline::Apply() then
// reads without touching Theme again.
bool BuildFields(
    const GV2RuntimeCore::FScreenRequest& Request,
    const TArray<FGV2UiBindingHandle>& Handles,
    TArray<FGV2ScreenFieldValue>& OutFields,
    const FGV2PresentationPrepareContext* PrepareContext = nullptr);

// DUC-09: lets a nested-screen-fields consumer re-resolve one envelope's compiled
// schema by schema_id after ProjectMaterializedValue already validated it -- a cache
// hit against the same session-scoped cache, needed only to fill
// FGV2ScreenFieldValue::CompiledSchema.
std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> GetCompiledSchema(
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
