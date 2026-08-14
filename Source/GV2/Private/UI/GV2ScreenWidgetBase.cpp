#include "UI/GV2ScreenWidgetBase.h"

#include "Bridge/GV2StableIdUE.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "UI/GV2DynamicScreenElement.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2ScreenWidget, Log, All);

namespace
{
struct FGV2ScreenElementRecord
{
    TObjectPtr<UWidget> Widget;
    FGV2ScreenFieldDescriptor Descriptor;
};

struct FGV2ScreenApplyRecord
{
    TObjectPtr<UWidget> Widget;
    FGV2ScreenFieldValue PreviousValue;
    FGV2ScreenFieldValue CandidateValue;
    bool bReset = false;
};

bool IsCanonicalFieldId(const FName FieldId)
{
    const FString Value = FieldId.ToString();
    return GV2StableIdUE::IsValidSegment(Value);
}

bool IsCanonicalSchemaId(const FString& Value)
{
    return GV2StableIdUE::IsOfKind(Value, "schema");
}

bool CollectElements(
    const UGV2ScreenWidgetBase& Screen,
    TArray<FGV2ScreenElementRecord>& OutElements,
    FString& OutError)
{
    OutElements.Reset();
    if (Screen.WidgetTree == nullptr)
    {
        OutError = TEXT("screen has no WidgetTree");
        return false;
    }

    TSet<FName> SeenFieldIds;
    Screen.WidgetTree->ForEachWidget([&OutElements, &SeenFieldIds, &OutError](UWidget* Widget)
    {
        if (!OutError.IsEmpty()
            || Widget == nullptr
            || !Widget->GetClass()->ImplementsInterface(UGV2DynamicScreenElement::StaticClass()))
        {
            return;
        }

        const FGV2ScreenFieldDescriptor Descriptor =
            IGV2DynamicScreenElement::Execute_GetScreenFieldDescriptor(Widget);
        if (!Descriptor.IsConfigured())
        {
            return;
        }
        if (!IsCanonicalFieldId(Descriptor.FieldId))
        {
            OutError = FString::Printf(
                TEXT("dynamic element '%s' has non-canonical field_id '%s'"),
                *Widget->GetName(),
                *Descriptor.FieldId.ToString());
            return;
        }
        if (!IsCanonicalSchemaId(Descriptor.SchemaId))
        {
            OutError = FString::Printf(
                TEXT("dynamic element '%s' has invalid schema_id '%s'"),
                *Widget->GetName(),
                *Descriptor.SchemaId);
            return;
        }
        if (SeenFieldIds.Contains(Descriptor.FieldId))
        {
            OutError = FString::Printf(
                TEXT("duplicate dynamic screen field '%s'"),
                *Descriptor.FieldId.ToString());
            return;
        }

        SeenFieldIds.Add(Descriptor.FieldId);
        OutElements.Add({Widget, Descriptor});
    });

    OutElements.Sort([](const FGV2ScreenElementRecord& Left, const FGV2ScreenElementRecord& Right)
    {
        return Left.Descriptor.FieldId.LexicalLess(Right.Descriptor.FieldId);
    });
    return OutError.IsEmpty();
}

bool BuildApplyPlan(
    const UGV2ScreenWidgetBase& Screen,
    const TArray<FGV2ScreenFieldValue>& ScreenFields,
    TArray<FGV2ScreenApplyRecord>& OutPlan,
    FString& OutError,
    const bool bCapturePreviousValues)
{
    TArray<FGV2ScreenElementRecord> Elements;
    if (!CollectElements(Screen, Elements, OutError))
    {
        return false;
    }

    TMap<FName, const FGV2ScreenFieldValue*> ValuesById;
    for (const FGV2ScreenFieldValue& FieldValue : ScreenFields)
    {
        if (!IsCanonicalFieldId(FieldValue.FieldId))
        {
            OutError = FString::Printf(
                TEXT("payload has non-canonical field_id '%s'"),
                *FieldValue.FieldId.ToString());
            return false;
        }
        if (ValuesById.Contains(FieldValue.FieldId))
        {
            OutError = FString::Printf(
                TEXT("payload contains duplicate field '%s'"),
                *FieldValue.FieldId.ToString());
            return false;
        }
        if (!IsCanonicalSchemaId(FieldValue.SchemaId))
        {
            OutError = FString::Printf(
                TEXT("field '%s' has invalid schema_id '%s'"),
                *FieldValue.FieldId.ToString(),
                *FieldValue.SchemaId);
            return false;
        }
        ValuesById.Add(FieldValue.FieldId, &FieldValue);
    }

    TSet<FName> ConsumedFieldIds;
    OutPlan.Reset();
    OutPlan.Reserve(Elements.Num());
    for (const FGV2ScreenElementRecord& Element : Elements)
    {
        const FGV2ScreenFieldValue* const* Candidate = ValuesById.Find(Element.Descriptor.FieldId);
        if (Candidate == nullptr)
        {
            if (Element.Descriptor.bRequired)
            {
                OutError = FString::Printf(
                    TEXT("required field '%s' is absent"),
                    *Element.Descriptor.FieldId.ToString());
                return false;
            }
        }
        else
        {
            if ((*Candidate)->SchemaId != Element.Descriptor.SchemaId)
            {
                OutError = FString::Printf(
                    TEXT("field '%s' expects schema '%s' but received '%s'"),
                    *Element.Descriptor.FieldId.ToString(),
                    *Element.Descriptor.SchemaId,
                    *(*Candidate)->SchemaId);
                return false;
            }
            if (!IGV2DynamicScreenElement::Execute_CanApplyScreenField(
                    Element.Widget,
                    **Candidate))
            {
                OutError = FString::Printf(
                    TEXT("dynamic element rejected field '%s' during validation"),
                    *Element.Descriptor.FieldId.ToString());
                return false;
            }
            ConsumedFieldIds.Add(Element.Descriptor.FieldId);
        }

        FGV2ScreenApplyRecord& Record = OutPlan.AddDefaulted_GetRef();
        Record.Widget = Element.Widget;
        Record.bReset = Candidate == nullptr;
        if (Candidate != nullptr)
        {
            Record.CandidateValue = **Candidate;
        }
        if (bCapturePreviousValues
            && !IGV2DynamicScreenElement::Execute_CaptureScreenField(
                Element.Widget,
                Record.PreviousValue))
        {
            OutError = FString::Printf(
                TEXT("could not capture current value of field '%s'"),
                *Element.Descriptor.FieldId.ToString());
            return false;
        }
    }

    for (const TPair<FName, const FGV2ScreenFieldValue*>& Pair : ValuesById)
    {
        if (!ConsumedFieldIds.Contains(Pair.Key))
        {
            OutError = FString::Printf(
                TEXT("payload contains unknown field '%s'"),
                *Pair.Key.ToString());
            return false;
        }
    }
    return true;
}
}

