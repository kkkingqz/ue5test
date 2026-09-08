#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "Components/Image.h"

namespace GV2PresentationApply
{
bool Apply(const FGV2PreparedPresentationTransaction& Transaction, FString& OutError)
{
    for (const FPreparedImageResourceOperation& Operation : Transaction.GetImageResourceOperations())
    {
        UImage* Widget = Operation.TargetWidget.Get();
        if (Widget == nullptr)
        {
            OutError = TEXT("Image resource operation's target widget is no longer valid.");
            return false;
        }
        Widget->SetBrush(Operation.Brush);
        Widget->SetDesiredSizeOverride(Operation.Brush.ImageSize);
    }
    OutError.Reset();
    return true;
}
}
