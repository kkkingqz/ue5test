#include "GV2ContentEditor/SchemaFormModel.h"
#include <algorithm>
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
        for (const auto& Field : RootSpec->Fields)
        {
            if (Field.Spec == nullptr) continue;

            const auto* UiField = UiMetadata ? UiMetadata->FindField(Field.Name) : nullptr;

            FGV2FormFieldDescriptor Desc;
            Desc.FieldName = Field.Name;
            Desc.JsonPointer = "/data/" + Field.Name;
            Desc.bRequired = Field.bRequired;
            Desc.Spec = Field.Spec;

            if (UiField != nullptr && UiField->Label.has_value())
            {
                Desc.DisplayLabel = *UiField->Label;
            }
            else
            {
                Desc.DisplayLabel = FormatDefaultLabel(Field.Name);
            }

            if (UiField != nullptr && UiField->Description.has_value())
            {
                Desc.Description = *UiField->Description;
            }

            if (UiField != nullptr && UiField->Category.has_value())
            {
                Desc.Category = *UiField->Category;
            }
            else
            {
                Desc.Category = "General";
            }

            if (UiField != nullptr && UiField->Order.has_value())
            {
                Desc.Order = *UiField->Order;
            }
            else
            {
                Desc.Order = AutoOrder;
                AutoOrder += 10;
            }

            Desc.AdapterDescriptor = FGV2FieldAdapterRegistry::Get().DescribeField(*Field.Spec, UiField);
            Model.AllFields.push_back(std::move(Desc));
        }
    }

    // Process extension schemas
    for (const auto& Ext : ExtensionSchemas)
    {
        const auto& ExtRootSpec = Ext.GetCompiledRootSpec();
        if (ExtRootSpec != nullptr && ExtRootSpec->Kind == GV2ContentCore::EFieldKind::Object)
        {
            std::string ExtCategory = Ext.GetPackageId() + " Extension";
            std::int64_t ExtAutoOrder = 1000;

            for (const auto& Field : ExtRootSpec->Fields)
            {
                if (Field.Spec == nullptr) continue;

                FGV2FormFieldDescriptor Desc;
                Desc.FieldName = Field.Name;
                Desc.JsonPointer = "/extensions/" + Ext.GetPackageId() + "/" + Field.Name;
                Desc.bRequired = Field.bRequired;
                Desc.Spec = Field.Spec;
                Desc.DisplayLabel = FormatDefaultLabel(Field.Name);
                Desc.Category = ExtCategory;
                Desc.Order = ExtAutoOrder;
                ExtAutoOrder += 10;

                Desc.AdapterDescriptor = FGV2FieldAdapterRegistry::Get().DescribeField(*Field.Spec, nullptr);
                Model.AllFields.push_back(std::move(Desc));
            }
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
