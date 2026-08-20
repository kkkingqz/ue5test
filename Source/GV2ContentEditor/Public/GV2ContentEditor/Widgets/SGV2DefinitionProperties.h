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
DECLARE_DELEGATE_OneParam(FOnSaveCompleted, const FGV2EditorAuthoringResult& /*Result*/);

class GV2_CONTENT_EDITOR_API SGV2DefinitionProperties : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2DefinitionProperties) {}
        SLATE_EVENT(FOnFieldValueChanged, OnFieldValueChanged)
        SLATE_EVENT(FOnSaveCompleted, OnSaveCompleted)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void RefreshProperties();
    void FocusField(const FString& JsonPointer);

private:
    TSharedRef<SWidget> BuildToolbar();
    TSharedRef<SWidget> BuildCategorySection(const FGV2FormCategorySection& Category);
    TSharedRef<SWidget> BuildFieldRow(const FGV2FormFieldDescriptor& Field);
    TSharedRef<SWidget> CreateControlForField(const FGV2FormFieldDescriptor& Field);

    FReply HandleSaveClicked();
    FReply HandleRevertClicked();

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnFieldValueChanged OnFieldValueChanged;
    FOnSaveCompleted OnSaveCompleted;

    TSharedPtr<SScrollBox> ContentScrollBox;
    TMap<FString, TSharedPtr<SWidget>> FieldWidgets;
    TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> OwnedOptionLists;
    FString HighlightedPointer;
};

} // namespace GV2ContentEditor
#endif
