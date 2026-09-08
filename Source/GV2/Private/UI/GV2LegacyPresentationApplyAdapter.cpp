#include "UI/GV2LegacyPresentationApplyAdapter.h"

#include "Bridge/GV2BridgeTypes.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"

namespace
{
EGV2ImageRenderMode FromPreparedRenderMode(GV2PresentationApply::EPreparedImageRenderMode RenderMode)
{
    switch (RenderMode)
    {
    case GV2PresentationApply::EPreparedImageRenderMode::NineSlice:
        return EGV2ImageRenderMode::NineSlice;
    case GV2PresentationApply::EPreparedImageRenderMode::Tile:
        return EGV2ImageRenderMode::Tile;
    case GV2PresentationApply::EPreparedImageRenderMode::FixedAspect:
        return EGV2ImageRenderMode::FixedAspect;
    }
    return EGV2ImageRenderMode::FixedAspect;
}

// PSC-09B: replicates FGV2TextPropertyConsumer::Commit()/Reset()'s exact original
// per-target dispatch (now reading a reconstructed FGV2TextViewModel instead of the
// consumer's own PreparedText member) -- every real target is either a GV2-owned widget
// wrapper or reached only through GV2's own UGV2TextPipeline UCLASS. bIsReset selects
// between the two ORIGINALLY DIFFERENT UGV2RichTextWidgetBase redirect rules (Commit
// redirected to the inner RichTextBlock when present; Reset did not); OutError is null
// for a Reset call (fire-and-forget, matching Reset()'s own original shape, which never
// checked these return values either).
bool DispatchTextOperation(UWidget* TargetWidget, const FGV2TextViewModel& Text, bool bIsReset, FString* OutError)
{
    UWidget* ResolvedWidget = TargetWidget;
    if (UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = TW->GetTextBlock() ? Cast<UWidget>(TW->GetTextBlock()) : Cast<UWidget>(TW);
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = bIsReset
            ? Cast<UWidget>(RTW)
            : (RTW->GetRichTextBlock() ? Cast<UWidget>(RTW->GetRichTextBlock()) : Cast<UWidget>(RTW));
    }
    else if (UGV2ButtonWidgetBase* BW = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        ResolvedWidget = BW;
    }
    else if (UGV2DropdownSelectWidgetBase* DW = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        ResolvedWidget = DW;
    }

    if (!ResolvedWidget)
    {
        if (OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        }
        return false;
    }

    if (UGV2TextWidgetBase* TextWidget = Cast<UGV2TextWidgetBase>(ResolvedWidget))
    {
        const bool bResult = TextWidget->ApplyText(Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2TextWidgetBase::ApplyText rejected the resolved text");
        }
        return bResult;
    }
    if (UGV2ButtonWidgetBase* ButtonWidget = Cast<UGV2ButtonWidgetBase>(ResolvedWidget))
    {
        const bool bResult = ButtonWidget->ApplyText(Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2ButtonWidgetBase::ApplyText rejected the resolved text");
        }
        return bResult;
    }
    if (UGV2DropdownSelectWidgetBase* DropdownWidget = Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget))
    {
        const bool bResult = DropdownWidget->ApplyPlaceholderText(Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2DropdownSelectWidgetBase::ApplyPlaceholderText rejected the resolved text");
        }
        return bResult;
    }
    if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(ResolvedWidget))
    {
        const bool bResult = RichText->ApplyText(Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2RichTextWidgetBase::ApplyText rejected the resolved text");
        }
        return bResult;
    }
    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(ResolvedWidget))
    {
        if (UGV2ButtonWidgetBase* ParentButton = TextBlock->GetTypedOuter<UGV2ButtonWidgetBase>())
        {
            ParentButton->ApplyText(Text);
        }
        else if (UGV2TextWidgetBase* ParentTW = TextBlock->GetTypedOuter<UGV2TextWidgetBase>())
        {
            ParentTW->ApplyText(Text);
        }
        const bool bResult = UGV2TextPipeline::Apply(TextBlock, Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2TextPipeline::Apply rejected the resolved text");
        }
        return bResult;
    }
    if (UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(ResolvedWidget))
    {
        if (UGV2RichTextWidgetBase* ParentRT = RichTextBlock->GetTypedOuter<UGV2RichTextWidgetBase>())
        {
            ParentRT->ApplyText(Text);
        }
        const bool bResult = UGV2TextPipeline::ApplyRichText(RichTextBlock, Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2TextPipeline::ApplyRichText rejected the resolved text");
        }
        return bResult;
    }
    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(ResolvedWidget))
    {
        const bool bResult = UGV2TextPipeline::ApplyHint(EditableBox, Text);
        if (!bResult && OutError != nullptr)
        {
            *OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2TextPipeline::ApplyHint rejected the resolved text");
        }
        return bResult;
    }

    if (OutError != nullptr)
    {
        *OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a supported text renderer");
    }
    return false;
}
}

