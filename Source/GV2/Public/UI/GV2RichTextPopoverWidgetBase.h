#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "CommonUserWidget.h"
#include "GV2RichTextPopoverWidgetBase.generated.h"

class UBorder;
class UCommonTextBlock;
class UGV2RichTextWidgetBase;
class UImage;
class USizeBox;

UCLASS(Blueprintable)
class GV2_API UGV2RichTextPopoverWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { Key = InKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return Key; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    bool InitializePopover(const FGV2RichTextHoverViewModel& InModel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Rich Text")
    const FGV2RichTextHoverViewModel& GetPopoverModel() const;

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UFUNCTION(BlueprintNativeEvent, Category = "GV2|UI|Rich Text")
    bool ApplyImageResource(const FString& ResourceId);
    virtual bool ApplyImageResource_Implementation(const FString& ResourceId);

    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Rich Text", meta = (DisplayName = "On Popover Applied"))
    void OnPopoverApplied();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> PopoverBorder;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<USizeBox> PopoverWidth;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UGV2RichTextWidgetBase> DescriptionText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Properties")
    FName Key;

    FGV2UiPropertyHostState PropertyHostState;

    UPROPERTY(Transient)
    FGV2RichTextHoverViewModel Model;
};
