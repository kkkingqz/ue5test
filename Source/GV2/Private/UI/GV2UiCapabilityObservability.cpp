#include "UI/GV2UiCapabilityObservability.h"

#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Widget.h"
#include "GV2ContentCore/UiSchema.h"

namespace
{
using namespace GV2ContentCore;

// A resource capability constrains which resources its target accepts: a tile block rejects
// non-tile art, a fixed_aspect block rejects mismatched ratios. The default probe pair is
// therefore not universally applicable, and a rejected probe must not be read as "the
// capability is unwired". These candidates are tried until two of them are accepted.
//
// DCA-21: this is core-level code (module GV2), so it must not name a higher package's
// content (ADR-0035 §5) -- candidates come from whatever the loaded UGV2ImageResourceCatalog
// actually contains, not a hardcoded list of specific package_id-prefixed resource ids. A
// session with more or fewer image resources than today's core+textsystem set changes what
// this returns without anyone editing this file.
// PAH-08: phase=prepare -- observability harness, outside any presentation
// transaction; it probes capabilities, it does not apply a revision.
TArray<FString> GetResourceProbeCandidates()
{
    TArray<FString> Candidates;
    if (const UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalog::GetSessionCatalog())
    {
        for (const FGV2ImageResourceDefinition& Entry : Catalog->GetEntries())
        {
            Candidates.Add(Entry.ResourceId);
        }
    }
    return Candidates;
}

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
            const TArray<FString> Candidates = GetResourceProbeCandidates();
            if (Candidates.Num() >= 2)
            {
                return TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>(
                    FGV2PreparedUiValue::MakeStableId(Candidates[0], TEXT("resource")),
                    FGV2PreparedUiValue::MakeStableId(Candidates[1], TEXT("resource")));
            }
        }
        return TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>>();
    case EGV2PreparedUiValueKind::Null:
    case EGV2PreparedUiValueKind::Object:
    case EGV2PreparedUiValueKind::Array:
        return TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>>();
    }
    return TOptional<TPair<FGV2PreparedUiValue, FGV2PreparedUiValue>>();
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
    case EGV2PreparedUiValueKind::Null:
    case EGV2PreparedUiValueKind::Object:
    case EGV2PreparedUiValueKind::Array:
        return nullptr;
    }
    return nullptr;
}

UWidget* ResolveCapabilityTarget(UUserWidget* HostWidget, const FGV2UiPropertyCapability& Cap)
{
    if (HostWidget == nullptr)
    {
        return nullptr;
    }
    return (Cap.TargetName != NAME_None)
        ? HostWidget->GetWidgetFromName(Cap.TargetName)
        : Cast<UWidget>(HostWidget);
}

void ResetCapabilityTarget(UUserWidget* HostWidget, const FGV2UiPropertyCapability& Cap)
{
    if (UWidget* Target = ResolveCapabilityTarget(HostWidget, Cap))
    {
        FGV2ImageResourcePropertyConsumer Consumer;
        Consumer.Reset(Target);
    }
}

