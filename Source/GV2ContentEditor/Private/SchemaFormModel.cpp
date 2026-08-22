#include "GV2ContentEditor/SchemaFormModel.h"
#include <algorithm>
#include <functional>
#include <limits>

namespace GV2ContentEditor
{

namespace
{

std::string FormatDefaultLabel(const std::string& Name)
{
    if (Name.empty()) return "";
    std::string Out;
    bool bNewWord = true;
    for (char c : Name)
    {
        if (c == '_' || c == '-')
        {
            Out += ' ';
            bNewWord = true;
        }
        else if (bNewWord)
        {
            Out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            bNewWord = false;
        }
        else
        {
            Out += c;
        }
    }
    return Out;
}

void AppendFields(
    FGV2SchemaFormModel& Model,
    const std::vector<GV2ContentCore::FCompiledObjectField>& Fields,
    const std::string& PointerPrefix,
    const std::string& LabelPrefix,
    const std::string& DotPathPrefix,
    const std::string& DefaultCategory,
    std::int64_t& AutoOrder,
    const GV2ContentCore::FSchemaUiMetadata* UiMetadata)
{
    for (const auto& Field : Fields)
    {
        if (Field.Spec == nullptr) continue;

        const std::string DotPath = DotPathPrefix.empty() ? Field.Name : (DotPathPrefix + "." + Field.Name);
        const std::string DataDotPath = "data." + DotPath;

        const GV2ContentCore::FFieldUiMetadata* UiField = nullptr;
        if (UiMetadata != nullptr)
        {
            UiField = UiMetadata->FindField(DotPath);
            if (!UiField) UiField = UiMetadata->FindField(DataDotPath);
            if (!UiField) UiField = UiMetadata->FindField(Field.Name);
        }

        const std::string FieldPointer = PointerPrefix + "/" + Field.Name;
        const std::string FieldLabel = LabelPrefix + FormatDefaultLabel(Field.Name);
        const std::string Category = (UiField && UiField->Category.has_value()) ? *UiField->Category : DefaultCategory;

        FGV2FormFieldDescriptor Desc;
        Desc.FieldName = Field.Name;
        Desc.JsonPointer = FieldPointer;
        Desc.bRequired = Field.bRequired;
        Desc.Spec = Field.Spec;
        Desc.DisplayLabel = (UiField && UiField->Label.has_value()) ? *UiField->Label : FieldLabel;
        if (UiField && UiField->Description.has_value()) Desc.Description = *UiField->Description;
        Desc.Category = Category;
        Desc.Order = (UiField && UiField->Order.has_value()) ? *UiField->Order : AutoOrder;
        if (!(UiField && UiField->Order.has_value())) AutoOrder += 10;
        Desc.DefaultValue = Field.Spec->DefaultValue;
        Desc.AdapterDescriptor = FGV2FieldAdapterRegistry::Get().DescribeField(*Field.Spec, UiField);

        Model.AllFields.push_back(std::move(Desc));

        if (Field.Spec->Kind == GV2ContentCore::EFieldKind::Object)
        {
            AppendFields(
                Model, Field.Spec->Fields, FieldPointer, FieldLabel + " / ",
                DotPath, Category, AutoOrder, UiMetadata);
        }
    }
}

} // namespace

FGV2SchemaFormModel FGV2SchemaFormModel::BuildFromSchema(
    const GV2ContentCore::FSchemaResource& Schema,
    const GV2ContentCore::FSchemaUiMetadata* UiMetadata,
    const std::vector<GV2ContentCore::FExtensionSchemaResource>& ExtensionSchemas)
{
    FGV2SchemaFormModel Model;
    Model.DefinitionType = Schema.GetKey().DefinitionType;
    Model.SchemaId = Schema.GetSchemaId();

    const auto& RootSpec = Schema.GetCompiledRootSpec();
    if (RootSpec != nullptr && RootSpec->Kind == GV2ContentCore::EFieldKind::Object)
    {
        std::int64_t AutoOrder = 100;
        AppendFields(Model, RootSpec->Fields, "/data", "", "", "General", AutoOrder, UiMetadata);
    }

    // Process extension schemas
    for (const auto& Ext : ExtensionSchemas)
    {
        const auto& ExtRootSpec = Ext.GetCompiledRootSpec();
        if (ExtRootSpec != nullptr && ExtRootSpec->Kind == GV2ContentCore::EFieldKind::Object)
        {
            std::string ExtCategory = Ext.GetPackageId() + " Extension";
            std::int64_t ExtAutoOrder = 1000;

            AppendFields(
                Model, ExtRootSpec->Fields,
                "/extensions/" + Ext.GetKey().ExtensionNamespace,
                "", "extensions." + Ext.GetKey().ExtensionNamespace,
                ExtCategory, ExtAutoOrder, nullptr);
        }
    }

    // Sort all fields by order
    std::stable_sort(Model.AllFields.begin(), Model.AllFields.end(), [](const FGV2FormFieldDescriptor& A, const FGV2FormFieldDescriptor& B) {
        return A.Order < B.Order;
    });

    // Group fields into categories
    std::map<std::string, std::size_t> CategoryIndices;
    for (const auto& Field : Model.AllFields)
    {
        auto It = CategoryIndices.find(Field.Category);
        if (It == CategoryIndices.end())
        {
            CategoryIndices[Field.Category] = Model.Categories.size();
            FGV2FormCategorySection Section;
            Section.CategoryName = Field.Category;
            Section.Fields.push_back(Field);
            Model.Categories.push_back(std::move(Section));
        }
        else
        {
            Model.Categories[It->second].Fields.push_back(Field);
        }
    }

    return Model;
}

} // namespace GV2ContentEditor
