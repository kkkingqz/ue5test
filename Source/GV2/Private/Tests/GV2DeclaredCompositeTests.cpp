#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"
#include "UObject/UnrealType.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