namespace GV2LegacyPresentationApplyAdapter
{
bool Apply(const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction, FString& OutError)
{
    for (const GV2PresentationApply::FPreparedImageHostOperation& Operation : Transaction.GetImageHostOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (Operation.bResetToDefault)
        {
            // Reaches past the host's own Apply UFUNCTION directly into its inner
            // UImage, the same way Commit()'s Reset always did -- Reset clears the
            // physical brush without going through bookkeeping meant for a resolved
            // Commit.
            if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(Widget))
            {
                PortraitWidget->SetVisibility(ESlateVisibility::Collapsed);
                if (UImage* Img = PortraitWidget->GetPortraitImage())
                {
                    Img->SetBrush(FSlateBrush());
                }
            }
            else if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(Widget))
            {
                if (UImage* Img = ImageBase->GetImageWidget())
                {
                    Img->SetBrush(FSlateBrush());
                }
            }
            continue;
        }

        FGV2ResolvedImageResource Resolved;
        Resolved.ResourceId = Operation.Resolved.ResourceId;
        Resolved.RenderMode = FromPreparedRenderMode(Operation.Resolved.RenderMode);
        Resolved.FixedAspectRatio = Operation.Resolved.FixedAspectRatio;
        Resolved.Brush = Operation.Resolved.Brush;

