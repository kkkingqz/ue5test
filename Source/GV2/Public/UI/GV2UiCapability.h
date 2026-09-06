#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "GV2ContentCore/UiSchema.h"

enum class EGV2UiCapabilityTargetType : uint8
{
    RendererControl,
    CollectionHost,
    NestedScreen,
    CustomControl
};

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

struct FGV2UiCapabilityTree;

/**
 * Descriptor of a single property capability supported by a widget.
 * Explicitly binds a property name to a target renderer control, collection host, or sub-screen.
 */
struct GV2_API FGV2UiPropertyCapability
{
    FString PropertyName;
    EGV2PreparedUiValueKind SupportedKind = EGV2PreparedUiValueKind::Null;
    EGV2UiCapabilityTargetType TargetType = EGV2UiCapabilityTargetType::RendererControl;
    FName TargetName = NAME_None;

    // GBH-06: which of TargetName's own DescribeUiCapabilities entries this delegating
    // declaration addresses, by that capability's own PropertyName. Empty means "resolve
    // by kind alone" (DUC-07's original behavior), valid only while TargetName declares
    // exactly one capability of SupportedKind -- two or more is ambiguous and rejected,
    // not silently matched to whichever one iteration happens to find first.
    FString ChildCapabilityName;

    // Semantic constraints
    FString TargetKind; // For Ref/StableId, e.g. "resource", "item", "screen"
    TOptional<int64> IntMin;
    TOptional<int64> IntMax;
    TOptional<double> NumberMin;
    TOptional<double> NumberMax;

    // Collection policy
    bool bRequiresKeyedIdentity = false;
    FString KeyPropertyName = TEXT("key");
    TSubclassOf<UUserWidget> EntryWidgetClass = nullptr;

    // Nested structures
    TSharedPtr<FGV2UiCapabilityTree> ChildTree;
    TSharedPtr<FGV2UiPropertyCapability> ItemCapability;

    bool operator==(const FGV2UiPropertyCapability& Other) const;
    bool operator!=(const FGV2UiPropertyCapability& Other) const { return !(*this == Other); }
};

/**
 * Tree of property capabilities declared by a widget class or instance.
 */
struct GV2_API FGV2UiCapabilityTree
{
    TMap<FString, FGV2UiPropertyCapability> Properties;

    const FGV2UiPropertyCapability* FindProperty(const FString& InName) const
    {
        return Properties.Find(InName);
    }

    bool HasProperty(const FString& InName) const
    {
        return Properties.Contains(InName);
    }

    int32 Num() const { return Properties.Num(); }
    bool IsEmpty() const { return Properties.IsEmpty(); }
};

/**
 * Fluent builder for declaring capability trees without manual struct boilerplate.
 */
class GV2_API FGV2UiCapabilityBuilder
{
public:
    FGV2UiCapabilityBuilder() = default;

    FGV2UiCapabilityBuilder& AddText(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddImage(const FString& Name, const FName& TargetName, const FString& TargetKind = TEXT("resource"));
    FGV2UiCapabilityBuilder& AddBoolean(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddInteger(const FString& Name, const FName& TargetName, TOptional<int64> Min = {}, TOptional<int64> Max = {});
    FGV2UiCapabilityBuilder& AddNumber(const FString& Name, const FName& TargetName, TOptional<double> Min = {}, TOptional<double> Max = {});
    FGV2UiCapabilityBuilder& AddString(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddKey(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddBinding(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddKeyedCollection(const FString& Name, const FName& TargetName, FGV2UiPropertyCapability ItemCapability, const FString& KeyField = TEXT("key"), TSubclassOf<UUserWidget> EntryWidgetClass = nullptr);
    FGV2UiCapabilityBuilder& AddKeyedCollection(const FString& Name, const FName& TargetName, FGV2UiCapabilityTree ItemCapabilityTree, const FString& KeyField = TEXT("key"), TSubclassOf<UUserWidget> EntryWidgetClass = nullptr);
    FGV2UiCapabilityBuilder& AddNestedScreenCollection(const FString& Name, const FName& TargetName, const FString& KeyField = TEXT("key"));
    FGV2UiCapabilityBuilder& AddCustom(const FString& Name, EGV2PreparedUiValueKind Kind, EGV2UiCapabilityTargetType TargetType, const FName& TargetName);

    // GBH-06: sets ChildCapabilityName on an already-added property (by Name). Called
    // after the matching AddXxx above rather than added as a parameter to each of them,
    // so existing call sites that do not delegate to a named child capability are
    // untouched.
    FGV2UiCapabilityBuilder& SetChildCapabilityName(const FString& Name, const FString& ChildCapabilityName);

    FGV2UiCapabilityTree Build() const { return Tree; }

private:
    FGV2UiCapabilityTree Tree;
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
