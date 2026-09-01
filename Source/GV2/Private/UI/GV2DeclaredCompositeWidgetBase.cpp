#include "UI/GV2DeclaredCompositeWidgetBase.h"

namespace
{
// GBH-02A: the only kinds proven end-to-end through UGV2DeclaredCompositeWidgetBase
// itself. Everything else in EGV2DeclaredUiCapabilityKind must carry UMETA(Hidden).
const TSet<EGV2DeclaredUiCapabilityKind>& GetProvenDesignerKinds()
{
    static const TSet<EGV2DeclaredUiCapabilityKind> Kinds = {
        EGV2DeclaredUiCapabilityKind::Boolean,
        EGV2DeclaredUiCapabilityKind::Integer,
        EGV2DeclaredUiCapabilityKind::Number,
        EGV2DeclaredUiCapabilityKind::String,
        EGV2DeclaredUiCapabilityKind::Key,
        EGV2DeclaredUiCapabilityKind::Text,
        EGV2DeclaredUiCapabilityKind::ResourceRef,
        EGV2DeclaredUiCapabilityKind::Binding,
        EGV2DeclaredUiCapabilityKind::NestedScreen,
    };
    return Kinds;
}
}

EGV2DesignerKindStatus FGV2DesignerCapabilityKindGate::GetKindStatus(EGV2DeclaredUiCapabilityKind Kind)
{
    const UEnum* Enum = StaticEnum<EGV2DeclaredUiCapabilityKind>();
    if (Enum == nullptr)
    {
        return EGV2DesignerKindStatus::Hidden;
    }
    const int32 Index = Enum->GetIndexByValue(static_cast<int64>(Kind));
    if (Index == INDEX_NONE)
    {
        return EGV2DesignerKindStatus::Hidden;
    }
    return Enum->HasMetaData(TEXT("Hidden"), Index)
        ? EGV2DesignerKindStatus::Hidden
        : EGV2DesignerKindStatus::Supported;
}

bool FGV2DesignerCapabilityKindGate::IsHiddenKind(EGV2DeclaredUiCapabilityKind Kind, FString* OutReason)
{
    if (GetKindStatus(Kind) != EGV2DesignerKindStatus::Hidden)
    {
        return false;
    }
    if (OutReason != nullptr)
    {
        const UEnum* Enum = StaticEnum<EGV2DeclaredUiCapabilityKind>();
        const int32 Index = Enum != nullptr ? Enum->GetIndexByValue(static_cast<int64>(Kind)) : INDEX_NONE;
        *OutReason = Index != INDEX_NONE ? Enum->GetMetaData(TEXT("ToolTip"), Index) : FString();
    }
    return true;
}

TArray<FGV2HiddenDesignerKindInfo> FGV2DesignerCapabilityKindGate::GetHiddenKinds()
{
    TArray<FGV2HiddenDesignerKindInfo> Result;
    const UEnum* Enum = StaticEnum<EGV2DeclaredUiCapabilityKind>();
    if (Enum == nullptr)
    {
        return Result;
    }
    const int32 NumEnums = Enum->NumEnums() - 1; // exclude the implicit MAX sentinel
    for (int32 Index = 0; Index < NumEnums; ++Index)
    {
        const EGV2DeclaredUiCapabilityKind Kind = static_cast<EGV2DeclaredUiCapabilityKind>(Enum->GetValueByIndex(Index));
        FString Reason;
        if (IsHiddenKind(Kind, &Reason))
        {
            Result.Add({Kind, Reason});
        }
    }
    return Result;
}

bool FGV2DesignerCapabilityKindGate::ValidateAllKindsClassified(TArray<FString>& OutDiagnostics)
{
    const UEnum* Enum = StaticEnum<EGV2DeclaredUiCapabilityKind>();
    if (Enum == nullptr)
    {
        OutDiagnostics.Add(TEXT("EGV2DeclaredUiCapabilityKind enum not found via StaticEnum"));
        return false;
    }

    bool bSuccess = true;
    const int32 NumEnums = Enum->NumEnums() - 1; // exclude the implicit MAX sentinel
    for (int32 Index = 0; Index < NumEnums; ++Index)
    {
        const EGV2DeclaredUiCapabilityKind Kind = static_cast<EGV2DeclaredUiCapabilityKind>(Enum->GetValueByIndex(Index));
        const FString KindName = Enum->GetNameStringByIndex(Index);
        const bool bHiddenTag = Enum->HasMetaData(TEXT("Hidden"), Index);
        const bool bProven = GetProvenDesignerKinds().Contains(Kind);

        if (bHiddenTag && bProven)
        {
            OutDiagnostics.Add(FString::Printf(
                TEXT("Kind '%s' is both UMETA(Hidden) and listed as proven end-to-end -- contradictory classification"),
                *KindName));
            bSuccess = false;
        }
        else if (!bHiddenTag && !bProven)
        {
            OutDiagnostics.Add(FString::Printf(
                TEXT("Kind '%s' is selectable (no UMETA(Hidden)) but has no recorded end-to-end proof -- mark it Hidden or add it to the proven-kinds list"),
                *KindName));
            bSuccess = false;
        }
        else if (bHiddenTag && Enum->GetMetaData(TEXT("ToolTip"), Index).IsEmpty())
        {
            OutDiagnostics.Add(FString::Printf(
                TEXT("Kind '%s' is UMETA(Hidden) but has no recorded ToolTip reason"),
                *KindName));
            bSuccess = false;
        }
    }
    return bSuccess;
}

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
