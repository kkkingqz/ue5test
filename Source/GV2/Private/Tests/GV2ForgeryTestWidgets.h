#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/GV2UiPropertyHost.h"
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

UCLASS()
class UGV2ForgeryEntryTestWidget
    : public UUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
{
    GENERATED_BODY()

public:
    // RunUiCapabilityObservabilityHarness's recursion constructs its own fresh instance of
    // this class via CreateWidget, so the test has no seam to configure that specific
    // instance directly -- mutating the CDO's own field does not propagate into that new
    // instance (NewObject's property init runs before the CDO's runtime-mutated value would
    // apply here). A plain static, read at describe-time, sidesteps that entirely.
    static EGV2ForgeryMode& ModeForNextInstance();

    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // NoOpConsumer: the setter deliberately never stores the value it is given, so two
    // distinct commits are indistinguishable at the getter the harness reads back through --
    // exactly a consumer wired to nothing.
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) override { (void)InBindingHandle; }
    virtual FGV2UiBindingHandle GetBindingHandle() const override { return FGV2UiBindingHandle::Create(TEXT("forgery_fixed@1:1")); }

private:
    FGV2UiPropertyHostState PropertyHostState;
};
