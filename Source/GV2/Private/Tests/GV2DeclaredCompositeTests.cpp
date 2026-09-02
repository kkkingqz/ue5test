#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TabContainerWidgetBase.h"
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

    // GBH-06: matches DeclaredComposite's own default Number range ([0..1], chosen to
    // retroactively match the canonical ValueBar/ProgressBar fixture without a content
    // migration) so this schema exercises only the kind mismatch under test, not an
    // unrelated range mismatch against that default.
    auto MakeNumberFieldSpec = []() -> std::shared_ptr<FCompiledUiFieldSpec>
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::Number;
        Spec->Scalar->MinimumNumber = 0.0;
        Spec->Scalar->MaximumNumber = 1.0;
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
    FGV2DeclaredCompositeConstraintsAndSelectorTest,
    "GV2.UI.DeclaredComposite.ConstraintsAndSelector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeConstraintsAndSelectorTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    UClass* const CompositeClass = FindObject<UClass>(nullptr, TEXT("/Script/GV2.GV2DeclaredCompositeWidgetBase"));
    TestNotNull(TEXT("GBH-06: generic declared composite class exists"), CompositeClass);
    if (CompositeClass == nullptr)
    {
        return false;
    }

    // 1. REM-01's own confirmed example, closed: a declaration targeting a real
    // UGV2ProgressBarWidgetBase (which natively declares "percent" in [0..1]) now
    // carries that same range on the DECLARATION side (GBH-06's default matches it
    // without requiring a content migration). The existing, unmodified
    // CheckUiSchemaCapabilityCompatibility (schema range vs capability range) then
    // rejects a schema wider than the range and accepts one that fits inside it --
    // exactly the milestone check this task exists to prove.
    {
        UGV2DeclaredCompositeWidgetBase* const Composite = NewObject<UGV2DeclaredCompositeWidgetBase>(GetTransientPackage(), CompositeClass);
        TestNotNull(TEXT("GBH-06: composite instance created"), Composite);
        if (Composite == nullptr)
        {
            return false;
        }
        Composite->WidgetTree = NewObject<UWidgetTree>(Composite);
        UGV2ProgressBarWidgetBase* const Bar = Composite->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
            UGV2ProgressBarWidgetBase::StaticClass(), TEXT("Bar"));
        Composite->WidgetTree->RootWidget = Bar;

        FGV2DeclaredUiCapability Entry;
        Entry.PropertyName = FName(TEXT("value"));
        Entry.ChildWidgetName = FName(TEXT("Bar"));
        Entry.Kind = EGV2DeclaredUiCapabilityKind::Number;
        // NumberMin/NumberMax left at their [0..1] defaults -- the point being proven
        // is that the default itself now matches ProgressBar's real range, not that
        // this test had to set it explicitly.
        Composite->DeclaredCapabilities.Add(Entry);

        FGV2UiCapabilityBuilder Builder;
        Composite->DescribeUiCapabilities(Builder);
        const FGV2UiCapabilityTree Capabilities = Builder.Build();
        const FGV2UiPropertyCapability* const ValueCap = Capabilities.FindProperty(TEXT("value"));
        TestNotNull(TEXT("GBH-06: 'value' capability produced"), ValueCap);
        if (ValueCap != nullptr)
        {
            TestTrue(TEXT("GBH-06: declaration's NumberMin is set"), ValueCap->NumberMin.IsSet());
            TestEqual(TEXT("GBH-06: declaration's NumberMin matches ProgressBar's own [0..1]"), ValueCap->NumberMin.GetValue(), 0.0);
            TestTrue(TEXT("GBH-06: declaration's NumberMax is set"), ValueCap->NumberMax.IsSet());
            TestEqual(TEXT("GBH-06: declaration's NumberMax matches ProgressBar's own [0..1]"), ValueCap->NumberMax.GetValue(), 1.0);
        }

        auto MakeNumberField = [](TOptional<double> Min, TOptional<double> Max) -> FCompiledUiFieldSpec
        {
            FCompiledUiFieldSpec Spec;
            Spec.Kind = EUiFieldKind::Scalar;
            Spec.Scalar = FScalarFieldSpec{};
            Spec.Scalar->Kind = EScalarFieldKind::Number;
            if (Min.IsSet()) Spec.Scalar->MinimumNumber = Min.GetValue();
            if (Max.IsSet()) Spec.Scalar->MaximumNumber = Max.GetValue();
            return Spec;
        };

        // 1a. REM-01's exact confirmed shape: schema [0..100] against capability
        // [0..1] -- previously accepted (capability was unbounded), now rejected.
        FCompiledUiFieldSpec WideSchema;
        WideSchema.Kind = EUiFieldKind::Object;
        WideSchema.Fields.push_back({ "value", true, std::make_shared<FCompiledUiFieldSpec>(MakeNumberField(0.0, 100.0)) });
        TArray<FGV2UiSchemaCompatibilityDiagnostic> WideDiagnostics;
        const bool bWideCompatible = CheckUiSchemaCapabilityCompatibility(
            WideSchema, Capabilities, TEXT("test:schema.gbh06_wide_probe.v1"), TEXT(""), WideDiagnostics);
        TestFalse(TEXT("GBH-06: schema [0..100] against declared [0..1] is rejected"), bWideCompatible);
        TestTrue(
            TEXT("GBH-06: rejection is the typed range_unsupported diagnostic"),
            WideDiagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
            {
                return Diagnostic.Code == TEXT("core:diagnostic.ui_capability.range_unsupported");
            }));

        // 1b. A schema narrower than the declared range fits and is accepted.
        FCompiledUiFieldSpec NarrowSchema;
        NarrowSchema.Kind = EUiFieldKind::Object;
        NarrowSchema.Fields.push_back({ "value", true, std::make_shared<FCompiledUiFieldSpec>(MakeNumberField(0.0, 0.5)) });
        TArray<FGV2UiSchemaCompatibilityDiagnostic> NarrowDiagnostics;
        const bool bNarrowCompatible = CheckUiSchemaCapabilityCompatibility(
            NarrowSchema, Capabilities, TEXT("test:schema.gbh06_narrow_probe.v1"), TEXT(""), NarrowDiagnostics);
        TestTrue(TEXT("GBH-06: schema [0..0.5] against declared [0..1] is accepted"), bNarrowCompatible);
        TestEqual(TEXT("GBH-06: accepted narrow schema produces no diagnostics"), NarrowDiagnostics.Num(), 0);

        // 1c. GBH-08: a second DeclaredComposite instance on the SAME class, this time
        // declaring [0..100] against the same real ProgressBar[0..1] child. Before
        // GBH-08, declaration<->child compatibility (the DUC-07 check inside
        // PrepareUiHostProperties) only compared SupportedKind -- a declaration this
        // wide would have been silently accepted as long as both sides said "Number".
        // The schema here is built to MATCH the (wide) declaration, so the schema<->
        // declaration check passes cleanly; only the declaration<->child subset check
        // (IsUiCapabilitySubset, now reused by both paths) can catch this.
        UGV2DeclaredCompositeWidgetBase* const WideComposite = NewObject<UGV2DeclaredCompositeWidgetBase>(GetTransientPackage(), CompositeClass);
        TestNotNull(TEXT("GBH-08: second composite instance created"), WideComposite);
        if (WideComposite != nullptr)
        {
            WideComposite->WidgetTree = NewObject<UWidgetTree>(WideComposite);
            UGV2ProgressBarWidgetBase* const WideBar = WideComposite->WidgetTree->ConstructWidget<UGV2ProgressBarWidgetBase>(
                UGV2ProgressBarWidgetBase::StaticClass(), TEXT("Bar"));
            WideComposite->WidgetTree->RootWidget = WideBar;

            FGV2DeclaredUiCapability WideEntry;
            WideEntry.PropertyName = FName(TEXT("value"));
            WideEntry.ChildWidgetName = FName(TEXT("Bar"));
            WideEntry.Kind = EGV2DeclaredUiCapabilityKind::Number;
            WideEntry.NumberMin = 0.0;
            WideEntry.NumberMax = 100.0;
            WideComposite->DeclaredCapabilities.Add(WideEntry);

            FGV2UiCapabilityBuilder WideBuilder;
            WideComposite->DescribeUiCapabilities(WideBuilder);

            FCompiledUiFieldSpec MatchingWideSchema;
            MatchingWideSchema.Kind = EUiFieldKind::Object;
            MatchingWideSchema.Fields.push_back({ "value", true, std::make_shared<FCompiledUiFieldSpec>(MakeNumberField(0.0, 100.0)) });

            TMap<FString, FGV2PreparedUiValue> WideFields;
            WideFields.Add(TEXT("value"), FGV2PreparedUiValue::MakeNumber(50.0));
            const TSharedRef<const FGV2PreparedUiObject> WideCandidate = FGV2PreparedUiObject::Create(WideFields);

            FGV2UiHostMutationPlan WidePlan;
            TArray<FGV2UiSchemaCompatibilityDiagnostic> WidePrepareDiagnostics;
            const bool bWidePrepared = PrepareUiHostProperties(
                WideComposite, WideBuilder.Build(), *WideCandidate, MatchingWideSchema,
                TEXT("test:schema.gbh08_declaration_vs_child_probe.v1"), TEXT(""),
                WideComposite->GetPropertyHostState().GetLastCommittedProperties(),
                WidePlan, WidePrepareDiagnostics);
            TestFalse(
                TEXT("GBH-08: declaration [0..100] against real child ProgressBar[0..1] is rejected, not just kind-checked"),
                bWidePrepared);
            TestTrue(
                *FString::Printf(TEXT("GBH-08: rejection is the typed range_unsupported diagnostic [Diagnostics: %s]"),
                    WidePrepareDiagnostics.Num() > 0 ? *WidePrepareDiagnostics[0].ToString() : TEXT("")),
                WidePrepareDiagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
                {
                    return Diagnostic.Code == TEXT("core:diagnostic.ui_consumer.range_unsupported");
                }));
        }
    }

    // 2. Ambiguous child capability: UGV2TabContainerWidgetBase natively declares two
    // Key-kind capabilities of its own ("default_tab_key" and "key", both real,
    // both independently consumed -- see GV2PropertyConsumers.cpp). A declaration
    // targeting it with Kind=Key, a property name matching neither of those two, and
    // no explicit ChildCapabilityName cannot be resolved unambiguously and must be
    // rejected rather than silently picking whichever one iteration finds first.
    {
        UGV2DeclaredCompositeWidgetBase* const Composite = NewObject<UGV2DeclaredCompositeWidgetBase>(GetTransientPackage(), CompositeClass);
        Composite->WidgetTree = NewObject<UWidgetTree>(Composite);
        UGV2TabContainerWidgetBase* const Tabs = Composite->WidgetTree->ConstructWidget<UGV2TabContainerWidgetBase>(
            UGV2TabContainerWidgetBase::StaticClass(), TEXT("Tabs"));
        Composite->WidgetTree->RootWidget = Tabs;

        FGV2DeclaredUiCapability AmbiguousEntry;
        AmbiguousEntry.PropertyName = FName(TEXT("some_key"));
        AmbiguousEntry.ChildWidgetName = FName(TEXT("Tabs"));
        AmbiguousEntry.Kind = EGV2DeclaredUiCapabilityKind::Key;
        Composite->DeclaredCapabilities.Add(AmbiguousEntry);

        FGV2UiCapabilityBuilder Builder;
        Composite->DescribeUiCapabilities(Builder);

        FCompiledUiFieldSpec KeySchema;
        KeySchema.Kind = EUiFieldKind::Object;
        KeySchema.Fields.push_back({ "some_key", true, std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key) });

        TMap<FString, FGV2PreparedUiValue> KeyFields;
        KeyFields.Add(TEXT("some_key"), FGV2PreparedUiValue::MakeKey(TEXT("anything")));
        const TSharedRef<const FGV2PreparedUiObject> KeyCandidate = FGV2PreparedUiObject::Create(KeyFields);

        FGV2UiHostMutationPlan AmbiguousPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> AmbiguousDiagnostics;
        const bool bAmbiguousPrepared = PrepareUiHostProperties(
            Composite, Builder.Build(), *KeyCandidate, KeySchema,
            TEXT("test:schema.gbh06_ambiguous_probe.v1"), TEXT(""),
            Composite->GetPropertyHostState().GetLastCommittedProperties(),
            AmbiguousPlan, AmbiguousDiagnostics);
        TestFalse(TEXT("GBH-06: ambiguous child capability (no selector, no name match) is rejected"), bAmbiguousPrepared);
        TestTrue(
            TEXT("GBH-06: rejection is the typed ambiguous_child_capability diagnostic"),
            AmbiguousDiagnostics.ContainsByPredicate([](const FGV2UiSchemaCompatibilityDiagnostic& Diagnostic)
            {
                return Diagnostic.Code == TEXT("core:diagnostic.ui_consumer.ambiguous_child_capability");
            }));

        // 3. The same ambiguity, resolved by an explicit ChildCapabilityName: setting
        // it to "key" picks that one capability unambiguously and Prepare succeeds.
        Composite->DeclaredCapabilities[0].ChildCapabilityName = FName(TEXT("key"));
        FGV2UiCapabilityBuilder ResolvedBuilder;
        Composite->DescribeUiCapabilities(ResolvedBuilder);

        FGV2UiHostMutationPlan ResolvedPlan;
        TArray<FGV2UiSchemaCompatibilityDiagnostic> ResolvedDiagnostics;
        const bool bResolvedPrepared = PrepareUiHostProperties(
            Composite, ResolvedBuilder.Build(), *KeyCandidate, KeySchema,
            TEXT("test:schema.gbh06_ambiguous_probe.v1"), TEXT(""),
            Composite->GetPropertyHostState().GetLastCommittedProperties(),
            ResolvedPlan, ResolvedDiagnostics);
        TestTrue(
            *FString::Printf(TEXT("GBH-06: explicit ChildCapabilityName resolves the same ambiguity [Diagnostics: %s]"),
                ResolvedDiagnostics.Num() > 0 ? *ResolvedDiagnostics[0].ToString() : TEXT("")),
            bResolvedPrepared);
    }

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

    // Currently-Hidden kinds and their recorded reasons. GBH-02B closed REM-05 --
    // CollectionHost moved from Hidden to Supported (see
    // GV2.UI.DeclaredComposite.CollectionHostFirstEntry for its own E2E proof); only
    // RichTextSpans remains Hidden.
    TestEqual(
        TEXT("GBH-02B: CollectionHost status is Supported"),
        FGV2DesignerCapabilityKindGate::GetKindStatus(EGV2DeclaredUiCapabilityKind::CollectionHost),
        EGV2DesignerKindStatus::Supported);
    TestEqual(
        TEXT("GBH-02A: RichTextSpans status is Hidden"),
        FGV2DesignerCapabilityKindGate::GetKindStatus(EGV2DeclaredUiCapabilityKind::RichTextSpans),
        EGV2DesignerKindStatus::Hidden);

    FString RichTextSpansReason;
    TestTrue(
        TEXT("GBH-02A: RichTextSpans is registered Hidden"),
        FGV2DesignerCapabilityKindGate::IsHiddenKind(EGV2DeclaredUiCapabilityKind::RichTextSpans, &RichTextSpansReason));
    TestFalse(TEXT("GBH-02A: RichTextSpans has a recorded reason"), RichTextSpansReason.IsEmpty());

    const TArray<FGV2HiddenDesignerKindInfo> HiddenKinds = FGV2DesignerCapabilityKindGate::GetHiddenKinds();
    TestEqual(TEXT("GBH-02B: exactly 1 Hidden kind (RichTextSpans)"), HiddenKinds.Num(), 1);

    // Currently-selectable kinds remain Supported: the 8 flat RendererControl kinds proven
    // by DUC-05/06/07/08 (this file's own earlier tests, plus the real TopBar conversion),
    // NestedScreen proven by DUC-09/10/11, and (GBH-02B) CollectionHost proven by
    // GV2.UI.DeclaredComposite.CollectionHostFirstEntry.
    const EGV2DeclaredUiCapabilityKind SupportedKinds[] = {
        EGV2DeclaredUiCapabilityKind::Boolean,
        EGV2DeclaredUiCapabilityKind::Integer,
        EGV2DeclaredUiCapabilityKind::Number,
        EGV2DeclaredUiCapabilityKind::String,
        EGV2DeclaredUiCapabilityKind::Key,
        EGV2DeclaredUiCapabilityKind::Text,
        EGV2DeclaredUiCapabilityKind::ResourceRef,
        EGV2DeclaredUiCapabilityKind::Binding,
        EGV2DeclaredUiCapabilityKind::CollectionHost,
        EGV2DeclaredUiCapabilityKind::NestedScreen,
    };
    for (const EGV2DeclaredUiCapabilityKind Kind : SupportedKinds)
    {
        TestEqual(
            *FString::Printf(TEXT("GBH-02A/B: kind %d is Supported"), static_cast<int32>(Kind)),
            FGV2DesignerCapabilityKindGate::GetKindStatus(Kind),
            EGV2DesignerKindStatus::Supported);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DeclaredCompositeCollectionHostFirstEntryTest,
    "GV2.UI.DeclaredComposite.CollectionHostFirstEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DeclaredCompositeCollectionHostFirstEntryTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    // GBH-02B: REM-05 closed. DescribeUiCapabilities' CollectionHost case now wires
    // EntryWidgetClass/KeyPropertyName into AddKeyedCollection, and the item's own
    // capability tree comes from EntryWidgetClass's own CDO -- the same
    // independent-second-source pattern DUC-07/GBH-06 established for ChildWidgetName,
    // and the exact precedent UGV2ButtonListWidgetBase already uses. This proves the
    // full path a Designer author actually gets: declaration -> schema/materialization
    // -> creation of the FIRST entry of a genuinely empty collection -> child capability
    // subset check -> Commit -> observable renderer state.
    UClass* const CompositeClass = FindObject<UClass>(nullptr, TEXT("/Script/GV2.GV2DeclaredCompositeWidgetBase"));
    TestNotNull(TEXT("GBH-02B: generic declared composite class exists"), CompositeClass);
    if (CompositeClass == nullptr)
    {
        return false;
    }

    // WBP_Button, not the bare native class: UGV2ButtonWidgetBase's own "text" capability
    // targets "LabelText", which only resolves on a real WidgetTree -- the same
    // requirement FGV2ScreenPreflightPredictsDeepChildFailureTest's fixture already
    // documents for this exact class.
    UClass* ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
    TestNotNull(TEXT("GBH-02B: real WBP_Button class loads"), ButtonClass);
    if (ButtonClass == nullptr)
    {
        return false;
    }

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* TestWorld = GameInstance->GetWorld();

    UGV2DeclaredCompositeWidgetBase* const Composite = CreateWidget<UGV2DeclaredCompositeWidgetBase>(TestWorld, CompositeClass);
    TestNotNull(TEXT("GBH-02B: composite instance created"), Composite);
    if (Composite == nullptr)
    {
        GameInstance->RemoveFromRoot();
        return false;
    }

    Composite->WidgetTree = NewObject<UWidgetTree>(Composite);
    UGV2ListViewWidgetBase* const ItemsList = Composite->WidgetTree->ConstructWidget<UGV2ListViewWidgetBase>(
        UGV2ListViewWidgetBase::StaticClass(), TEXT("ItemsList"));
    Composite->WidgetTree->RootWidget = ItemsList;
    UVerticalBox* const ContainerPanel = NewObject<UVerticalBox>(ItemsList);
    ItemsList->SetContainerPanel(ContainerPanel);
    TestEqual(TEXT("GBH-02B: fixture collection starts genuinely empty"), ContainerPanel->GetChildrenCount(), 0);

    FGV2DeclaredUiCapability CollectionEntry;
    CollectionEntry.PropertyName = FName(TEXT("items"));
    CollectionEntry.ChildWidgetName = FName(TEXT("ItemsList"));
    CollectionEntry.Kind = EGV2DeclaredUiCapabilityKind::CollectionHost;
    CollectionEntry.EntryWidgetClass = ButtonClass;
    CollectionEntry.KeyPropertyName = TEXT("key");
    Composite->DeclaredCapabilities.Add(CollectionEntry);

    FGV2UiCapabilityBuilder Builder;
    Composite->DescribeUiCapabilities(Builder);
    const FGV2UiCapabilityTree Capabilities = Builder.Build();
    const FGV2UiPropertyCapability* const ItemsCap = Capabilities.FindProperty(TEXT("items"));
    TestNotNull(TEXT("GBH-02B: 'items' capability produced"), ItemsCap);
    if (ItemsCap != nullptr)
    {
        TestEqual(TEXT("GBH-02B: 'items' targets CollectionHost"), ItemsCap->TargetType, EGV2UiCapabilityTargetType::CollectionHost);
        TestEqual(TEXT("GBH-02B: 'items' carries the declared EntryWidgetClass"), ItemsCap->EntryWidgetClass.Get(), ButtonClass);
        TestTrue(TEXT("GBH-02B: 'items' requires keyed identity"), ItemsCap->bRequiresKeyedIdentity);
        TestNotNull(TEXT("GBH-02B: 'items' has an item capability descriptor"), ItemsCap->ItemCapability.Get());
        if (ItemsCap->ItemCapability.IsValid() && ItemsCap->ItemCapability->ChildTree.IsValid())
        {
            // Read from ButtonClass's own CDO, not hand-declared: proves the item
            // contract is the same independent-second-source delegation as
            // ChildWidgetName's own capability, not a manual re-statement of it.
            const FGV2UiCapabilityTree& ItemTree = *ItemsCap->ItemCapability->ChildTree;
            TestNotNull(TEXT("GBH-02B: item capability includes Button's own 'text'"), ItemTree.FindProperty(TEXT("text")));
            TestNotNull(TEXT("GBH-02B: item capability includes Button's own 'binding'"), ItemTree.FindProperty(TEXT("binding")));
            TestNotNull(TEXT("GBH-02B: item capability includes Button's own 'key'"), ItemTree.FindProperty(TEXT("key")));
        }
    }

    auto MakeItemSchema = []() -> FCompiledUiFieldSpec
    {
        auto KeySpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key);
        auto TextSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
        auto BindingSpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding);
        FCompiledUiFieldSpec ItemObjectSpec;
        ItemObjectSpec.Kind = EUiFieldKind::Object;
        ItemObjectSpec.Fields.push_back({ "key", true, KeySpec });
        ItemObjectSpec.Fields.push_back({ "text", true, TextSpec });
        ItemObjectSpec.Fields.push_back({ "binding", false, BindingSpec });
        return ItemObjectSpec;
    };

    auto MakeItemsSchema = [&MakeItemSchema]() -> FCompiledUiFieldSpec
    {
        auto ArraySpec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Array);
        ArraySpec->KeyedBy = std::string("key");
        ArraySpec->Items = std::make_shared<FCompiledUiFieldSpec>(MakeItemSchema());
        FCompiledUiFieldSpec Schema;
        Schema.Kind = EUiFieldKind::Object;
        Schema.Fields.push_back({ "items", false, ArraySpec });
        return Schema;
    };
    const FCompiledUiFieldSpec Schema = MakeItemsSchema();

    auto MakeItemValue = [](const TCHAR* Key, const TCHAR* Text) -> FGV2PreparedUiValue
    {
        FGV2TextViewModel TextModel;
        TextModel.Text = FText::FromString(Text);
        TMap<FString, FGV2PreparedUiValue> Fields;
        Fields.Add(TEXT("key"), FGV2PreparedUiValue::MakeKey(Key));
        Fields.Add(TEXT("text"), FGV2PreparedUiValue::MakeText(TextModel));
        Fields.Add(TEXT("binding"), FGV2PreparedUiValue::MakeBinding(FGV2UiBindingHandle::Create(TEXT("action@1:1"))));
        return FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(Fields));
    };

    TMap<FString, FGV2PreparedUiValue> FirstHostFields;
    TArray<FGV2PreparedUiValue> FirstItemsArray;
    FirstItemsArray.Add(MakeItemValue(TEXT("first_item"), TEXT("First")));
    FirstHostFields.Add(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(FirstItemsArray)));
    const TSharedRef<const FGV2PreparedUiObject> FirstCandidate = FGV2PreparedUiObject::Create(FirstHostFields);

    FGV2UiHostMutationPlan Plan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
    const bool bPrepared = PrepareUiHostProperties(
        Composite, Builder.Build(), *FirstCandidate, Schema,
        TEXT("test:schema.gbh02b_collection_first_entry.v1"), TEXT(""),
        Composite->GetPropertyHostState().GetLastCommittedProperties(),
        Plan, Diagnostics);
    TestTrue(
        *FString::Printf(TEXT("GBH-02B: Prepare creates the first entry of a genuinely empty collection [Diagnostics: %s]"),
            Diagnostics.Num() > 0 ? *Diagnostics[0].ToString() : TEXT("")),
        bPrepared);

    FString FailedPath, CommitError;
    const bool bCommitted = bPrepared && CommitUiHostProperties(Composite, Plan, FailedPath, CommitError);
    TestTrue(*FString::Printf(TEXT("GBH-02B: Commit succeeds [Error: %s]"), *CommitError), bCommitted);

    // Observable renderer state: a real Button widget now exists in the previously-empty
    // panel, with the committed key/text.
    TestEqual(TEXT("GBH-02B: ListView now has exactly one entry"), ItemsList->GetEntryCount(), 1);
    TestEqual(TEXT("GBH-02B: ContainerPanel now has exactly one child"), ContainerPanel->GetChildrenCount(), 1);
    UGV2ButtonWidgetBase* const FirstButton = ItemsList->GetEntry<UGV2ButtonWidgetBase>(FName(TEXT("first_item")));
    TestNotNull(TEXT("GBH-02B: first entry resolves as a real UGV2ButtonWidgetBase"), FirstButton);
    if (FirstButton != nullptr)
    {
        TestEqual(TEXT("GBH-02B: first entry key matches"), FirstButton->GetKey(), FName(TEXT("first_item")));
    }

    // A second entry added afterward proves this generalizes past "exactly the first
    // item ever" -- the collection keeps working once it is no longer empty.
    TMap<FString, FGV2PreparedUiValue> SecondHostFields;
    TArray<FGV2PreparedUiValue> SecondItemsArray;
    SecondItemsArray.Add(MakeItemValue(TEXT("first_item"), TEXT("First")));
    SecondItemsArray.Add(MakeItemValue(TEXT("second_item"), TEXT("Second")));
    SecondHostFields.Add(TEXT("items"), FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(SecondItemsArray)));
    const TSharedRef<const FGV2PreparedUiObject> SecondCandidate = FGV2PreparedUiObject::Create(SecondHostFields);

    FGV2UiHostMutationPlan SecondPlan;
    TArray<FGV2UiSchemaCompatibilityDiagnostic> SecondDiagnostics;
    const bool bSecondPrepared = PrepareUiHostProperties(
        Composite, Builder.Build(), *SecondCandidate, Schema,
        TEXT("test:schema.gbh02b_collection_first_entry.v1"), TEXT(""),
        Composite->GetPropertyHostState().GetLastCommittedProperties(),
        SecondPlan, SecondDiagnostics);
    TestTrue(TEXT("GBH-02B: Prepare succeeds for a second entry on the now-non-empty collection"), bSecondPrepared);
    FString SecondFailedPath, SecondCommitError;
    const bool bSecondCommitted = bSecondPrepared && CommitUiHostProperties(Composite, SecondPlan, SecondFailedPath, SecondCommitError);
    TestTrue(TEXT("GBH-02B: second Commit succeeds"), bSecondCommitted);
    TestEqual(TEXT("GBH-02B: ListView now has two entries"), ItemsList->GetEntryCount(), 2);

    GameInstance->Shutdown();
    if (TestWorld != nullptr)
    {
        TestWorld->DestroyWorld(false);
        GEngine->DestroyWorldContext(TestWorld);
    }
    GameInstance->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCapabilitySubsetConstraintCoverageTest,
    "GV2.UI.CapabilitySubsetConstraintCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCapabilitySubsetConstraintCoverageTest::RunTest(const FString& Parameters)
{
    // GBH-08 follow-up. KeyPropertyName and EntryWidgetClass are constraint-bearing fields
    // that IsUiCapabilitySubset did not compare. The omission was argued from CollectionHost
    // being Hidden -- an argument GBH-02B retired one commit later by making it selectable.
    // These cases pin both comparisons; the static_assert next to the function is what stops
    // the *next* field from slipping through the same way.

    auto MakeCollectionCap = [](const TCHAR* KeyProperty, UClass* EntryClass)
    {
        FGV2UiPropertyCapability Cap;
        Cap.PropertyName = TEXT("items");
        Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
        Cap.TargetType = EGV2UiCapabilityTargetType::CollectionHost;
        Cap.bRequiresKeyedIdentity = true;
        Cap.KeyPropertyName = KeyProperty;
        Cap.EntryWidgetClass = EntryClass;
        return Cap;
    };

    // 1. Identical identity contract is a subset of itself.
    {
        const FGV2UiPropertyCapability Required = MakeCollectionCap(TEXT("key"), UGV2ButtonWidgetBase::StaticClass());
        const FGV2UiPropertyCapability Provided = MakeCollectionCap(TEXT("key"), UGV2ButtonWidgetBase::StaticClass());

        EGV2UiCapabilitySubsetMismatch Mismatch = EGV2UiCapabilitySubsetMismatch::None;
        FString Detail;
        TestTrue(TEXT("matching key property and entry class are accepted"),
            IsUiCapabilitySubset(Required, Provided, Mismatch, Detail));
    }

    // 2. A declaration keying by "id" against a child keying by "key" must be rejected: the
    //    collection consumer looks the item key up by the declared name, so it would find no
    //    key at all rather than a differently-named one.
    {
        const FGV2UiPropertyCapability Required = MakeCollectionCap(TEXT("id"), UGV2ButtonWidgetBase::StaticClass());
        const FGV2UiPropertyCapability Provided = MakeCollectionCap(TEXT("key"), UGV2ButtonWidgetBase::StaticClass());

        EGV2UiCapabilitySubsetMismatch Mismatch = EGV2UiCapabilitySubsetMismatch::None;
        FString Detail;
        TestFalse(TEXT("differing key property is rejected"),
            IsUiCapabilitySubset(Required, Provided, Mismatch, Detail));
        TestEqual(TEXT("rejection reports KeyPropertyMismatch"),
            Mismatch, EGV2UiCapabilitySubsetMismatch::KeyPropertyMismatch);
        TestTrue(TEXT("detail names both sides"), Detail.Contains(TEXT("id")) && Detail.Contains(TEXT("key")));
    }

    // 3. Naming a different entry widget class than the child repeats is a disagreement, not
    //    a narrowing.
    {
        const FGV2UiPropertyCapability Required = MakeCollectionCap(TEXT("key"), UGV2TextWidgetBase::StaticClass());
        const FGV2UiPropertyCapability Provided = MakeCollectionCap(TEXT("key"), UGV2ButtonWidgetBase::StaticClass());

        EGV2UiCapabilitySubsetMismatch Mismatch = EGV2UiCapabilitySubsetMismatch::None;
        FString Detail;
        TestFalse(TEXT("differing entry widget class is rejected"),
            IsUiCapabilitySubset(Required, Provided, Mismatch, Detail));
        TestEqual(TEXT("rejection reports EntryWidgetClassMismatch"),
            Mismatch, EGV2UiCapabilitySubsetMismatch::EntryWidgetClassMismatch);
    }

    // 4. An unset entry class on the declaration inherits the child's rather than conflicting.
    {
        FGV2UiPropertyCapability Required = MakeCollectionCap(TEXT("key"), nullptr);
        const FGV2UiPropertyCapability Provided = MakeCollectionCap(TEXT("key"), UGV2ButtonWidgetBase::StaticClass());

        EGV2UiCapabilitySubsetMismatch Mismatch = EGV2UiCapabilitySubsetMismatch::None;
        FString Detail;
        TestTrue(TEXT("unset entry class on the declaration is accepted"),
            IsUiCapabilitySubset(Required, Provided, Mismatch, Detail));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
