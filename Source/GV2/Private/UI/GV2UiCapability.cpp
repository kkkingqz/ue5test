#include "UI/GV2UiCapability.h"

static bool IsModNamespace(const FString& InSchemaId)
{
    int32 ColonIdx = INDEX_NONE;
    if (!InSchemaId.FindChar(TEXT(':'), ColonIdx) || ColonIdx <= 0)
    {
        return false;
    }
    const FString Ns = InSchemaId.Left(ColonIdx);
    return Ns != TEXT("core") && Ns != TEXT("textsystem") && Ns != TEXT("rh");
}

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
        default:
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
        && KeyPropertyName == Other.KeyPropertyName;
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

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddObject(const FString& Name, const FName& TargetName, FGV2UiCapabilityTree InChildTree)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Object;
    Cap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    Cap.TargetName = TargetName;
    Cap.ChildTree = MakeShared<FGV2UiCapabilityTree>(MoveTemp(InChildTree));
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddKeyedCollection(
    const FString& Name,
    const FName& TargetName,
    FGV2UiPropertyCapability ItemCapability,
    const FString& KeyField)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = EGV2PreparedUiValueKind::Array;
    Cap.TargetType = EGV2UiCapabilityTargetType::CollectionHost;
    Cap.TargetName = TargetName;
    Cap.bRequiresKeyedIdentity = true;
    Cap.KeyPropertyName = KeyField;
    Cap.ItemCapability = MakeShared<FGV2UiPropertyCapability>(MoveTemp(ItemCapability));
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

FGV2UiCapabilityBuilder& FGV2UiCapabilityBuilder::AddCustom(
    const FString& Name,
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FName& TargetName)
{
    FGV2UiPropertyCapability Cap;
    Cap.PropertyName = Name;
    Cap.SupportedKind = Kind;
    Cap.TargetType = TargetType;
    Cap.TargetName = TargetName;
    Tree.Properties.Add(Name, MoveTemp(Cap));
    return *this;
}

// --- CheckUiSchemaCapabilityCompatibility ---

bool CheckUiSchemaCapabilityCompatibility(
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FGV2UiCapabilityTree& Capabilities,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics)
{
    const bool bIsMod = IsModNamespace(SchemaId);
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
                Diag.bFatal = !bIsMod;
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

            // Kind compatibility check
            const EGV2PreparedUiValueKind ExpectedKind = MapFieldSpecToPreparedKind(*FieldSpec);
            if (Cap->SupportedKind != ExpectedKind)
            {
                FGV2UiSchemaCompatibilityDiagnostic Diag;
                Diag.Code = TEXT("core:diagnostic.ui_capability.kind_mismatch");
                Diag.PropertyPath = ChildPath;
                Diag.SchemaId = SchemaId;
                Diag.bFatal = !bIsMod;
                Diag.Message = FString::Printf(TEXT("Kind mismatch for property '%s'"), *FieldName);
                OutDiagnostics.Add(MoveTemp(Diag));
                bSuccess = false;
                continue;
            }

            // Target kind check for Ref / StableId
            if (FieldSpec->Kind == GV2ContentCore::EUiFieldKind::Ref)
            {
                if (!Cap->TargetKind.IsEmpty() && !FieldSpec->RefTargetKind.empty())
                {
                    const FString SchemaTargetKind = UTF8_TO_TCHAR(FieldSpec->RefTargetKind.c_str());
                    if (SchemaTargetKind != Cap->TargetKind)
                    {
                        FGV2UiSchemaCompatibilityDiagnostic Diag;
                        Diag.Code = TEXT("core:diagnostic.ui_capability.target_kind_mismatch");
                        Diag.PropertyPath = ChildPath;
                        Diag.SchemaId = SchemaId;
                        Diag.bFatal = !bIsMod;
                        Diag.Message = FString::Printf(
                            TEXT("Target kind mismatch for ref '%s': schema requires '%s', capability supports '%s'"),
                            *FieldName, *SchemaTargetKind, *Cap->TargetKind);
                        OutDiagnostics.Add(MoveTemp(Diag));
                        bSuccess = false;
                        continue;
                    }
                }
            }

            // Numeric range checks (subset rule: schema range must fit within capability range)
            if (ExpectedKind == EGV2PreparedUiValueKind::Integer && FieldSpec->Scalar.has_value())
            {
                const GV2ContentCore::FScalarFieldSpec& Scalar = *FieldSpec->Scalar;
                if (Cap->IntMin.IsSet() && (!Scalar.MinimumInteger.has_value() || *Scalar.MinimumInteger < *Cap->IntMin))
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_capability.range_unsupported");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.bFatal = !bIsMod;
                    Diag.Message = FString::Printf(TEXT("Schema integer min for '%s' exceeds capability bound"), *FieldName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    bSuccess = false;
                }
                if (Cap->IntMax.IsSet() && (!Scalar.MaximumInteger.has_value() || *Scalar.MaximumInteger > *Cap->IntMax))
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_capability.range_unsupported");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.bFatal = !bIsMod;
                    Diag.Message = FString::Printf(TEXT("Schema integer max for '%s' exceeds capability bound"), *FieldName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    bSuccess = false;
                }
            }
            else if (ExpectedKind == EGV2PreparedUiValueKind::Number && FieldSpec->Scalar.has_value())
            {
                const GV2ContentCore::FScalarFieldSpec& Scalar = *FieldSpec->Scalar;
                if (Cap->NumberMin.IsSet() && (!Scalar.MinimumNumber.has_value() || *Scalar.MinimumNumber < *Cap->NumberMin))
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_capability.range_unsupported");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.bFatal = !bIsMod;
                    Diag.Message = FString::Printf(TEXT("Schema number min for '%s' exceeds capability bound"), *FieldName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    bSuccess = false;
                }
                if (Cap->NumberMax.IsSet() && (!Scalar.MaximumNumber.has_value() || *Scalar.MaximumNumber > *Cap->NumberMax))
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_capability.range_unsupported");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.bFatal = !bIsMod;
                    Diag.Message = FString::Printf(TEXT("Schema number max for '%s' exceeds capability bound"), *FieldName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    bSuccess = false;
                }
            }

            // Recursive Object check
            if (FieldSpec->Kind == GV2ContentCore::EUiFieldKind::Object && Cap->ChildTree.IsValid())
            {
                if (!CheckUiSchemaCapabilityCompatibility(*FieldSpec, *Cap->ChildTree, SchemaId, ChildPath, OutDiagnostics))
                {
                    bSuccess = false;
                }
            }

            // Keyed Collection check
            if (FieldSpec->Kind == GV2ContentCore::EUiFieldKind::Array && Cap->bRequiresKeyedIdentity)
            {
                if (!FieldSpec->KeyedBy.has_value())
                {
                    FGV2UiSchemaCompatibilityDiagnostic Diag;
                    Diag.Code = TEXT("core:diagnostic.ui_capability.collection_identity_mismatch");
                    Diag.PropertyPath = ChildPath;
                    Diag.SchemaId = SchemaId;
                    Diag.bFatal = !bIsMod;
                    Diag.Message = FString::Printf(TEXT("Collection '%s' requires keyed elements, but schema array has no keyed_by"), *FieldName);
                    OutDiagnostics.Add(MoveTemp(Diag));
                    bSuccess = false;
                }
            }
        }
    }

    return bSuccess;
}
