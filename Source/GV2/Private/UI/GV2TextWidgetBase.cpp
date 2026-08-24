#include "UI/GV2TextWidgetBase.h"

#include "CommonTextBlock.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2UiCapability.h"

void UGV2TextWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2TextWidgetBase::ApplyText(const FGV2TextViewModel& Content)
{
    if (Content.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    if (TextBlock != nullptr && !UGV2TextPipeline::Apply(TextBlock, Content))
    {
        return false;
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

bool UGV2TextWidgetBase::ApplyCentralStyle_Implementation()
{
    return TextBlock != nullptr && UGV2TextPipeline::Apply(TextBlock, CurrentContent);
}

void UGV2TextWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("TextBlock")));
}
