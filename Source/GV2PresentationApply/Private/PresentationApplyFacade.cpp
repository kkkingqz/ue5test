#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"

namespace GV2PresentationApply
{
namespace
{
// PSC-11: same "overloaded lambda set" idiom the transaction's own kind walk uses.
template <typename... Ts>
struct TOverloaded : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts>
TOverloaded(Ts...) -> TOverloaded<Ts...>;

// PSC-11: a role is looked for on the target first and then up its outer chain. That second
// step is not convenience -- a prepared operation may legitimately target an inner UMG
// primitive (a UCommonTextBlock inside a GV2 text widget, a scroll box inside a dropdown)
// while the widget that owns the bookkeeping is its outer. Before PSC-11 the adapter did
// this with GetTypedOuter<ConcreteClass>; the role interface expresses the same reach
// without naming a class this module cannot name.
template <typename TRole>
TRole* FindRole(UObject* Target)
{
    for (UObject* Candidate = Target; Candidate != nullptr; Candidate = Candidate->GetOuter())
    {
        if (TRole* Role = Cast<TRole>(Candidate))
        {
            return Role;
        }
    }
    return nullptr;
}

FPreparedTextScalePolicy ScalePolicyOf(const FPreparedTextValue& Value)
{
    FPreparedTextScalePolicy Policy;
    Policy.BaseFontSize = Value.ResolvedBaseFontSize;
    Policy.MinReadableFontSize = Value.ResolvedMinReadableFontSize;
    Policy.ReferenceViewportHeight = Value.ResolvedReferenceViewportHeight;
    Policy.ScaleCurve = Value.ResolvedFontScaleCurve;
    return Policy;
}

// The three plain-renderer writes the upper UGV2TextPipeline used to perform. They are the
// same operations this module already applies for PlainText/RichTextRender/TextHint, built
// here from the text value instead of being routed back up.
bool ApplyPlainTextRenderer(UCommonTextBlock* Widget, const FPreparedTextValue& Value, FString& OutError)
{
    if (Value.ResolvedStyleClass == nullptr || !Value.bHasResolvedPresentation
        || Value.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: plain renderer rejected the resolved text");
        return false;
    }
    Widget->SetStyle(Value.ResolvedStyleClass);
    const FPreparedTextScalePolicy Policy = ScalePolicyOf(Value);
    Widget->SetText(Value.Text);
    FSlateFontInfo FontInfo = Widget->GetFont();
    FontInfo.Size = EvaluatePreparedFontSize(Policy, ResolveLiveViewportHeight(Widget, Policy.ReferenceViewportHeight));
    Widget->SetFont(FontInfo);
    return true;
}

bool ApplyRichTextRenderer(UCommonRichTextBlock* Widget, const FPreparedTextValue& Value, FString& OutError)
{
    if (!Value.bHasResolvedPresentation || (Value.NormalizedMarkup.IsEmpty() && !Value.Text.IsEmpty()))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: rich renderer rejected the resolved text");
        return false;
    }
    if (Value.ResolvedStyleClass != nullptr)
    {
        Widget->SetStyle(Value.ResolvedStyleClass);
    }
    if (Value.bHasResolvedDefaultStyle)
    {
        FTextBlockStyle FinalStyle = Value.ResolvedDefaultStyle;
        const FPreparedTextScalePolicy Policy = ScalePolicyOf(Value);
        FinalStyle.SetFontSize(EvaluatePreparedFontSize(
            Policy, ResolveLiveViewportHeight(Widget, Policy.ReferenceViewportHeight)));
        Widget->SetDefaultTextStyle(FinalStyle);
    }
    Widget->SetText(FText::FromString(Value.NormalizedMarkup));
    return true;
}

// The prepared text operation, for every kind of target it can carry.
bool DispatchPreparedText(UWidget* Target, const FPreparedTextValue& Value, bool bIsReset, FString& OutError)
{
    if (Target == nullptr)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    // A GV2-owned text host writes through its own value sink, which performs the same
    // renderer write the pipeline used to perform for it.
    if (IGV2PreparedTextTarget* Role = Cast<IGV2PreparedTextTarget>(Target))
    {
        return Role->ApplyPreparedText(Value, bIsReset, OutError);
    }

    // A plain renderer: apply it here, then let the owning host -- if there is one -- record
    // the same text, exactly as the adapter's parent-notify did.
    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(Target))
    {
        if (IGV2PreparedTextTarget* Owner = FindRole<IGV2PreparedTextTarget>(TextBlock->GetOuter()))
        {
            FString OwnerError;
            Owner->ApplyPreparedText(Value, bIsReset, OwnerError);
        }
        return ApplyPlainTextRenderer(TextBlock, Value, OutError);
    }
    if (UCommonRichTextBlock* RichBlock = Cast<UCommonRichTextBlock>(Target))
    {
        if (IGV2PreparedTextTarget* Owner = FindRole<IGV2PreparedTextTarget>(RichBlock->GetOuter()))
        {
            FString OwnerError;
            Owner->ApplyPreparedText(Value, bIsReset, OwnerError);
        }
        return ApplyRichTextRenderer(RichBlock, Value, OutError);
    }
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Target))
    {
        EditableBox->SetHintText(Value.Text);
        return true;
    }

    OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a supported text renderer");
    return false;
}

