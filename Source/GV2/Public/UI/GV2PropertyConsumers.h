#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "UObject/StrongObjectPtr.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiHostSemanticState.h"

class UWidget;
class UGV2ScreenWidgetBase;
class FGV2PresentationPrepareContext;
class FGV2UiHostMutationPlan;
struct FGV2ScreenMutationPlan;

/**
 * Base interface for all reusable property consumers.
 * Encapsulates preflight, prepare, commit, and reset phases for a specific property kind and target.
 */
class GV2_API IGV2PropertyConsumer
{
public:
    virtual ~IGV2PropertyConsumer() = default;

    virtual EGV2PreparedUiValueKind GetSupportedKind() const = 0;
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const = 0;

    /**
     * Fallible prepare phase.
     * Verifies target presence, validates formats, resolves assets/styles/markup.
     * Guaranteed to NOT mutate physical widget state.
     */
    virtual bool Prepare(
        const FGV2PreparedUiValue& Value,
        const FGV2UiPropertyCapability& Capability,
        UWidget* TargetWidget,
        FString& OutError) = 0;

    // PSC-06 (ADR-0043 D1): injected unconditionally by PrepareUiHostProperties right
    // after this consumer is created, for every consumer kind -- default no-op, since
    // most consumers resolve nothing session-scoped. A consumer that does (NestedScreen,
    // resource ids) overrides this and stores the pointer for its own Prepare() to use;
    // null is accepted only by context-free test/standalone paths whose value kinds need no
    // session authority. Consumers that resolve content fail Prepare when it is absent.
    virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) {}

    // PSC-09A/09B (ADR-0043 D2/D3, Payload.md M3): builds this consumer's whole physical
    // effect as prepared operations, appended to OutTransaction. A consumer that overrides
    // this and returns true performs NO physical mutation of its own -- the reverse (a lower
    // operation calling back up into this consumer, or into any GV2 resolver) is exactly the
    // shape ADR-0043 D2 forbids and is never a valid override.
    //
    // The default returns false, which now means only that the consumer's effect is not
    // expressible from (target, prepared value) alone: the two recursive consumers
    // (FGV2KeyedCollectionPropertyConsumer, FGV2TabContainerTabsPropertyConsumer) commit
    // their children first and build their own transaction inside Commit() from the result.
    // Both still apply it through the same single facade, which is what
    // Tools/Testing/validate_property_consumer_transaction_coverage.py checks for the
    // consumer set derived from this header's own declarations -- so `false` here is never
    // "still applies physically by itself".
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const
    {
        return false;
    }

    /**
     * Infallible commit phase.
     * Applies already prepared state to the target widget.
     */
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) = 0;

    // PCC-06 / DUC-10: Commit fault injection is a test-only traversal hook.
    // Most consumers have no nested commit and keep the ordinary Commit behavior;
    // nested consumers override it to forward a fully scoped property path.
    virtual bool CommitWithFailureInjector(
        UWidget* TargetWidget,
        FString& OutError,
        const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
        const FString& PropertyPath)
    {
        return Commit(TargetWidget, OutError);
    }

    /**
     * Reset phase: restores default state when property is missing without default.
     */
    virtual void Reset(UWidget* TargetWidget) = 0;
};

/**
 * Text consumer: applies localized TextSpec / TextViewModel strictly via UGV2TextPipeline.
 * Direct SetText calls on target widgets are prohibited.
 */
class GV2_API FGV2TextPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Text; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/11 (ADR-0043 D2/D3): carries the resolved text/style/markup only. The
    // per-target dispatch this operation used to perform directly in Commit() is now the
    // Apply facade's, which reaches a GV2-owned text host through IGV2PreparedTextTarget
    // and writes a plain CommonUI renderer itself.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    FGV2TextViewModel PreparedText;
};

#include "UI/GV2ImageResourceCatalog.h"

/**
 * Image resource consumer: applies StableId(resource) strictly via FGV2ImagePresentation.
 * Direct SetBrush / SetBrushFromTexture calls on target widgets are prohibited.
 */
class GV2_API FGV2ImageResourcePropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::StableId; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/10B (ADR-0043 D2/D3): Prepare has already validated the resource and
    // scaling constraints. This emits the finished value-only operation for every
    // supported target shape; Commit never re-resolves Theme or catalog state.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

    // PSC-10B (ADR-0043 D1): Prepare resolves through the pinned session snapshot;
    // missing context is a typed failure rather than a global-catalog fallback.
    virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) override { PrepareContext = InContext; }

