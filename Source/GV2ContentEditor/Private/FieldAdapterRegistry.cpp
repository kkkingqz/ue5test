#include "GV2ContentEditor/FieldAdapterRegistry.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/ScalarValidation.h"

namespace GV2ContentEditor
{

namespace
{

class FDefaultScalarAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "DefaultScalarAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::Text; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* UiMeta) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;

        if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
        {
            const auto& S = *Spec.Scalar;
            Desc.ScalarKind = S.Kind;
            switch (S.Kind)
            {
            case GV2ContentCore::EScalarFieldKind::Boolean:
                Desc.ControlType = EFieldControlType::Checkbox;
                Desc.bAllowsDirectTextEntry = false;
                break;
            case GV2ContentCore::EScalarFieldKind::Integer:
                if (UiMeta && UiMeta->WidgetHint.has_value() && *UiMeta->WidgetHint == "slider")
                {
                    Desc.ControlType = EFieldControlType::Slider;
                }
                else
                {
                    Desc.ControlType = EFieldControlType::IntegerNumeric;
                }
                if (S.MinimumInteger.has_value()) Desc.MinValue = static_cast<double>(*S.MinimumInteger);
                if (S.MaximumInteger.has_value()) Desc.MaxValue = static_cast<double>(*S.MaximumInteger);
                break;
            case GV2ContentCore::EScalarFieldKind::Number:
                if (UiMeta && UiMeta->WidgetHint.has_value() && *UiMeta->WidgetHint == "slider")
                {
                    Desc.ControlType = EFieldControlType::Slider;
                }
                else
                {
                    Desc.ControlType = EFieldControlType::DoubleNumeric;
                }
                if (S.MinimumNumber.has_value()) Desc.MinValue = *S.MinimumNumber;
                if (S.MaximumNumber.has_value()) Desc.MaxValue = *S.MaximumNumber;
                break;
            case GV2ContentCore::EScalarFieldKind::String:
                if (UiMeta && UiMeta->WidgetHint.has_value() && *UiMeta->WidgetHint == "multiline")
                {
                    Desc.ControlType = EFieldControlType::MultilineText;
                }
                else
                {
                    Desc.ControlType = EFieldControlType::Text;
                }
                break;
            default:
                Desc.ControlType = EFieldControlType::Text;
                break;
            }
        }
        return Desc;
    }
};

class FEnumAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "EnumAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::EnumDropdown; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::EnumDropdown;
        Desc.bAllowsDirectTextEntry = false;

        if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
        {
            Desc.ScalarKind = Spec.Scalar->Kind;
            for (const auto& Val : Spec.Scalar->EnumValues)
            {
                if (Val.IsString())
                {
                    Desc.EnumChoices.push_back(Val.AsString());
                }
                else if (Val.IsInteger())
                {
                    Desc.EnumChoices.push_back(std::to_string(Val.AsInteger()));
                }
            }
        }
        return Desc;
    }
};

class FDefinitionReferenceAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "DefinitionReferenceAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::ReferencePicker; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::ReferencePicker;
        Desc.TargetReferenceKind = Spec.ExpectedStableIdKind;
        return Desc;
    }
};

class FResourceAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "ResourceAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::ResourcePicker; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::ResourcePicker;
        Desc.TargetResourceClass = Spec.ResourceClass;
        return Desc;
    }
};

class FTextIdAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "TextIdAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::TextIdPicker; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::TextIdPicker;
        return Desc;
    }
};

class FArrayAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "ArrayAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::ArrayEditor; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::ArrayEditor;
        Desc.bAllowsDirectTextEntry = false;
        return Desc;
    }
};

class FObjectAdapter final : public IGV2FieldAdapter
{
public:
    std::string GetAdapterName() const override { return "ObjectAdapter"; }
    EFieldControlType GetControlType() const override { return EFieldControlType::ObjectEditor; }

    FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const override
    {
        FFieldAdapterDescriptor Desc;
        Desc.AdapterName = GetAdapterName();
        Desc.SemanticKind = Spec.Kind;
        Desc.ControlType = EFieldControlType::ObjectEditor;
        Desc.bAllowsDirectTextEntry = false;
        return Desc;
    }
};

} // namespace

FGV2FieldAdapterRegistry& FGV2FieldAdapterRegistry::Get()
{
    static FGV2FieldAdapterRegistry Instance;
    return Instance;
}

FGV2FieldAdapterRegistry::FGV2FieldAdapterRegistry()
{
    RegisterDefaultAdapters();
}

void FGV2FieldAdapterRegistry::RegisterDefaultAdapters()
{
    Adapters.push_back(std::make_unique<FDefaultScalarAdapter>());
    Adapters.push_back(std::make_unique<FEnumAdapter>());
    Adapters.push_back(std::make_unique<FDefinitionReferenceAdapter>());
    Adapters.push_back(std::make_unique<FResourceAdapter>());
    Adapters.push_back(std::make_unique<FTextIdAdapter>());
    Adapters.push_back(std::make_unique<FArrayAdapter>());
    Adapters.push_back(std::make_unique<FObjectAdapter>());
}

const IGV2FieldAdapter* FGV2FieldAdapterRegistry::ResolveAdapter(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const GV2ContentCore::FFieldUiMetadata* /*UiMeta*/) const
{
    if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
    {
        if (Spec.Scalar->Kind == GV2ContentCore::EScalarFieldKind::Enum)
        {
            return Adapters[1].get(); // FEnumAdapter
        }
        return Adapters[0].get(); // FDefaultScalarAdapter
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Reference)
    {
        return Adapters[2].get(); // FDefinitionReferenceAdapter
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::ResourceReference)
    {
        return Adapters[3].get(); // FResourceAdapter
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::TextId)
    {
        return Adapters[4].get(); // FTextIdAdapter
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Array)
    {
        return Adapters[5].get(); // FArrayAdapter
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Object || Spec.Kind == GV2ContentCore::EFieldKind::Union)
    {
        return Adapters[6].get(); // FObjectAdapter
    }

    return Adapters[0].get();
}

FFieldAdapterDescriptor FGV2FieldAdapterRegistry::DescribeField(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const GV2ContentCore::FFieldUiMetadata* UiMeta) const
{
    const IGV2FieldAdapter* Adapter = ResolveAdapter(Spec, UiMeta);
    if (Adapter != nullptr)
    {
        return Adapter->Describe(Spec, UiMeta);
    }
    return {};
}

} // namespace GV2ContentEditor
