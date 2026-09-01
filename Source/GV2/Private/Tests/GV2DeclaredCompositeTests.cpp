#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiSchemaCache.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include <algorithm>

namespace
{
struct FDeclaredCapabilityExpectation
{
    const TCHAR* EnumName;
    const TCHAR* PropertyName;
    const TCHAR* ChildWidgetName;
    EGV2PreparedUiValueKind PreparedKind;
    EGV2UiCapabilityTargetType TargetType;
    const TCHAR* TargetKind;
};

bool SetDeclaredCapabilityEntry(
    FAutomationTestBase& Test,
    FScriptArrayHelper& Entries,
    const FStructProperty& EntryProperty,
    const FDeclaredCapabilityExpectation& Expected)
{
    UScriptStruct* const EntryStruct = EntryProperty.Struct;
    const FNameProperty* const PropertyNameProperty = FindFProperty<FNameProperty>(EntryStruct, TEXT("PropertyName"));
    const FNameProperty* const ChildWidgetNameProperty = FindFProperty<FNameProperty>(EntryStruct, TEXT("ChildWidgetName"));
    const FEnumProperty* const KindProperty = FindFProperty<FEnumProperty>(EntryStruct, TEXT("Kind"));
    UEnum* const KindEnum = FindObject<UEnum>(nullptr, TEXT("/Script/GV2.EGV2DeclaredUiCapabilityKind"));

    Test.TestNotNull(TEXT("DUC-05: declared capability has PropertyName"), PropertyNameProperty);
    Test.TestNotNull(TEXT("DUC-05: declared capability has ChildWidgetName"), ChildWidgetNameProperty);
    Test.TestNotNull(TEXT("DUC-05: declared capability has Kind enum"), KindProperty);
    Test.TestNotNull(TEXT("DUC-05: declared capability kind enum is registered"), KindEnum);
    if (PropertyNameProperty == nullptr || ChildWidgetNameProperty == nullptr || KindProperty == nullptr || KindEnum == nullptr)
    {
        return false;
    }

    const int64 EnumValue = KindEnum->GetValueByNameString(Expected.EnumName);
    Test.TestTrue(
        *FString::Printf(TEXT("DUC-05: kind '%s' is available to Designer"), Expected.EnumName),
        EnumValue != INDEX_NONE);
    if (EnumValue == INDEX_NONE)
    {
        return false;
    }

    const int32 EntryIndex = Entries.AddValue();
    void* const Entry = Entries.GetRawPtr(EntryIndex);
    PropertyNameProperty->SetPropertyValue_InContainer(Entry, FName(Expected.PropertyName));
    ChildWidgetNameProperty->SetPropertyValue_InContainer(Entry, FName(Expected.ChildWidgetName));
    KindProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        KindProperty->ContainerPtrToValuePtr<void>(Entry),
        EnumValue);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DeclaredCompositeTest,
    "GV2.UI.DeclaredComposite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeTest::RunTest(const FString& Parameters)
{
    UClass* const CompositeClass = FindObject<UClass>(nullptr, TEXT("/Script/GV2.GV2DeclaredCompositeWidgetBase"));
    TestNotNull(TEXT("DUC-05: generic declared composite class exists"), CompositeClass);
    if (CompositeClass == nullptr)
    {
        return false;
    }

    TestTrue(
        TEXT("DUC-05: generic composite is a property host"),
        CompositeClass->ImplementsInterface(UGV2UiPropertyHost::StaticClass()));
    TestTrue(
        TEXT("DUC-05: generic composite can be a Screen Field host"),
        CompositeClass->ImplementsInterface(UGV2ScreenFieldHost::StaticClass()));

    FArrayProperty* const DeclaredCapabilitiesProperty =
        FindFProperty<FArrayProperty>(CompositeClass, TEXT("DeclaredCapabilities"));
    TestNotNull(TEXT("DUC-05: Designer has a declared capability list"), DeclaredCapabilitiesProperty);
    if (DeclaredCapabilitiesProperty == nullptr)
    {
        return false;
    }
    TestTrue(
        TEXT("DUC-05: declared capability list is editable in Designer"),
        DeclaredCapabilitiesProperty->HasAnyPropertyFlags(CPF_Edit));

    FStructProperty* const EntryProperty = CastField<FStructProperty>(DeclaredCapabilitiesProperty->Inner);
    TestNotNull(TEXT("DUC-05: declared capability list stores triples"), EntryProperty);
    if (EntryProperty == nullptr)
    {
        return false;
    }

    UUserWidget* const Composite = NewObject<UUserWidget>(GetTransientPackage(), CompositeClass);
    TestNotNull(TEXT("DUC-05: generic composite instance can be created"), Composite);
    IGV2UiPropertyHost* const PropertyHost = Composite != nullptr ? Cast<IGV2UiPropertyHost>(Composite) : nullptr;
    TestNotNull(TEXT("DUC-05: generic composite instance exposes property host interface"), PropertyHost);
    if (PropertyHost == nullptr)
    {
        return false;
    }

    FScriptArrayHelper Entries(DeclaredCapabilitiesProperty, DeclaredCapabilitiesProperty->ContainerPtrToValuePtr<void>(Composite));
    Entries.EmptyValues();
    const FDeclaredCapabilityExpectation Expectations[] = {
        { TEXT("Boolean"), TEXT("bool_value"), TEXT("BooleanChild"), EGV2PreparedUiValueKind::Boolean, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("Integer"), TEXT("integer_value"), TEXT("IntegerChild"), EGV2PreparedUiValueKind::Integer, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("Number"), TEXT("number_value"), TEXT("NumberChild"), EGV2PreparedUiValueKind::Number, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("String"), TEXT("string_value"), TEXT("StringChild"), EGV2PreparedUiValueKind::String, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("Key"), TEXT("key_value"), TEXT("KeyChild"), EGV2PreparedUiValueKind::Key, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("Text"), TEXT("text_value"), TEXT("TextChild"), EGV2PreparedUiValueKind::Text, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("ResourceRef"), TEXT("resource_value"), TEXT("ResourceChild"), EGV2PreparedUiValueKind::StableId, EGV2UiCapabilityTargetType::RendererControl, TEXT("resource") },
        { TEXT("Binding"), TEXT("binding_value"), TEXT("BindingChild"), EGV2PreparedUiValueKind::Binding, EGV2UiCapabilityTargetType::RendererControl, TEXT("") },
        { TEXT("CollectionHost"), TEXT("collection_value"), TEXT("CollectionChild"), EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost, TEXT("") },
        { TEXT("RichTextSpans"), TEXT("spans_value"), TEXT("RichTextChild"), EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CustomControl, TEXT("") },
        { TEXT("NestedScreen"), TEXT("screens_value"), TEXT("ScreensChild"), EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::NestedScreen, TEXT("") },
    };

    for (const FDeclaredCapabilityExpectation& Expected : Expectations)
    {
        if (!SetDeclaredCapabilityEntry(*this, Entries, *EntryProperty, Expected))
        {
            return false;
        }
    }

    FGV2UiCapabilityBuilder Builder;
    PropertyHost->DescribeUiCapabilities(Builder);
    const FGV2UiCapabilityTree Capabilities = Builder.Build();
    TestEqual(
        TEXT("DUC-05: every consumer-backed kind produces a capability"),
        Capabilities.Num(),
        static_cast<int32>(UE_ARRAY_COUNT(Expectations)));
    for (const FDeclaredCapabilityExpectation& Expected : Expectations)
    {
        const FGV2UiPropertyCapability* const Capability = Capabilities.FindProperty(Expected.PropertyName);
        TestNotNull(*FString::Printf(TEXT("DUC-05: '%s' is declared"), Expected.PropertyName), Capability);
        if (Capability != nullptr)
        {
            TestEqual(*FString::Printf(TEXT("DUC-05: '%s' has the declared prepared kind"), Expected.PropertyName), Capability->SupportedKind, Expected.PreparedKind);
            TestEqual(*FString::Printf(TEXT("DUC-05: '%s' has the declared target type"), Expected.PropertyName), Capability->TargetType, Expected.TargetType);
            TestEqual(*FString::Printf(TEXT("DUC-05: '%s' targets the declared child"), Expected.PropertyName), Capability->TargetName, FName(Expected.ChildWidgetName));
            TestEqual(*FString::Printf(TEXT("DUC-05: '%s' preserves its target kind"), Expected.PropertyName), Capability->TargetKind, FString(Expected.TargetKind));
        }
    }

    Entries.EmptyValues();
    const FDeclaredCapabilityExpectation MissingChild = {
        TEXT("Text"), TEXT("missing_text"), TEXT("MissingText"), EGV2PreparedUiValueKind::Text, EGV2UiCapabilityTargetType::RendererControl, TEXT("") };
    if (!SetDeclaredCapabilityEntry(*this, Entries, *EntryProperty, MissingChild))
    {
        return false;
    }

    FGV2UiCapabilityBuilder MissingChildBuilder;
    PropertyHost->DescribeUiCapabilities(MissingChildBuilder);
    GV2ContentCore::FCompiledUiFieldSpec Schema;
    Schema.Kind = GV2ContentCore::EUiFieldKind::Object;
    Schema.Fields.push_back({ "missing_text", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Text) });

