#include "UI/GV2UiCapability.h"
#include "Blueprint/UserWidget.h"

// GV2ContentCore's compiled UI schema has one Scalar kind wrapping a nested
// EScalarFieldKind (bool/integer/number/string), not four separate top-level
// kinds, so the sub-kind must be read off Spec.Scalar to map correctly.
static EGV2PreparedUiValueKind MapFieldSpecToPreparedKind(const GV2ContentCore::FCompiledUiFieldSpec& Spec)
{
    using namespace GV2ContentCore;
    switch (Spec.Kind)
    {
    case EUiFieldKind::Scalar:
        if (!Spec.Scalar.has_value())
        {
            return EGV2PreparedUiValueKind::Null;
        }
        switch (Spec.Scalar->Kind)
        {
        case EScalarFieldKind::Boolean:
            return EGV2PreparedUiValueKind::Boolean;
        case EScalarFieldKind::Integer:
            return EGV2PreparedUiValueKind::Integer;
        case EScalarFieldKind::Number:
            return EGV2PreparedUiValueKind::Number;
        case EScalarFieldKind::String:
            return EGV2PreparedUiValueKind::String;
        case EScalarFieldKind::Enum:
            return EGV2PreparedUiValueKind::Null;
        }
    case EUiFieldKind::Key:
        return EGV2PreparedUiValueKind::Key;
    case EUiFieldKind::Text:
        return EGV2PreparedUiValueKind::Text;
    case EUiFieldKind::Ref:
        return EGV2PreparedUiValueKind::StableId;
    case EUiFieldKind::Binding:
        return EGV2PreparedUiValueKind::Binding;
    case EUiFieldKind::Object:
    case EUiFieldKind::ScreenFields:
        return EGV2PreparedUiValueKind::Object;
    case EUiFieldKind::Array:
        return EGV2PreparedUiValueKind::Array;
    default:
        return EGV2PreparedUiValueKind::Null;
    }
}

// --- FGV2UiPropertyCapability ---

bool FGV2UiPropertyCapability::operator==(const FGV2UiPropertyCapability& Other) const
{
    return PropertyName == Other.PropertyName
        && SupportedKind == Other.SupportedKind
        && TargetType == Other.TargetType
        && TargetName == Other.TargetName
        && TargetKind == Other.TargetKind
        && IntMin == Other.IntMin
        && IntMax == Other.IntMax
        && NumberMin == Other.NumberMin
        && NumberMax == Other.NumberMax
        && bRequiresKeyedIdentity == Other.bRequiresKeyedIdentity
        && KeyPropertyName == Other.KeyPropertyName
        && EntryWidgetClass == Other.EntryWidgetClass
        && ChildCapabilityName == Other.ChildCapabilityName;
}

