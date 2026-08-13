#include "UI/GV2TextWidgetBase.h"

#include "CommonTextBlock.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"

void UGV2TextWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2TextWidgetBase::ApplyText(const FGV2TextViewModel& Content)
{
    if (!UGV2TextPipeline::Apply(TextBlock, Content))
    {
        return false;
    }
    CurrentContent = Content;
    return true;
}

FText UGV2TextWidgetBase::GetTextContent() const
{
    return TextBlock != nullptr ? TextBlock->GetText() : FText::GetEmpty();
}

bool UGV2TextWidgetBase::ApplyCentralStyle_Implementation()
{
    return TextBlock != nullptr && UGV2TextPipeline::Apply(TextBlock, CurrentContent);
}
