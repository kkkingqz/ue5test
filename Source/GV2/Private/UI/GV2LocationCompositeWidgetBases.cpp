#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

// ============================================================================
// CommandPanel
// ============================================================================
void UGV2LocationCommandPanelWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();

    // PCC-09 (proposal 17.2 "Instance wiring check", BAI-11 precedent): the internal
    // repeater is wired once here, at instance construction, instead of lazily from
    // DescribeUiCapabilities -- querying capability must never create objects or mutate
    // the widget. Discarded return value: called for the wiring side effect only.
    ResolveRepeater();
}

UGV2ListViewWidgetBase* UGV2LocationCommandPanelWidgetBase::ResolveRepeater()
{
    if (ButtonRepeater != nullptr) return ButtonRepeater;
    if (ButtonContainer == nullptr) return nullptr;
    if (InternalRepeater == nullptr)
    {
        InternalRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalRepeater->SetContainerPanel(ButtonContainer);
    return InternalRepeater;
}

bool UGV2LocationCommandPanelWidgetBase::HasUsableRepeaterHost() const
{
    return ButtonRepeater != nullptr || ButtonContainer != nullptr;
}

void UGV2LocationCommandPanelWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (HasUsableRepeaterHost())
    {
        // DCA-03: ButtonWidgetClass is read as-is, not resolved through a fallback chain.
        // An unset class still declares the capability (the host exists); instantiating
        // a NEW entry against it fails downstream (missing_entry_class) instead of
        // silently substituting a default -- see missing_entry_class in
        // GV2PropertyConsumers.cpp.
        FGV2UiPropertyCapability BtnCap;
        BtnCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
        BtnCap.EntryWidgetClass = ButtonWidgetClass;
        OutBuilder.AddKeyedCollection(TEXT("items"), FName(TEXT("ButtonRepeater")), BtnCap, TEXT("key"), ButtonWidgetClass);
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}