// --- FGV2UiCapabilityBuilder ---

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddText(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Text;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddImage(const FString& Name, const FName& TargetName, const FString& TargetKind)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::StableId;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.TargetKind = TargetKind;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddBoolean(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Boolean;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddInteger(const FString& Name, const FName& TargetName, TOptional<int64> Min, TOptional<int64> Max)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Integer;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.IntMin = Min;
    Cap.IntMax = Max;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddNumber(const FString& Name, const FName& TargetName, TOptional<double> Min, TOptional<double> Max)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Number;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.NumberMin = Min;
    Cap.NumberMax = Max;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddString(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::String;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKey(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Key;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddBinding(const FString& Name, const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Binding;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKeyedCollection(
    const FString& Name,
    const FName& TargetName,
    FGV2UiPropertyCapability ItemCapability,
    const FString& KeyField,
    TSubclassOf<UUserWidget> EntryWidgetClass)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
    Cap.TargetType = EGV2UiCapabilityTargetType::CollectionHost;
    Cap.TargetName = TargetName;
    Cap.bRequiresKeyedIdentity = true;
    Cap.KeyPropertyName = KeyField;
    Cap.EntryWidgetClass = EntryWidgetClass;
    Cap.ItemCapability = MakeShared<FGV2UiPropertyCapability>(MoveTemp(ItemCapability));
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKeyedCollection(
    const FString& Name,
    const FName& TargetName,
    FGV2UiCapabilityTree ItemCapabilityTree,
    const FString& KeyField,
    TSubclassOf<UUserWidget> EntryWidgetClass)
{
    FGV2UiPropertyCapability ItemCap;
    ItemCap.SupportedKind = EGV2PreparedUiValueKind::Object;
    ItemCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    ItemCap.ChildTree = MakeShared<FGV2UiCapabilityTree>(MoveTemp(ItemCapabilityTree));
    ItemCap.EntryWidgetClass = EntryWidgetClass;
    return AddKeyedCollection(Name, TargetName, MoveTemp(ItemCap), KeyField, EntryWidgetClass);
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddNestedScreenCollection(
    const FString& Name,
    const FName& TargetName,
    const FString& KeyField)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
    Cap.TargetType = EGV2UiCapabilityTargetType::NestedScreen;
    Cap.TargetName = TargetName;
    Cap.bRequiresKeyedIdentity = true;
    Cap.KeyPropertyName = KeyField;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddCustom(
    const FString& Name,
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FName& TargetName)
{
    checkf(Kind != EGV2PreparedUiValueKind::Object && Kind != EGV2PreparedUiValueKind::Null,
        TEXT("Cannot declare capability with inapplicable kind (Null or Object)"));

    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = Kind;
    Cap.TargetType = TargetType;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::SetChildCapabilityName(const FString& Name, const FString& ChildCapabilityName)
{
    if (FGV2UiPropertyCapability* Cap = Tree.Properties.Find(Name))
    {
        Cap->ChildCapabilityName = ChildCapabilityName;
    }
    return *this;
}

// --- IsUiCapabilitySubset ---

bool IsUiCapabilitySubset(
    const FGV2UiPropertyCapability& Required,
    const FGV2UiPropertyCapability& Provided,
    EGV2UiCapabilitySubsetMismatch& OutMismatch,
    FString& OutDetail)
{
    OutMismatch = EGV2UiCapabilitySubsetMismatch::None;
    OutDetail.Reset();

    if (Required.SupportedKind != Provided.SupportedKind)
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::KindMismatch;
        OutDetail = TEXT("kind mismatch");
        return false;
    }

    if (!Required.TargetKind.IsEmpty() && !Provided.TargetKind.IsEmpty() && Required.TargetKind != Provided.TargetKind)
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::TargetKindMismatch;
        OutDetail = FString::Printf(
            TEXT("target_kind mismatch: required '%s', provided '%s'"),
            *Required.TargetKind, *Provided.TargetKind);
        return false;
    }

    if (Provided.IntMin.IsSet() && (!Required.IntMin.IsSet() || *Required.IntMin < *Provided.IntMin))
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::IntRangeMismatch;
        OutDetail = TEXT("integer min exceeds provided capability bound");
        return false;
    }
    if (Provided.IntMax.IsSet() && (!Required.IntMax.IsSet() || *Required.IntMax > *Provided.IntMax))
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::IntRangeMismatch;
        OutDetail = TEXT("integer max exceeds provided capability bound");
        return false;
    }

    if (Provided.NumberMin.IsSet() && (!Required.NumberMin.IsSet() || *Required.NumberMin < *Provided.NumberMin))
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::NumberRangeMismatch;
        OutDetail = TEXT("number min exceeds provided capability bound");
        return false;
    }
    if (Provided.NumberMax.IsSet() && (!Required.NumberMax.IsSet() || *Required.NumberMax > *Provided.NumberMax))
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::NumberRangeMismatch;
        OutDetail = TEXT("number max exceeds provided capability bound");
        return false;
    }

    if (Provided.bRequiresKeyedIdentity && !Required.bRequiresKeyedIdentity)
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::KeyedIdentityMismatch;
        OutDetail = TEXT("provided capability requires keyed elements, required side does not declare them");
        return false;
    }

    // GBH-08 follow-up: the identity field name is a constraint, not decoration -- the
    // collection consumer looks the item key up by the *declared* KeyPropertyName
    // (GV2PropertyConsumers.cpp), so a declaration naming "id" against a child that keys by
    // "key" finds no key at all. Deferring this comparison was argued from CollectionHost
    // being Hidden; GBH-02B made it selectable one commit later, which retired that argument.
    if (Required.KeyPropertyName != Provided.KeyPropertyName)
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::KeyPropertyMismatch;
        OutDetail = FString::Printf(
            TEXT("key property mismatch: required '%s', provided '%s'"),
            *Required.KeyPropertyName, *Provided.KeyPropertyName);
        return false;
    }

    // A declaration may leave the entry class unset (inherit the child's), but naming a
    // different one than the child repeats is a contract disagreement, not a narrowing.
    if (Required.EntryWidgetClass != nullptr
        && Provided.EntryWidgetClass != nullptr
        && Required.EntryWidgetClass != Provided.EntryWidgetClass)
    {
        OutMismatch = EGV2UiCapabilitySubsetMismatch::EntryWidgetClassMismatch;
        OutDetail = FString::Printf(
            TEXT("entry widget class mismatch: required '%s', provided '%s'"),
            *Required.EntryWidgetClass->GetName(), *Provided.EntryWidgetClass->GetName());
        return false;
    }

    if (Provided.ItemCapability.IsValid() && Required.ItemCapability.IsValid())
    {
        EGV2UiCapabilitySubsetMismatch ItemMismatch;
        FString ItemDetail;
        if (!IsUiCapabilitySubset(*Required.ItemCapability, *Provided.ItemCapability, ItemMismatch, ItemDetail))
        {
            OutMismatch = EGV2UiCapabilitySubsetMismatch::ItemMismatch;
            OutDetail = FString::Printf(TEXT("item %s"), *ItemDetail);
            return false;
        }
    }

    return true;
}

