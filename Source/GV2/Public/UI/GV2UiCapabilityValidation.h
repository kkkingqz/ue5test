#pragma once

#include "CoreMinimal.h"
#include "GV2ContentCore/UiSchema.h"
#include "UI/GV2UiCapability.h"

struct GV2_API FGV2UiSchemaCompatibilityDiagnostic
{
    FString Code;
    FString PropertyPath;
    FString Message;
    FString SchemaId;

    FString ToString() const
    {
        return FString::Printf(TEXT("[%s] %s at '%s' (schema: %s)"),
            *Code, *Message, *PropertyPath, *SchemaId);
    }
};

/**
 * GBH-08: why a Required capability failed to fit inside a Provided one, distinguishing
 * "not found at all" (KindMismatch) from every dimension that can independently make an
 * otherwise-same-kind capability too wide. Callers map this to their own diagnostic code
 * namespace (schema<->widget uses `core:diagnostic.ui_capability.*`, declaration<->child
 * uses `core:diagnostic.ui_consumer.*`) -- the *rule* is shared, the reported code is not,
 * since the two callers were already distinct, pre-existing diagnostic vocabularies and
 * unifying those too would be a second, unrelated migration.
 */
enum class EGV2UiCapabilitySubsetMismatch : uint8
{
    None,
    KindMismatch,
    TargetKindMismatch,
    IntRangeMismatch,
    NumberRangeMismatch,
    KeyedIdentityMismatch,
    KeyPropertyMismatch,
    EntryWidgetClassMismatch,
    ItemMismatch,
};

/**
 * GBH-08: the one subset-compatibility rule used by both schema<->Widget compatibility
 * (CheckUiSchemaCapabilityCompatibility, after projecting each schema field into this same
 * descriptor shape) and DeclaredComposite<->child compatibility (after
 * ResolveDelegatedChildCapability picks which child capability is meant) -- replacing what
 * would otherwise be two independent implementations of "does Required fit inside
 * Provided" that could silently drift apart. Compares, in order: SupportedKind; TargetKind
 * (only when both sides declare one); IntMin/IntMax and NumberMin/NumberMax (Required's
 * claimed range must fit entirely within Provided's, exactly like
 * `SchemaContract ⊆ WidgetCapabilities`'s existing numeric rule -- Provided leaving a bound
 * unset means unconstrained on that side, not "reject everything"); bRequiresKeyedIdentity
 * (Provided requiring it means Required must also declare it); and, recursively, one level
 * of ItemCapability if both sides carry one (a schema's own further-nested Object item
 * fields are still walked by CheckUiSchemaCapabilityCompatibility's own recursion, not by
 * this function -- this only compares the two capability descriptors at the leaf, not an
 * arbitrarily deep schema field tree). Required wider than Provided fails at the first
 * dimension that does not fit, populating OutMismatch and a human OutDetail; Required
 * narrower than or equal to Provided succeeds.
 */
GV2_API bool IsUiCapabilitySubset(
    const FGV2UiPropertyCapability& Required,
    const FGV2UiPropertyCapability& Provided,
    EGV2UiCapabilitySubsetMismatch& OutMismatch,
    FString& OutDetail);

/**
 * Validates that SchemaContract is a subset of WidgetCapabilities: SchemaContract ⊆ WidgetCapabilities.
 * Performs recursive verification across kinds, target_kinds, ranges, and nested structures.
 */
GV2_API bool CheckUiSchemaCapabilityCompatibility(
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FGV2UiCapabilityTree& Capabilities,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics);

/**
 * GBH-06: resolves which single entry of ChildCapabilities a delegating declaration of
 * DeclaredKind refers to, replacing DoesCapabilityTreeSupportKind's "any capability of
 * this kind" scan with an actual disambiguation. If ChildCapabilityName is non-empty,
 * resolves by that exact PropertyName and requires its SupportedKind to match -- unknown
 * name or kind mismatch fails. If empty, scans for DeclaredKind: exactly one candidate
 * resolves (matching prior behavior when there was never any ambiguity to begin with);
 * Two or more candidates try FallbackNameHint next -- typically the delegating
 * declaration's own top-level PropertyName, since content authored before GBH-06 existed
 * sometimes already names its property the same as the child capability it means (e.g. a
 * composite's own "default_tab_key" property targeting a child that itself declares a
 * "default_tab_key" capability among others of the same kind) -- so pre-existing content
 * is not forced through a migration pass only to re-state what its own naming already
 * expressed. Only when FallbackNameHint also fails to resolve uniquely is this rejected
 * as genuinely ambiguous, demanding an explicit ChildCapabilityName. Returns the resolved
 * capability via OutResolved (nullptr on failure), always sets OutError describing why on
 * failure, and sets bOutAmbiguous so the caller can report a distinct diagnostic code for
 * "ambiguous" versus "not found"/"wrong kind".
 */
GV2_API bool ResolveDelegatedChildCapability(
    const FGV2UiCapabilityTree& ChildCapabilities,
    EGV2PreparedUiValueKind DeclaredKind,
    const FString& ChildCapabilityName,
    const FGV2UiPropertyCapability*& OutResolved,
    FString& OutError,
    bool& bOutAmbiguous,
    const FString& FallbackNameHint = FString());
