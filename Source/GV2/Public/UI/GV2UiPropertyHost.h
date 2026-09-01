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
 * Provides capability caching, schema validation, last committed property
 * tracking, and this host's identity within its enclosing host (DUC-01).
 *
 * A UCLASS embedding this as `UPROPERTY() FGV2UiPropertyHostState PropertyHostState;`
 * gets HostIdentity exposed in Designer for free, on every placement, with no
 * per-class property declaration -- the meaning of that one value depends on
 * where this host sits: at screen level it is the Screen Field's `field_id`
 * (matched by `UGV2ScreenWidgetBase`'s `IGV2ScreenFieldHost` tree walk), at
 * composite level (DUC-05+) it is the property name a parent composite's flat
 * capability list addresses this child by. Screen and composite differ only in
 * whether the value is checked against the permission matrix, not in what the
 * value means, so introducing two separate identity properties would just give
 * an asset author two ways to say the same thing and no way to know which one
 * is live for a given placement.
 */
USTRUCT()
struct GV2_API FGV2UiPropertyHostState
{
    GENERATED_BODY()

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

    FName GetHostIdentity() const { return HostIdentity; }
    void SetHostIdentity(FName InHostIdentity) { HostIdentity = InHostIdentity; }

private:
    // DUC-01: this host's identity within its enclosing host -- Designer-authored,
    // one meaning, not per-class. See the class comment above.
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (DisplayName = "Host Identity"))
    FName HostIdentity;

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

    /** DUC-01: this host's identity within its enclosing host -- see FGV2UiPropertyHostState. */
    FName GetHostIdentity() const { return GetPropertyHostState().GetHostIdentity(); }
    void SetHostIdentity(FName InHostIdentity) { GetPropertyHostState().SetHostIdentity(InHostIdentity); }
};
