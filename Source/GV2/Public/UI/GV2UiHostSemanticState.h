#pragma once

#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapabilityValidation.h"
#include "UI/GV2UiPropertyHost.h"

struct GV2_API FGV2UiHostCommittedSnapshot
{
    FGV2PreparedUiObject Properties;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> Schema;
    FString SchemaId;
};

class GV2_API FGV2UiHostSemanticState final : public FGV2UiHostStateExtension
{
public:
    const FGV2PreparedUiObject& GetLastCommittedProperties() const { return LastCommittedProperties; }
    const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec>& GetLastCommittedSchema() const { return LastCommittedSchema; }
    const FString& GetLastCommittedSchemaId() const { return LastCommittedSchemaId; }

    FGV2UiHostCommittedSnapshot GetCommittedSnapshot() const
    {
        return {LastCommittedProperties, LastCommittedSchema, LastCommittedSchemaId};
    }

    void RestoreCommittedSnapshot(FGV2UiHostCommittedSnapshot InSnapshot)
    {
        LastCommittedProperties = MoveTemp(InSnapshot.Properties);
        LastCommittedSchema = MoveTemp(InSnapshot.Schema);
        LastCommittedSchemaId = MoveTemp(InSnapshot.SchemaId);
    }

    void SetLastCommittedSnapshot(
        FGV2PreparedUiObject InProperties,
        std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> InSchema,
        FString InSchemaId)
    {
        LastCommittedProperties = MoveTemp(InProperties);
        LastCommittedSchema = MoveTemp(InSchema);
        LastCommittedSchemaId = MoveTemp(InSchemaId);
    }

    void SetLastCommittedProperties(FGV2PreparedUiObject InProperties)
    {
        LastCommittedProperties = MoveTemp(InProperties);
        LastCommittedSchema.reset();
        LastCommittedSchemaId.Reset();
    }

    const TArray<FGV2UiSchemaCompatibilityDiagnostic>& GetLastDiagnostics() const { return LastDiagnostics; }
    void SetLastDiagnostics(TArray<FGV2UiSchemaCompatibilityDiagnostic> InDiagnostics)
    {
        LastDiagnostics = MoveTemp(InDiagnostics);
    }

private:
    FGV2PreparedUiObject LastCommittedProperties;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> LastCommittedSchema;
    FString LastCommittedSchemaId;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> LastDiagnostics;
};

GV2_API FGV2UiHostSemanticState& GetUiHostSemanticState(FGV2UiPropertyHostState& State);
GV2_API const FGV2UiHostSemanticState& GetUiHostSemanticState(const FGV2UiPropertyHostState& State);

GV2_API bool IsHostClaimedKeyCapability(FName PropertyName);
