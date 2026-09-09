#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
#include "GV2TextWidgetBase.generated.h"

class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2TextWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2ScreenFieldHost
    , public IGV2TextPipelineHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyText(const FGV2TextViewModel& Content);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FText GetTextContent() const;

    UCommonTextBlock* GetTextBlock() const;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> TextBlock;

private:
    UPROPERTY(Transient)
    FGV2TextViewModel CurrentContent;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
