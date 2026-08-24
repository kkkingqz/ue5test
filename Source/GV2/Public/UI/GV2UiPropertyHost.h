#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PreparedUiValue.h"
#include "GV2UiPropertyHost.generated.h"

UINTERFACE(MinimalAPI)
class UGV2UiPropertyHost : public UInterface
{
    GENERATED_BODY()
};

/**
 * Shared state and helper for any widget implementing IGV2UiPropertyHost.
 * Provides capability caching, schema validation, and last committed property tracking.
 */
class GV2_API FGV2UiPropertyHostState
{
public:
    FGV2UiPropertyHostState() = default;

    const FGV2UiCapabilityTree& GetCapabilities() const { return CachedCapabilities; }
    void SetCapabilities(FGV2UiCapabilityTree InCapabilities) { CachedCapabilities = MoveTemp(InCapabilities); }

    bool ValidateSchemaCompatibility(
        const GV2ContentCore::FCompiledUiFieldSpec& Schema,
        const FString& SchemaId,
        TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics) const
    {
        return CheckUiSchemaCapabilityCompatibility(Schema, CachedCapabilities, SchemaId, TEXT(""), OutDiagnostics);
    }

    const FGV2PreparedUiObject& GetLastCommittedProperties() const { return LastCommittedProperties; }
    void SetLastCommittedProperties(FGV2PreparedUiObject InProperties) { LastCommittedProperties = MoveTemp(InProperties); }

    const TArray<FGV2UiSchemaCompatibilityDiagnostic>& GetLastDiagnostics() const { return LastDiagnostics; }
    void SetLastDiagnostics(TArray<FGV2UiSchemaCompatibilityDiagnostic> InDiagnostics) { LastDiagnostics = MoveTemp(InDiagnostics); }

private:
    FGV2UiCapabilityTree CachedCapabilities;
    FGV2PreparedUiObject LastCommittedProperties;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> LastDiagnostics;
};

/**
 * Native interface implemented by any widget supporting data-driven UI property binding.
 */
class GV2_API IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    /** Describe the capability tree of this widget */
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const = 0;

    /** Access shared host state */
    virtual FGV2UiPropertyHostState& GetPropertyHostState() = 0;
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const = 0;
};