bool PrepareAndCommitSingleProperty(
    UUserWidget* HostWidget,
    const FString& PropName,
    const FGV2UiPropertyCapability& Cap,
    const FCompiledUiFieldSpecPtr& FieldSpec,
    const FGV2PreparedUiValue& Value,
    FString& OutState,
    FString& OutFailureDetail)
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
        TArray<FString> Reasons;
        for (const FGV2UiSchemaCompatibilityDiagnostic& Diag : Diagnostics)
        {
            Reasons.Add(FString::Printf(TEXT("%s (%s)"), *Diag.Message, *Diag.Code));
        }
        OutFailureDetail = Reasons.Num() > 0 ? FString::Join(Reasons, TEXT("; ")) : TEXT("prepare rejected without diagnostic");
        return false;
    }

    FString FailedPath, Error;
    if (!CommitUiHostProperties(HostWidget, Plan, FailedPath, Error))
    {
        OutFailureDetail = FString::Printf(TEXT("commit failed at '%s': %s"), *FailedPath, *Error);
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

    // DUC-03: `key` is now one shared IGV2UiPropertyHost::GetKey(), not per-class storage --
    // one generic readback here instead of the same three-line block repeated for every host
    // that declares a `key` capability. Not the same thing as `selected_key`/
    // `default_tab_key` below, which remain their own distinct capabilities.
    if (const IGV2UiPropertyHost* PropertyHost = Cast<IGV2UiPropertyHost>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("key=\"%s\""), *PropertyHost->GetKey().ToString()));
    }

    if (const UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("text=\"%s\""), *TextBlock->GetText().ToString()));
    }
    if (const UProgressBar* Bar = Cast<UProgressBar>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("percent=%f"), Bar->GetPercent()));
    }
    if (const UGV2ProgressBarWidgetBase* ProgressHost = Cast<UGV2ProgressBarWidgetBase>(TargetWidget))
    {
        // A declared composite can target a reusable WBP_ProgressBar directly.
        // Its Number consumer updates the adapter's stored progress, so the
        // observability probe must read that adapter state rather than only a raw
        // UProgressBar target used by the direct widget capability.
        Parts.Add(FString::Printf(TEXT("percent=%f"), ProgressHost->GetProgress()));
    }
    if (const UImage* Image = Cast<UImage>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("brush_resource=%p"), Image->GetBrush().GetResourceObject()));
    }
    if (const IGV2UiBindingTarget* BindingTarget = Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("binding=\"%s\""), *BindingTarget->GetBindingHandle().ToString()));
    }
    if (const UCheckBox* CB = Cast<UCheckBox>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("checked=%d"), CB->IsChecked() ? 1 : 0));
        Parts.Add(FString::Printf(TEXT("interaction_enabled=%d"), CB->GetIsEnabled() ? 1 : 0));
    }
    if (const UEditableTextBox* ETB = Cast<UEditableTextBox>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("text=\"%s\""), *ETB->GetText().ToString()));
        Parts.Add(FString::Printf(TEXT("hint=\"%s\""), *ETB->GetHintText().ToString()));
        Parts.Add(FString::Printf(TEXT("is_read_only=%d"), ETB->GetIsReadOnly() ? 1 : 0));
        if (const UGV2InputFieldWidgetBase* Input = ETB->GetTypedOuter<UGV2InputFieldWidgetBase>())
        {
            Parts.Add(FString::Printf(TEXT("max_length=%lld"), Input->GetMaxLength()));
        }
    }
    if (const UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("rich_text=\"%s\""), *RichTextBlock->GetText().ToString()));
    }
    if (const UGV2PortraitWidgetBase* PW = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        // Without reading the inner brush a portrait capability looks unobservable even
        // though the consumer applied a resource to it.
        if (const UImage* PortraitImage = PW->GetPortraitImage())
        {
            Parts.Add(FString::Printf(TEXT("portrait_brush=%p"), PortraitImage->GetBrush().GetResourceObject()));
        }
    }
    if (const UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        if (const UCommonRichTextBlock* InnerRich = RTW->GetRichTextBlock())
        {
            Parts.Add(FString::Printf(TEXT("rich_text=\"%s\""), *InnerRich->GetText().ToString()));
        }
    }
    if (const UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        if (const UCommonTextBlock* InnerText = TW->GetTextBlock())
        {
            Parts.Add(FString::Printf(TEXT("text=\"%s\""), *InnerText->GetText().ToString()));
        }
    }
    if (const UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        // When a capability targets the image host rather than the raw UImage, the brush
        // lives one level down; without reading it the capability looks unwired.
        if (const UImage* InnerImage = const_cast<UGV2ImageWidgetBase*>(ImageBase)->GetImageWidget())
        {
            Parts.Add(FString::Printf(TEXT("image_brush=%p"), InnerImage->GetBrush().GetResourceObject()));
        }
    }
    if (const UGV2DropdownSelectWidgetBase* Dropdown = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("selected_key=\"%s\""), *Dropdown->GetSelectedKey().ToString()));
        Parts.Add(FString::Printf(TEXT("is_open=%d"), Dropdown->IsDropdownOpen() ? 1 : 0));
    }
    if (const UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget))
    {
        Parts.Add(FString::Printf(TEXT("default_tab_key=\"%s\""), *TabContainer->GetDefaultTabKey().ToString()));
    }
    // A composite may declare a Text capability that targets a nested Button host rather than
    // a raw text block; without reading the nested label such a capability looks unobservable.
    if (const UGV2ButtonWidgetBase* ButtonHost = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        if (const UCommonTextBlock* ButtonLabel = ButtonHost->GetLabelText())
        {
            Parts.Add(FString::Printf(TEXT("label=\"%s\""), *ButtonLabel->GetText().ToString()));
        }
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
        if (Cap.TargetType == EGV2UiCapabilityTargetType::CollectionHost)
        {
            // PCC-10: a collection capability is not itself a RendererControl -- but every
            // entry it repeats is an IGV2UiPropertyHost with its own capability tree, and
            // that tree was exactly UPP-R1's blind spot (sweep proved the host's own
            // capabilities, never looked inside what the collection actually repeats).
            // Instantiate one fresh entry widget and recurse the identical rule into it.
            if (Cap.EntryWidgetClass == nullptr)
            {
                OutFailures.Add({ PropName,
                    TEXT("core:diagnostic.ui_observability.no_entry_widget_class: collection capability "
                         "declares no EntryWidgetClass to instantiate and sweep") });
                bAllObservable = false;
                continue;
            }

            UWorld* World = HostWidget != nullptr ? HostWidget->GetWorld() : nullptr;
            UUserWidget* EntryWidget = World != nullptr ? CreateWidget<UUserWidget>(World, Cap.EntryWidgetClass) : nullptr;
            IGV2UiPropertyHost* EntryHost = Cast<IGV2UiPropertyHost>(EntryWidget);
            if (EntryHost == nullptr)
            {
                OutFailures.Add({ PropName,
                    FString::Printf(TEXT("core:diagnostic.ui_observability.entry_not_property_host: entry widget "
                         "class '%s' could not be instantiated or does not implement IGV2UiPropertyHost"),
                        *Cap.EntryWidgetClass->GetName()) });
                bAllObservable = false;
                continue;
            }

            FGV2UiCapabilityBuilder EntryBuilder;
            EntryHost->DescribeUiCapabilities(EntryBuilder);
            const FGV2UiCapabilityTree EntryCaps = EntryBuilder.Build();

            TArray<FGV2UiObservabilityFailure> EntryFailures;
            if (!RunUiCapabilityObservabilityHarness(EntryWidget, EntryCaps, EntryFailures))
            {
                for (const FGV2UiObservabilityFailure& EntryFailure : EntryFailures)
                {
                    OutFailures.Add({
                        FString::Printf(TEXT("%s[].%s"), *PropName, *EntryFailure.PropertyName),
                        EntryFailure.Reason });
                }
                bAllObservable = false;
            }
            continue;
        }

        if (Cap.TargetType != EGV2UiCapabilityTargetType::RendererControl)
        {
            // NestedScreen/composite capabilities are proven by the composite migration
            // tasks (UPP-20+), which have their own consumers.
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

        FString StateAfterA, StateAfterB, FailureDetail;
        bool bOkA = PrepareAndCommitSingleProperty(HostWidget, PropName, Cap, FieldSpec, Pair->Key, StateAfterA, FailureDetail);
        bool bOkB = bOkA
            && PrepareAndCommitSingleProperty(HostWidget, PropName, Cap, FieldSpec, Pair->Value, StateAfterB, FailureDetail);

        // Resource capabilities constrain their accepted resources (tile blocks, fixed-aspect
        // blocks). A rejected default probe means the probe is wrong for this target, not that
        // the capability is unwired -- retry across the candidate set before concluding.
        if ((!bOkA || !bOkB)
            && Cap.SupportedKind == EGV2PreparedUiValueKind::StableId
            && Cap.TargetKind == TEXT("resource"))
        {
            const TArray<FString> ProbeCandidates = GetResourceProbeCandidates();
            TArray<FString> Accepted;
            TArray<FString> AcceptedStates;
            for (const FString& Candidate : ProbeCandidates)
            {
                FString State, Detail;
                if (PrepareAndCommitSingleProperty(
                        HostWidget, PropName, Cap, FieldSpec,
                        FGV2PreparedUiValue::MakeStableId(Candidate, TEXT("resource")), State, Detail))
                {
                    if (!AcceptedStates.Contains(State))
                    {
                        Accepted.Add(Candidate);
                        AcceptedStates.Add(State);
                    }
                    if (AcceptedStates.Num() == 2)
                    {
                        break;
                    }
                }
            }
            if (UWidget* ProbeTarget = ResolveCapabilityTarget(HostWidget, Cap))
            {
                FString PolicyText = TEXT("<not an image host>");
                if (const UGV2ImageWidgetBase* ImgHost = Cast<UGV2ImageWidgetBase>(ProbeTarget))
                {
                    PolicyText = FString::Printf(TEXT("policy=%d aspect=%f"),
                        static_cast<int32>(ImgHost->GetScalePolicy()), ImgHost->GetFixedAspectRatio());
                }
                FailureDetail += FString::Printf(
                    TEXT(" | retry over %d candidates accepted %d distinct states; target=%s %s"),
                    ProbeCandidates.Num(),
                    AcceptedStates.Num(), *ProbeTarget->GetClass()->GetName(), *PolicyText);
            }
            if (AcceptedStates.Num() >= 2)
            {
                bOkA = bOkB = true;
                StateAfterA = AcceptedStates[0];
                StateAfterB = AcceptedStates[1];
            }
            else if (Accepted.Num() == 1)
            {
                // Only one resource in the repository is compatible with this target, so an
                // A/B pair cannot exist. The capability is still provably wired if applying
                // that resource produces a state distinguishable from the reset state.
                FString AppliedState = AcceptedStates[0];
                ResetCapabilityTarget(HostWidget, Cap);
                const FString ResetState = CaptureUiTargetState(ResolveCapabilityTarget(HostWidget, Cap));
                bOkA = bOkB = true;
                StateAfterA = AppliedState;
                StateAfterB = ResetState;
            }
        }

        if (!bOkA || !bOkB)
        {
            OutFailures.Add({ PropName,
                FString(TEXT("core:diagnostic.ui_observability.prepare_or_commit_failed: ")) + FailureDetail + TEXT(" -- capability could not be "
                     "prepared/committed for at least one probe value") });
            bAllObservable = false;
            continue;
        }

        if (StateAfterA == StateAfterB)
        {
            UWidget* DiagTarget = ResolveCapabilityTarget(HostWidget, Cap);
            FString TargetDesc = DiagTarget != nullptr ? DiagTarget->GetClass()->GetName() : TEXT("<null>");
            if (UGV2ImageWidgetBase* ImgDiag = Cast<UGV2ImageWidgetBase>(DiagTarget))
            {
                TargetDesc += FString::Printf(TEXT(" inner_image=%s"), ImgDiag->GetImageWidget() ? TEXT("bound") : TEXT("null"));
            }
            OutFailures.Add({ PropName,
                FString::Printf(TEXT("core:diagnostic.ui_observability.not_distinguishable: target=%s Capture(Commit(A)) == "
                                      "Capture(Commit(B)) == \"%s\""), *TargetDesc, *StateAfterA) });
            bAllObservable = false;
        }
    }

    return bAllObservable;
}