    FGV2TextViewModel TextValue;
    TextValue.Text = FText::FromString(TEXT("missing target must fail before commit"));
    TMap<FString, FGV2PreparedUiValue> Fields;
    Fields.Add(TEXT("missing_text"), FGV2PreparedUiValue::MakeText(TextValue));
    const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(MoveTemp(Fields));
    FGV2UiHostMutationPlan Plan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
    const bool bPrepared = PrepareUiHostProperties(
        Composite,
        MissingChildBuilder.Build(),
        *Candidate,
        Schema,
        TEXT("core:schema.ui_field.declared_composite_probe.v1"),
        TEXT("declared_composite"),
        PropertyHost->GetPropertyHostState().GetLastCommittedProperties(),
        Plan,
        Diagnostics);
    TestFalse(TEXT("DUC-05: missing declared child is rejected in preflight"), bPrepared);
    TestTrue(
        TEXT("DUC-05: missing declared child reports typed missing_target diagnostic"),
        Diagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
        {
            return Diagnostic.Code == TEXT("core:diagnostic.ui_consumer.missing_target");
        }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DeclaredCompositeFlatSchemaTest,
    "GV2.UI.DeclaredComposite.FlatSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeFlatSchemaTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // Source 1 is the authored content schema. It intentionally names the composite
    // properties themselves, never the internal capabilities of either child.
    FGV2UiSchemaCache SchemaCache({ FPaths::ProjectDir() / TEXT("GameData/textsystem") });
    FString SchemaError;
    const FCompiledUiFieldSpecPtr Schema = SchemaCache.GetCompiledSchema(
        "textsystem:schema.ui_field.declared_composite_fixture.v1",
        SchemaError);
    TestNotNull(TEXT("DUC-06: fixture's authored flat schema loads from content"), Schema.get());
    if (Schema == nullptr)
    {
        AddError(SchemaError);
        return false;
    }

    TestEqual(TEXT("DUC-06: composite schema root is an object"), Schema->Kind, EUiFieldKind::Object);
    TestEqual(TEXT("DUC-06: declared two-element block has exactly two schema fields"), static_cast<int32>(Schema->Fields.size()), 2);

    const auto FindField = [Schema](const char* Name) -> const FCompiledUiFieldSpecPtr*
    {
        const auto It = std::find_if(
            Schema->Fields.begin(),
            Schema->Fields.end(),
            [Name](const FCompiledUiObjectField& Field) { return Field.Name == Name; });
        return It != Schema->Fields.end() ? &It->Spec : nullptr;
    };

    const FCompiledUiFieldSpecPtr* const DaySpec = FindField("day");
    const FCompiledUiFieldSpecPtr* const ValueSpec = FindField("value");
    TestNotNull(TEXT("DUC-06: author writes day directly"), DaySpec);
    TestNotNull(TEXT("DUC-06: author writes value directly"), ValueSpec);
    TestTrue(TEXT("DUC-06: no leaked child field day.text exists"), FindField("day.text") == nullptr);
    TestTrue(TEXT("DUC-06: no leaked child field value.percent exists"), FindField("value.percent") == nullptr);
    if (DaySpec == nullptr || ValueSpec == nullptr || *DaySpec == nullptr || *ValueSpec == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("DUC-06: day has TextSpec shape"), (*DaySpec)->Kind, EUiFieldKind::Text);
    TestEqual(TEXT("DUC-06: value has scalar shape"), (*ValueSpec)->Kind, EUiFieldKind::Scalar);
    TestTrue(
        TEXT("DUC-06: value has number shape"),
        (*ValueSpec)->Scalar.has_value() && (*ValueSpec)->Scalar->Kind == EScalarFieldKind::Number);

    // Source 2 is the Designer-authored declaration on the real WBP. The two
    // sources meet only at Schema \u2286 Capabilities; DUC-07 will separately compare
    // this declaration with the children themselves.
    UClass* const FixtureClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/TextSystem/UI/Widgets/WBP_DeclaredCompositeFlatFixture.WBP_DeclaredCompositeFlatFixture_C"));
    TestNotNull(TEXT("DUC-06: declared composite fixture class loads"), FixtureClass);
    UUserWidget* const Fixture = FixtureClass != nullptr ? FixtureClass->GetDefaultObject<UUserWidget>() : nullptr;
    IGV2UiPropertyHost* const PropertyHost = Fixture != nullptr ? Cast<IGV2UiPropertyHost>(Fixture) : nullptr;
    TestNotNull(TEXT("DUC-06: fixture exposes its Designer declaration"), PropertyHost);
    if (PropertyHost == nullptr)
    {
        return false;
    }

    FGV2UiCapabilityBuilder Builder;
    PropertyHost->DescribeUiCapabilities(Builder);
    const FGV2UiCapabilityTree Capabilities = Builder.Build();
    TestEqual(TEXT("DUC-06: declaration produces exactly two top-level capabilities"), Capabilities.Num(), 2);

    const FGV2UiPropertyCapability* const DayCapability = Capabilities.FindProperty(TEXT("day"));
    const FGV2UiPropertyCapability* const ValueCapability = Capabilities.FindProperty(TEXT("value"));
    TestNotNull(TEXT("DUC-06: declaration produces day directly"), DayCapability);
    TestNotNull(TEXT("DUC-06: declaration produces value directly"), ValueCapability);
    if (DayCapability == nullptr || ValueCapability == nullptr)
    {
        return false;
    }
    TestEqual(TEXT("DUC-06: day declaration is Text"), DayCapability->SupportedKind, EGV2PreparedUiValueKind::Text);
    TestEqual(TEXT("DUC-06: value declaration is Number"), ValueCapability->SupportedKind, EGV2PreparedUiValueKind::Number);

    TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
    const bool bCompatible = CheckUiSchemaCapabilityCompatibility(
        *Schema,
        Capabilities,
        TEXT("textsystem:schema.ui_field.declared_composite_fixture.v1"),
        TEXT(""),
        Diagnostics);
    TestTrue(TEXT("DUC-06: authored flat schema is compatible with declared properties"), bCompatible);
    TestEqual(TEXT("DUC-06: flat schema compatibility produces no diagnostics"), Diagnostics.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DeclaredCompositeChildKindCompatibilityTest,
    "GV2.UI.DeclaredComposite.ChildKindCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeChildKindCompatibilityTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    UClass* const CompositeClass = FindObject<UClass>(nullptr, TEXT("/Script/GV2.GV2DeclaredCompositeWidgetBase"));
    TestNotNull(TEXT("DUC-07: generic declared composite class exists"), CompositeClass);
    if (CompositeClass == nullptr)
    {
        return false;
    }

    UUserWidget* const Composite = NewObject<UUserWidget>(GetTransientPackage(), CompositeClass);
    TestNotNull(TEXT("DUC-07: composite instance can be created"), Composite);
    IGV2UiPropertyHost* const PropertyHost = Composite != nullptr ? Cast<IGV2UiPropertyHost>(Composite) : nullptr;
    TestNotNull(TEXT("DUC-07: composite instance exposes property host interface"), PropertyHost);
    if (PropertyHost == nullptr)
    {
        return false;
    }

    // DayText's own DescribeUiCapabilities is authored on UGV2TextWidgetBase, entirely
    // independently of whatever this composite instance below declares for it -- neither
    // side is derived from the other. It only ever declares a Text-kind capability.
    Composite->WidgetTree = NewObject<UWidgetTree>(Composite);
    UVerticalBox* const Root = Composite->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
    Composite->WidgetTree->RootWidget = Root;
    UGV2TextWidgetBase* const DayText = Composite->WidgetTree->ConstructWidget<UGV2TextWidgetBase>(UGV2TextWidgetBase::StaticClass(), TEXT("DayText"));
    TestNotNull(TEXT("DUC-07: Text-only child constructed"), DayText);
    if (DayText == nullptr)
    {
        return false;
    }
    Root->AddChildToVerticalBox(DayText);

    FArrayProperty* const DeclaredCapabilitiesProperty =
        FindFProperty<FArrayProperty>(CompositeClass, TEXT("DeclaredCapabilities"));
    TestNotNull(TEXT("DUC-07: declared capability list exists"), DeclaredCapabilitiesProperty);
    if (DeclaredCapabilitiesProperty == nullptr)
    {
        return false;
    }
    FStructProperty* const EntryProperty = CastField<FStructProperty>(DeclaredCapabilitiesProperty->Inner);
    TestNotNull(TEXT("DUC-07: declared capability list stores triples"), EntryProperty);
    if (EntryProperty == nullptr)
    {
        return false;
    }

    // Negative case: this composite instance declares `day` as Number against DayText --
    // impossible, since DayText's own, independent capability tree only ever declares Text.
    FScriptArrayHelper Entries(DeclaredCapabilitiesProperty, DeclaredCapabilitiesProperty->ContainerPtrToValuePtr<void>(Composite));
    Entries.EmptyValues();
    const FDeclaredCapabilityExpectation BadDeclaration = {
        TEXT("Number"), TEXT("day"), TEXT("DayText"), EGV2PreparedUiValueKind::Number, EGV2UiCapabilityTargetType::RendererControl, TEXT("") };
    if (!SetDeclaredCapabilityEntry(*this, Entries, *EntryProperty, BadDeclaration))
    {
        return false;
    }

    auto MakeNumberFieldSpec = []() -> std::shared_ptr<FCompiledUiFieldSpec>
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::Number;
        return Spec;
    };

    FCompiledUiFieldSpec NumberSchema;
    NumberSchema.Kind = EUiFieldKind::Object;
    NumberSchema.Fields.push_back({ "day", true, MakeNumberFieldSpec() });

    FGV2UiCapabilityBuilder BadBuilder;
    PropertyHost->DescribeUiCapabilities(BadBuilder);

    TMap<FString, FGV2PreparedUiValue> NumberFields;
    NumberFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeNumber(5.0));
    const TSharedRef<const FGV2PreparedUiObject> NumberCandidate = FGV2PreparedUiObject::Create(MoveTemp(NumberFields));

