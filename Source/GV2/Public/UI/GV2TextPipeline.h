#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateTypes.h"
#include "GV2TextPipeline.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;

UCLASS()
class GV2_API UGV2TextPipeline : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static bool Resolve(
        const FString& TextId,
        const TArray<FGV2UiControlValue>& Args,
        FName StyleToken,
        FGV2TextViewModel& OutText,
        FString& OutError);

    static bool ResolveStyle(FName StyleToken, FTextBlockStyle& OutStyle);
    static TSubclassOf<UCommonTextStyle> ResolveStyleClass(FName StyleToken);
    static bool Apply(UCommonTextBlock* Widget, const FGV2TextViewModel& Text);
    static bool NormalizeMarkup(const FString& Source, FString& OutMarkup, FString& OutError);
};
