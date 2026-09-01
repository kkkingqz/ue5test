#include "UI/GV2DeclaredCompositeWidgetBase.h"

void UGV2DeclaredCompositeWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    for (const FGV2DeclaredUiCapability& DeclaredCapability : DeclaredCapabilities)
    {
        const FString PropertyName = DeclaredCapability.PropertyName.ToString();
        const FName ChildWidgetName = DeclaredCapability.ChildWidgetName;

        switch (DeclaredCapability.Kind)
        {
        case EGV2DeclaredUiCapabilityKind::Boolean:
            OutBuilder.AddBoolean(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::Integer:
            OutBuilder.AddInteger(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::Number:
            OutBuilder.AddNumber(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::String:
            OutBuilder.AddString(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::Key:
            OutBuilder.AddKey(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::Text:
            OutBuilder.AddText(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::ResourceRef:
            OutBuilder.AddImage(PropertyName, ChildWidgetName, TEXT("resource"));
            break;
        case EGV2DeclaredUiCapabilityKind::Binding:
            OutBuilder.AddBinding(PropertyName, ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::CollectionHost:
            OutBuilder.AddCustom(
                PropertyName,
                EGV2PreparedUiValueKind::Array,
                EGV2UiCapabilityTargetType::CollectionHost,
                ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::RichTextSpans:
            OutBuilder.AddCustom(
                PropertyName,
                EGV2PreparedUiValueKind::Array,
                EGV2UiCapabilityTargetType::CustomControl,
                ChildWidgetName);
            break;
        case EGV2DeclaredUiCapabilityKind::NestedScreen:
            OutBuilder.AddNestedScreenCollection(PropertyName, ChildWidgetName);
            break;
        }
    }
}
