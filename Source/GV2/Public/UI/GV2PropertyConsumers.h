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
 */
class GV2_API FGV2KeyPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Key; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

private:
    FString PreparedValue;
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
 * Factory for creating standard property consumers matching capability kinds.
 */
class GV2_API FGV2PropertyConsumerFactory
{
public:
    static TSharedPtr<IGV2PropertyConsumer> CreateConsumer(
        EGV2PreparedUiValueKind Kind,
        EGV2UiCapabilityTargetType TargetType,
        const FString& TargetKind = TEXT(""));
};
