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
    const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec>& GetLastCommittedSchema() const { return LastCommittedSchema; }
    const FString& GetLastCommittedSchemaId() const { return LastCommittedSchemaId; }

    struct FCommittedSnapshot
    {
        FGV2PreparedUiObject Properties;
        std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> Schema;
        FString SchemaId;
    };

    FCommittedSnapshot GetCommittedSnapshot() const
    {
        return { LastCommittedProperties, LastCommittedSchema, LastCommittedSchemaId };
    }

    void RestoreCommittedSnapshot(FCommittedSnapshot InSnapshot)
    {
        LastCommittedProperties = MoveTemp(InSnapshot.Properties);
        LastCommittedSchema = MoveTemp(InSnapshot.Schema);
        LastCommittedSchemaId = MoveTemp(InSnapshot.SchemaId);
    }

    // GBF-04 (ADR-0041): a rollback is built from the exact state that was committed,
    // so the value alone is not a sufficient snapshot when a field changes schema.
    void SetLastCommittedSnapshot(
        FGV2PreparedUiObject InProperties,
        std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> InSchema,
        FString InSchemaId)
    {
        LastCommittedProperties = MoveTemp(InProperties);
        LastCommittedSchema = MoveTemp(InSchema);
        LastCommittedSchemaId = MoveTemp(InSchemaId);
    }

    // Compatibility helper for tests and legacy direct callers that have no compiled
    // schema. Such a non-empty value deliberately cannot later serve as a rollback
    // snapshot: Prepare rejects it with core:diagnostic.ui_rollback.missing_committed_schema.
    void SetLastCommittedProperties(FGV2PreparedUiObject InProperties)
    {
        LastCommittedProperties = MoveTemp(InProperties);
        LastCommittedSchema.reset();
        LastCommittedSchemaId.Reset();
    }

    const TArray<FGV2UiSchemaCompatibilityDiagnostic>& GetLastDiagnostics() const { return LastDiagnostics; }
    void SetLastDiagnostics(TArray<FGV2UiSchemaCompatibilityDiagnostic> InDiagnostics) { LastDiagnostics = MoveTemp(InDiagnostics); }

    FName GetHostIdentity() const { return HostIdentity; }
    void SetHostIdentity(FName InHostIdentity) { HostIdentity = InHostIdentity; }

    FName GetKey() const { return Key; }
    void SetKey(FName InKey) { Key = InKey; }

private:
    // DUC-01: this host's identity within its enclosing host -- Designer-authored,
    // one meaning, not per-class. See the class comment above.
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (DisplayName = "Host Identity"))
    FName HostIdentity;

    // DUC-03: runtime-assigned local identity key (e.g. a collection item's key),
    // applied by FGV2KeyPropertyConsumer -- not Designer-authored, unlike HostIdentity
    // above. One declaration here instead of an identical private FName Key member
    // repeated on every class that used to hand-roll its own SetKey/GetKey.
    UPROPERTY(Transient)
    FName Key;

    FGV2UiCapabilityTree CachedCapabilities;
    FGV2PreparedUiObject LastCommittedProperties;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> LastCommittedSchema;
    FString LastCommittedSchemaId;
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

    /**
     * DUC-03: local identity key (e.g. a collection item's key), shared the same way as
     * HostIdentity above. FGV2KeyPropertyConsumer::Commit/Reset call this directly through
     * IGV2UiPropertyHost -- a new host declaring a `key` capability works without any edit
     * to the consumer, as long as it implements this interface (which any IGV2UiPropertyHost
     * already does).
     */
    FName GetKey() const { return GetPropertyHostState().GetKey(); }
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }
};
