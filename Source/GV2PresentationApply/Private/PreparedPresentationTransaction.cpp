#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"

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

    for (const FPreparedBooleanOperation& Operation : Transaction.GetBooleanOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (Widget == nullptr)
        {
            continue;
        }
        switch (Operation.Target)
        {
        case EPreparedBooleanTarget::WidgetEnabled:
            Widget->SetIsEnabled(Operation.Value);
            break;
        case EPreparedBooleanTarget::CheckBoxChecked:
            if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget))
            {
                CheckBox->SetIsChecked(Operation.Value);
            }
            break;
        case EPreparedBooleanTarget::EditableTextReadOnly:
            if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
            {
                EditableBox->SetIsReadOnly(Operation.Value);
            }
            break;
        case EPreparedBooleanTarget::RequiresLegacyAdapter:
            // Left for GV2LegacyPresentationApplyAdapter -- see this operation's own
            // enum doc comment.
            break;
        }
    }

    for (const FPreparedEditableTextValueOperation& Operation : Transaction.GetEditableTextValueOperations())
    {
        if (UEditableTextBox* Widget = Operation.TargetWidget.Get())
        {
            Widget->SetText(Operation.Value);
        }
    }

    for (const FPreparedProgressBarOperation& Operation : Transaction.GetProgressBarOperations())
    {
        if (UProgressBar* Widget = Operation.TargetWidget.Get())
        {
            Widget->SetPercent(Operation.Percent);
        }
    }

    // Integer/String/Key/Binding operations carry no target this module can Cast to
    // (see each struct's own doc comment) -- entirely GV2LegacyPresentationApplyAdapter
    // territory, nothing to do here.

    OutError.Reset();
    return true;
}
}
