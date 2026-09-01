#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2DeclaredCompositeWidgetBase.generated.h"

/**
 * Designer-facing kind of one flat property mapping in a declared composite.
 * Null and direct Object are deliberately absent: PCC-05 marks both inapplicable to
 * direct UI mutation. Array has one entry per consumer target shape.
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
    CollectionHost,
    RichTextSpans,
    NestedScreen,
};

/**
 * One declared capability: presentation property name, child widget name, and its kind.
 * The list is intentionally flat; it is a contract authored beside the UMG tree, not a
 * recursive description inferred from a child's own implementation.
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
