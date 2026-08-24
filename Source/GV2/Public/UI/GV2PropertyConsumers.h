#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapability.h"

class UWidget;

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
};

/**
 * Boolean consumer: applies boolean flag (e.g. SetIsEnabled / visibility).
 */
class GV2_API FGV2BooleanPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Boolean; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

private:
    bool bPreparedValue = false;
};

/**
 * Integer consumer: applies bounded integer value.
 */
class GV2_API FGV2IntegerPropertyConsumer : public IGV2PropertyConsumer
{
public:
    virtual EGV2PreparedUiValueKind GetSupportedKind() const override { return EGV2PreparedUiValueKind::Integer; }
    virtual bool CanConsume(const FGV2PreparedUiValue& Value) const override;
    virtual bool Prepare(const FGV2PreparedUiValue& Value, const FGV2UiPropertyCapability& Capability, UWidget* TargetWidget, FString& OutError) override;
    virtual bool Commit(UWidget* TargetWidget, FString& OutError) override;
    virtual void Reset(UWidget* TargetWidget) override;

private:
    int64 PreparedValue = 0;
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
