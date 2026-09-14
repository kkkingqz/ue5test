#include "GV2Module.h"
#include "GV2BuildIdentity.gen.h"
#include "GV2ContentCore/CanonicalHash.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_LINUX
#include <dlfcn.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGV2, Log, All);

FString FGV2RuntimeModuleIdentity::ToJson() const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("source_revision"), SourceRevision);
    Root->SetStringField(TEXT("source_diff_hash"), SourceDiffHash);
    Root->SetStringField(TEXT("build_fingerprint"), BuildFingerprint);
    Root->SetStringField(TEXT("engine_version"), EngineVersion);

    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(Root, Writer);
    return OutputString;
}

FGV2RuntimeModuleIdentity FGV2RuntimeModuleIdentity::FromJson(const FString& JsonString)
{
    FGV2RuntimeModuleIdentity Identity;
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
    {
        Root->TryGetStringField(TEXT("source_revision"), Identity.SourceRevision);
        Root->TryGetStringField(TEXT("source_diff_hash"), Identity.SourceDiffHash);
        Root->TryGetStringField(TEXT("build_fingerprint"), Identity.BuildFingerprint);
        Root->TryGetStringField(TEXT("engine_version"), Identity.EngineVersion);
    }
    return Identity;
}

FGV2Module& FGV2Module::Get()
{
    return FModuleManager::LoadModuleChecked<FGV2Module>("GV2");
}

void FGV2Module::StartupModule()
{
    const FGV2RuntimeModuleIdentity Identity = GetRuntimeIdentity();
    UE_LOG(LogGV2, Display, TEXT("GV2_RUNTIME_IDENTITY:%s"), *Identity.ToJson());
    WriteRuntimeIdentityArtifacts();
}

void FGV2Module::ShutdownModule()
{
}

FString FGV2Module::ComputeBuildFingerprint(const FString& BinariesDir)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*BinariesDir))
    {
        return TEXT("missing_binaries");
    }

    TArray<FString> FoundFiles;
    PlatformFile.FindFiles(FoundFiles, *BinariesDir, TEXT(".so"));

    TArray<FString> ProjectSoFiles;
    for (const FString& FilePath : FoundFiles)
    {
        const FString Filename = FPaths::GetCleanFilename(FilePath);
        if (Filename.StartsWith(TEXT("libUnrealEditor-GV2")) && Filename.EndsWith(TEXT(".so")))
        {
            ProjectSoFiles.Add(FilePath);
        }
    }

    if (ProjectSoFiles.IsEmpty())
    {
        return TEXT("missing_binaries");
    }

    ProjectSoFiles.Sort();

    GV2ContentCore::FSha256Builder Builder;

    for (const FString& FilePath : ProjectSoFiles)
    {
        const FString Filename = FPaths::GetCleanFilename(FilePath);
        const int64 FileSize = PlatformFile.FileSize(*FilePath);
        if (FileSize < 0)
        {
            return FString::Printf(TEXT("error:%s:cannot_stat"), *Filename);
        }

        const std::string FilenameUtf8 = TCHAR_TO_UTF8(*Filename);
        Builder.Update(FilenameUtf8);
        Builder.Update(":");
        const std::string SizeStr = std::to_string(FileSize);
        Builder.Update(SizeStr);
        Builder.Update(":");

        TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*FilePath));
        if (!FileHandle)
        {
            return FString::Printf(TEXT("error:%s:cannot_open"), *Filename);
        }

        constexpr int64 ChunkSize = 65536;
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(ChunkSize);

        int64 BytesRemaining = FileSize;
        while (BytesRemaining > 0)
        {
            const int64 BytesToRead = FMath::Min(BytesRemaining, ChunkSize);
            if (!FileHandle->Read(Buffer.GetData(), BytesToRead))
            {
                return FString::Printf(TEXT("error:%s:read_failed"), *Filename);
            }
            Builder.Update(Buffer.GetData(), static_cast<std::size_t>(BytesToRead));
            BytesRemaining -= BytesToRead;
        }
    }

    return UTF8_TO_TCHAR(Builder.FinalizeHex().c_str());
}

FGV2RuntimeModuleIdentity FGV2Module::GetRuntimeIdentity() const
{
    FScopeLock Lock(&IdentityLock);
    if (bIdentityCached)
    {
        return CachedIdentity;
    }

    FGV2RuntimeModuleIdentity Identity;
    Identity.SourceRevision = TEXT(GV2_BUILD_SOURCE_REVISION);
    Identity.SourceDiffHash = TEXT(GV2_BUILD_SOURCE_DIFF_HASH);
    // Compiled into this module by the engine headers it was built against, so the
    // reported value follows the loaded binary rather than the environment running it.
    Identity.EngineVersion = FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);

    FString BinariesDir;
#if PLATFORM_LINUX
    Dl_info Info;
    if (dladdr((const void*)&FGV2Module::Get, &Info) && Info.dli_fname)
    {
        const FString LoadedPath = UTF8_TO_TCHAR(Info.dli_fname);
        BinariesDir = FPaths::GetPath(LoadedPath);
    }
#endif
    if (BinariesDir.IsEmpty() || !FPaths::DirectoryExists(BinariesDir))
    {
        BinariesDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Linux"));
    }

    Identity.BuildFingerprint = ComputeBuildFingerprint(BinariesDir);

    CachedIdentity = Identity;
    bIdentityCached = true;
    return Identity;
}

void FGV2Module::WriteRuntimeIdentityArtifacts() const
{
    const FGV2RuntimeModuleIdentity Identity = GetRuntimeIdentity();
    const FString JsonString = Identity.ToJson();

    const FString ReportArtifact = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("Reports"), TEXT("runtime_identity.json"));
    FFileHelper::SaveStringToFile(JsonString, *ReportArtifact, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    const FString LogArtifact = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("runtime_identity.json"));
    FFileHelper::SaveStringToFile(JsonString, *LogArtifact, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

IMPLEMENT_PRIMARY_GAME_MODULE(FGV2Module, GV2, "GV2");
