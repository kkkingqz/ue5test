#include "UI/GV2ProgressBarWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"

void UGV2ProgressBarWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedProgressBarStyle; design-time preview
    // re-applies what this widget's own inner bar already carries. See
    // UGV2SeparatorWidgetBase::NativePreConstruct for why runtime styling cannot live here.
    if (IsDesignTime() && ProgressBar != nullptr)
    {
        ApplyProgressBarStyleValues(ProgressBar->GetWidgetStyle(), ProgressBar->GetFillColorAndOpacity());
    }
}

void UGV2ProgressBarWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddNumber(TEXT("percent"), FName(TEXT("ProgressBar")), 0.0, 1.0);
    bool bHasLabel = (LabelText != nullptr);
    if (!bHasLabel)
    {
        if (const UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
        {
            if (const UWidgetTree* Tree = BGClass->GetWidgetTreeArchetype())
            {
                bHasLabel = (Tree->FindWidget(FName(TEXT("LabelText"))) != nullptr);
            }
        }
    }
    if (bHasLabel)
    {
        OutBuilder.AddText(TEXT("label"), FName(TEXT("LabelText")));
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

void UGV2ProgressBarWidgetBase::ApplyProgress(const float Percent)
{
    if (ProgressBar != nullptr)
    {
        ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
    }
    CurrentPercent = Percent;
}

float UGV2ProgressBarWidgetBase::GetProgress() const
{
    return ProgressBar != nullptr ? ProgressBar->GetPercent() : CurrentPercent;
}

void UGV2ProgressBarWidgetBase::ApplyProgressBarStyleValues(const FProgressBarStyle& WidgetStyle, const FLinearColor& FillColor)
{
    if (ProgressBar != nullptr)
    {
        ProgressBar->SetWidgetStyle(WidgetStyle);
        ProgressBar->SetFillColorAndOpacity(FillColor);
    }
}

bool UGV2ProgressBarWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: carried by FPreparedProgressBarStyle, written by ApplyProgressBarStyleValues.
    return true;
}

