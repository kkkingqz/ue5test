#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/SchemaFormModel.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SScrollBox.h"

namespace GV2ContentEditor
{

DECLARE_DELEGATE(FOnFieldValueChanged);

class GV2_CONTENT_EDITOR_API SGV2DefinitionProperties : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2DefinitionProperties) {}
        SLATE_EVENT(FOnFieldValueChanged, OnFieldValueChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void RefreshProperties();

private:
    TSharedRef<SWidget> BuildCategorySection(const FGV2FormCategorySection& Category);
    TSharedRef<SWidget> BuildFieldRow(const FGV2FormFieldDescriptor& Field);
    TSharedRef<SWidget> CreateControlForField(const FGV2FormFieldDescriptor& Field);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnFieldValueChanged OnFieldValueChanged;

    TSharedPtr<SScrollBox> ContentScrollBox;
};

} // namespace GV2ContentEditor
#endif
