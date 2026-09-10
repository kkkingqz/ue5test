#include "GV2PresentationApply/GV2WidgetTextApply.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"

namespace
{
bool ApplyTransaction(
    const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction)
{
    FGV2PresentationApplyResult Result;
    return FGV2PresentationApply::Apply(Transaction, Result);
}
}

bool FGV2WidgetTextApply::Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text)
{
    if (Widget == nullptr || !Text.bHasResolvedPresentation
        || Text.ResolvedStyleClass == nullptr
        || Text.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }

    GV2PresentationApply::FPreparedPlainTextOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Style = Text.ResolvedStyleClass;
    Operation.Text = Text.Text;
    Operation.ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
    Operation.ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
    Operation.ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
    Operation.ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddPlainTextOperation(MoveTemp(Operation));
    return ApplyTransaction(Transaction);
}

bool FGV2WidgetTextApply::ApplyRichText(
    UCommonRichTextBlock* Widget,
    const FGV2TextViewModel& Text,
    const UWidget* /*ContextWidget*/)
{
    if (Widget == nullptr || !Text.bHasResolvedPresentation
        || (Text.NormalizedMarkup.IsEmpty() && !Text.Text.IsEmpty()))
    {
        return false;
    }

    GV2PresentationApply::FPreparedRichTextRenderOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Style = Text.ResolvedStyleClass;
    Operation.DefaultStyle = Text.ResolvedDefaultStyle;
    Operation.bHasDefaultStyle = Text.bHasResolvedDefaultStyle;
    Operation.Markup = Text.NormalizedMarkup;
    Operation.ScalePolicy.BaseFontSize = Text.ResolvedBaseFontSize;
    Operation.ScalePolicy.MinReadableFontSize = Text.ResolvedMinReadableFontSize;
    Operation.ScalePolicy.ReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
    Operation.ScalePolicy.ScaleCurve = Text.ResolvedFontScaleCurve;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddRichTextRenderOperation(MoveTemp(Operation));
    return ApplyTransaction(Transaction);
}

bool FGV2WidgetTextApply::ApplyHint(
    UEditableTextBox* Widget,
    const FGV2TextViewModel& Text)
{
    if (Widget == nullptr || Text.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }

    GV2PresentationApply::FPreparedTextHintOperation Operation;
    Operation.TargetWidget = Widget;
    Operation.Text = Text.Text;
    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddTextHintOperation(MoveTemp(Operation));
    return ApplyTransaction(Transaction);
}