// GBH-08: projects one compiled schema field into the same FGV2UiPropertyCapability shape
// IsUiCapabilitySubset already compares capabilities in, so CheckUiSchemaCapabilityCompatibility
// can delegate its leaf-level kind/target_kind/range/keyed-identity checks to that one shared
// function instead of re-implementing the same rules against a differently-shaped schema
// field. Only the leaf shape is projected here; a schema's own further-nested Object item
// fields are still walked by this function's own recursion below, not by the projection.
static FGV2UiPropertyCapability ProjectSchemaFieldToCapability(const GV2ContentCore::FCompiledUiFieldSpec& FieldSpec)
{
    FGV2UiPropertyCapability Required;
    Required.SupportedKind = MapFieldSpecToPreparedKind(FieldSpec);

    if (FieldSpec.Kind == GV2ContentCore::EUiFieldKind::Ref && !FieldSpec.RefTargetKind.empty())
    {
        Required.TargetKind = UTF8_TO_TCHAR(FieldSpec.RefTargetKind.c_str());
    }

    if (FieldSpec.Scalar.has_value())
    {
        const GV2ContentCore::FScalarFieldSpec& Scalar = *FieldSpec.Scalar;
        if (Scalar.MinimumInteger.has_value()) Required.IntMin = *Scalar.MinimumInteger;
        if (Scalar.MaximumInteger.has_value()) Required.IntMax = *Scalar.MaximumInteger;
        if (Scalar.MinimumNumber.has_value()) Required.NumberMin = *Scalar.MinimumNumber;
        if (Scalar.MaximumNumber.has_value()) Required.NumberMax = *Scalar.MaximumNumber;
    }

    if (FieldSpec.Kind == GV2ContentCore::EUiFieldKind::Array)
    {
        Required.bRequiresKeyedIdentity = FieldSpec.KeyedBy.has_value();
        if (FieldSpec.KeyedBy.has_value())
        {
            // GBF-02: keyed_by is an identity-field name, not merely an on/off
            // requirement. Preserve it on the schema side so the shared subset
            // rule can compare the actual schema contract with the declaration.
            Required.KeyPropertyName = UTF8_TO_TCHAR(FieldSpec.KeyedBy->c_str());
        }
    }

    return Required;
}

// --- CheckUiSchemaCapabilityCompatibility ---

