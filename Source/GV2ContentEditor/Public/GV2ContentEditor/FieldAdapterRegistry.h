#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Value.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace GV2ContentEditor
{

enum class EFieldControlType : std::uint8_t
{
    Text,
    MultilineText,
    IntegerNumeric,
    DoubleNumeric,
    Slider,
    Checkbox,
    EnumDropdown,
    ReferencePicker,
    ResourcePicker,
    TextIdPicker,
    ArrayEditor,
    ObjectEditor,
    ReadOnlyViewer
};

struct GV2_CONTENT_EDITOR_API FFieldAdapterDescriptor final
{
    std::string AdapterName;
    EFieldControlType ControlType = EFieldControlType::Text;
    bool bAllowsDirectTextEntry = true;
    std::vector<std::string> EnumChoices;
    std::string TargetReferenceKind;
    std::string TargetResourceClass;
    std::optional<double> MinValue;
    std::optional<double> MaxValue;
};

class GV2_CONTENT_EDITOR_API IGV2FieldAdapter
{
public:
    virtual ~IGV2FieldAdapter() = default;

    virtual std::string GetAdapterName() const = 0;
    virtual EFieldControlType GetControlType() const = 0;
    virtual FFieldAdapterDescriptor Describe(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* UiMeta) const = 0;
};

class GV2_CONTENT_EDITOR_API FGV2FieldAdapterRegistry final
{
public:
    static FGV2FieldAdapterRegistry& Get();

    FGV2FieldAdapterRegistry();
    ~FGV2FieldAdapterRegistry() = default;

    // Non-copyable
    FGV2FieldAdapterRegistry(const FGV2FieldAdapterRegistry&) = delete;
    FGV2FieldAdapterRegistry& operator=(const FGV2FieldAdapterRegistry&) = delete;

    const IGV2FieldAdapter* ResolveAdapter(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* UiMeta = nullptr) const;

    FFieldAdapterDescriptor DescribeField(
        const GV2ContentCore::FCompiledFieldSpec& Spec,
        const GV2ContentCore::FFieldUiMetadata* UiMeta = nullptr) const;

private:
    void RegisterDefaultAdapters();

    std::vector<std::unique_ptr<IGV2FieldAdapter>> Adapters;
};

} // namespace GV2ContentEditor