bool UGV2ScreenWidgetBase::ApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields)
{
    TArray<FGV2ScreenApplyRecord> ApplyPlan;
    FString Error;
    if (!BuildApplyPlan(*this, ScreenFields, ApplyPlan, Error, true))
    {
        UE_LOG(LogGV2ScreenWidget, Error, TEXT("ApplyScreenFields rejected: %s"), *Error);
        return false;
    }

    int32 AppliedCount = 0;
    for (FGV2ScreenApplyRecord& Record : ApplyPlan)
    {
        const bool bApplied = Record.bReset
            ? IGV2DynamicScreenElement::Execute_ResetScreenField(Record.Widget)
            : IGV2DynamicScreenElement::Execute_ApplyScreenField(
                Record.Widget,
                Record.CandidateValue);
        if (!bApplied)
        {
            // The element that returned false may have changed local state before reporting
            // the failure, so restore it together with every previously committed element.
            for (int32 RollbackIndex = AppliedCount; RollbackIndex >= 0; --RollbackIndex)
            {
                FGV2ScreenApplyRecord& AppliedRecord = ApplyPlan[RollbackIndex];
                if (!IGV2DynamicScreenElement::Execute_ApplyScreenField(
                        AppliedRecord.Widget,
                        AppliedRecord.PreviousValue))
                {
                    UE_LOG(
                        LogGV2ScreenWidget,
                        Error,
                        TEXT("Rollback failed for dynamic field '%s'"),
                        *AppliedRecord.PreviousValue.FieldId.ToString());
                }
            }
            UE_LOG(LogGV2ScreenWidget, Error, TEXT("ApplyScreenFields failed during commit"));
            return false;
        }
        ++AppliedCount;
    }

    OnScreenFieldsApplied();
    return true;
}

bool UGV2ScreenWidgetBase::CanApplyScreenFields(
    const TArray<FGV2ScreenFieldValue>& ScreenFields) const
{
    TArray<FGV2ScreenApplyRecord> ApplyPlan;
    FString Error;
    return BuildApplyPlan(*this, ScreenFields, ApplyPlan, Error, false);
}

TArray<FGV2ScreenFieldDescriptor> UGV2ScreenWidgetBase::GetScreenFieldContract() const
{
    TArray<FGV2ScreenElementRecord> Elements;
    FString Error;
    if (!CollectElements(*this, Elements, Error))
    {
        UE_LOG(LogGV2ScreenWidget, Warning, TEXT("GetScreenFieldContract failed: %s"), *Error);
        return {};
    }

    TArray<FGV2ScreenFieldDescriptor> Result;
    Result.Reserve(Elements.Num());
    for (const FGV2ScreenElementRecord& Element : Elements)
    {
        Result.Add(Element.Descriptor);
    }
    return Result;
}
