#include "UI/GV2UiCapabilityObservability.h"

#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "GV2ContentCore/UiSchema.h"

namespace
{
using namespace GV2ContentCore;

TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>> MakeDistinctValuePair(const FGV2UiPropertyCapability& Cap)
{
    switch (Cap.SupportedKind)
    {
    case EGV2PreparedUiValueKind::Boolean:
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeBoolean(false), FGV2PreparedUiValue::MakeBoolean(true));

    case EGV2PreparedUiValueKind::Integer:
    {
        const int64 Lo = Cap.IntMin.Get(0);
        const int64 Hi = Cap.IntMax.IsSet() && Cap.IntMax.GetValue() > Lo ? Cap.IntMax.GetValue() : Lo + 1;
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeInteger(Lo), FGV2PreparedUiValue::MakeInteger(Hi));
    }

    case EGV2PreparedUiValueKind::Number:
    {
        const double Lo = Cap.NumberMin.Get(0.0);
        const double Hi = Cap.NumberMax.IsSet() && Cap.NumberMax.GetValue() > Lo ? Cap.NumberMax.GetValue() : Lo + 1.0;
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeNumber(Lo), FGV2PreparedUiValue::MakeNumber(Hi));
    }

    case EGV2PreparedUiValueKind::String:
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeString(TEXT("gv2_observability_probe_a")),
            FGV2PreparedUiValue::MakeString(TEXT("gv2_observability_probe_b")));

    case EGV2PreparedUiValueKind::Key:
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeKey(TEXT("probe_key_a")),
            FGV2PreparedUiValue::MakeKey(TEXT("probe_key_b")));

    case EGV2PreparedUiValueKind::Text:
    {
        FGV2TextViewModel A;
        A.Text = FText::FromString(TEXT("Probe A"));
        FGV2TextViewModel B;
        B.Text = FText::FromString(TEXT("Probe B"));
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeText(A), FGV2PreparedUiValue::MakeText(B));
    }

    case EGV2PreparedUiValueKind::Binding:
        return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
            FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("probe@1:1"))),
            FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("probe@1:2"))));

    case EGV2PreparedUiValueKind::StableId:
        if (Cap.TargetKind == TEXT("resource"))
        {
            return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
                FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_icon"), TEXT("resource")),
                FGV2PreparedUiValue::MakeStableId(TEXT("textsystem:resource.ui.missing_portrait"), TEXT("resource")));
        }
        return TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>>();
    default:
        return TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>>();
    }
}

FCompiledUiFieldSpecPtr MakeMatchingFieldSpec(const FGV2UiPropertyCapability& Cap)
{
    switch (Cap.SupportedKind)
    {
    case EGV2PreparedUiValueKind::Boolean:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::Boolean;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Integer:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Integer;
        if (Cap.IntMin.IsSet()) { Scalar.MinimumInteger = Cap.IntMin.GetValue(); }
        if (Cap.IntMax.IsSet()) { Scalar.MaximumInteger = Cap.IntMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::Number:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Number;
        if (Cap.NumberMin.IsSet()) { Scalar.MinimumNumber = Cap.NumberMin.GetValue(); }
        if (Cap.NumberMax.IsSet()) { Scalar.MaximumNumber = Cap.NumberMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::String:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::String;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Key:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key);
    case EGV2PreparedUiValueKind::Text:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
    case EGV2PreparedUiValueKind::Binding:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding);
    case EGV2PreparedUiValueKind::StableId:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Ref);
        Spec->RefTargetKind = Cap.TargetKind.IsEmpty() ? "resource" : TCHAR_TO_UTF8(*Cap.TargetKind);
        return Spec;
    }
    default:
        return nullptr;
    }
}

