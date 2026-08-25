#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2TextPipeline.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiTheme.h"

void UGV2ModalWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ModalWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (BackdropButton != nullptr)
    {
        BackdropButton->OnClicked.AddDynamic(this, &UGV2ModalWidgetBase::HandleBackdropButtonClicked);
    }
}

void UGV2ModalWidgetBase::NativeDestruct()
{
    if (BackdropButton != nullptr)
    {
        BackdropButton->OnClicked.RemoveDynamic(this, &UGV2ModalWidgetBase::HandleBackdropButtonClicked);
    }
    Super::NativeDestruct();
}

void UGV2ModalWidgetBase::HandleBackdropButtonClicked()
{
    SubmitBackdropClose();
}

void UGV2ModalWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle)
{
    BackdropCloseBinding = InBindingHandle;
    if (BackdropButton != nullptr)
    {
        BackdropButton->SetIsEnabled(BackdropCloseBinding.IsValid());
    }
}

FGV2UiBindingHandle UGV2ModalWidgetBase::GetBindingHandle() const
{
    return BackdropCloseBinding;
}

bool UGV2ModalWidgetBase::ApplyTitle(const FGV2TextViewModel& InTitle)
{
    CurrentTitle = InTitle;
    if (TitleText != nullptr)
    {
        return UGV2TextPipeline::Apply(TitleText, CurrentTitle);
    }
    return true;
}

bool UGV2ModalWidgetBase::ApplyContent(const FGV2TextViewModel& InContent)
{
    CurrentContent = InContent;
    if (ContentText != nullptr)
    {
        return UGV2TextPipeline::Apply(ContentText, CurrentContent);
    }
    return true;
}

EGV2SubmitUiInteractionResult UGV2ModalWidgetBase::SubmitBackdropClose()
{
    if (BackdropCloseBinding.IsValid())
    {
        return FGV2UiInteractionEmitter::Submit(this, BackdropCloseBinding, {});
    }
    return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
}

void UGV2ModalWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("title"), FName(TEXT("TitleText")));
    OutBuilder.AddText(TEXT("content"), FName(TEXT("ContentText")));
    FGV2UiPropertyCapability ButtonCap;
    ButtonCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    ButtonCap.EntryWidgetClass = UGV2ButtonWidgetBase::StaticClass();
    OutBuilder.AddKeyedCollection(
        TEXT("buttons"),
        FName(TEXT("ButtonList")),
        ButtonCap,
        TEXT("key"),
        UGV2ButtonWidgetBase::StaticClass());
    OutBuilder.AddBinding(TEXT("backdrop_close_action"), NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

bool UGV2ModalWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    if (TitleText != nullptr)
    {
        UGV2TextPipeline::Apply(TitleText, CurrentTitle);
    }
    if (ContentText != nullptr)
    {
        UGV2TextPipeline::Apply(ContentText, CurrentContent);
    }
    return true;
}
