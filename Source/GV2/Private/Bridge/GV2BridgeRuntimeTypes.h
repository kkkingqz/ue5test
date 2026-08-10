#pragma once

#include "Bridge/GV2BridgeTypes.h"

struct FGV2UiInputFieldDefinition
{
    FName Name;
    EGV2UiControlValueType Type = EGV2UiControlValueType::Null;
    bool bRequired = false;
};

struct FGV2UiBindingDefinition
{
    TArray<FString> NodeKeyPath;
    FString ElementId;
    FString CommandId;
    TArray<FGV2UiControlValue> BoundArgs;
    TArray<FGV2UiInputFieldDefinition> InputFields;
    FString InputSchemaId;
};

struct FGV2UiBindingRecord
{
    int32 SessionGeneration = 0;
    FString UiInstanceId;
    int64 Revision = 0;
    TArray<FString> NodeKeyPath;
    FString ElementId;
    FString CommandId;
    TArray<FGV2UiControlValue> BoundArgs;
    TMap<FName, EGV2UiControlValueType> InputFieldTypes;
    TSet<FName> RequiredInputFields;
    FString InputSchemaId;
};

struct FGV2UiIngressItem
{
    FGV2UiBindingHandle BindingHandle;
    FGV2UiBindingRecord Binding;
    TArray<FGV2UiControlValue> InputValues;
    int64 Sequence = 0;
};
