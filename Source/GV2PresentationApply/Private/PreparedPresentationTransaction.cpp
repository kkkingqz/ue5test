#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Math/UnrealMathUtility.h"

namespace GV2PresentationApply
{
namespace
{
// PSC-10A: standard "overloaded lambda set" idiom -- TVariant's Visit() dispatches to
// whichever operator() matches the alternative's concrete type. If a new alternative is
// added to FGV2PreparedOperationVariant without a corresponding lambda here, overload
// resolution finds no match and the build fails -- the compiler IS the exhaustiveness
// gate, not a `default:` branch or a hand-maintained kind list.
template <typename... Ts>
struct TOverloaded : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts>
TOverloaded(Ts...) -> TOverloaded<Ts...>;
}

EGV2PreparedOperationKind GetPreparedOperationKind(const FGV2PreparedOperationVariant& Operation)
{
    return static_cast<EGV2PreparedOperationKind>(Operation.GetIndex());
}

bool Apply(const FGV2PreparedPresentationTransaction& Transaction, FString& OutError)
{
    OutError.Reset();
    bool bFailed = false;

    for (const FGV2PreparedOperationVariant& Operation : Transaction.GetOperations())
    {
        if (bFailed)
        {
            break;
        }

        Visit(TOverloaded{
            [&bFailed, &OutError](const FPreparedImageResourceOperation& Op)
            {
                UImage* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    OutError = TEXT("Image resource operation's target widget is no longer valid.");
                    bFailed = true;
                    return;
                }
                Widget->SetBrush(Op.Brush);
                Widget->SetDesiredSizeOverride(Op.Brush.ImageSize);
            },
            // ImageHost/Number/Integer/String/Key/Binding/RichTextSpans/Text/
            // KeyedCollection/TabContainer operations carry no target this module can
            // Cast to (see each struct's own doc comment) -- entirely
            // GV2LegacyPresentationApplyAdapter territory, nothing to do here.
            [](const FPreparedImageHostOperation&) {},
            [](const FPreparedBooleanOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                switch (Op.Target)
                {
                case EPreparedBooleanTarget::WidgetEnabled:
                    Widget->SetIsEnabled(Op.Value);
                    break;
                case EPreparedBooleanTarget::CheckBoxChecked:
                    if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget))
                    {
                        CheckBox->SetIsChecked(Op.Value);
                    }
                    break;
                case EPreparedBooleanTarget::EditableTextReadOnly:
                    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                    {
                        EditableBox->SetIsReadOnly(Op.Value);
                    }
                    break;
                case EPreparedBooleanTarget::RequiresLegacyAdapter:
                    // Left for GV2LegacyPresentationApplyAdapter -- see this operation's
                    // own enum doc comment.
                    break;
                }
            },
            [](const FPreparedEditableTextValueOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetText(Op.Value);
                }
            },
            [](const FPreparedProgressBarOperation& Op)
            {
                if (UProgressBar* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetPercent(Op.Percent);
                }
            },
            [](const FPreparedNumberOperation&) {},
            [](const FPreparedIntegerOperation&) {},
            [](const FPreparedStringOperation&) {},
            [](const FPreparedKeyOperation&) {},
            [](const FPreparedBindingOperation&) {},
            [](const FPreparedRichTextSpansOperation&) {},
            [](const FPreparedTextOperation&) {},
            [](const FPreparedKeyedCollectionOperation&) {},
            [](const FPreparedTabContainerOperation&) {},
            [](const FPreparedPlainTextOperation& Op)
            {
                UCommonTextBlock* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                Widget->SetStyle(Op.Style);
                Widget->SetText(Op.Text);

                // Apply DPI-aware scaled font size without wiping out the widget's
                // FontObject/typeface.
                FSlateFontInfo FontInfo = Widget->GetFont();
                if (!FMath::IsNearlyEqual(FontInfo.Size, Op.FontSize, 0.01f))
                {
                    FontInfo.Size = Op.FontSize;
                    Widget->SetFont(FontInfo);
                }
            },
            [](const FPreparedRichTextRenderOperation& Op)
            {
                UCommonRichTextBlock* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                if (Op.Style != nullptr)
                {
                    Widget->SetStyle(Op.Style);
                }
                if (Op.bHasDefaultStyle)
                {
                    Widget->SetDefaultTextStyle(Op.DefaultStyle);
                }
                Widget->SetText(FText::FromString(Op.Markup));
            },
            [](const FPreparedTextHintOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetHintText(Op.Text);
                }
            }
        }, Operation);
    }

    if (bFailed)
    {
        return false;
    }
    return true;
}
}
