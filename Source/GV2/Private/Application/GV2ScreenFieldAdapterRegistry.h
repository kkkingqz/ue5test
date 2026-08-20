#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <string_view>

class FGV2ScreenFieldAdapterRegistry
{
public:
    static const FGV2ScreenFieldAdapterRegistry& Get();

    bool PrepareBindingDefinitions(
        const GV2RuntimeCore::FScreenRequest& Request,
        TArray<FGV2UiBindingDefinition>& OutDefinitions) const;

    bool BuildFields(
        const GV2RuntimeCore::FScreenRequest& Request,
        const TArray<FGV2UiBindingHandle>& Handles,
        TArray<FGV2ScreenFieldValue>& OutFields) const;

    int32 Num() const;

private:
    using FObject = GV2RuntimeCore::FValue::FObject;
    using FPrepareBindings = bool (*)(
        const std::string& ScreenId,
        const GV2RuntimeCore::FScreenField& Field,
        const FObject& Value,
        TArray<FGV2UiBindingDefinition>& OutDefinitions);
    using FBuildField = bool (*)(
        const GV2RuntimeCore::FScreenField& Field,
        const FObject& Value,
        const TArray<FGV2UiBindingHandle>& Handles,
        int32& InOutHandleIndex,
        FGV2ScreenFieldValue& OutField);

    struct FAdapter
    {
        std::string_view SchemaId;
        FPrepareBindings PrepareBindings = nullptr;
        FBuildField BuildField = nullptr;
    };

public:
    const FAdapter* Find(std::string_view SchemaId) const;

private:
    FGV2ScreenFieldAdapterRegistry();

    TArray<FAdapter> Adapters;
};
