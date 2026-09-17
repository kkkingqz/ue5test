#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2UiBindingTarget.h"
#include "GV2ForgeryTestWidgets.generated.h"

/**
 * PCC-10: the three forgery shapes RunUiCapabilityObservabilityHarness's Done text
 * requires be proven -- not incidentally discovered, but deliberately constructed --
 * for an entry sitting *inside* a CollectionHost capability, where the harness's own
 * recursion (not the top-level sweep) is what must catch them.
 */
UENUM()
enum class EGV2ForgeryMode : uint8
{
    NoOpConsumer,
    DetachedRenderer,
    UnimplementableKind
};

class FScopedForgeryMode;

UCLASS(meta = (GV2TestOnly))
class UGV2ForgeryEntryTestWidget
    : public UUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
{
    GENERATED_BODY()

public:
    // CFC-02A: GetModeForNextInstance returns the active global forgery mode for new instances.
    // Mutation is restricted to FScopedForgeryMode via friend access.
    static EGV2ForgeryMode GetModeForNextInstance();

    virtual void PostInitProperties() override;
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // NoOpConsumer: the setter deliberately never stores the value it is given, so two
    // distinct commits are indistinguishable at the getter the harness reads back through --
    // exactly a consumer wired to nothing.
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) override { (void)InBindingHandle; }
    virtual FGV2UiBindingHandle GetBindingHandle() const override { return FGV2UiBindingHandle::Create(TEXT("forgery_fixed@1:1")); }

    EGV2ForgeryMode GetActiveMode() const { return ActiveMode.Get(GetModeForNextInstance()); }

private:
    friend class FScopedForgeryMode;
    static void SetModeForNextInstance(EGV2ForgeryMode InMode);

    FGV2UiPropertyHostState PropertyHostState;
    mutable TOptional<EGV2ForgeryMode> ActiveMode;
};

/**
 * CFC-02A: RAII helper to scope EGV2ForgeryMode modifications.
 * Guarantees previous mode is restored upon leaving scope, even with early returns or exceptions.
 */
class FScopedForgeryMode final
{
public:
    explicit FScopedForgeryMode(EGV2ForgeryMode InMode);
    ~FScopedForgeryMode();

    FScopedForgeryMode(const FScopedForgeryMode&) = delete;
    FScopedForgeryMode& operator=(const FScopedForgeryMode&) = delete;

    EGV2ForgeryMode GetPreviousMode() const { return PreviousMode; }

private:
    EGV2ForgeryMode PreviousMode;
};

/**
 * DUC-03: proves FGV2KeyPropertyConsumer needs no edit for a new host that declares `key` --
 * this class is added only here, in a test, and never appears anywhere in
 * GV2PropertyConsumers.cpp. It implements nothing beyond the bare minimum
 * IGV2UiPropertyHost requires; GetKey()/SetKey() come from the interface's shared
 * FGV2UiPropertyHostState, not from any code written for this class specifically.
 */
UCLASS(meta = (GV2TestOnly))
class UGV2NewHostAddedOnlyInTestWidget
    : public UUserWidget
    , public IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

private:
    FGV2UiPropertyHostState PropertyHostState;
};

/**
 * PSC-10B: a Separator whose `meta = (BindWidget)` sub-widgets are populated the way a
 * Widget Blueprint's generated class would populate them, so a C++-only test can observe
 * the PHYSICAL result of a central-style operation. It adds no behaviour of its own -- it
 * neither overrides ApplyCentralStyle nor touches a Theme -- so what the test observes is
 * UGV2SeparatorWidgetBase's own production write path, not a stand-in for it.
 */
UCLASS(meta = (GV2TestOnly))
class UGV2SeparatorBoundTestWidget : public UGV2SeparatorWidgetBase
{
    GENERATED_BODY()

public:
    void BuildBoundSubWidgets();
    void SetTestOrientation(EOrientation InOrientation) { Orientation = InOrientation; }

    float ReadAppliedThickness() const;
    FSlateBrush ReadAppliedBrush() const;
};

/**
 * PSC-10B: same seam as UGV2SeparatorBoundTestWidget, for the progress-bar style role.
 */
UCLASS(meta = (GV2TestOnly))
class UGV2ProgressBarBoundTestWidget : public UGV2ProgressBarWidgetBase
{
    GENERATED_BODY()

public:
    void BuildBoundSubWidgets();
    FLinearColor ReadAppliedFillColor() const;
};

/**
 * PEP-06A: a rich text widget whose `meta = (BindWidget)` sub-widgets are populated the way
 * a Widget Blueprint would populate them, so a C++-only test can drive real interactive-span
 * rendering (CaptureHoverableSpanAnchors et al.) without depending on a game-content WBP
 * (ADR-0046/TSR-10 forbids a contract test referencing a non-core content path).
 */
UCLASS(meta = (GV2TestOnly))
class UGV2RichTextBoundTestWidget : public UGV2RichTextWidgetBase
{
    GENERATED_BODY()

public:
    void BuildBoundSubWidgets();
};