private:
    const FGV2PresentationPrepareContext* PrepareContext = nullptr;
    // STATUS-012: Prepare resolves the resource and keeps the RESULT, not just the
    // id. Commit applies this; it never asks the catalog again, so what is applied
    // is what preparation validated (ADR-0042, INV-P5).
    FGV2ResolvedImageResource PreparedResource;
    FString PreparedResourceId;
    EGV2PrimitiveScalePolicy PreparedScalePolicy = EGV2PrimitiveScalePolicy::Unset;
    TOptional<float> PreparedFixedAspectRatio;
};

/**
 * Boolean consumer: applies boolean flag (e.g. SetIsEnabled / visibility).
 */
class GV2_API FGV2BooleanPropertyConsumer : public IGV2PropertyConsumer
{
public:
    FGV2BooleanPropertyConsumer(const FString& InPropertyName = FString()) : PropertyName(InPropertyName) {}
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Boolean; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B (ADR-0043 D2/D3): builds a FPreparedBooleanOperation already narrowed to
    // exactly which setter Commit()/Reset() would have called. See GV2PropertyConsumers.cpp
    // for the exact target/property-name -> EPreparedBooleanTarget mapping.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    bool bPreparedValue = false;
    FString PropertyName;
};

/**
 * Integer consumer: applies bounded integer value.
 */
class GV2_API FGV2IntegerPropertyConsumer : public IGV2PropertyConsumer
{
public:
    FGV2IntegerPropertyConsumer(const FString& InPropertyName = FString()) : PropertyName(InPropertyName) {}
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Integer; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/11 (ADR-0043 D2/D3): carries the resolved value only -- the target host
    // (UGV2InputFieldWidgetBase) receives it through IGV2PreparedIntegerTarget, so the
    // Apply facade performs the mutation without naming that class.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    int64 PreparedValue = 0;
    FString PropertyName;
};

/**
 * Number consumer: applies numeric value (e.g. percent to UProgressBar).
 */
class GV2_API FGV2NumberPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Number; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/11 (ADR-0043 D2/D3): UProgressBar is a plain UMG target -- GV2PresentationApply
    // applies a FPreparedProgressBarOperation for it directly. UGV2ProgressBarWidgetBase
    // is GV2-owned, so that case builds a FPreparedNumberOperation instead, delivered
    // through IGV2PreparedNumberTarget.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    double PreparedValue = 0.0;
};

/**
 * String consumer: applies string value.
 */
class GV2_API FGV2StringPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::String; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/11 (ADR-0043 D2/D3): carries the resolved value only. The truncation limit is
    // read back from the host through IGV2PreparedIntegerTarget::GetPreparedMaxLength, so
    // the facade applies it without naming UGV2InputFieldWidgetBase.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    FString PreparedValue;
};

/**
 * Key consumer: applies local identity key without coercion.
 *
 * DUC-03: routes by Capability.PropertyName, not by TargetWidget type -- "selected_key"
 * (UGV2DropdownSelectWidgetBase) and "default_tab_key" (UGV2TabContainerWidgetBase) are
 * semantically distinct capabilities that happen to also use the Key value kind; every
 * other property name is the generic `key` identity, applied through IGV2UiPropertyHost's
 * shared GetKey()/SetKey() (GV2UiPropertyHost.h) with no per-class branch. A new host
 * declaring `key` therefore needs no edit here at all, as long as it implements
 * IGV2UiPropertyHost -- which any host capable of declaring a capability already does.
 */
class GV2_API FGV2KeyPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Key; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    /**
     * A reset mutation's consumer never goes through Prepare() (GV2UiMutationPlan.cpp
     * creates it and marks it bIsReset without preparing a value), so the routing name has
     * to be injected directly by the caller right after FGV2PropertyConsumerFactory::
     * CreateConsumer -- otherwise Reset() would have no way to tell "selected_key"/
     * "default_tab_key" apart from the generic `key`.
     */
    void SetPropertyNameForRouting(const FString& InPropertyName) { PropertyName = InPropertyName; }

    // PSC-09B/11 (ADR-0043 D2/D3): carries the resolved value and the capability's own
    // name only. The target routes it through IGV2PreparedKeyTarget, and a target that
    // refuses the name produces the "no branch matched" typed failure in the facade.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    FString PreparedValue;
    FString PropertyName;
};

/**
 * Binding consumer: applies FGV2UiBindingHandle.
 * Physical widget receives strictly opaque handle, never raw command ID or args.
 */