void ApplyCentralStyleRole(UWidget* Widget, const FPreparedCentralStylePayload& Payload, bool& bFailed, FString& OutError)
{
    // A role delivered to a target that does not declare it is a defect, not a no-op: the
    // whole point of one interface per role is that the mismatch is diagnosable.
    auto Mismatch = [Widget, &bFailed, &OutError](const TCHAR* RoleName)
    {
        bFailed = true;
        OutError = FString::Printf(
            TEXT("central_style_target_mismatch: %s style targets '%s'"),
            RoleName,
            Widget != nullptr ? *Widget->GetClass()->GetName() : TEXT("<null>"));
    };

    Visit(TOverloaded{
        [&](const FPreparedSeparatorStyle& Style)
        {
            if (IGV2PreparedSeparatorStyleTarget* Role = Cast<IGV2PreparedSeparatorStyleTarget>(Widget))
            { Role->ApplyPreparedSeparatorStyle(Style); } else { Mismatch(TEXT("separator")); }
        },
        [&](const FPreparedTintStyle& Style)
        {
            if (IGV2PreparedTintStyleTarget* Role = Cast<IGV2PreparedTintStyleTarget>(Widget))
            { Role->ApplyPreparedTintStyle(Style); } else { Mismatch(TEXT("tint")); }
        },
        [&](const FPreparedItemPaddingStyle& Style)
        {
            if (IGV2PreparedItemPaddingStyleTarget* Role = Cast<IGV2PreparedItemPaddingStyleTarget>(Widget))
            { Role->ApplyPreparedItemPaddingStyle(Style); } else { Mismatch(TEXT("item padding")); }
        },
        [&](const FPreparedProgressBarStyle& Style)
        {
            if (IGV2PreparedProgressBarStyleTarget* Role = Cast<IGV2PreparedProgressBarStyleTarget>(Widget))
            { Role->ApplyPreparedProgressBarStyle(Style); } else { Mismatch(TEXT("progress bar")); }
        },
        [&](const FPreparedLoadingIndicatorStyle& Style)
        {
            if (IGV2PreparedLoadingIndicatorStyleTarget* Role = Cast<IGV2PreparedLoadingIndicatorStyleTarget>(Widget))
            { Role->ApplyPreparedLoadingIndicatorStyle(Style); } else { Mismatch(TEXT("loading indicator")); }
        },
        [&](const FPreparedButtonStyle& Style)
        {
            if (IGV2PreparedButtonStyleTarget* Role = Cast<IGV2PreparedButtonStyleTarget>(Widget))
            { Role->ApplyPreparedButtonStyle(Style); } else { Mismatch(TEXT("button")); }
        },
        [&](const FPreparedCheckboxStyle& Style)
        {
            if (IGV2PreparedCheckboxStyleTarget* Role = Cast<IGV2PreparedCheckboxStyleTarget>(Widget))
            { Role->ApplyPreparedCheckboxStyle(Style); } else { Mismatch(TEXT("checkbox")); }
        },
        [&](const FPreparedInputFieldStyle& Style)
        {
            if (IGV2PreparedInputFieldStyleTarget* Role = Cast<IGV2PreparedInputFieldStyleTarget>(Widget))
            { Role->ApplyPreparedInputFieldStyle(Style); } else { Mismatch(TEXT("input field")); }
        },
        [&](const FPreparedDropdownStyle& Style)
        {
            if (IGV2PreparedDropdownStyleTarget* Role = Cast<IGV2PreparedDropdownStyleTarget>(Widget))
            { Role->ApplyPreparedDropdownStyle(Style); } else { Mismatch(TEXT("dropdown")); }
        },
        [&](const FPreparedRichTextStyle& Style)
        {
            if (IGV2PreparedRichTextStyleTarget* Role = Cast<IGV2PreparedRichTextStyleTarget>(Widget))
            { Role->ApplyPreparedRichTextStyle(Style); } else { Mismatch(TEXT("rich text")); }
        },
        [&](const FPreparedRichTextPopoverStyle& Style)
        {
            if (IGV2PreparedRichTextPopoverStyleTarget* Role = Cast<IGV2PreparedRichTextPopoverStyleTarget>(Widget))
            { Role->ApplyPreparedRichTextPopoverStyle(Style); } else { Mismatch(TEXT("rich text popover")); }
        }
    }, Payload);
}
}
}

