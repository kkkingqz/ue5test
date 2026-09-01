#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapability.h"

class UWidget;
class FGV2UiHostMutationPlan;

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

    /**
     * Infallible commit phase.
     * Applies already prepared state to the target widget.
     */
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) = 0;

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

private:
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
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TMap<FName, TObjectPtr<UWidget>>& GetActiveWidgetsByKey() const { return ActiveWidgetsByKey; }

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

    const TArray<FGV2CollectionItemDiscrepancy>& GetDiscrepancies() const { return Discrepancies; }

    static const TArray<FGV2CollectionItemDiscrepancy>& GetAllRecordedDiscrepancies();
    static void ClearAllRecordedDiscrepancies();

private:
    struct FPreparedCollectionItem
    {
        FName Key;
        TObjectPtr<UWidget> Widget;
        TSharedPtr<FGV2UiHostMutationPlan> Plan;
        bool bIsHost = false;
    };

    FString KeyPropertyName = TEXT("key");
    TArray<FPreparedCollectionItem> PreparedItems;
    TMap<FName, TObjectPtr<UWidget>> ActiveWidgetsByKey;
    TMap<FName, TObjectPtr<UWidget>> CandidateWidgetsByKey;

    GV2ContentCore::FCompiledUiFieldSpecPtr CompiledItemSpec;
    FString ContextSchemaId;
    FString ContextPropertyPath;
    FString ContextScreenId;
    FString ContextFieldId;
    TArray<FGV2CollectionItemDiscrepancy> Discrepancies;
};

/**
 * RichText spans consumer: parses and preflights interactive spans array for UGV2RichTextWidgetBase.
 * Validates canonical span keys, hover payloads, themes, and ensures all referenced runs match.
 */
class GV2_API FGV2RichTextSpansPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TArray<FGV2RichTextSpanViewModel>& GetPreparedSpans() const { return PreparedSpans; }

private:
    TArray<FGV2RichTextSpanViewModel> PreparedSpans;
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
    struct FPreparedTabItem
    {
        FName Key;
        FGV2TextViewModel Title;
        FString ScreenId;
        TSubclassOf<UGV2ScreenWidgetBase> ScreenWidgetClass;
        TObjectPtr<UGV2ScreenWidgetBase> ScreenWidget;
        TSharedPtr<FGV2UiHostMutationPlan> ChildMutationPlan;
        bool bHasChildPlan = false;
    };

    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Array; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

    const TArray<FPreparedTabItem>& GetPreparedTabs() const { return PreparedTabs; }

private:
    TArray<FPreparedTabItem> PreparedTabs;
    TMap<FName, TObjectPtr<UGV2ScreenWidgetBase>> CandidateWidgetsByKey;
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
