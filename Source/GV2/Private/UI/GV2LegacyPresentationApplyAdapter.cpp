#include "UI/GV2LegacyPresentationApplyAdapter.h"

#include "Bridge/GV2BridgeTypes.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiPropertyHost.h"

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

    OutError.Reset();
    return true;
}
}
