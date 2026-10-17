#pragma once

#include "CommonUserWidget.h"
#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "GV2RecoveryScreenWidget.generated.h"

class UCommonTextBlock;

UCLASS()
class GV2_API UGV2RecoveryScreenWidget
    : public UCommonUserWidget
    , public IGV2PreparedViewportRefreshTarget
{
    GENERATED_BODY()

public:
    bool InitializeRecoveryScreen(const FString& InTitle, const FString& InMessage);

    const FString& GetTitle() const { return ErrorTitle; }
    const FString& GetMessage() const { return ErrorMessage; }

    // A resize is not a new desired presentation: the gap is recomputed from the policy this
    // widget resolved once, with no second Theme read. The recovery surface deliberately
    // resolves that policy from the core-minimal Theme rather than from a session snapshot --
    // it exists for the case where there is no healthy session to prepare from.
    virtual void RefreshPreparedViewportPresentation(float ViewportHeight) override;

    // The gap this widget currently enforces between title and message, in slate units.
    float GetEnforcedTitleToMessageGap() const { return EnforcedTitleToMessageGap; }

protected:
    virtual void NativeDestruct() override;

private:
    void ApplyTitleToMessageGap(float ViewportHeight);

    GV2PresentationApply::FPreparedViewportScalePolicy ScalePolicy;
    float EnforcedTitleToMessageGap = 0.0f;

    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlock> TitleLabel;

    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlock> MessageLabel;

    UPROPERTY(Transient)
    FString ErrorTitle;

    UPROPERTY(Transient)
    FString ErrorMessage;
};
