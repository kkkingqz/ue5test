#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "GV2DebugStartScreenWidget.generated.h"

class UButton;
class UCommonTextBlock;

UCLASS()
class GV2_API UGV2DebugStartScreenWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    bool InitializeStartScreen(const FGV2ButtonViewModel& InStartButtonModel);

protected:
    virtual void NativeDestruct() override;

private:
    UFUNCTION()
    void HandleStartClicked();

    EGV2SubmitUiInteractionResult SubmitStart();

    UPROPERTY(Transient)
    TObjectPtr<UButton> StartButton;

    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlock> StartLabel;

    UPROPERTY(Transient)
    FGV2ButtonViewModel StartButtonModel;
};
