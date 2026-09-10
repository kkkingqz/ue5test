#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"

class UCommonRichTextBlock;
class UCommonTextBlock;
class UEditableTextBox;
class UWidget;
namespace GV2PresentationApply { struct FPreparedTextScalePolicy; }

// Internal adapter used by physical widget bases already inside this module. It is not an
// exported Apply surface; every method builds one prepared operation and invokes the
// single FGV2PresentationApply transaction facade.
class FGV2WidgetTextApply
{
public:
    static bool Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text);
    static bool ApplyRichText(
        UCommonRichTextBlock* Widget,
        const FGV2TextViewModel& Text,
        const UWidget* ContextWidget = nullptr);
    static bool ApplyHint(UEditableTextBox* Widget, const FGV2TextViewModel& Text);

    // PSC-14: physical-only refresh helpers. Unlike Apply(), these preserve text and style
    // identity and update only the font size derived from an already-resolved policy.
    static void RefreshFont(
        UCommonTextBlock* Widget,
        const FGV2TextViewModel& Text,
        float ViewportHeight);
    static void RefreshFont(
        UCommonTextBlock* Widget,
        const GV2PresentationApply::FPreparedTextScalePolicy& Policy,
        float ViewportHeight);
};
