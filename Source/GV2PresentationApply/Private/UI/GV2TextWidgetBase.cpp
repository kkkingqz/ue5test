#include "UI/GV2TextWidgetBase.h"

#include "CommonTextBlock.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiCapability.h"

void UGV2TextWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    FGV2WidgetTextApply::RefreshFont(GetTextBlock(), CurrentContent, ViewportHeight);
}

bool UGV2TextWidgetBase::ApplyText(const FGV2TextViewModel& Content)
{
    if (Content.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    // PSC-11: through the accessor, not the bare BindWidget member -- see
    // UGV2RichTextWidgetBase::ApplyText for why the retired adapter's redirect made the
    // name-lookup fallback part of this path's behaviour.
    if (UCommonTextBlock* Renderer = GetTextBlock())
    {
        if (!FGV2WidgetTextApply::Apply(Renderer, Content))
        {
            return false;
        }
    }
    CurrentContent = Content;
    return true;
}

FText UGV2TextWidgetBase::GetTextContent() const
{
    return TextBlock != nullptr ? TextBlock->GetText() : CurrentContent.Text;
}

UCommonTextBlock* UGV2TextWidgetBase::GetTextBlock() const
{
    return TextBlock != nullptr ? TextBlock.Get() : Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("TextBlock")));
}

void UGV2TextWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("TextBlock")));
}

bool UGV2TextWidgetBase::ApplyPreparedText(
    const GV2PresentationApply::FPreparedTextValue& Value,
    bool /*bIsReset*/,
    FString& OutError)
{
    if (!ApplyText(FGV2TextViewModel::FromPrepared(Value)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2TextWidgetBase::ApplyText rejected the resolved text");
        return false;
    }
    return true;
}