class GV2_API FGV2BindingPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Binding; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    // PSC-09B/11 (ADR-0043 D2/D3): carries the resolved handle's serialized value only.
    // IGV2UiBindingTarget derives from the lower module's IGV2PreparedBindingTarget, so the
    // facade delivers the value without naming a GV2 type.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

private:
    FGV2UiBindingHandle PreparedBinding;
};

/**
 * Diagnostic record representing a discrepancy between a compiled collection item schema
 * and an entry widget's declared capabilities (proposal Section 32, UPP-R1 / PCC-01).
 */
struct GV2_API FGV2CollectionItemDiscrepancy
{
    FString ScreenId;
    FString FieldId;
    FString SchemaId;
    FString PropertyPath;
    FString WidgetClass;
    FString Capability;
    FString Code;
    FString Message;

    FString ToSection32String() const;
    FString ToLogString() const;
};

/**
 * Keyed collection consumer: reconciles an array of objects into a collection container
 * (e.g. UGV2ListViewWidgetBase or UPanelWidget), matching elements by stable FName keys.
 * Fully atomic: prepares ALL child items and their mutation plans before Commit.
 * If any item fails Prepare, no widget is mutated or committed.
 */
class GV2_API FGV2KeyedCollectionPropertyConsumer : public IGV2PropertyConsumer
{
public:
    FGV2KeyedCollectionPropertyConsumer();
    virtual ~FGV2KeyedCollectionPropertyConsumer() override;

    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    // GBH-10/11 (ADR-0041): lets a test (or a future nested caller) inject a Commit-phase
    // failure on a specific reused item's own property, scoped as "[key].<child_path>" --
    // the same scoping FGV2TabContainerTabsPropertyConsumer already uses for its nested
    // screens. Production callers omit the injector, same as everywhere else.
    virtual bool CommitWithFailureInjector(
        UWidget* TargetWidget,
        FString& OutError,
        const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
        const FString& PropertyPath) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TMap<FName, TWeakObjectPtr<UWidget>>& GetActiveWidgetsByKey() const { return ActiveWidgetsByKey; }
    const TMap<FName, TStrongObjectPtr<UWidget>>& GetCandidateWidgetsByKey() const { return CandidateWidgetsByKey; }

    void SetCompiledItemSpec(
        GV2ContentCore::FCompiledUiFieldSpecPtr InItemSpec,
        const FString& InSchemaId = FString(),
        const FString& InPropertyPath = FString(),
        const FString& InScreenId = FString(),
        const FString& InFieldId = FString())
    {
        CompiledItemSpec = InItemSpec;
        ContextSchemaId = InSchemaId;
        ContextPropertyPath = InPropertyPath;
        ContextScreenId = InScreenId;
        ContextFieldId = InFieldId;
    }

    // Required by recursively prepared item hosts: their resource, RichText and nested
    // screen consumers must see the same immutable session snapshot as the collection.
    virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) override { PrepareContext = InContext; }

    const TArray<FGV2CollectionItemDiscrepancy>& GetDiscrepancies() const { return Discrepancies; }

    static const TArray<FGV2CollectionItemDiscrepancy>& GetAllRecordedDiscrepancies();
    static void ClearAllRecordedDiscrepancies();

private:
    struct FPreparedCollectionItem
    {
        FName Key;
        TWeakObjectPtr<UWidget> Widget;
        TSharedPtr<FGV2UiHostMutationPlan> Plan;
        bool bIsHost = false;
        // GBF-05 (ADR-0041): restore this exact accounting snapshot only after
        // the paired physical inverse succeeds. Collection commit publishes item
        // snapshots as a batch, but keeping the prior tuple with the item makes
        // the invariant explicit at this nested transaction boundary.
        FGV2UiHostCommittedSnapshot PreviousCommittedSnapshot;
        // GBH-10 (ADR-0041): set only when Widget is a REUSED entry (found in
        // ExistingWidgets during Prepare, not freshly created). RollbackPlan restores it
        // to its own previous committed value if Commit fails on a later item in the
        // same collection -- a freshly created entry was never shown, so there is
        // nothing on it to roll back.
        bool bIsReused = false;
        TSharedPtr<FGV2UiHostMutationPlan> RollbackPlan;
        // GBH-10 (ADR-0041): this item's own candidate value, captured at Prepare time so
        // Commit can record it as the item host's LastCommittedProperties once the whole
        // collection commits cleanly -- without this, a collection item's own previous
        // value is never tracked anywhere, and a later revision's RollbackPlan would have
        // nothing but an empty object to prepare against (an all-Reset plan, not an
        // actual restore).
        TSharedPtr<const FGV2PreparedUiObject> CommittedValue;
        GV2PresentationApply::FGV2PreparedPresentationTransaction CentralStyleTransaction;
    };

    FString KeyPropertyName = TEXT("key");
    TArray<FPreparedCollectionItem> PreparedItems;
    TMap<FName, TWeakObjectPtr<UWidget>> ActiveWidgetsByKey;
    TMap<FName, TStrongObjectPtr<UWidget>> CandidateWidgetsByKey;

    GV2ContentCore::FCompiledUiFieldSpecPtr CompiledItemSpec;
    FString ContextSchemaId;
    FString ContextPropertyPath;
    FString ContextScreenId;
    FString ContextFieldId;
    const FGV2PresentationPrepareContext* PrepareContext = nullptr;
    TArray<FGV2CollectionItemDiscrepancy> Discrepancies;
};

