#pragma once

#include "Components/RichTextBlockDecorator.h"
#include "GV2RichTextSpanDecorator.generated.h"

UCLASS()
class GV2_API UGV2RichTextSpanDecorator : public URichTextBlockDecorator
{
    GENERATED_BODY()

public:
    virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
};
