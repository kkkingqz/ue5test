#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UObject/Interface.h"

#include "PreparedApplyTargets.generated.h"

// PSC-11 (ADR-0043 D2): the physical roles a prepared operation can target.
//
// Until PSC-11 the exhaustive dispatch lived in TWO places: GV2PresentationApply::Apply for
// the plain Engine/UMG targets it can Cast to, and GV2LegacyPresentationApplyAdapter for the
// GV2-owned UCLASSes it cannot. Two entry points meant every caller had to remember to call
// both, and "the transaction was applied" was not a single fact.
//
// These interfaces remove the second entry point WITHOUT moving any UCLASS (that is PSC-12):
// a GV2-owned widget declares the physical role it can perform, and the lower module reaches
// it through the role rather than through its concrete type. Every method is a value sink --
// it receives finished values and writes them. None can reach a Theme, catalog, snapshot or
// any other authority, because the arguments carry no way to ask for one and this module
// cannot name a type that does.
//
// Each role is its own interface on purpose. A single interface with one no-op-defaulted
// method per role would let a widget declare the interface, forget the override, and report
// a successful commit while writing nothing -- the exact shape this pipeline exists to make
// impossible. A missing role interface is a diagnosable target mismatch instead.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedTextTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedTextTarget
{
    GENERATED_BODY()

public:
    // bIsReset distinguishes the two originally different redirect rules a reset had; see
    // the operation's own doc comment. OutError is filled only on failure.
    virtual bool ApplyPreparedText(
        const GV2PresentationApply::FPreparedTextValue& Value,
        bool bIsReset,
        FString& OutError) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedImageHostTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedImageHostTarget
{
    GENERATED_BODY()

public:
    virtual bool ApplyPreparedImageHost(
        const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
        FString& OutError) = 0;
    virtual void ResetPreparedImageHost() = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedBooleanTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedBooleanTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedBoolean(FName PropertyName, bool bValue) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedNumberTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedNumberTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedNumber(double Value) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedIntegerTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedIntegerTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedInteger(int64 Value) = 0;
    // The limit this target currently enforces, so a prepared string operation can be
    // truncated to it without asking the target's concrete class.
    virtual int64 GetPreparedMaxLength() const = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedKeyTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedKeyTarget
{
    GENERATED_BODY()

public:
    // Returns false when the property name is one this target does not route, so the caller
    // can report an unhandled declared capability instead of a silent successful commit.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedBindingTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedBindingTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedBinding(const FString& SerializedHandle) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedRichTextSpansTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedRichTextSpansTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedRichTextSpans(const TArray<GV2PresentationApply::FPreparedRichTextSpan>& Spans) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedKeyedCollectionTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedKeyedCollectionTarget
{
    GENERATED_BODY()

public:
    // The panel whose children this collection owns. The lower module performs the ordered
    // rebuild itself; the target only says where.
    virtual UPanelWidget* GetPreparedCollectionPanel() const = 0;
    virtual void ResetPreparedCollection() = 0;
    // Called after the rebuild, for a target that keeps its own key -> widget bookkeeping.
    virtual void OnPreparedCollectionSettled(const TArray<GV2PresentationApply::FPreparedKeyedCollectionEntry>& Entries) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedTabContainerTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedTabContainerTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedTabs(const TArray<GV2PresentationApply::FPreparedTabEntry>& Entries) = 0;
    virtual void ResetPreparedTabs() = 0;
};

// One interface per central-style role, for the reason stated at the top of this file: a
// role whose target does not declare it is a diagnosable mismatch, not a silent no-op.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedSeparatorStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedSeparatorStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedSeparatorStyle(const GV2PresentationApply::FPreparedSeparatorStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedTintStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedTintStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedTintStyle(const GV2PresentationApply::FPreparedTintStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedItemPaddingStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedItemPaddingStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedItemPaddingStyle(const GV2PresentationApply::FPreparedItemPaddingStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedProgressBarStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedProgressBarStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedProgressBarStyle(const GV2PresentationApply::FPreparedProgressBarStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedLoadingIndicatorStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedLoadingIndicatorStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedLoadingIndicatorStyle(const GV2PresentationApply::FPreparedLoadingIndicatorStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedButtonStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedButtonStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedButtonStyle(const GV2PresentationApply::FPreparedButtonStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedCheckboxStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedCheckboxStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedCheckboxStyle(const GV2PresentationApply::FPreparedCheckboxStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedInputFieldStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedInputFieldStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedInputFieldStyle(const GV2PresentationApply::FPreparedInputFieldStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedDropdownStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedDropdownStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedDropdownStyle(const GV2PresentationApply::FPreparedDropdownStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedRichTextStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedRichTextStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedRichTextStyle(const GV2PresentationApply::FPreparedRichTextStyle& Style) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGV2PreparedRichTextPopoverStyleTarget : public UInterface { GENERATED_BODY() };

class GV2PRESENTATIONAPPLY_API IGV2PreparedRichTextPopoverStyleTarget
{
    GENERATED_BODY()

public:
    virtual void ApplyPreparedRichTextPopoverStyle(const GV2PresentationApply::FPreparedRichTextPopoverStyle& Style) = 0;
};
