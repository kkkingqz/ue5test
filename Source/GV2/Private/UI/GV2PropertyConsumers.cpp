#include "UI/GV2PropertyConsumers.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2UiInteractionEmitter.h"

// --- FGV2TextPropertyConsumer ---

bool FGV2TextPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsText();
}

bool FGV2TextPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for text capability");
        return false;
    }

    UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(TargetWidget);
    if (!TextBlock)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UCommonTextBlock");
        return false;
    }

    if (!Value.IsText())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Text value");
        return false;
    }

    PreparedText = Value.AsText();
    return true;
}

bool FGV2TextPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(TargetWidget);
    if (!TextBlock)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UCommonTextBlock");
        return false;
    }

    return UGV2TextPipeline::Apply(TextBlock, PreparedText);
}

void FGV2TextPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(TargetWidget))
    {
        UGV2TextPipeline::Apply(TextBlock, FGV2TextViewModel());
    }
}

// --- FGV2ImageResourcePropertyConsumer ---

bool FGV2ImageResourcePropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsStableId() && Value.AsStableId().TargetKind == TEXT("resource");
}

bool FGV2ImageResourcePropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for image capability");
        return false;
    }

    UImage* ImageWidget = Cast<UImage>(TargetWidget);
    if (!ImageWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage");
        return false;
    }

    if (!Value.IsStableId() || Value.AsStableId().TargetKind != TEXT("resource"))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected StableId with target_kind 'resource'");
        return false;
    }

    PreparedResourceId = Value.AsStableId().Id;
    return true;
}

bool FGV2ImageResourcePropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    UImage* ImageWidget = Cast<UImage>(TargetWidget);
    if (!ImageWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage");
        return false;
    }

    FGV2ResolvedImageResource Resolved;
    return FGV2ImagePresentation::ResolveAndApply(ImageWidget, PreparedResourceId, EGV2PrimitiveScalePolicy::Fill, {}, Resolved, OutError);
}

void FGV2ImageResourcePropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        ImageWidget->SetBrush(FSlateBrush());
    }
}

// --- FGV2BooleanPropertyConsumer ---

bool FGV2BooleanPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBoolean();
}

bool FGV2BooleanPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for boolean capability");
        return false;
    }

    if (!Value.IsBoolean())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Boolean value");
        return false;
    }

    bPreparedValue = Value.AsBoolean();
    return true;
}

bool FGV2BooleanPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    TargetWidget->SetIsEnabled(bPreparedValue);
    return true;
}

void FGV2BooleanPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        TargetWidget->SetIsEnabled(true);
    }
}

// --- FGV2IntegerPropertyConsumer ---

bool FGV2IntegerPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsInteger();
}

bool FGV2IntegerPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for integer capability");
        return false;
    }

    if (!Value.IsInteger())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Integer value");
        return false;
    }

    PreparedValue = Value.AsInteger();
    return true;
}

bool FGV2IntegerPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    return true;
}

void FGV2IntegerPropertyConsumer::Reset(UWidget* TargetWidget)
{
    PreparedValue = 0;
}

// --- FGV2NumberPropertyConsumer ---

bool FGV2NumberPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsNumber();
}

bool FGV2NumberPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for number capability");
        return false;
    }

    if (!Value.IsNumber())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Number value");
        return false;
    }

    PreparedValue = Value.AsNumber();
    return true;
}

bool FGV2NumberPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        PB->SetPercent(static_cast<float>(PreparedValue));
    }
    return true;
}

void FGV2NumberPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        PB->SetPercent(0.0f);
    }
}

// --- FGV2StringPropertyConsumer ---

bool FGV2StringPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsString();
}

bool FGV2StringPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for string capability");
        return false;
    }

    if (!Value.IsString())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected String value");
        return false;
    }

    PreparedValue = Value.AsString();
    return true;
}

bool FGV2StringPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    return true;
}

void FGV2StringPropertyConsumer::Reset(UWidget* TargetWidget)
{
    PreparedValue.Empty();
}

// --- FGV2KeyPropertyConsumer ---

bool FGV2KeyPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsKey();
}

bool FGV2KeyPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for key capability");
        return false;
    }

    if (!Value.IsKey())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Key value");
        return false;
    }

    PreparedValue = Value.AsKey();
    return true;
}

bool FGV2KeyPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    return true;
}

void FGV2KeyPropertyConsumer::Reset(UWidget* TargetWidget)
{
    PreparedValue.Empty();
}

// --- FGV2BindingPropertyConsumer ---

bool FGV2BindingPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBinding();
}

bool FGV2BindingPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for binding capability");
        return false;
    }

    if (!Value.IsBinding())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Binding value");
        return false;
    }

    PreparedBinding = Value.AsBinding();
    return true;
}

bool FGV2BindingPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    if (IGV2UiInteractionEmitter* Emitter = Cast<IGV2UiInteractionEmitter>(TargetWidget))
    {
        Emitter->SetBindingHandle(PreparedBinding);
    }
    return true;
}

void FGV2BindingPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (IGV2UiInteractionEmitter* Emitter = Cast<IGV2UiInteractionEmitter>(TargetWidget))
    {
        Emitter->SetBindingHandle(FGV2UiBindingHandle());
    }
}

// --- FGV2PropertyConsumerFactory ---

TSharedPtr<IGV2PropertyConsumer> FGV2PropertyConsumerFactory::CreateConsumer(
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FString& TargetKind)
{
    switch (Kind)
    {
    case EGV2PreparedUiValueKind::Text:
        return MakeShared<FGV2TextPropertyConsumer>();
    case EGV2PreparedUiValueKind::StableId:
        if (TargetKind == TEXT("resource"))
        {
            return MakeShared<FGV2ImageResourcePropertyConsumer>();
        }
        break;
    case EGV2PreparedUiValueKind::Boolean:
        return MakeShared<FGV2BooleanPropertyConsumer>();
    case EGV2PreparedUiValueKind::Integer:
        return MakeShared<FGV2IntegerPropertyConsumer>();
    case EGV2PreparedUiValueKind::Number:
        return MakeShared<FGV2NumberPropertyConsumer>();
    case EGV2PreparedUiValueKind::String:
        return MakeShared<FGV2StringPropertyConsumer>();
    case EGV2PreparedUiValueKind::Key:
        return MakeShared<FGV2KeyPropertyConsumer>();
    case EGV2PreparedUiValueKind::Binding:
        return MakeShared<FGV2BindingPropertyConsumer>();
    default:
        break;
    }
    return nullptr;
}
