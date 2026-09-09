#include "UI/GV2SeparatorWidgetBase.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"

void UGV2SeparatorWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();

    // PSC-10B: runtime styling no longer happens here. NativePreConstruct runs whenever UMG
    // rebuilds the widget -- before any session exists, on a CDO, inside the asset editor --
    // so it can never be a legitimate place to read runtime authority; that is exactly how
    // GetConfiguredTheme() stayed reachable from a lifecycle callback. At runtime the style
    // now arrives as a prepared operation. Only design-time preview still writes, and it
    // writes back values this widget already carries in its own serialized sub-widgets.
    if (IsDesignTime())
    {
        const float SerializedThickness = (SeparatorSizeBox != nullptr)
            ? (Orientation == Orient_Horizontal ? SeparatorSizeBox->GetHeightOverride() : SeparatorSizeBox->GetWidthOverride())
            : 0.0f;
        ApplySeparatorStyleValues(
            SeparatorImage != nullptr ? SeparatorImage->GetBrush() : FSlateBrush(),
            SerializedThickness,
            Orientation == Orient_Horizontal);
    }
}

void UGV2SeparatorWidgetBase::ApplySeparatorStyleValues(const FSlateBrush& Brush, float Thickness, bool bHorizontal)
{
    if (SeparatorSizeBox == nullptr || SeparatorImage == nullptr)
    {
        return;
    }

    SeparatorImage->SetBrush(Brush);
    if (bHorizontal)
    {
        SeparatorSizeBox->ClearWidthOverride();
        SeparatorSizeBox->SetHeightOverride(Thickness);
    }
    else
    {
        SeparatorSizeBox->SetWidthOverride(Thickness);
        SeparatorSizeBox->ClearHeightOverride();
    }
}

bool UGV2SeparatorWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: this class's central style is carried by FPreparedSeparatorStyle and written
    // by ApplySeparatorStyleValues. The interface method survives only until PSC-11 removes
    // it as a runtime API; it deliberately does nothing rather than resolving a second time.
    return true;
}