/**
 * RichText spans consumer: parses and preflights interactive spans array for UGV2RichTextWidgetBase.
 * Validates canonical span keys, binding, and each span's optional hover -- a nested screen
 * resolved and instantiated off-tree the same way FGV2TabContainerTabsPropertyConsumer resolves
 * each tab's screen (PEP-05, mirroring DUC-09/10/11). A popover opening a span's hover never
 * resolves or styles anything itself (PSC-10B): everything below is already done by the time
 * a span is prepared, independent of whether that span is ever actually hovered.
 */
class GV2_API FGV2RichTextSpansPropertyConsumer : public IGV2PropertyConsumer
{
public:
    // Per-span prepare-time state: the reflected view model plus the private nested-screen
    // bookkeeping Commit needs (child field plan, central style transaction), mirroring
    // FGV2TabContainerTabsPropertyConsumer::FPreparedTabItem's own split between what the
    // widget stores (FGV2TabItemEntry/FGV2RichTextSpanViewModel) and what only Prepare/Commit
    // need to carry between the two calls.
    struct FPreparedSpanItem
    {
        FGV2RichTextSpanViewModel Span;
        TSubclassOf<UGV2ScreenWidgetBase> HoverScreenWidgetClass;
        TSharedPtr<FGV2ScreenMutationPlan> HoverChildScreenPlan;
        GV2PresentationApply::FGV2PreparedPresentationTransaction HoverCentralStyleTransaction;
        bool bHoverHasChildPlan = false;
    };

    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TArray<FPreparedSpanItem>& GetPreparedSpans() const { return PreparedSpans; }

    // PSC-09B/11 (ADR-0043 D2/D3): flattens PreparedSpans into the lower module's own
    // canonical FPreparedRichTextSpan (plain Core types only). The host reconstructs the
    // USTRUCT array in its own IGV2PreparedRichTextSpansTarget sink.
    virtual bool BuildPreparedOperation(
        UWidget* TargetWidget,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
        FString& OutError) const override;

    // Prepare resolves hover presentation from the immutable session snapshot. Missing
    // context is a typed failure when hover data is present.
    virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) override { PrepareContext = InContext; }

    // DUC-11: same composition-cycle-guard wiring FGV2TabContainerTabsPropertyConsumer
    // exposes -- a span's hover nested screen can recurse into a tab container or another
    // rich text field just as readily as a tab's own nested screen can.
    void SetActiveCompositionChain(const TArray<FString>* InChain) { ActiveCompositionChain = InChain; }

private:
    // GBF-07: the real rollback boundary, named so it does not match the bare "Commit"
    // pattern the ownership gate treats as a leaf -- mirrors FGV2TabContainerTabsPropertyConsumer's
    // own Commit()/CommitWithFailureInjector() split (also the fixed two-name search
    // validate_property_consumer_transaction_coverage.py uses). This IS the interface's own
    // virtual (PCC-06/DUC-10 fault injection), not a same-named private helper -- a bare
    // 2-param helper of the same name would hide it (-Woverloaded-virtual). Commit() is a
    // thin delegate to this, matching every other nested consumer's own shape.
    virtual bool CommitWithFailureInjector(
        UWidget* TargetWidget,
        FString& OutError,
        const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
        const FString& PropertyPath) override;

    TArray<FPreparedSpanItem> PreparedSpans;
    const FGV2PresentationPrepareContext* PrepareContext = nullptr;
    const TArray<FString>* ActiveCompositionChain = nullptr;
    bool bHasAcceptedRevision = false;
};

class UGV2ScreenWidgetBase;

