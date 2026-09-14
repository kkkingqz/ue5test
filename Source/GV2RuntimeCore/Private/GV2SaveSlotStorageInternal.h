#pragma once

#include "GV2RuntimeCore/GV2HostServices.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GV2RuntimeCore::Internal
{

enum class EFilesystemOpKind
{
    Exists,
    IsRegularFile,
    ReadFile,
    WriteFile,
    Rename,
    Remove,
    ListDirectory,
};

struct FFilesystemOpRecord
{
    std::size_t Ordinal = 0;
    EFilesystemOpKind Kind = EFilesystemOpKind::ReadFile;
    std::string Description;
    std::filesystem::path Path1;
    std::filesystem::path Path2;
};

namespace Contract
{

struct FContractOpStep
{
    std::size_t Ordinal = 0;
    EFilesystemOpKind Kind = EFilesystemOpKind::Exists;
    std::string Description;
    bool bIsCommitPoint = false;
};

inline std::vector<FContractOpStep> GetFirstWriteContractStages()
{
    return {
        {1, EFilesystemOpKind::Exists, "check_head_exists", false},
        {2, EFilesystemOpKind::Exists, "check_legacy_exists", false},
        {3, EFilesystemOpKind::WriteFile, "write_new_temp_generation", false},
        {4, EFilesystemOpKind::Rename, "commit_new_generation", false},
        {5, EFilesystemOpKind::WriteFile, "write_temp_head", false},
        {6, EFilesystemOpKind::Rename, "commit_head", true},
        {7, EFilesystemOpKind::ListDirectory, "cleanup_list_root", false},
    };
}

inline std::vector<FContractOpStep> GetOverwriteContractStages()
{
    return {
        {1, EFilesystemOpKind::Exists, "check_head_exists", false},
        {2, EFilesystemOpKind::IsRegularFile, "check_head_is_regular", false},
        {3, EFilesystemOpKind::ReadFile, "write_read_head", false},
        {4, EFilesystemOpKind::WriteFile, "write_new_temp_generation", false},
        {5, EFilesystemOpKind::Rename, "commit_new_generation", false},
        {6, EFilesystemOpKind::WriteFile, "write_temp_head", false},
        {7, EFilesystemOpKind::Rename, "commit_head", true},
        {8, EFilesystemOpKind::ListDirectory, "cleanup_list_root", false},
    };
}

inline std::vector<FContractOpStep> GetLegacyMigrationContractStages()
{
    return {
        {1, EFilesystemOpKind::Exists, "check_head_exists", false},
        {2, EFilesystemOpKind::Exists, "check_legacy_exists", false},
        {3, EFilesystemOpKind::IsRegularFile, "check_legacy_is_regular", false},
        {4, EFilesystemOpKind::ReadFile, "read_legacy_for_migration", false},
        {5, EFilesystemOpKind::WriteFile, "write_legacy_temp_generation", false},
        {6, EFilesystemOpKind::Rename, "commit_legacy_generation", false},
        {7, EFilesystemOpKind::WriteFile, "write_new_temp_generation", false},
        {8, EFilesystemOpKind::Rename, "commit_new_generation", false},
        {9, EFilesystemOpKind::WriteFile, "write_temp_head", false},
        {10, EFilesystemOpKind::Rename, "commit_head", true},
        {11, EFilesystemOpKind::Remove, "cleanup_migrated_legacy_file", false},
        {12, EFilesystemOpKind::ListDirectory, "cleanup_list_root", false},
    };
}

inline std::size_t GetCommitOrdinal(const std::vector<FContractOpStep>& Stages)
{
    for (const auto& Step : Stages)
    {
        if (Step.bIsCommitPoint)
        {
            return Step.Ordinal;
        }
    }
    return 0;
}

} // namespace Contract

struct FSlotHead
{
    std::int32_t Version = 1;
    std::string CurrentGen;
    std::string PreviousGen;
};

ESaveSlotResult ParseHeadDocument(
    const std::string& SlotId,
    const std::string& Content,
    FSlotHead& OutHead);

std::string SerializeHeadDocument(const FSlotHead& Head);

class ISaveSlotFilesystem
{
public:
    virtual ~ISaveSlotFilesystem() = default;

    virtual bool Exists(const std::filesystem::path& Path, bool& bOutExists, const std::string& Desc) = 0;
    virtual bool IsRegularFile(const std::filesystem::path& Path, bool& bOutIsRegularFile, const std::string& Desc) = 0;
    virtual bool ReadFile(const std::filesystem::path& Path, std::string& OutBytes, const std::string& Desc) = 0;
    virtual bool WriteFile(const std::filesystem::path& Path, const std::string& Bytes, const std::string& Desc) = 0;
    virtual bool Rename(const std::filesystem::path& From, const std::filesystem::path& To, const std::string& Desc) = 0;
    virtual bool Remove(const std::filesystem::path& Path, const std::string& Desc) = 0;
    virtual bool ListDirectory(const std::filesystem::path& Path, std::vector<std::filesystem::path>& OutEntries, const std::string& Desc) = 0;
};

class FDefaultSaveSlotFilesystem final : public ISaveSlotFilesystem
{
public:
    bool Exists(const std::filesystem::path& Path, bool& bOutExists, const std::string& Desc) override;
    bool IsRegularFile(const std::filesystem::path& Path, bool& bOutIsRegularFile, const std::string& Desc) override;
    bool ReadFile(const std::filesystem::path& Path, std::string& OutBytes, const std::string& Desc) override;
    bool WriteFile(const std::filesystem::path& Path, const std::string& Bytes, const std::string& Desc) override;
    bool Rename(const std::filesystem::path& From, const std::filesystem::path& To, const std::string& Desc) override;
    bool Remove(const std::filesystem::path& Path, const std::string& Desc) override;
    bool ListDirectory(const std::filesystem::path& Path, std::vector<std::filesystem::path>& OutEntries, const std::string& Desc) override;
};

class FInstrumentedSaveSlotFilesystem final : public ISaveSlotFilesystem
{
public:
    explicit FInstrumentedSaveSlotFilesystem(std::shared_ptr<ISaveSlotFilesystem> InUnderlying = nullptr);

    std::size_t InjectedFailureOrdinal = 0;
    std::size_t CrashAtOrdinal = 0;
    std::function<void(std::size_t, const FFilesystemOpRecord&)> OnOpPreHook;

    std::vector<FFilesystemOpRecord> Trace;
    std::size_t NextOrdinal = 1;

    bool Exists(const std::filesystem::path& Path, bool& bOutExists, const std::string& Desc) override;
    bool IsRegularFile(const std::filesystem::path& Path, bool& bOutIsRegularFile, const std::string& Desc) override;
    bool ReadFile(const std::filesystem::path& Path, std::string& OutBytes, const std::string& Desc) override;
    bool WriteFile(const std::filesystem::path& Path, const std::string& Bytes, const std::string& Desc) override;
    bool Rename(const std::filesystem::path& From, const std::filesystem::path& To, const std::string& Desc) override;
    bool Remove(const std::filesystem::path& Path, const std::string& Desc) override;
    bool ListDirectory(const std::filesystem::path& Path, std::vector<std::filesystem::path>& OutEntries, const std::string& Desc) override;

    void Reset();

private:
    std::size_t BeginOp(EFilesystemOpKind Kind, const std::string& Desc, const std::filesystem::path& P1, const std::filesystem::path& P2 = {});
    void CheckPostOp(std::size_t Ord);

    std::shared_ptr<ISaveSlotFilesystem> Underlying;
};

} // namespace GV2RuntimeCore::Internal

namespace GV2RuntimeCore::Testing
{

class FGV2SaveSlotStorageTestAccess
{
public:
    static FSaveSlotStorageOpenResult OpenWithFilesystem(
        const std::filesystem::path& RootDir,
        std::shared_ptr<Internal::ISaveSlotFilesystem> Filesystem);
};

} // namespace GV2RuntimeCore::Testing
