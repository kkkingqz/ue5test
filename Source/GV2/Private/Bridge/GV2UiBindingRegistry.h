#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"

enum class EGV2BindingResolveResult : uint8
{
    Found,
    Invalid,
    Stale
};

struct FGV2PreparedBindingSet
{
    FString UiInstanceId;
    int64 Revision = 0;
    int64 NextHandleCounter = 0;
    TMap<FGV2UiBindingHandle, FGV2UiBindingRecord> Records;
    TArray<FGV2UiBindingHandle> Handles;
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

    bool PrepareBindings(
        const FString& UiInstanceId,
        int64 Revision,
        const TArray<FGV2UiBindingDefinition>& Definitions,
        FGV2PreparedBindingSet& OutCandidate) const;
    bool CommitPreparedBindings(FGV2PreparedBindingSet&& Candidate);

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