/**
 * TabContainer tabs consumer: parses and preflights tabs array for UGV2TabContainerWidgetBase.
 * Resolves each tab's screen_id via Screen Registry during Prepare off-tree.
 * Creates/reconciles child screen widgets off-tree and validates child fields.
 * If any child fails, rejects entire tab container off-tree.
 */
class GV2_API FGV2TabContainerTabsPropertyConsumer : public IGV2PropertyConsumer
{
public:
    FGV2TabContainerTabsPropertyConsumer();
    virtual ~FGV2TabContainerTabsPropertyConsumer() override;

    struct FPreparedTabItem
    {
        FName Key;
        FGV2TextViewModel Title;
        FString ScreenId;
        TSubclassOf<UGV2ScreenWidgetBase> ScreenWidgetClass;
        TWeakObjectPtr<UGV2ScreenWidgetBase> ScreenWidget;
        // DUC-09: prepared through the child screen's own PrepareScreenFields, the
        // same public two-phase API a top-level screen uses -- not a hand-rolled
        // mutation plan built against a schema synthesized from its capability.
        TSharedPtr<FGV2ScreenMutationPlan> ChildScreenPlan;
        GV2PresentationApply::FGV2PreparedPresentationTransaction CentralStyleTransaction;
        bool bHasChildPlan = false;
    };

    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual bool CommitWithFailureInjector(
        UWidget* TargetWidget,
        FString& OutError,
        const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
        const FString& PropertyPath) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TArray<FPreparedTabItem>& GetPreparedTabs() const { return PreparedTabs; }
    const TMap<FName, TStrongObjectPtr<UGV2ScreenWidgetBase>>& GetCandidateWidgetsByKey() const { return CandidateWidgetsByKey; }

    // DUC-11: injected by PrepareUiHostProperties (GV2UiMutationPlan.cpp) right
    // after this consumer is created, mirroring FGV2KeyedCollectionPropertyConsumer's
    // SetCompiledItemSpec. Non-owning; the caller's array outlives this Prepare() call.
    void SetActiveCompositionChain(const TArray<FString>* InChain) { ActiveCompositionChain = InChain; }

    // Prepare resolves every embedded screen through the immutable session snapshot.
    // Missing context is a typed failure; no configured/global resolver exists.
    virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) override { PrepareContext = InContext; }

private:
    TArray<FPreparedTabItem> PreparedTabs;
    TMap<FName, TStrongObjectPtr<UGV2ScreenWidgetBase>> CandidateWidgetsByKey;
    const TArray<FString>* ActiveCompositionChain = nullptr;
    const FGV2PresentationPrepareContext* PrepareContext = nullptr;
    // GBF-05: publishing is allowed only for a revision Prepare accepted. Without
    // this, Commit after a rejected Prepare publishes the empty tab list, which is
    // an application of state, not the absence of one.
    bool bHasAcceptedRevision = false;
};

/**
 * Classification of EGV2PreparedUiValueKind in the UI Property Pipeline.
 */
enum class EGV2PropertyConsumerKindStatus : uint8
{
    Supported,
    Inapplicable
};

struct GV2_API FGV2InapplicableKindInfo
{
    EGV2PreparedUiValueKind Kind;
    FString Reason;
};

/**
 * Factory for creating standard property consumers matching capability kinds.
 * Enforces compile-time and runtime completeness across all EGV2PreparedUiValueKind values.
 */
class GV2_API FGV2PropertyConsumerFactory
{
public:
    static TSharedPtr<IGV2PropertyConsumer> CreateConsumer(
        EGV2PreparedUiValueKind Kind,
        EGV2UiCapabilityTargetType TargetType,
        const FString& TargetKind = TEXT(""));

    /** Returns status indicating whether this kind is supported by a consumer or explicitly inapplicable. */
    static EGV2PropertyConsumerKindStatus GetKindHandlingStatus(EGV2PreparedUiValueKind Kind);

    /** Returns true if the kind is explicitly inapplicable, optionally returning the architectural justification. */
    static bool IsInapplicableKind(EGV2PreparedUiValueKind Kind, FString* OutReason = nullptr);

    /** Returns all explicitly registered inapplicable kinds with reasons. */
    static TArray<FGV2InapplicableKindInfo> GetInapplicableKinds();

    /**
     * Gate validating that every value of EGV2PreparedUiValueKind is either
     * supported with a non-null consumer or recorded with an inapplicable reason.
     */
    static bool ValidateAllKindsHandled(TArray<FString>& OutDiagnostics);
};