    FGV2UiHostMutationPlan BadPlan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> BadDiagnostics;
    const bool bBadPrepared = PrepareUiHostProperties(
        Composite,
        BadBuilder.Build(),
        *NumberCandidate,
        NumberSchema,
        TEXT("core:schema.ui_field.declared_composite_kind_probe.v1"),
        TEXT(""),
        PropertyHost->GetPropertyHostState().GetLastCommittedProperties(),
        BadPlan,
        BadDiagnostics);
    TestFalse(TEXT("DUC-07: declaring Number against a Text-only child is rejected"), bBadPrepared);
    TestTrue(
        TEXT("DUC-07: rejection reports the typed target_kind_mismatch diagnostic"),
        BadDiagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
        {
            return Diagnostic.Code == TEXT("core:diagnostic.ui_consumer.target_kind_mismatch");
        }));

    // Positive case, same two independent sources: correcting only the declared Kind to
    // Text (matching what DayText independently declares for itself) is now accepted --
    // proving the rejection above tracked the child's own capability, not something the
    // composite's declaration alone could ever fail against.
    Entries.EmptyValues();
    const FDeclaredCapabilityExpectation GoodDeclaration = {
        TEXT("Text"), TEXT("day"), TEXT("DayText"), EGV2PreparedUiValueKind::Text, EGV2UiCapabilityTargetType::RendererControl, TEXT("") };
    if (!SetDeclaredCapabilityEntry(*this, Entries, *EntryProperty, GoodDeclaration))
    {
        return false;
    }

    FCompiledUiFieldSpec TextSchema;
    TextSchema.Kind = EUiFieldKind::Object;
    TextSchema.Fields.push_back({ "day", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text) });

    FGV2UiCapabilityBuilder GoodBuilder;
    PropertyHost->DescribeUiCapabilities(GoodBuilder);

    FGV2TextViewModel DayTextValue;
    DayTextValue.Text = FText::FromString(TEXT("Monday"));
    TMap<FString, FGV2PreparedUiValue> TextFields;
    TextFields.Add(TEXT("day"), FGV2PreparedUiValue::MakeText(DayTextValue));
    const TSharedRef<const FGV2PreparedUiObject> TextCandidate = FGV2PreparedUiObject::Create(MoveTemp(TextFields));

    FGV2UiHostMutationPlan GoodPlan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> GoodDiagnostics;
    const bool bGoodPrepared = PrepareUiHostProperties(
        Composite,
        GoodBuilder.Build(),
        *TextCandidate,
        TextSchema,
        TEXT("core:schema.ui_field.declared_composite_kind_probe.v1"),
        TEXT(""),
        PropertyHost->GetPropertyHostState().GetLastCommittedProperties(),
        GoodPlan,
        GoodDiagnostics);
    TestTrue(TEXT("DUC-07: declaring Text against the same Text-only child is accepted"), bGoodPrepared);
    TestEqual(TEXT("DUC-07: accepted declaration produces no diagnostics"), GoodDiagnostics.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DeclaredCompositeKindSelectabilityGateTest,
    "GV2.UI.DeclaredComposite.KindSelectabilityGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeKindSelectabilityGateTest::RunTest(const FString& Parameters)
{
    // GBH-02A: every EGV2DeclaredUiCapabilityKind value must be classified -- Supported
    // (selectable, proven end-to-end) or Hidden (UMETA(Hidden), reason recorded). A brand
    // new enum value with neither fails this gate instead of silently defaulting to
    // selectable-but-unproven.
    TArray<FString> GateDiagnostics;
    const bool bAllClassified = FGV2DesignerCapabilityKindGate::ValidateAllKindsClassified(GateDiagnostics);
    TestTrue(
        *FString::Printf(TEXT("GBH-02A: every Designer kind is classified [Diagnostics: %s]"), *FString::Join(GateDiagnostics, TEXT("; "))),
        bAllClassified);
    TestEqual(TEXT("GBH-02A: zero unclassified-kind diagnostics"), GateDiagnostics.Num(), 0);

    // Currently-Hidden kinds and their recorded reasons.
    TestEqual(
        TEXT("GBH-02A: CollectionHost status is Hidden"),
        FGV2DesignerCapabilityKindGate::GetKindStatus(EGV2DeclaredUiCapabilityKind::CollectionHost),
        EGV2DesignerKindStatus::Hidden);
    TestEqual(
        TEXT("GBH-02A: RichTextSpans status is Hidden"),
        FGV2DesignerCapabilityKindGate::GetKindStatus(EGV2DeclaredUiCapabilityKind::RichTextSpans),
        EGV2DesignerKindStatus::Hidden);

    FString CollectionHostReason;
    TestTrue(
        TEXT("GBH-02A: CollectionHost is registered Hidden"),
        FGV2DesignerCapabilityKindGate::IsHiddenKind(EGV2DeclaredUiCapabilityKind::CollectionHost, &CollectionHostReason));
    TestFalse(TEXT("GBH-02A: CollectionHost has a recorded reason"), CollectionHostReason.IsEmpty());

    FString RichTextSpansReason;
    TestTrue(
        TEXT("GBH-02A: RichTextSpans is registered Hidden"),
        FGV2DesignerCapabilityKindGate::IsHiddenKind(EGV2DeclaredUiCapabilityKind::RichTextSpans, &RichTextSpansReason));
    TestFalse(TEXT("GBH-02A: RichTextSpans has a recorded reason"), RichTextSpansReason.IsEmpty());

    const TArray<FGV2HiddenDesignerKindInfo> HiddenKinds = FGV2DesignerCapabilityKindGate::GetHiddenKinds();
    TestEqual(TEXT("GBH-02A: exactly 2 Hidden kinds (CollectionHost, RichTextSpans)"), HiddenKinds.Num(), 2);

    // Currently-selectable kinds remain Supported: the 8 flat RendererControl kinds proven
    // by DUC-05/06/07/08 (this file's own earlier tests, plus the real TopBar conversion),
    // and NestedScreen proven by DUC-09/10/11.
    const EGV2DeclaredUiCapabilityKind SupportedKinds[] = {
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
    for (const EGV2DeclaredUiCapabilityKind Kind : SupportedKinds)
    {
        TestEqual(
            *FString::Printf(TEXT("GBH-02A: kind %d is Supported"), static_cast<int32>(Kind)),
            FGV2DesignerCapabilityKindGate::GetKindStatus(Kind),
            EGV2DesignerKindStatus::Supported);
    }

    // REM-05, reproduced directly: the reason CollectionHost is Hidden, not merely a
    // documentation claim. DescribeUiCapabilities' CollectionHost case calls AddCustom(...),
    // which has no EntryWidgetClass parameter at all -- so on a *genuinely empty* collection
    // (no existing entries to infer a class from), Prepare cannot create the first item
    // through this delegation path, regardless of what the schema itself declares.
    {
        UClass* const CompositeClass = FindObject<UClass>(nullptr, TEXT("/Script/GV2.GV2DeclaredCompositeWidgetBase"));
        TestNotNull(TEXT("GBH-02A: generic declared composite class exists"), CompositeClass);
        if (CompositeClass == nullptr)
        {
            return false;
        }

        UUserWidget* const Composite = NewObject<UUserWidget>(GetTransientPackage(), CompositeClass);
        IGV2UiPropertyHost* const PropertyHost = Composite != nullptr ? Cast<IGV2UiPropertyHost>(Composite) : nullptr;
        TestNotNull(TEXT("GBH-02A: composite instance exposes property host interface"), PropertyHost);
        if (PropertyHost == nullptr)
        {
            return false;
        }

        Composite->WidgetTree = NewObject<UWidgetTree>(Composite);
        UGV2ListViewWidgetBase* const ItemsList = Composite->WidgetTree->ConstructWidget<UGV2ListViewWidgetBase>(
            UGV2ListViewWidgetBase::StaticClass(), TEXT("ItemsList"));
        Composite->WidgetTree->RootWidget = ItemsList;
        UVerticalBox* const ContainerPanel = NewObject<UVerticalBox>(ItemsList);
        ItemsList->SetContainerPanel(ContainerPanel);
        TestEqual(TEXT("GBH-02A: fixture collection starts genuinely empty"), ContainerPanel->GetChildrenCount(), 0);

        FArrayProperty* const DeclaredCapabilitiesProperty =
            FindFProperty<FArrayProperty>(CompositeClass, TEXT("DeclaredCapabilities"));
        FStructProperty* const EntryProperty =
            DeclaredCapabilitiesProperty != nullptr ? CastField<FStructProperty>(DeclaredCapabilitiesProperty->Inner) : nullptr;
        TestNotNull(TEXT("GBH-02A: declared capability list stores triples"), EntryProperty);
        if (DeclaredCapabilitiesProperty == nullptr || EntryProperty == nullptr)
        {
            return false;
        }

        FScriptArrayHelper Entries(DeclaredCapabilitiesProperty, DeclaredCapabilitiesProperty->ContainerPtrToValuePtr<void>(Composite));
        Entries.EmptyValues();
        const FDeclaredCapabilityExpectation CollectionDeclaration = {
            TEXT("CollectionHost"), TEXT("items"), TEXT("ItemsList"), EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CollectionHost, TEXT("") };
        if (!SetDeclaredCapabilityEntry(*this, Entries, *EntryProperty, CollectionDeclaration))
        {
            return false;
        }

        FGV2UiCapabilityBuilder Builder;
        PropertyHost->DescribeUiCapabilities(Builder);

        auto ItemSpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Object);
        ItemSpec->Fields.push_back({ "key", true, std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Key) });
        auto ItemsArraySpec = std::make_shared<GV2ContentCore::FCompiledUiFieldSpec>(GV2ContentCore::EUiFieldKind::Array);
        ItemsArraySpec->KeyedBy = std::string("key");
        ItemsArraySpec->Items = ItemSpec;

        GV2ContentCore::FCompiledUiFieldSpec Schema;
        Schema.Kind = GV2ContentCore::EUiFieldKind::Object;
        Schema.Fields.push_back({ "items", false, ItemsArraySpec });

        TMap<FString, FGV2PreparedUiValue> FirstItemMap;
        FirstItemMap.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(TEXT("first")));
        TArray<FGV2PreparedUiValue> ItemsArray;
        ItemsArray.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(FirstItemMap)));
        TMap<FString, FGV2PreparedUiValue> HostFields;
        HostFields.Add(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(ItemsArray)));
        const TSharedRef<const FGV2PreparedUiObject> Candidate = FGV2PreparedUiObject::Create(HostFields);

        FGV2UiHostMutationPlan Plan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
        const bool bPrepared = PrepareUiHostProperties(
            Composite,
            Builder.Build(),
            *Candidate,
            Schema,
            TEXT("test:schema.gbh02_collection_host_probe.v1"),
            TEXT(""),
            PropertyHost->GetPropertyHostState().GetLastCommittedProperties(),
            Plan,
            Diagnostics);
        TestFalse(TEXT("GBH-02A: CollectionHost cannot create the first item of an empty collection through DeclaredComposite"), bPrepared);
        TestTrue(
            TEXT("GBH-02A: rejection is specifically missing_entry_class, justifying the Hidden classification"),
            Diagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
            {
                return Diagnostic.Message.Contains(TEXT("missing_entry_class"));
            }));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
