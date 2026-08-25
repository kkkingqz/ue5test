#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "UI/GV2UiSchemaCache.h"

#include <string_view>

// UPP-27: every Screen Field is now applied through the schema-driven pipeline
// below -- there is no per-schema adapter left (Num() == 0 permanently; see
// GV2.Runtime.Presentation.CentralPresentationPathSourceAudit). PrepareBindingDefinitions
// and BuildFields both walk the field's *compiled* schema (FGV2UiSchemaCache) against
// its raw Lua value, so a field is known iff a *.schema.json5 file declares that
// schema_id -- not because this class hardcodes it.
class FGV2ScreenFieldAdapterRegistry
{
public:
    static const FGV2ScreenFieldAdapterRegistry& Get();

    bool PrepareBindingDefinitions(
        const GV2RuntimeCore::FScreenRequest& Request,
        TArray<FGV2UiBindingDefinition>& OutDefinitions) const;

    // Materializes every field's raw Lua value into a FGV2PreparedUiObject candidate
    // (plus its compiled schema) using the resolved Handles from a prior
    // PrepareBindingDefinitions() + FGV2UiBindingRegistry::PrepareBindings() pass --
    // Handles must be in the same order PrepareBindingDefinitions produced them in,
    // since both passes walk the same schema/value tree in the same deterministic order.
    bool BuildFields(
        const GV2RuntimeCore::FScreenRequest& Request,
        const TArray<FGV2UiBindingHandle>& Handles,
        TArray<FGV2ScreenFieldValue>& OutFields) const;

    bool IsKnownSchema(const std::string& SchemaId) const;

    // Retained only so the still-meaningful "0 legacy adapters" regression
    // (GV2.Runtime.Presentation.CentralPresentationPathSourceAudit) keeps compiling.
    int32 Num() const { return 0; }

private:
    FGV2ScreenFieldAdapterRegistry();

    FGV2UiSchemaCache SchemaCache;
};
