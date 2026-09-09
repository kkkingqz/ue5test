#pragma once

#include "GV2PresentationApply/PreparedPresentationTransaction.h"

// PSC-11: a caller-side convenience over the single Apply façade, not a second entry point.
// It exists only because most GV2 call sites already carry an `FString& OutError` of their
// own and would otherwise each declare a result and copy one field out of it. It is
// deliberately not exported: the module's public surface still has exactly one way to apply
// a transaction, and this adds no behaviour beyond forwarding the diagnostic.
inline bool GV2ApplyTransaction(
    const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction,
    FString& OutError)
{
    FGV2PresentationApplyResult Result;
    const bool bApplied = FGV2PresentationApply::Apply(Transaction, Result);
    OutError = Result.Error;
    return bApplied;
}