bool CheckUiSchemaCapabilityCompatibility(
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FGV2UiCapabilityTree& Capabilities,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics)
{
    bool bSuccess = true;

    if (Schema.Kind == GV2ContentCore::EUiFieldKind::Object || Schema.Kind == GV2ContentCore::EUiFieldKind::ScreenFields)
    {
        for (const auto& FieldEntry : Schema.Fields)
        {
            const FString FieldName = UTF8_TO_TCHAR(FieldEntry.Name.c_str());
            const FString ChildPath = PropertyPathPrefix.IsEmpty()
                ? FieldName
                : FString::Printf(TEXT("%s.%s"), *PropertyPathPrefix, *FieldName);

            const FGV2UiPropertyCapability* Cap = Capabilities.FindProperty(FieldName);
            if (!Cap)
            {
                // Extra property in schema not supported by widget capabilities
                FGV2UiSchemaCompatibilityDiagnostic Diag;
                Diag.Code = TEXT("core:diagnostic.ui_capability.unknown_schema_property");
                Diag.PropertyPath = ChildPath;
                Diag.SchemaId = SchemaId;
                Diag.Message = FString::Printf(TEXT("Schema property '%s' is not supported by widget capabilities"), *FieldName);
                OutDiagnostics.Add(MoveTemp(Diag));
                bSuccess = false;
                continue;
            }

            const GV2ContentCore::FCompiledUiFieldSpecPtr& FieldSpec = FieldEntry.Spec;
            if (!FieldSpec)
            {
                continue;
            }

            // GBH-08: kind, target_kind, int/number range, and keyed-identity are all
            // checked by the one shared IsUiCapabilitySubset rule -- the schema field is
            // projected into the same descriptor shape a capability already has, so this
            // is not a second implementation of "does Required fit inside Provided".
            const EGV2PreparedUiValueKind ExpectedKind = MapFieldSpecToPreparedKind(*FieldSpec);
            const FGV2UiPropertyCapability RequiredFromSchema = ProjectSchemaFieldToCapability(*FieldSpec);
            EGV2UiCapabilitySubsetMismatch SubsetMismatch;
            FString SubsetDetail;
            if (!IsUiCapabilitySubset(RequiredFromSchema, *Cap, SubsetMismatch, SubsetDetail))
            {
                FGV2UiSchemaCompatibilityDiagnostic Diag;
                Diag.PropertyPath = ChildPath;
                Diag.SchemaId = SchemaId;
                switch (SubsetMismatch)
                {
                case EGV2UiCapabilitySubsetMismatch::TargetKindMismatch:
                    Diag.Code = TEXT("core:diagnostic.ui_capability.target_kind_mismatch");
                    Diag.Message = FString::Printf(
                        TEXT("Target kind mismatch for ref '%s': schema requires '%s', capability supports '%s'"),
                        *FieldName, *RequiredFromSchema.TargetKind, *Cap->TargetKind);
                    break;
                case EGV2UiCapabilitySubsetMismatch::IntRangeMismatch:
                case EGV2UiCapabilitySubsetMismatch::NumberRangeMismatch:
                    Diag.Code = TEXT("core:diagnostic.ui_capability.range_unsupported");
                    Diag.Message = FString::Printf(TEXT("Schema range for '%s' exceeds capability bound: %s"), *FieldName, *SubsetDetail);
                    break;
                case EGV2UiCapabilitySubsetMismatch::KeyedIdentityMismatch:
                    Diag.Code = TEXT("core:diagnostic.ui_capability.collection_identity_mismatch");
                    Diag.Message = FString::Printf(TEXT("Collection '%s' requires keyed elements, but schema array has no keyed_by"), *FieldName);
                    break;
                case EGV2UiCapabilitySubsetMismatch::KeyPropertyMismatch:
                    Diag.Code = TEXT("core:diagnostic.ui_capability.key_property_mismatch");
                    Diag.Message = FString::Printf(
                        TEXT("Collection '%s' key property mismatch: %s"), *FieldName, *SubsetDetail);
                    break;
                case EGV2UiCapabilitySubsetMismatch::EntryWidgetClassMismatch:
                    // EntryWidgetClass exists only on the declaration side; kept so
                    // the switch stays exhaustive against future projection changes.
                    Diag.Code = TEXT("core:diagnostic.ui_capability.collection_identity_mismatch");
                    Diag.Message = FString::Printf(TEXT("Collection '%s' identity contract mismatch: %s"), *FieldName, *SubsetDetail);
                    break;
                case EGV2UiCapabilitySubsetMismatch::ItemMismatch:
                    // Not reached from this projection (RequiredFromSchema never sets
                    // ItemCapability -- the schema's own nested item fields are walked by
                    // this function's own recursion below instead), kept only so the
                    // switch stays exhaustive against future EGV2UiCapabilitySubsetMismatch
                    // values.
                case EGV2UiCapabilitySubsetMismatch::KindMismatch:
                case EGV2UiCapabilitySubsetMismatch::None:
                default:
                    Diag.Code = TEXT("core:diagnostic.ui_capability.kind_mismatch");
                    Diag.Message = FString::Printf(TEXT("Kind mismatch for property '%s'"), *FieldName);
                    break;
                }
                OutDiagnostics.Add(MoveTemp(Diag));
                bSuccess = false;
                if (SubsetMismatch == EGV2UiCapabilitySubsetMismatch::KindMismatch
                    || SubsetMismatch == EGV2UiCapabilitySubsetMismatch::TargetKindMismatch)
                {
                    continue;
                }
            }

            // Collection element recursive check (PCC-02)
            if (FieldSpec->Kind == GV2ContentCore::EUiFieldKind::Array && FieldSpec->Items != nullptr)
            {
                const FGV2UiCapabilityTree* ItemTree = nullptr;
                if (Cap->ItemCapability.IsValid() && Cap->ItemCapability->ChildTree.IsValid())
                {
                    ItemTree = Cap->ItemCapability->ChildTree.Get();
                }
                else if (Cap->ChildTree.IsValid())
                {
                    ItemTree = Cap->ChildTree.Get();
                }

                if (ItemTree != nullptr)
                {
                    if (FieldSpec->Items->Kind != GV2ContentCore::EUiFieldKind::Object)
                    {
                        FGV2UiSchemaCompatibilityDiagnostic Diag;
                        Diag.Code = TEXT("core:diagnostic.ui_capability.kind_mismatch");
                        Diag.PropertyPath = FString::Printf(TEXT("%s[]"), *ChildPath);
                        Diag.SchemaId = SchemaId;
                                Diag.Message = FString::Printf(
                            TEXT("Collection '%s' item kind mismatch: schema expects non-object, capability supports Object"),
                            *FieldName);
                        OutDiagnostics.Add(MoveTemp(Diag));
                        bSuccess = false;
                    }
                    else
                    {
                        const FString ItemPath = FString::Printf(TEXT("%s[]"), *ChildPath);
                        if (!CheckUiSchemaCapabilityCompatibility(*FieldSpec->Items, *ItemTree, SchemaId, ItemPath, OutDiagnostics))
                        {
                            bSuccess = false;
                        }
                    }
                }
                else if (Cap->ItemCapability.IsValid() && Cap->ItemCapability->SupportedKind != EGV2PreparedUiValueKind::Null && Cap->ItemCapability->SupportedKind != EGV2PreparedUiValueKind::Object)
                {
                    const EGV2PreparedUiValueKind ItemKind = Cap->ItemCapability->SupportedKind;
                    const EGV2PreparedUiValueKind ExpectedItemKind = MapFieldSpecToPreparedKind(*FieldSpec->Items);
                    if (ItemKind != ExpectedItemKind)
                    {
                        FGV2UiSchemaCompatibilityDiagnostic Diag;
                        Diag.Code = TEXT("core:diagnostic.ui_capability.kind_mismatch");
                        Diag.PropertyPath = FString::Printf(TEXT("%s[]"), *ChildPath);
                        Diag.SchemaId = SchemaId;
                                Diag.Message = FString::Printf(
                            TEXT("Collection '%s' item kind mismatch"),
                            *FieldName);
                        OutDiagnostics.Add(MoveTemp(Diag));
                        bSuccess = false;
                    }
                }
            }
        }
    }

    return bSuccess;
}

