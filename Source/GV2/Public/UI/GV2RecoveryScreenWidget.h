#pragma once

#include "CommonUserWidget.h"
#include "GV2RecoveryScreenWidget.generated.h"

class UCommonTextBlock;

UCLASS()
class GV2_API UGV2RecoveryScreenWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    bool InitializeRecoveryScreen(const FString& InTitle, const FString& InMessage);

    const FString& GetTitle() const { return ErrorTitle; }
    const FString& GetMessage() const { return ErrorMessage; }

protected:
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlock> TitleLabel;

    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlock> MessageLabel;

    UPROPERTY(Transient)
    FString ErrorTitle;

    UPROPERTY(Transient)
    FString ErrorMessage;
};