bool PrepareAndCommitSingleProperty(
    UUserWidget* HostWidget,
    const FString& PropName,
    const FGV2UiPropertyCapability& Cap,
    const FCompiledUiFieldSpecPtr& FieldSpec,
    const FGV2PreparedUiValue& Value,
    FString& OutState)
{
    FGV2UiCapabilityTree SingleCap;
    SingleCap.Properties.Add(PropName, Cap);

    FCompiledUiFieldSpec Schema;
    Schema.Kind = EUiFieldKind::Object;
    Schema.Fields.push_back({ TCHAR_TO_UTF8(*PropName), false, FieldSpec });

    TArray<TPair<FString, FGV2PreparedUiValue>> Fields;
    Fields.Emplace(PropName, Value);
    const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));
    const FGV2PreparedUiObject EmptyPrev;

    FGV2UiHostMutationPlan Plan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
    if (!PrepareUiHostProperties(
            HostWidget, SingleCap, *Candidate, Schema, TEXT("core:schema.ui_field.observability_probe.v1"),
            TEXT("observability_probe"), EmptyPrev, Plan, Diagnostics))
    {
        return false;
    }

    FString FailedPath, Error;
    if (!CommitUiHostProperties(HostWidget, Plan, FailedPath, Error))
    {
        return false;
    }

    UWidget* Target = nullptr;
    if (HostWidget != nullptr)
    {
        Target = (Cap.TargetName != NAME_None)
            ? HostWidget->GetWidgetFromName(Cap.TargetName)
            : Cast<UWidget>(HostWidget);
    }
    OutState = CaptureUiTargetState(Target);
    return true;
}
}

FString CaptureUiTargetState(const UWidget* TargetWidget)
{
    if (TargetWidget == nullptr)
    {
        return TEXT("<null>");
    }

    TArray<FString> Parts;
    Parts.Add(FString::Printf(TEXT("enabled=%d"), TargetWidget->GetIsEnabled() ? 1 : 0));

    if (const UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("text=\"%s\""), *TextBlock->GetText().ToString()));
    }
    if (const UProgressBar* Bar = Cast<UProgressBar>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("percent=%f"), Bar->GetPercent()));
    }
    if (const UImage* Image = Cast<UImage>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("brush_resource=%p"), Image->GetBrush().GetResourceObject()));
    }
    if (const IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("binding=\"%s\""), *BindingTarget->GetBindingHandle().ToString()));
    }
    if (const UGV2ButtonWidgetBase* Button = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("key=\"%s\""), *Button->GetKey().ToString()));
    }
    return FString::Join(Parts, TEXT("|"));
}

bool RunUiCapabilityObservabilityHarness(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    TArray<FGV2UiObservabilityFailure>& OutFailures)
{
    using namespace GV2ContentCore;

    bool bAllObservable = true;
    for (const auto& Entry : Capabilities.Properties)
    {
        const FString& PropName = Entry.Key;
        const FGV2UiPropertyCapability& Cap = Entry.Value;
        if (Cap.TargetType != EGV2UiCapabilityTargetType::RendererControl)
        {
            // CollectionHost/NestedScreen/composite capabilities are proven by the
            // composite migration tasks (UPP-20+), which have their own consumers.
            continue;
        }

        const TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>> Pair = MakeDistinctValuePair(Cap);
        if (!Pair.IsSet())
        {
            OutFailures.Add({ PropName,
                TEXT("core:diagnostic.ui_observability.no_distinct_pair: harness cannot synthesize two "
                     "distinguishable values for this capability's kind") });
            bAllObservable = false;
            continue;
        }

        const FCompiledUiFieldSpecPtr FieldSpec = MakeMatchingFieldSpec(Cap);
        if (!FieldSpec)
        {
            OutFailures.Add({ PropName,
                TEXT("core:diagnostic.ui_observability.no_matching_schema: harness cannot build a schema "
                     "matching this capability's kind") });
            bAllObservable = false;
            continue;
        }

        FString StateAfterA, StateAfterB;
        const bool bOkA = PrepareAndCommitSingleProperty(HostWidget, PropName, Cap, FieldSpec, Pair->Key, StateAfterA);
        const bool bOkB = bOkA
            && PrepareAndCommitSingleProperty(HostWidget, PropName, Cap, FieldSpec, Pair->Value, StateAfterB);

        if (!bOkA || !bOkB)
        {
            OutFailures.Add({ PropName,
                TEXT("core:diagnostic.ui_observability.prepare_or_commit_failed: capability could not be "
                     "prepared/committed for at least one probe value") });
            bAllObservable = false;
            continue;
        }

        if (StateAfterA == StateAfterB)
        {
            OutFailures.Add({ PropName,
                FString::Printf(TEXT("core:diagnostic.ui_observability.not_distinguishable: Capture(Commit(A)) == "
                                      "Capture(Commit(B)) == \"%s\""), *StateAfterA) });
            bAllObservable = false;
        }
    }

    return bAllObservable;
}
