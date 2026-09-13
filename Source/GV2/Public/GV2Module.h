#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

struct GV2_API FGV2RuntimeModuleIdentity
{
    FString SourceRevision;
    FString SourceDiffHash;
    FString BuildFingerprint;

    FString ToJson() const;
    static FGV2RuntimeModuleIdentity FromJson(const FString& JsonString);
};

class GV2_API FGV2Module : public IModuleInterface
{
public:
    static FGV2Module& Get();

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    FGV2RuntimeModuleIdentity GetRuntimeIdentity() const;
    void WriteRuntimeIdentityArtifacts() const;
    static FString ComputeBuildFingerprint(const FString& BinariesDir);

private:
    mutable FCriticalSection IdentityLock;
    mutable bool bIdentityCached = false;
    mutable FGV2RuntimeModuleIdentity CachedIdentity;
};
