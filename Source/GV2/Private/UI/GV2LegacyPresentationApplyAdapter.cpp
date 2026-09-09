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
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"

namespace
{
// PSC-10A: same "overloaded lambda set" idiom as GV2PresentationApply::Apply() -- see
// that function's own TOverloaded doc comment. This module's copy is independent (this
// file lives in GV2, the other in GV2PresentationApply; duplicating five lines avoids
// creating a dependency between them purely for a generic idiom).
template <typename... Ts>
struct TOverloaded : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts>
TOverloaded(Ts...) -> TOverloaded<Ts...>;

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
    OutError.Reset();
    bool bFailed = false;

    for (const GV2PresentationApply::FGV2PreparedOperationVariant& Operation : Transaction.GetOperations())
    {
        if (bFailed)
        {
            break;
        }

        Visit(TOverloaded{
            // ImageResource/EditableTextValue/ProgressBar/PlainText/RichTextRender/
            // TextHint operations are entirely GV2PresentationApply's own territory
            // (plain Engine/UMG/CommonUI targets) -- nothing to do here.
            [](const GV2PresentationApply::FPreparedImageResourceOperation&) {},
            [&bFailed, &OutError](const GV2PresentationApply::FPreparedImageHostOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Op.bResetToDefault)
                {
                    // Reaches past the host's own Apply UFUNCTION directly into its
                    // inner UImage, the same way Commit()'s Reset always did -- Reset
                    // clears the physical brush without going through bookkeeping meant
                    // for a resolved Commit.
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
                    return;
                }

                FGV2ResolvedImageResource Resolved;
                Resolved.ResourceId = Op.Resolved.ResourceId;
                Resolved.RenderMode = FromPreparedRenderMode(Op.Resolved.RenderMode);
                Resolved.FixedAspectRatio = Op.Resolved.FixedAspectRatio;
                Resolved.Brush = Op.Resolved.Brush;

                if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(Widget))
                {
                    if (!ImageBase->ApplyResolvedImageResource(Resolved, OutError))
                    {
                        bFailed = true;
                    }
                }
                else if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(Widget))
                {
                    if (!PortraitWidget->ApplyResolvedPortrait(Resolved, OutError))
                    {
                        bFailed = true;
                    }
                }
            },
            [](const GV2PresentationApply::FPreparedBooleanOperation& Op)
            {
                if (Op.Target != GV2PresentationApply::EPreparedBooleanTarget::RequiresLegacyAdapter)
                {
                    return;
                }
                UWidget* Widget = Op.TargetWidget.Get();
                if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(Widget))
                {
                    if (Op.PropertyName == TEXT("is_open"))
                    {
                        Dropdown->SetDropdownOpen(Op.Value);
                    }
                }
            },
            [](const GV2PresentationApply::FPreparedEditableTextValueOperation&) {},
            [](const GV2PresentationApply::FPreparedProgressBarOperation&) {},
            [](const GV2PresentationApply::FPreparedNumberOperation& Op)
            {
                if (UGV2ProgressBarWidgetBase* ProgressHost = Cast<UGV2ProgressBarWidgetBase>(Op.TargetWidget.Get()))
                {
                    ProgressHost->ApplyProgress(static_cast<float>(Op.Value));
                }
            },
            [](const GV2PresentationApply::FPreparedIntegerOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                {
                    if (UGV2InputFieldWidgetBase* InputField = EditableBox->GetTypedOuter<UGV2InputFieldWidgetBase>())
                    {
                        InputField->SetMaxLength(Op.Value);
                    }
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
            [](const GV2PresentationApply::FPreparedStringOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                {
                    FString FinalText = Op.Value;
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
            },
            [&bFailed, &OutError](const GV2PresentationApply::FPreparedKeyOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                // DUC-03: "selected_key" and "default_tab_key" are their own
                // capabilities, not the generic `key` identity, and are routed by name
                // so they can never be shadowed by (or shadow) a host's real `key`.
                if (Op.PropertyName == TEXT("selected_key"))
                {
                    if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(Widget))
                    {
                        Dropdown->SetSelectedKey(FName(*Op.Value));
                        return;
                    }
                }
                else if (Op.PropertyName == TEXT("default_tab_key"))
                {
                    if (UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(Widget))
                    {
                        TabContainer->ApplyDefaultTabKey(FName(*Op.Value));
                        return;
                    }
                }
                else if (IGV2UiPropertyHost* Host = Cast<IGV2UiPropertyHost>(Widget))
                {
                    Host->SetKey(FName(*Op.Value));
                    return;
                }

                // A host that declares a `key`-kind capability but has no branch here
                // would otherwise report a successful commit while storing nothing --
                // the exact shape this pipeline exists to make impossible. Unhandled
                // target type/property is a defect, not a no-op.
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_consumer.unhandled_target: '%s' capability declared for '%s' has no commit branch"),
                    *Op.PropertyName.ToString(),
                    Widget != nullptr ? *Widget->GetClass()->GetName() : TEXT("<null>"));
                bFailed = true;
            },
            [](const GV2PresentationApply::FPreparedBindingOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(Widget))
                {
                    BindingTarget->SetBindingHandle(FGV2UiBindingHandle::FromSerialized(Op.SerializedHandle));
                }
            },
            [](const GV2PresentationApply::FPreparedRichTextSpansOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                UGV2RichTextWidgetBase* RichTextWidget = Cast<UGV2RichTextWidgetBase>(Widget);
                if (!RichTextWidget && Widget)
                {
                    RichTextWidget = Widget->GetTypedOuter<UGV2RichTextWidgetBase>();
                }
                if (RichTextWidget == nullptr)
                {
                    return;
                }

                TArray<FGV2RichTextSpanViewModel> Spans;
                Spans.Reserve(Op.Spans.Num());
                for (const GV2PresentationApply::FPreparedRichTextSpan& FlatSpan : Op.Spans)
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
            },
            [&bFailed, &OutError](const GV2PresentationApply::FPreparedTextOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }

                FGV2TextViewModel Text;
                Text.Text = Op.Value.Text;
                Text.StyleToken = Op.Value.StyleToken;
                Text.NormalizedMarkup = Op.Value.NormalizedMarkup;

                if (Op.bIsReset)
                {
                    // Fire-and-forget, matching Reset()'s own original shape, which
                    // never checked these return values either.
                    DispatchTextOperation(Widget, Text, true, nullptr);
                }
                else if (!DispatchTextOperation(Widget, Text, false, &OutError))
                {
                    bFailed = true;
                }
            },
            [](const GV2PresentationApply::FPreparedKeyedCollectionOperation& Op)
            {
                UWidget* TargetWidget = Op.TargetWidget.Get();
                if (TargetWidget == nullptr)
                {
                    return;
                }

                UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget);
                UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(TargetWidget);

                if (Op.bIsReset)
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
                    return;
                }

                UPanelWidget* Panel = ListView
                    ? ListView->GetContainerPanel()
                    : (ButtonList ? Cast<UPanelWidget>(ButtonList->GetButtonContainer()) : Cast<UPanelWidget>(TargetWidget));

                if (Panel != nullptr)
                {
                    Panel->ClearChildren();
                    for (const GV2PresentationApply::FPreparedKeyedCollectionEntry& Entry : Op.OrderedEntries)
                    {
                        if (UWidget* Child = Entry.Widget.Get())
                        {
                            Panel->AddChild(Child);
                        }
                    }
                }

                // Newly added slots have no styling of their own; let the container
                // reapply its central style (e.g. per-item slot padding) now that the
                // collection has settled.
                //
                // PSC-10B: this is the last surviving PULL of central style from a Commit
                // path, and it is on the list to go. For a class already converted to the
                // push model it is already a no-op -- FGV2LayeredUiReconciler applies that
                // class's prepared central-style operation right after this screen's field
                // commit, which is after this line has run and the slots exist. It stays
                // only until the remaining style consumers are converted, at which point
                // it is deleted rather than left as an alternative route.
                if (TargetWidget->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()))
                {
                    IGV2UiStyleConsumer::Execute_ApplyCentralStyle(TargetWidget);
                }

                if (ListView != nullptr)
                {
                    TMap<FName, TObjectPtr<UWidget>> ActiveWidgetsByKey;
                    for (const GV2PresentationApply::FPreparedKeyedCollectionEntry& Entry : Op.OrderedEntries)
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
            },
            [](const GV2PresentationApply::FPreparedTabContainerOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(Widget);
                if (!TabContainer && Widget)
                {
                    TabContainer = Widget->GetTypedOuter<UGV2TabContainerWidgetBase>();
                }
                if (TabContainer == nullptr)
                {
                    return;
                }

                if (Op.bIsReset)
                {
                    TabContainer->ResetTabContainerModel();
                    return;
                }

                TArray<FGV2TabItemEntry> Entries;
                TMap<FName, UGV2ScreenWidgetBase*> Widgets;
                Entries.Reserve(Op.Entries.Num());
                for (const GV2PresentationApply::FPreparedTabEntry& FlatEntry : Op.Entries)
                {
                    FGV2TabItemEntry Entry;
                    Entry.Key = FlatEntry.Key;
                    Entry.Title.Text = FlatEntry.Title.Text;
                    Entry.Title.StyleToken = FlatEntry.Title.StyleToken;
                    Entry.Title.NormalizedMarkup = FlatEntry.Title.NormalizedMarkup;
                    Entry.ScreenId = FlatEntry.ScreenId;
                    Entries.Add(MoveTemp(Entry));

                    if (UGV2ScreenWidgetBase* ScreenWidget = Cast<UGV2ScreenWidgetBase>(FlatEntry.ScreenWidget.Get()))
                    {
                        Widgets.Add(FlatEntry.Key, ScreenWidget);
                    }
                }
                TabContainer->ApplyTabEntries(Entries, Widgets);
            },
            [](const GV2PresentationApply::FPreparedPlainTextOperation&) {},
            [](const GV2PresentationApply::FPreparedRichTextRenderOperation&) {},
            [](const GV2PresentationApply::FPreparedTextHintOperation&) {},
            [&bFailed, &OutError](const GV2PresentationApply::FPreparedCentralStyleOperation& Op)
            {
                // PSC-10B: every branch is a pure value handoff into the target's own
                // value-sink method. No Theme, no settings, no token lookup -- Prepare
                // already turned the theme into the brush/colour/margin sitting in Op.
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }

                Visit(TOverloaded{
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedSeparatorStyle& Style)
                    {
                        UGV2SeparatorWidgetBase* Separator = Cast<UGV2SeparatorWidgetBase>(Widget);
                        if (Separator == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: separator style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        Separator->ApplySeparatorStyleValues(Style.Brush, Style.Thickness, Style.bHorizontal);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedTintStyle& Style)
                    {
                        UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(Widget);
                        if (ImageBase == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: tint style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        ImageBase->ApplyImageTintStyleValue(Style.Tint);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedItemPaddingStyle& Style)
                    {
                        UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(Widget);
                        if (ButtonList == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: item padding style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        ButtonList->ApplyItemPaddingStyleValue(Style.Padding);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedProgressBarStyle& Style)
                    {
                        UGV2ProgressBarWidgetBase* ProgressBar = Cast<UGV2ProgressBarWidgetBase>(Widget);
                        if (ProgressBar == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: progress bar style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        ProgressBar->ApplyProgressBarStyleValues(Style.WidgetStyle, Style.FillColor);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedLoadingIndicatorStyle& Style)
                    {
                        UGV2LoadingIndicatorWidgetBase* LoadingIndicator = Cast<UGV2LoadingIndicatorWidgetBase>(Widget);
                        if (LoadingIndicator == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: loading indicator style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        LoadingIndicator->ApplyLoadingIndicatorStyleValues(Style.Brush, Style.Period, Style.Radius, Style.Pieces);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedButtonStyle& Style)
                    {
                        UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(Widget);
                        if (Button == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: button style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        Button->ApplyButtonStyleValues(Style.ButtonStyle, Style.DefaultLabelStyle, Style.DefaultLabelScale);
                    },
                    [Widget, &bFailed, &OutError](const GV2PresentationApply::FPreparedCheckboxStyle& Style)
                    {
                        UGV2CheckboxWidgetBase* Checkbox = Cast<UGV2CheckboxWidgetBase>(Widget);
                        if (Checkbox == nullptr)
                        {
                            bFailed = true;
                            OutError = FString::Printf(TEXT("central_style_target_mismatch: checkbox style targets '%s'"), *Widget->GetClass()->GetName());
                            return;
                        }
                        Checkbox->ApplyCheckboxStyleValues(Style.WidgetStyle, Style.DefaultLabelStyle);
                    }
                }, Op.Payload);
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
