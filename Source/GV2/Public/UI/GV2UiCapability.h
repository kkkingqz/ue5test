#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "GV2ContentCore/UiSchema.h"

enum class EGV2UiCapabilityTargetType : uint8
{
    RendererControl,
    CollectionHost,
    NestedScreen,
    CustomControl
};

struct GV2_API FGV2UiSchemaCompatibilityDiagnostic
{
    FString Code;
    FString PropertyPath;
    FString Message;
    FString SchemaId;
    bool bFatal = true;

    FString ToString() const
    {
        return FString::Printf(TEXT("[%s] %s at '%s' (schema: %s, fatal: %s)"),
            *Code, *Message, *PropertyPath, *SchemaId, bFatal ? TEXT("true") : TEXT("false"));
    }
};

struct FGV2UiCapabilityTree;

/**
 * Descriptor of a single property capability supported by a widget.
 * Explicitly binds a property name to a target renderer control, collection host, or sub-screen.
 */
struct GV2_API FGV2UiPropertyCapability
{
    FString PropertyName;
    EGV2PreparedUiValueKind SupportedKind = EGV2PreparedUiValueKind::Null;
    EGV2UiCapabilityTargetType TargetType = EGV2UiCapabilityTargetType::RendererControl;
    FName TargetName = NAME_None;

    // Semantic constraints
    FString TargetKind; // For Ref/StableId, e.g. "resource", "item", "screen"
    TOptional<int64> IntMin;
    TOptional<int64> IntMax;
    TOptional<double> NumberMin;
    TOptional<double> NumberMax;

    // Collection policy
    bool bRequiresKeyedIdentity = false;
    FString KeyPropertyName = TEXT("key");
    TSubclassOf<UUserWidget> EntryWidgetClass = nullptr;

    // Nested structures
    TSharedPtr<FGV2UiCapabilityTree> ChildTree;
    TSharedPtr<FGV2UiPropertyCapability> ItemCapability;

    bool operator==(const FGV2UiPropertyCapability& Other) const;
    bool operator!=(const FGV2UiPropertyCapability& Other) const { return !(*this == Other); }
};

/**
 * Tree of property capabilities declared by a widget class or instance.
 */
struct GV2_API FGV2UiCapabilityTree
{
    TMap<FString, FGV2UiPropertyCapability> Properties;

    const FGV2UiPropertyCapability* FindProperty(const FString& InName) const
    {
        return Properties.Find(InName);
    }

    bool HasProperty(const FString& InName) const
    {
        return Properties.Contains(InName);
    }

    int32 Num() const { return Properties.Num(); }
    bool IsEmpty() const { return Properties.IsEmpty(); }
};

/**
 * Fluent builder for declaring capability trees without manual struct boilerplate.
 */
class GV2_API FGV2UiCapabilityBuilder
{
public:
    FGV2UiCapabilityBuilder() = default;

    FGV2UiCapabilityBuilder& AddText(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddImage(const FString& Name, const FName& TargetName, const FString& TargetKind = TEXT("resource"));
    FGV2UiCapabilityBuilder& AddBoolean(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddInteger(const FString& Name, const FName& TargetName, TOptional<int64> Min = {}, TOptional<int64> Max = {});
    FGV2UiCapabilityBuilder& AddNumber(const FString& Name, const FName& TargetName, TOptional<double> Min = {}, TOptional<double> Max = {});
    FGV2UiCapabilityBuilder& AddString(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddKey(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddBinding(const FString& Name, const FName& TargetName);
    FGV2UiCapabilityBuilder& AddKeyedCollection(const FString& Name, const FName& TargetName, FGV2UiPropertyCapability ItemCapability, const FString& KeyField = TEXT("key"), TSubclassOf<UUserWidget> EntryWidgetClass = nullptr);
    FGV2UiCapabilityBuilder& AddKeyedCollection(const FString& Name, const FName& TargetName, FGV2UiCapabilityTree ItemCapabilityTree, const FString& KeyField = TEXT("key"), TSubclassOf<UUserWidget> EntryWidgetClass = nullptr);
    FGV2UiCapabilityBuilder& AddNestedScreenCollection(const FString& Name, const FName& TargetName, const FString& KeyField = TEXT("key"));
    FGV2UiCapabilityBuilder& AddCustom(const FString& Name, EGV2PreparedUiValueKind Kind, EGV2UiCapabilityTargetType TargetType, const FName& TargetName);

    FGV2UiCapabilityTree Build() const { return Tree; }

private:
    FGV2UiCapabilityTree Tree;
};

/**
 * Validates that SchemaContract is a subset of WidgetCapabilities: SchemaContract ⊆ WidgetCapabilities.
 * Performs recursive verification across kinds, target_kinds, ranges, and nested structures.
 */
GV2_API bool CheckUiSchemaCapabilityCompatibility(
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FGV2UiCapabilityTree& Capabilities,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics);

/**
 * DUC-07: true if ChildCapabilities (independently declared by a named target widget's own
 * DescribeUiCapabilities, not derived from the composite that addresses it) contains at
 * least one property of DeclaredKind. A composite's Designer-authored capability declaration
 * is checked against this -- the child's own declaration is the second, independent source;
 * neither side is derived from the other, so a mismatch (e.g. `Number` declared against a
 * child that only ever declares `Text`) is a real, catchable drift rather than true by
 * construction.
 */
GV2_API bool DoesCapabilityTreeSupportKind(
    const FGV2UiCapabilityTree& ChildCapabilities,
    EGV2PreparedUiValueKind DeclaredKind);
