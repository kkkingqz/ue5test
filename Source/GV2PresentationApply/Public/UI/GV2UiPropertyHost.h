#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "UObject/Interface.h"
#include "UI/GV2UiCapability.h"
#include "GV2UiPropertyHost.generated.h"

class GV2PRESENTATIONAPPLY_API FGV2UiHostStateExtension
{
public:
    virtual ~FGV2UiHostStateExtension() = default;
};

// PSC-11: every property host IS a prepared-key target. Deriving the role here rather than
// declaring it on each host preserves DUC-03 -- a new host that declares a `key` capability
// still needs no edit anywhere -- while giving the lower Apply module a way to reach the
// write without naming a GV2 type.
UINTERFACE(MinimalAPI)
class UGV2UiPropertyHost : public UGV2PreparedKeyTarget
{
    GENERATED_BODY()
};

/**
 * Shared state and helper for any widget implementing IGV2UiPropertyHost.
 * Provides value-only capability caching and this host's identity within its enclosing
 * host (DUC-01). Schema/rollback bookkeeping is owned by the semantic GV2 layer and hangs
 * off the opaque extension slot; the lower module cannot name those types.
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
struct GV2PRESENTATIONAPPLY_API FGV2UiPropertyHostState
{
    GENERATED_BODY()

public:
    FGV2UiPropertyHostState() = default;

    const FGV2UiCapabilityTree& GetCapabilities() const { return CachedCapabilities; }
    void SetCapabilities(FGV2UiCapabilityTree InCapabilities) { CachedCapabilities = MoveTemp(InCapabilities); }

    FName GetHostIdentity() const { return HostIdentity; }
    void SetHostIdentity(FName InHostIdentity) { HostIdentity = InHostIdentity; }

    FName GetKey() const { return Key; }
    void SetKey(FName InKey) { Key = InKey; }

    const TSharedPtr<FGV2UiHostStateExtension>& GetSemanticExtension() const { return SemanticExtension; }
    void SetSemanticExtension(TSharedPtr<FGV2UiHostStateExtension> InExtension)
    {
        SemanticExtension = MoveTemp(InExtension);
    }

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
    TSharedPtr<FGV2UiHostStateExtension> SemanticExtension;
};

/**
 * DUC-03: key capability names that belong to one specific host class rather than to the
 * generic `key` identity -- `selected_key` is UGV2DropdownSelectWidgetBase's own capability
 * and `default_tab_key` is UGV2TabContainerWidgetBase's. Each owner routes its own name in
 * its ApplyPreparedKey override; every other host must REFUSE such a name, so the caller
 * reports core:diagnostic.ui_consumer.unhandled_target instead of quietly storing a named
 * capability's value in the host's own identity slot. A commit that succeeds while writing
 * the wrong field is the failure shape this pipeline exists to prevent.
 *
 * This is a list, and a list is the weak form of the rule. It is held to the actual set by
 * GV2.UI.PreparedKeyCapabilityRouting, which derives every non-generic key capability name
 * from the DescribeUiCapabilities of every native IGV2UiPropertyHost by reflection and fails
 * if the two differ -- so a new named key capability cannot be declared without either
 * routing it or appearing here.
 */
/**
 * Native interface implemented by any widget supporting data-driven UI property binding.
 */
class GV2PRESENTATIONAPPLY_API IGV2UiPropertyHost : public IGV2PreparedKeyTarget
{
    GENERATED_BODY()

public:
    // PSC-11: the generic identity write, shared by every host. A host that routes a NAMED
    // key capability of its own overrides this and falls back to it for every other name.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override
    {
        if (!PropertyName.IsNone() && PropertyName != TEXT("key"))
        {
            return false;
        }
        GetPropertyHostState().SetKey(Value);
        return true;
    }

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