        if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(Widget))
        {
            if (!ImageBase->ApplyResolvedImageResource(Resolved, OutError))
            {
                return false;
            }
        }
        else if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(Widget))
        {
            if (!PortraitWidget->ApplyResolvedPortrait(Resolved, OutError))
            {
                return false;
            }
        }
    }

    for (const GV2PresentationApply::FPreparedBooleanOperation& Operation : Transaction.GetBooleanOperations())
    {
        if (Operation.Target != GV2PresentationApply::EPreparedBooleanTarget::RequiresLegacyAdapter)
        {
            continue;
        }
        UWidget* Widget = Operation.TargetWidget.Get();
        if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(Widget))
        {
            if (Operation.PropertyName == TEXT("is_open"))
            {
                Dropdown->SetDropdownOpen(Operation.Value);
            }
        }
    }

    for (const GV2PresentationApply::FPreparedNumberOperation& Operation : Transaction.GetNumberOperations())
    {
        if (UGV2ProgressBarWidgetBase* ProgressHost = Cast<UGV2ProgressBarWidgetBase>(Operation.TargetWidget.Get()))
        {
            ProgressHost->ApplyProgress(static_cast<float>(Operation.Value));
        }
    }

    for (const GV2PresentationApply::FPreparedIntegerOperation& Operation : Transaction.GetIntegerOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
        {
            if (UGV2InputFieldWidgetBase* InputField = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
            {
                InputField->SetMaxLength(Operation.Value);
            }
            if (Operation.Value > 0)
            {
                const FString Current = EditableBox->GetText().ToString();
                if (Current.Len() > Operation.Value)
                {
                    EditableBox->SetText(FText::FromString(Current.Left(static_cast<int32>(Operation.Value))));
                }
            }
        }
    }

    for (const GV2PresentationApply::FPreparedStringOperation& Operation : Transaction.GetStringOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
        {
            FString FinalText = Operation.Value;
            if (const UGV2InputFieldWidgetBase* Host = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
            {
                const int64 MaxLength = Host->GetMaxLength();
                if (MaxLength > 0 && FinalText.Len() > MaxLength)
                {
                    FinalText = FinalText.Left(static_cast<int32>(MaxLength));
                }
            }
            EditableBox->SetText(FText::FromString(FinalText));
        }
    }

    for (const GV2PresentationApply::FPreparedKeyOperation& Operation : Transaction.GetKeyOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        // DUC-03: "selected_key" and "default_tab_key" are their own capabilities, not
        // the generic `key` identity, and are routed by name so they can never be
        // shadowed by (or shadow) a host's real `key`.
        if (Operation.PropertyName == TEXT("selected_key"))
        {
            if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(Widget))
            {
                Dropdown->SetSelectedKey(FName(*Operation.Value));
                continue;
            }
        }
        else if (Operation.PropertyName == TEXT("default_tab_key"))
        {
            if (UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(Widget))
            {
                TabContainer->ApplyDefaultTabKey(FName(*Operation.Value));
                continue;
            }
        }
        else if (IGV2UiPropertyHost* Host = Cast<IGV2UiPropertyHost>(Widget))
        {
            Host->SetKey(FName(*Operation.Value));
            continue;
        }

        // A host that declares a `key`-kind capability but has no branch here would
        // otherwise report a successful commit while storing nothing -- the exact shape
        // this pipeline exists to make impossible. Unhandled target type/property is a
        // defect, not a no-op.
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.unhandled_target: '%s' capability declared for '%s' has no commit branch"),
            *Operation.PropertyName.ToString(),
            Widget != nullptr ? *Widget->GetClass()->GetName() : TEXT("<null>"));
        return false;
    }

    for (const GV2PresentationApply::FPreparedBindingOperation& Operation : Transaction.GetBindingOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(Widget))
        {
            BindingTarget->SetBindingHandle(FGV2UiBindingHandle::FromSerialized(Operation.SerializedHandle));
        }
    }

    for (const GV2PresentationApply::FPreparedRichTextSpansOperation& Operation : Transaction.GetRichTextSpansOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        UGV2RichTextWidgetBase* RichTextWidget = Cast<UGV2RichTextWidgetBase>(Widget);
        if (!RichTextWidget && Widget)
        {
            RichTextWidget = Widget->GetTypedOuter<UGV2RichTextWidgetBase>();
        }
        if (RichTextWidget == nullptr)
        {
            continue;
        }

        TArray<FGV2RichTextSpanViewModel> Spans;
        Spans.Reserve(Operation.Spans.Num());
        for (const GV2PresentationApply::FPreparedRichTextSpan& FlatSpan : Operation.Spans)
        {
            FGV2RichTextSpanViewModel Span;
            Span.SpanId = FlatSpan.SpanId;
            Span.Key = FlatSpan.Key;
            Span.Hover.Title.Text = FlatSpan.Hover.Title;
            Span.Hover.Description.Text = FlatSpan.Hover.Description;
            Span.Hover.ImageResourceId = FlatSpan.Hover.ImageResourceId;
            Span.Binding = FGV2UiBindingHandle::FromSerialized(FlatSpan.SerializedBinding);
            Spans.Add(MoveTemp(Span));
        }
        RichTextWidget->ApplySpans(Spans);
    }

    for (const GV2PresentationApply::FPreparedKeyedCollectionOperation& Operation : Transaction.GetKeyedCollectionOperations())
    {
        UWidget* TargetWidget = Operation.TargetWidget.Get();
        if (TargetWidget == nullptr)
        {
            continue;
        }

        UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget);
        UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(TargetWidget);

        if (Operation.bIsReset)
        {
            if (ListView != nullptr)
            {
                ListView->ClearEntries();
            }
            else if (ButtonList != nullptr)
            {
                if (ButtonList->GetButtonContainer())
                {
                    ButtonList->GetButtonContainer()->ClearChildren();
                }
            }
            else if (UPanelWidget* Panel = Cast<UPanelWidget>(TargetWidget))
            {
                Panel->ClearChildren();
            }
            continue;
        }

        UPanelWidget* Panel = ListView
            ? ListView->GetContainerPanel()
            : (ButtonList ? Cast<UPanelWidget>(ButtonList->GetButtonContainer()) : Cast<UPanelWidget>(TargetWidget));

        if (Panel != nullptr)
        {
            Panel->ClearChildren();
            for (const GV2PresentationApply::FPreparedKeyedCollectionEntry& Entry : Operation.OrderedEntries)
            {
                if (UWidget* Child = Entry.Widget.Get())
                {
                    Panel->AddChild(Child);
                }
            }
        }

        // Newly added slots have no styling of their own; let the container reapply its
        // central style (e.g. per-item slot padding) now that the collection has settled.
        if (TargetWidget->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()))
        {
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(TargetWidget);
        }

        if (ListView != nullptr)
        {
            TMap<FName, TObjectPtr<UWidget>> ActiveWidgetsByKey;
            for (const GV2PresentationApply::FPreparedKeyedCollectionEntry& Entry : Operation.OrderedEntries)
            {
                if (UWidget* Child = Entry.Widget.Get())
                {
                    ActiveWidgetsByKey.Add(Entry.Key, Child);
                }
            }
            ListView->SetActiveWidgetsMap(ActiveWidgetsByKey);
        }

        if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget->GetOuter()))
        {
            Dropdown->UpdateHeaderLabel();
        }
        else if (UGV2DropdownSelectWidgetBase* DropdownOuter = TargetWidget->GetTypedOuter<UGV2DropdownSelectWidgetBase>())
        {
            DropdownOuter->UpdateHeaderLabel();
        }
    }

    for (const GV2PresentationApply::FPreparedTextOperation& Operation : Transaction.GetTextOperations())
    {
        UWidget* Widget = Operation.TargetWidget.Get();
        if (Widget == nullptr)
        {
            continue;
        }

        FGV2TextViewModel Text;
        Text.Text = Operation.Value.Text;
        Text.StyleToken = Operation.Value.StyleToken;
        Text.NormalizedMarkup = Operation.Value.NormalizedMarkup;

        if (Operation.bIsReset)
        {
            DispatchTextOperation(Widget, Text, true, nullptr);
        }
        else if (!DispatchTextOperation(Widget, Text, false, &OutError))
        {
            return false;
        }
    }

    OutError.Reset();
    return true;
}
}
