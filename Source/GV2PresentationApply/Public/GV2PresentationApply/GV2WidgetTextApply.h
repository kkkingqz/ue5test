#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"

class UCommonRichTextBlock;
class UCommonTextBlock;
class UEditableTextBox;
class UWidget;

class GV2PRESENTATIONAPPLY_API FGV2WidgetTextApply
{
public:
    static bool Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text);
    static bool ApplyRichText(
        UCommonRichTextBlock* Widget,
        const FGV2TextViewModel& Text,
        const UWidget* ContextWidget = nullptr);
    static bool ApplyHint(UEditableTextBox* Widget, const FGV2TextViewModel& Text);
};
