#pragma once

#include "GV2RuntimeCore/GV2HostServices.h"

#include <cstddef>
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
