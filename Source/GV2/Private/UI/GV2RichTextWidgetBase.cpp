#include "UI/GV2RichTextWidgetBase.h"

#include "CommonRichTextBlock.h"

void UGV2RichTextWidgetBase::ApplyRichTextContent(const FText& Content)
{
    check(RichTextBlock != nullptr);
    RichTextBlock->SetText(Content);
}