bool ResolveDelegatedChildCapability(
    const FGV2UiCapabilityTree& ChildCapabilities,
    EGV2PreparedUiValueKind DeclaredKind,
    const FString& ChildCapabilityName,
    const FGV2UiPropertyCapability*& OutResolved,
    FString& OutError,
    bool& bOutAmbiguous,
    const FString& FallbackNameHint)
{
    OutResolved = nullptr;
    bOutAmbiguous = false;

    if (!ChildCapabilityName.IsEmpty())
    {
        const FGV2UiPropertyCapability* Found = ChildCapabilities.FindProperty(ChildCapabilityName);
        if (Found == nullptr)
        {
            OutError = FString::Printf(TEXT("child has no capability named '%s'"), *ChildCapabilityName);
            return false;
        }
        if (Found->SupportedKind != DeclaredKind)
        {
            OutError = FString::Printf(TEXT("child capability '%s' does not declare the expected kind"), *ChildCapabilityName);
            return false;
        }
        OutResolved = Found;
        return true;
    }

    int32 CandidateCount = 0;
    for (const auto& Entry : ChildCapabilities.Properties)
    {
        if (Entry.Value.SupportedKind == DeclaredKind)
        {
            OutResolved = &Entry.Value;
            ++CandidateCount;
        }
    }

    if (CandidateCount == 1)
    {
        return true;
    }

    if (CandidateCount > 1 && !FallbackNameHint.IsEmpty())
    {
        if (const FGV2UiPropertyCapability* HintMatch = ChildCapabilities.FindProperty(FallbackNameHint))
        {
            if (HintMatch->SupportedKind == DeclaredKind)
            {
                OutResolved = HintMatch;
                return true;
            }
        }
    }

    OutResolved = nullptr;
    if (CandidateCount == 0)
    {
        OutError = TEXT("child does not declare a capability of the expected kind");
    }
    else
    {
        OutError = FString::Printf(
            TEXT("child declares %d capabilities of the expected kind; set ChildCapabilityName to disambiguate"),
            CandidateCount);
        bOutAmbiguous = true;
    }
    return false;
}
