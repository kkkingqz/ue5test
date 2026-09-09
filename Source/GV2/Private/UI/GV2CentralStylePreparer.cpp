#include "UI/GV2CentralStylePreparer.h"

#include "Application/GV2SessionContentSnapshot.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"

namespace
{
using namespace GV2PresentationApply;

FPreparedRichTextTokenStyle ResolveRichTextTokenStyle(
    const UGV2UiTheme& Theme,
    const FName Token,
    const TSubclassOf<UCommonTextStyle> StyleClass)
{
    FPreparedRichTextTokenStyle Result;
    // Mirrors ResolveStyleCore's own fallback: a declared token whose Style class is unset
    // resolves against the Theme's own TextStyle rather than producing nothing. Without it
    // the prepared table would disagree with the resolver it replaced for exactly the tokens
    // an author left half-filled.
    const TSubclassOf<UCommonTextStyle> EffectiveClass = StyleClass != nullptr ? StyleClass : Theme.TextStyle;
    Result.StyleClass = EffectiveClass;
    Result.UnscaledFontSize = Theme.ResolveUnscaledFontSize(Token);
    if (const UCommonTextStyle* CommonStyle = EffectiveClass != nullptr
            ? Cast<UCommonTextStyle>(EffectiveClass->GetDefaultObject())
            : nullptr)
    {
        CommonStyle->ToTextBlockStyle(Result.BaseStyle);
        Result.bResolved = true;
    }
    return Result;
}

FPreparedRichTextPopoverStyle ResolveRichTextPopoverStyle(const UGV2UiTheme& Theme)
{
    FPreparedRichTextPopoverStyle Result;
    Result.Background = Theme.RichTextPopoverBackground;
    Result.Padding = Theme.RichTextPopoverPadding;
    Result.MaxWidth = Theme.RichTextPopoverMaxWidth;
    Result.MaxHeight = Theme.RichTextPopoverMaxHeight;
    Result.ImageTint = Theme.ImageTint;
    Result.Scale.ScaleCurve = Theme.TextScaleCurve;
    Result.Scale.ReferenceViewportHeight = Theme.ReferenceViewportHeight;
    return Result;
}

// PSC-10B: the ONLY place a UGV2UiTheme is read on behalf of a styled widget. One `if` per
// style role, each turning theme fields into a finished value payload. A new styled class
// without a branch here emits nothing and is therefore visible as an unstyled widget, not as
// a silent second authority read -- there is no fallback path left for it to take.
void EmitForWidget(UWidget* Widget, const FGV2ResolvedUiTheme& ResolvedTheme, FGV2PreparedPresentationTransaction& OutTransaction)
{
    const UGV2UiTheme& Theme = *ResolvedTheme.Theme;

    if (UGV2SeparatorWidgetBase* Separator = Cast<UGV2SeparatorWidgetBase>(Widget))
    {
        FPreparedSeparatorStyle Style;
        Style.Brush = Theme.SeparatorBrush;
        Style.Thickness = Theme.SeparatorThickness;
        Style.bHorizontal = Separator->IsHorizontal();

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = Separator;
        Operation.Payload.Set<FPreparedSeparatorStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(Widget))
    {
        FPreparedTintStyle Style;
        Style.Tint = Theme.ImageTint;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = ImageBase;
        Operation.Payload.Set<FPreparedTintStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2ButtonListWidgetBase* ButtonList = Cast<UGV2ButtonListWidgetBase>(Widget))
    {
        FPreparedItemPaddingStyle Style;
        Style.Padding = Theme.ButtonListItemPadding;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = ButtonList;
        Operation.Payload.Set<FPreparedItemPaddingStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2ProgressBarWidgetBase* ProgressBar = Cast<UGV2ProgressBarWidgetBase>(Widget))
    {
        FPreparedProgressBarStyle Style;
        Style.WidgetStyle = Theme.ProgressBarStyle;
        Style.FillColor = Theme.ProgressFillColor;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = ProgressBar;
        Operation.Payload.Set<FPreparedProgressBarStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2LoadingIndicatorWidgetBase* LoadingIndicator = Cast<UGV2LoadingIndicatorWidgetBase>(Widget))
    {
        FPreparedLoadingIndicatorStyle Style;
        Style.Brush = Theme.LoadingIndicatorBrush;
        Style.Period = Theme.LoadingIndicatorPeriod;
        Style.Radius = Theme.LoadingIndicatorRadius;
        Style.Pieces = Theme.LoadingIndicatorPieces;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = LoadingIndicator;
        Operation.Payload.Set<FPreparedLoadingIndicatorStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    // Button must be tested before Checkbox only in the sense that both are independent
    // classes -- the order of these branches carries no precedence, each Cast is exact.
    if (UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(Widget))
    {
        FPreparedButtonStyle Style;
        Style.ButtonStyle = Theme.ButtonStyle;
        // The theme names the button-label default explicitly; the scale policy for it
        // comes from the theme's own default text token, resolved by the text pipeline so
        // this preparer does not become a second implementation of that math.
        Style.DefaultLabelStyle = Theme.ButtonLabelStyle;
        Style.DefaultLabelScale = UGV2TextPipeline::ResolveScalePolicyForTheme(&Theme, NAME_None);

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = Button;
        Operation.Payload.Set<FPreparedButtonStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2CheckboxWidgetBase* Checkbox = Cast<UGV2CheckboxWidgetBase>(Widget))
    {
        FPreparedCheckboxStyle Style;
        Style.WidgetStyle = Theme.CheckboxStyle;
        Style.DefaultLabelStyle = Theme.CheckboxLabelStyle;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = Checkbox;
        Operation.Payload.Set<FPreparedCheckboxStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(Widget))
    {
        FPreparedDropdownStyle Style;
        Style.HeaderStyle = Theme.DropdownHeaderStyle;
        Style.PopupBackground = Theme.DropdownPopupBackground;
        Style.PopupPadding = Theme.DropdownPopupPadding;
        Style.OptionItemPadding = Theme.DropdownOptionItemPadding;
        Style.MaxPopupHeight = Theme.DropdownMaxPopupHeight;
        Style.PopupScale.ScaleCurve = Theme.TextScaleCurve;
        Style.PopupScale.ReferenceViewportHeight = Theme.ReferenceViewportHeight;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = Dropdown;
        Operation.Payload.Set<FPreparedDropdownStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2InputFieldWidgetBase* InputField = Cast<UGV2InputFieldWidgetBase>(Widget))
    {
        FPreparedInputFieldStyle Style;
        Style.WidgetStyle = Theme.InputFieldStyle;
        Style.DefaultLabelStyle = Theme.InputFieldLabelStyle;
        // "body", not the theme's default text token: this class's token-less size has
        // always been the body size, and that is a behaviour to carry over, not to tidy.
        Style.DefaultLabelScale = UGV2TextPipeline::ResolveScalePolicyForTheme(&Theme, FName(TEXT("body")));

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = InputField;
        Operation.Payload.Set<FPreparedInputFieldStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
        return;
    }

    if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(Widget))
    {
        FPreparedRichTextStyle Style;
        Style.DefaultTokenName = Theme.DefaultTextStyleToken.IsNone()
            ? FName(TEXT("default"))
            : Theme.DefaultTextStyleToken;
        Style.DefaultStyleClass = Theme.RichTextStyle;
        Style.DefaultToken = ResolveRichTextTokenStyle(
            Theme,
            Style.DefaultTokenName,
            Theme.RichTextStyle);
        // The legacy default run used RichTextStyle verbatim; only named semantic style
        // and size tokens participate in viewport scaling. Preserve that distinction.
        Style.DefaultToken.UnscaledFontSize = 0.0f;
        for (const TPair<FName, FGV2TextStyleToken>& Pair : Theme.TextStyleTokens)
        {
            if (Pair.Key != Style.DefaultTokenName && Pair.Key != FName(TEXT("default")))
            {
                Style.StyleByToken.Add(
                    Pair.Key,
                    ResolveRichTextTokenStyle(Theme, Pair.Key, Pair.Value.Style));
            }
        }
        Style.ColorByToken = Theme.TextColorTokens;
        // NOT a copy of TextSizeTokens: UGV2UiTheme::ResolveUnscaledFontSize falls back to a
        // style token's own font size when the size table has no entry, and a raw copy would
        // silently collapse such a token to MinReadableFontSize at render time. The table is
        // built through that function, over every token either table declares.
        for (const TPair<FName, float>& Pair : Theme.TextSizeTokens)
        {
            Style.UnscaledSizeByToken.Add(Pair.Key, Theme.ResolveUnscaledFontSize(Pair.Key));
        }
        for (const TPair<FName, FGV2TextStyleToken>& Pair : Theme.TextStyleTokens)
        {
            Style.UnscaledSizeByToken.FindOrAdd(Pair.Key) = Theme.ResolveUnscaledFontSize(Pair.Key);
        }
        Style.ScalePolicy = UGV2TextPipeline::ResolveScalePolicyForTheme(&Theme, Style.DefaultTokenName);
        Style.InteractiveStyle = Theme.RichTextInteractiveStyle;
        // PSC-10B (ADR-0043 D1): already loaded once at snapshot build. Prepare performs no
        // load of its own -- this used to be a LoadSynchronous() per rich text widget per
        // reconcile, which is a content-resolution capability on a per-frame path.
        Style.PopoverClass = ResolvedTheme.RichTextPopoverClass.Get();
        Style.PopoverStyle = ResolveRichTextPopoverStyle(Theme);
        Style.bIsResolved = true;

        FPreparedCentralStyleOperation Operation;
        Operation.TargetWidget = RichText;
        Operation.Payload.Set<FPreparedRichTextStyle>(MoveTemp(Style));
        OutTransaction.AddCentralStyleOperation(MoveTemp(Operation));
    }
}

// PSC-10B: a class that styles its own bound sub-widgets from its own role OWNS that
// subtree. Descending into it would emit a second, generic role for a child whose style the
// parent has already decided -- a dropdown's header is not a plain button, and whichever
// operation happened to be applied last would silently win. Ownership is declared here, in
// the walk, rather than left to the order operations end up in.
bool OwnsSubtreeStyling(const UWidget* Widget)
{
    // These hosts create/reuse their children off-tree during their own Prepare. Their
    // consumers prepare those child subtrees with the same context. Stopping here avoids
    // applying duplicate operations to children still attached from the previous revision.
    return Widget->IsA<UGV2DropdownSelectWidgetBase>()
        || Widget->IsA<UGV2ButtonListWidgetBase>()
        || Widget->IsA<UGV2ListViewWidgetBase>()
        || Widget->IsA<UGV2TabContainerWidgetBase>();
}

void WalkWidget(
    UWidget* Widget,
    const FGV2ResolvedUiTheme& ResolvedTheme,
    TSet<UWidget*>& Visited,
    FGV2PreparedPresentationTransaction& OutTransaction)
{
    if (Widget == nullptr)
    {
        return;
    }
    bool bAlreadyVisited = false;
    Visited.Add(Widget, &bAlreadyVisited);
    if (bAlreadyVisited)
    {
        return;
    }

    EmitForWidget(Widget, ResolvedTheme, OutTransaction);

    if (OwnsSubtreeStyling(Widget))
    {
        return;
    }

    // A UUserWidget's bound sub-widgets live in its own WidgetTree, which ForEachWidget
    // enumerates flatly but does NOT descend into for nested user widgets -- so each nested
    // user widget is walked again here for its own tree. Plain panels reached through that
    // enumeration are covered by the Visited set rather than by a second traversal rule.
    if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
    {
        if (UserWidget->WidgetTree != nullptr)
        {
            UserWidget->WidgetTree->ForEachWidget([&ResolvedTheme, &Visited, &OutTransaction](UWidget* Child)
            {
                WalkWidget(Child, ResolvedTheme, Visited, OutTransaction);
            });
        }
        return;
    }

    if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
        {
            WalkWidget(Panel->GetChildAt(Index), ResolvedTheme, Visited, OutTransaction);
        }
    }
}
}

namespace GV2CentralStylePreparer
{
bool PrepareForSubtree(
    UWidget* Root,
    const FGV2PresentationPrepareContext& PrepareContext,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError)
{
    OutError.Reset();
    if (Root == nullptr)
    {
        return true;
    }
    // PSC-10B: a snapshot without a Theme cannot produce style, and since no widget pulls
    // one any more the result would be a silently unstyled tree. Fail loudly instead: the
    // whole point of the push model is that missing presentation authority is observable.
    if (PrepareContext.GetTheme().Theme.Get() == nullptr)
    {
        OutError = TEXT("core:diagnostic.ui_central_style.missing_theme: the session snapshot carries no Theme");
        return false;
    }

    TSet<UWidget*> Visited;
    WalkWidget(Root, PrepareContext.GetTheme(), Visited, OutTransaction);
    return true;
}
}
