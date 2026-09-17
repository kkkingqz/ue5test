#include "UI/GV2RichTextSpanDecorator.h"

#include "Components/RichTextBlock.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Framework/Text/SlateTextRun.h"
#include "Styling/CoreStyle.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "Widgets/SToolTip.h"

namespace
{
class FGV2RichTextSpanDecorator final : public ITextDecorator
{
public:
    explicit FGV2RichTextSpanDecorator(UGV2RichTextWidgetBase* InOwner)
        : Owner(InOwner)
    {
    }

    virtual bool Supports(
        const FTextRunParseResults& RunParseResult,
        const FString& Text) const override
    {
        UGV2RichTextWidgetBase* Widget = Owner.Get();
        return Widget != nullptr && RunParseResult.Name == TEXT("gv2");
    }

    virtual TSharedRef<ISlateRun> Create(
        const TSharedRef<FTextLayout>& TextLayout,
        const FTextRunParseResults& RunParseResult,
        const FString& OriginalText,
        const TSharedRef<FString>& InOutModelText,
        const ISlateStyle* Style) override
    {
        FTextRange ModelRange;
        ModelRange.BeginIndex = InOutModelText->Len();
        *InOutModelText += OriginalText.Mid(
            RunParseResult.ContentRange.BeginIndex,
            RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex);
        ModelRange.EndIndex = InOutModelText->Len();

        FRunInfo RunInfo(RunParseResult.Name);
        for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
        {
            RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(
                Pair.Value.BeginIndex,
                Pair.Value.EndIndex - Pair.Value.BeginIndex));
        }

        const auto MetaName = [&RunInfo](const TCHAR* Key)
        {
            const FString* Value = RunInfo.MetaData.Find(Key);
            return Value != nullptr ? FName(**Value) : NAME_None;
        };
        UGV2RichTextWidgetBase* Widget = Owner.Get();
        const FTextBlockStyle RunStyle = Widget != nullptr
            ? Widget->ResolveRunTextStyle(MetaName(TEXT("style")), MetaName(TEXT("color")), MetaName(TEXT("size")))
            : FTextBlockStyle::GetDefault();
        const FName InteractiveId = MetaName(TEXT("interactive"));
        if (InteractiveId.IsNone())
        {
            return FSlateTextRun::Create(RunInfo, InOutModelText, RunStyle, ModelRange);
        }
        FHyperlinkStyle HyperlinkStyle = Widget != nullptr
            ? Widget->ResolveInteractiveTextStyle(RunStyle)
            : FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(TEXT("Hyperlink"));

        const TWeakObjectPtr<UGV2RichTextWidgetBase> WeakOwner = Owner;
        const FSlateHyperlinkRun::FOnClick OnClick = FSlateHyperlinkRun::FOnClick::CreateLambda(
            [WeakOwner](const FSlateHyperlinkRun::FMetadata& Metadata)
            {
                UGV2RichTextWidgetBase* Widget = WeakOwner.Get();
                const FString* Id = Metadata.Find(TEXT("interactive"));
                if (Widget != nullptr && Id != nullptr)
                {
                    Widget->SubmitSpanInteraction(FName(**Id));
                }
            });
        // PEP-06C: load-bearing, not dead code, despite every method on the IToolTip it
        // returns being a no-op since PEP-06B. This is the ONLY per-run hook
        // FSlateHyperlinkRun::Create exposes -- the engine gives no other way to attach data
        // to one specific SRichTextHyperlink instance at the moment its SpanId is known
        // (SetTag() exists on SWidget, but nothing here ever gets a reference to the actual
        // widget being tooltipped to call it on; only the Metadata map is handed back).
        // Removing this line silently breaks NativeTick's hover detector (GV2RichTextWidgetBase
        // ::CaptureHoverableSpanAnchors reads Hyperlink->GetToolTip() back to a SpanId) with no
        // compile error -- proven by mutation: replacing this OnTooltip argument with
        // FSlateHyperlinkRun::FOnGenerateTooltip() (unbound) reddens GV2RichTextSpanHoverDetectorTests
        // ("Exactly one anchor is captured" becomes 0), confirmed by an actual mutation-then-revert
        // run, not by reading this comment.
        const FSlateHyperlinkRun::FOnGenerateTooltip OnTooltip =
            FSlateHyperlinkRun::FOnGenerateTooltip::CreateLambda(
                [WeakOwner](const FSlateHyperlinkRun::FMetadata& Metadata) -> TSharedRef<IToolTip>
                {
                    UGV2RichTextWidgetBase* Widget = WeakOwner.Get();
                    const FString* Id = Metadata.Find(TEXT("interactive"));
                    if (Widget != nullptr && Id != nullptr)
                    {
                        return Widget->CreateSpanToolTip(FName(**Id));
                    }
                    return SNew(SToolTip).Text(FText::GetEmpty());
                });

        return FSlateHyperlinkRun::Create(
            RunInfo,
            InOutModelText,
            HyperlinkStyle,
            OnClick,
            OnTooltip,
            FSlateHyperlinkRun::FOnGetTooltipText(),
            ModelRange);
    }

private:
    TWeakObjectPtr<UGV2RichTextWidgetBase> Owner;
};
}

TSharedPtr<ITextDecorator> UGV2RichTextSpanDecorator::CreateDecorator(
    URichTextBlock* InOwner)
{
    UGV2RichTextWidgetBase* Widget = InOwner != nullptr
        ? InOwner->GetTypedOuter<UGV2RichTextWidgetBase>()
        : nullptr;
    if (Widget == nullptr)
    {
        return nullptr;
    }
    return MakeShared<FGV2RichTextSpanDecorator>(Widget);
}
