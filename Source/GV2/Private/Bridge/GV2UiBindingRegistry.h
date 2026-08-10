#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"

enum class EGV2BindingResolveResult : uint8
{
    Found,
    Invalid,
    Stale
};

class FGV2UiBindingRegistry
{
public:
    void BeginSession(int32 InSessionGeneration);
    void EndSession();

    bool PublishBindings(
        const FString& UiInstanceId,
        int64 Revision,
        const TArray<FGV2UiBindingDefinition>& Definitions,
        TArray<FGV2UiBindingHandle>& OutHandles);

    EGV2BindingResolveResult Resolve(
        const FGV2UiBindingHandle& Handle,
        FGV2UiBindingRecord& OutRecord) const;

    int32 Num() const;
    int32 GetSessionGeneration() const;
    const FString& GetUiInstanceId() const;
    int64 GetRevision() const;

private:
    static bool TryReadTransientGeneration(
        const FString& Value,
        const FString& Prefix,
        int32& OutGeneration);
    static bool ValidateDefinition(const FGV2UiBindingDefinition& Definition);
    static FString MakeNodePathKey(const TArray<FString>& NodeKeyPath);

    int32 SessionGeneration = 0;
    FString CurrentUiInstanceId;
    int64 CurrentRevision = 0;
    int64 NextHandleCounter = 1;
    TMap<FGV2UiBindingHandle, FGV2UiBindingRecord> Records;
};
