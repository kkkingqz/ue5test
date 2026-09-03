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

    // PCC-10: hardcoding the bare native UGV2ButtonWidgetBase class unconditionally here
    // (instead of reading ButtonList's own configured class, the way ButtonList/
    // CommandPanel/PlayerStatus already do for their own collections) meant a bare,
    // unwired entry with no LabelText bound -- the collection sweep this task adds found
    // its "text" capability's target resolves to null in production. ButtonList already
    // owns the class element for exactly this entry; read it instead of re-declaring a
    // fallback here. The bare-native class remains the deliberate last resort for the
    // orthogonal case of no ButtonList child at all (a Modal variant without a button
    // host, not a class-element omission). DCA-03: only the inner resolution -- when
    // ButtonList exists but its own class element is unset -- drops the fallback; that
    // case now reads back as null, exactly as an unconfigured collection behaves
    // anywhere else, instead of silently substituting a default.
    TSubclassOf<UGV2ButtonWidgetBase> ResolvedButtonClass = UGV2ButtonWidgetBase::StaticClass();
    if (ButtonList != nullptr)
    {
        ResolvedButtonClass = ButtonList->GetButtonWidgetClass();
    }

    FGV2UiPropertyCapability ButtonCap;
    ButtonCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    ButtonCap.EntryWidgetClass = ResolvedButtonClass;
    OutBuilder.AddKeyedCollection(
        TEXT("buttons"),
        FName(TEXT("ButtonList")),
        ButtonCap,
        TEXT("key"),
        ResolvedButtonClass);
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