bool FGV2PresentationApply::Apply(
    const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction,
    FGV2PresentationApplyResult& OutResult)
{
    using namespace GV2PresentationApply;

    OutResult = FGV2PresentationApplyResult();
    bool bFailed = false;

    for (const FGV2PreparedOperationVariant& Operation : Transaction.GetOperations())
    {
        if (bFailed)
        {
            break;
        }

        Visit(TOverloaded{
            [&](const FPreparedImageResourceOperation& Op)
            {
                UImage* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    OutResult.Error = TEXT("Image resource operation's target widget is no longer valid.");
                    bFailed = true;
                    return;
                }
                Widget->SetBrush(Op.Brush);
                Widget->SetDesiredSizeOverride(Op.Brush.ImageSize);
            },
            [&](const FPreparedImageHostOperation& Op)
            {
                IGV2PreparedImageHostTarget* Role = Cast<IGV2PreparedImageHostTarget>(Op.TargetWidget.Get());
                if (Role == nullptr)
                {
                    return;
                }
                if (Op.bResetToDefault)
                {
                    Role->ResetPreparedImageHost();
                    return;
                }
                if (!Role->ApplyPreparedImageHost(Op.Resolved, OutResult.Error))
                {
                    bFailed = true;
                }
            },
            [&](const FPreparedBooleanOperation& Op)
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
                case EPreparedBooleanTarget::HostDeclaredBoolean:
                    if (IGV2PreparedBooleanTarget* Role = Cast<IGV2PreparedBooleanTarget>(Widget))
                    {
                        Role->ApplyPreparedBoolean(Op.PropertyName, Op.Value);
                    }
                    break;
                }
            },
            [&](const FPreparedEditableTextValueOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetText(Op.Value);
                }
            },
            [&](const FPreparedProgressBarOperation& Op)
            {
                if (UProgressBar* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetPercent(Op.Percent);
                }
            },
            [&](const FPreparedNumberOperation& Op)
            {
                if (IGV2PreparedNumberTarget* Role = Cast<IGV2PreparedNumberTarget>(Op.TargetWidget.Get()))
                {
                    Role->ApplyPreparedNumber(Op.Value);
                }
            },
            [&](const FPreparedIntegerOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (IGV2PreparedIntegerTarget* Role = FindRole<IGV2PreparedIntegerTarget>(Widget))
                {
                    Role->ApplyPreparedInteger(Op.Value);
                }
                if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                {
                    if (Op.Value > 0)
                    {
                        const FString Current = EditableBox->GetText().ToString();
                        if (Current.Len() > Op.Value)
                        {
                            EditableBox->SetText(FText::FromString(Current.Left(static_cast<int32>(Op.Value))));
                        }
                    }
                }
            },
            [&](const FPreparedStringOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                {
                    FString FinalText = Op.Value;
                    if (const IGV2PreparedIntegerTarget* Host = FindRole<IGV2PreparedIntegerTarget>(Widget))
                    {
                        const int64 MaxLength = Host->GetPreparedMaxLength();
                        if (MaxLength > 0 && FinalText.Len() > MaxLength)
                        {
                            FinalText = FinalText.Left(static_cast<int32>(MaxLength));
                        }
                    }
                    EditableBox->SetText(FText::FromString(FinalText));
                }
            },
            [&](const FPreparedKeyOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                IGV2PreparedKeyTarget* Role = Cast<IGV2PreparedKeyTarget>(Widget);
                if (Role != nullptr && Role->ApplyPreparedKey(Op.PropertyName, FName(*Op.Value)))
                {
                    return;
                }
                // A host that declares a `key`-kind capability but routes nothing would
                // otherwise report a successful commit while storing nothing -- the exact
                // shape this pipeline exists to make impossible.
                OutResult.Error = FString::Printf(
                    TEXT("core:diagnostic.ui_consumer.unhandled_target: '%s' capability declared for '%s' has no commit branch"),
                    *Op.PropertyName.ToString(),
                    Widget != nullptr ? *Widget->GetClass()->GetName() : TEXT("<null>"));
                bFailed = true;
            },
            [&](const FPreparedBindingOperation& Op)
            {
                if (IGV2PreparedBindingTarget* Role = Cast<IGV2PreparedBindingTarget>(Op.TargetWidget.Get()))
                {
                    Role->ApplyPreparedBinding(Op.SerializedHandle);
                }
            },
            [&](const FPreparedRichTextSpansOperation& Op)
            {
                if (IGV2PreparedRichTextSpansTarget* Role = FindRole<IGV2PreparedRichTextSpansTarget>(Op.TargetWidget.Get()))
                {
                    Role->ApplyPreparedRichTextSpans(Op.Spans);
                }
            },
            [&](const FPreparedTextOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                if (Op.bIsReset)
                {
                    // Fire-and-forget, matching Reset()'s own shape, which never checked
                    // these return values either.
                    FString ResetError;
                    DispatchPreparedText(Widget, Op.Value, true, ResetError);
                    return;
                }
                if (!DispatchPreparedText(Widget, Op.Value, false, OutResult.Error))
                {
                    bFailed = true;
                }
            },
            [&](const FPreparedKeyedCollectionOperation& Op)
            {
                UWidget* TargetWidget = Op.TargetWidget.Get();
                if (TargetWidget == nullptr)
                {
                    return;
                }
                IGV2PreparedKeyedCollectionTarget* Role = Cast<IGV2PreparedKeyedCollectionTarget>(TargetWidget);

                if (Op.bIsReset)
                {
                    if (Role != nullptr)
                    {
                        Role->ResetPreparedCollection();
                    }
                    else if (UPanelWidget* Panel = Cast<UPanelWidget>(TargetWidget))
                    {
                        Panel->ClearChildren();
                    }
                    return;
                }

                UPanelWidget* Panel = Role != nullptr
                    ? Role->GetPreparedCollectionPanel()
                    : Cast<UPanelWidget>(TargetWidget);
                if (Panel != nullptr)
                {
                    Panel->ClearChildren();
                    for (const FPreparedKeyedCollectionEntry& Entry : Op.OrderedEntries)
                    {
                        if (UWidget* Child = Entry.Widget.Get())
                        {
                            Panel->AddChild(Child);
                        }
                    }
                }
                if (Role != nullptr)
                {
                    Role->OnPreparedCollectionSettled(Op.OrderedEntries);
                }
                // The collection's panel may be an inner primitive whose OWNER keeps the
                // header/selection bookkeeping; notify it too, as the adapter's outer walk did.
                if (IGV2PreparedKeyedCollectionTarget* Owner =
                        FindRole<IGV2PreparedKeyedCollectionTarget>(TargetWidget->GetOuter()))
                {
                    if (Owner != Role)
                    {
                        Owner->OnPreparedCollectionSettled(Op.OrderedEntries);
                    }
                }
            },
            [&](const FPreparedTabContainerOperation& Op)
            {
                IGV2PreparedTabContainerTarget* Role = FindRole<IGV2PreparedTabContainerTarget>(Op.TargetWidget.Get());
                if (Role == nullptr)
                {
                    return;
                }
                if (Op.bIsReset)
                {
                    Role->ResetPreparedTabs();
                    return;
                }
                Role->ApplyPreparedTabs(Op.Entries);
            },
            [&](const FPreparedPlainTextOperation& Op)
            {
                UCommonTextBlock* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                if (Op.Style != nullptr)
                {
                    Widget->SetStyle(Op.Style);
                }
                Widget->SetText(Op.Text);
                FSlateFontInfo FontInfo = Widget->GetFont();
                const float ViewportHeight = ResolveLiveViewportHeight(Widget, Op.ScalePolicy.ReferenceViewportHeight);
                FontInfo.Size = EvaluatePreparedFontSize(Op.ScalePolicy, ViewportHeight);
                Widget->SetFont(FontInfo);
            },
            [&](const FPreparedRichTextRenderOperation& Op)
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
                    FTextBlockStyle FinalStyle = Op.DefaultStyle;
                    const float ViewportHeight = ResolveLiveViewportHeight(Widget, Op.ScalePolicy.ReferenceViewportHeight);
                    FinalStyle.SetFontSize(EvaluatePreparedFontSize(Op.ScalePolicy, ViewportHeight));
                    Widget->SetDefaultTextStyle(FinalStyle);
                }
                Widget->SetText(FText::FromString(Op.Markup));
            },
            [&](const FPreparedTextHintOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetHintText(Op.Text);
                }
            },
            [&](const FPreparedCentralStyleOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    // A target that no longer exists is nothing left to write, not a role
                    // mismatch: every other operation here treats a dead weak pointer that
                    // way, and so did the dispatch this replaced, which returned before it
                    // looked at the payload at all. Reporting a mismatch instead would fail
                    // the whole transaction for a widget that was legitimately collected.
                    return;
                }
                ApplyCentralStyleRole(Widget, Op.Payload, bFailed, OutResult.Error);
            }
        }, Operation);

        if (!bFailed)
        {
            ++OutResult.AppliedOperationCount;
        }
    }

    OutResult.bApplied = !bFailed;
    return OutResult.bApplied;
}
