#include "Bridge/GV2BridgeTypes.h"

// PSC-11: the one inflation of a prepared text value. See the declaration's own comment.
FGV2TextViewModel FGV2TextViewModel::FromPrepared(const GV2PresentationApply::FPreparedTextValue& Value)
{
    FGV2TextViewModel Text;
    Text.Text = Value.Text;
    Text.StyleToken = Value.StyleToken;
    Text.NormalizedMarkup = Value.NormalizedMarkup;
    Text.ResolvedStyleClass = Value.ResolvedStyleClass;
    Text.ResolvedBaseFontSize = Value.ResolvedBaseFontSize;
    Text.ResolvedMinReadableFontSize = Value.ResolvedMinReadableFontSize;
    Text.ResolvedReferenceViewportHeight = Value.ResolvedReferenceViewportHeight;
    Text.ResolvedFontScaleCurve = Value.ResolvedFontScaleCurve;
    Text.ResolvedDefaultStyle = Value.ResolvedDefaultStyle;
    Text.bHasResolvedPresentation = Value.bHasResolvedPresentation;
    Text.bHasResolvedDefaultStyle = Value.bHasResolvedDefaultStyle;
    return Text;
}
