#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2DeclaredCompositeWidgetBase.generated.h"

/**
 * Designer-facing kind of one flat property mapping in a declared composite.
 * Null and direct Object are deliberately absent: PCC-05 marks both inapplicable to
 * direct UI mutation. Array has one entry per consumer target shape.
 *
 * GBH-02A completeness gate: every value here is either selectable (no UMETA(Hidden))
 * with a proven end-to-end path through UGV2DeclaredCompositeWidgetBase specifically --
 * not merely a capability-tree shape check, and not proof via a leaf widget's own native
 * DescribeUiCapabilities, a different delegation path -- or Hidden with a recorded
 * ToolTip reason. FGV2DesignerCapabilityKindGate::ValidateAllKindsClassified enumerates
 * the live UEnum, so a new value added here without updating that gate's proven-kind list
 * (and, if selectable, without UMETA(Hidden)) fails the gate instead of silently defaulting
 * to selectable-but-unproven -- the exact shape of defect REM-05 found in CollectionHost.
 */
UENUM(BlueprintType)
enum class EGV2DeclaredUiCapabilityKind : uint8
{
    Boolean,
    Integer,
    Number,
    String,
    Key,
    Text,
    ResourceRef,
    Binding,

    // GBH-02B: REM-05 closed. DescribeUiCapabilities' CollectionHost case now calls
    // AddKeyedCollection with EntryWidgetClass/KeyPropertyName (present on
    // FGV2DeclaredUiCapability since GBH-06, unused until now) -- the item's own
    // capability tree is read from EntryWidgetClass's own CDO DescribeUiCapabilities,
    // the same independent-second-source pattern already used for ChildWidgetName, and
    // the exact precedent UGV2ButtonListWidgetBase already uses for its own entry class.
    // Proven end-to-end by GV2.UI.DeclaredComposite.CollectionHostFirstEntry: a genuinely
    // empty collection creates its first real entry through Designer declaration ->
    // schema/materialization -> Commit -> observable renderer state.
    CollectionHost,

    // GBH-02A: this kind's only proof is FGV2RichTextSpansPropertyConsumer applied to
    // UGV2RichTextWidgetBase's own *native* DescribeUiCapabilities (a leaf widget
    // declaring its own "spans" capability directly) -- a different delegation path from
    // a DeclaredComposite mapping a Designer property to a RichText *child*. No test
    // exercises Designer declaration -> child resolution -> Commit -> observed spans
    // through UGV2DeclaredCompositeWidgetBase for this kind. Hidden until that path has
    // its own proof, by the same rule REM-05 applies to CollectionHost.
    RichTextSpans UMETA(Hidden),

    // DUC-09/10/11: proven end-to-end through DeclaredComposite -- TabsHost declares
    // this kind targeting a real TabContainer child, tabs resolve real nested screens,
    // and the composition-cycle guard is covered by its own red/green tests.
    NestedScreen,
};

/**
 * Classification of EGV2DeclaredUiCapabilityKind for the Designer selection surface (GBH-02A).
 */
enum class EGV2DesignerKindStatus : uint8
{
    Supported,
    Hidden
};

struct GV2_API FGV2HiddenDesignerKindInfo
{
    EGV2DeclaredUiCapabilityKind Kind;
    FString Reason;
};

/**
 * GBH-02A completeness gate over EGV2DeclaredUiCapabilityKind, symmetric to
 * FGV2PropertyConsumerFactory's PCC-05 gate over EGV2PreparedUiValueKind: every enum
 * value must be either Supported (selectable, with a proven end-to-end path -- listed in
 * the internal proven-kinds set) or Hidden (UMETA(Hidden) on the enumerator, with a
 * recorded ToolTip reason). Unlike PCC-05's gate, status is read from the live UEnum
 * metadata rather than duplicated in a second switch, so the enum's actual Designer
 * surface and this gate's classification cannot drift apart.
 */
class GV2_API FGV2DesignerCapabilityKindGate
{
public:
    /** Reads UMETA(Hidden) on the live UEnum for this value. */
    static EGV2DesignerKindStatus GetKindStatus(EGV2DeclaredUiCapabilityKind Kind);

    /** Returns true if the kind is Hidden, optionally returning its recorded ToolTip reason. */
    static bool IsHiddenKind(EGV2DeclaredUiCapabilityKind Kind, FString* OutReason = nullptr);

    /** Returns every currently Hidden kind with its recorded reason. */
    static TArray<FGV2HiddenDesignerKindInfo> GetHiddenKinds();

    /**
     * Gate validating that every live value of EGV2DeclaredUiCapabilityKind is either
     * Supported (in the internal proven-kinds list) XOR Hidden (UMETA(Hidden) with a
     * non-empty ToolTip reason) -- never both, never neither. A new enum value added
     * without updating the proven-kinds list and without UMETA(Hidden) fails this gate.
     */
    static bool ValidateAllKindsClassified(TArray<FString>& OutDiagnostics);
};

/**
 * One declared capability: presentation property name, child widget name, its kind, and
 * (GBH-06) the kind-dependent constraints and child-capability selector a flat triple
 * cannot express. The list is intentionally flat; it is a contract authored beside the
 * UMG tree, not a recursive description inferred from a child's own implementation --
 * these fields are authored independently of ChildWidgetName's own DescribeUiCapabilities
 * (never read from it), the same independent-sources discipline DUC-07 established for
 * Kind alone. The schema form visible to a content author is unchanged; only this
 * Designer-side declaration is enriched.
 */
USTRUCT(BlueprintType)
struct GV2_API FGV2DeclaredUiCapability
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities")
    FName PropertyName;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities")
    FName ChildWidgetName;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities")
    EGV2DeclaredUiCapabilityKind Kind = EGV2DeclaredUiCapabilityKind::Text;

    // GBH-06: which of ChildWidgetName's own declared capabilities (by its PropertyName)
    // this entry delegates to. Required whenever ChildWidgetName declares more than one
    // capability of Kind -- left NAME_None otherwise falls back to resolving by Kind
    // alone, which is rejected as ambiguous if that ever stops being unique.
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities")
    FName ChildCapabilityName;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::Number", EditConditionHides))
    double NumberMin = 0.0;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::Number", EditConditionHides))
    double NumberMax = 1.0;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::Integer", EditConditionHides))
    int64 IntMin = 0;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::Integer", EditConditionHides))
    int64 IntMax = 100;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::ResourceRef", EditConditionHides))
    FString TargetKind = TEXT("resource");

    // GBH-06: reserved for GBH-02B -- CollectionHost stays Hidden (GBH-02A) and these are
    // not yet wired into DescribeUiCapabilities, so the item-contract question GBH-02B
    // still has to answer does not require another struct migration when it lands.
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::CollectionHost", EditConditionHides))
    TSubclassOf<UUserWidget> EntryWidgetClass;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (EditCondition = "Kind == EGV2DeclaredUiCapabilityKind::CollectionHost", EditConditionHides))
    FString KeyPropertyName = TEXT("key");
};

/**
 * Generic property host whose capability tree is declared in a Widget Blueprint.
 * DUC-05 owns declaration only; DUC-06/07 add schema projection and the independent
 * validation of each declared mapping against the selected child capability.
 */
UCLASS(Blueprintable)
class GV2_API UGV2DeclaredCompositeWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2ScreenFieldHost
{
    GENERATED_BODY()

public:
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    // DUC-05: each entry remains visible and editable next to this Blueprint's WidgetTree.
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Capabilities", meta = (TitleProperty = "PropertyName"))
    TArray<FGV2DeclaredUiCapability> DeclaredCapabilities;

private:
    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
