#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2SaveSlotStorageInternal.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GV2RuntimeCore
{

bool IsValidSaveSlotId(const std::string& SlotId)
{
    if (SlotId.empty())
    {
        return false;
    }
    if (!(std::islower(static_cast<unsigned char>(SlotId.front()))))
    {
        return false;
    }
    for (const char Ch : SlotId)
    {
        const bool bLower = std::islower(static_cast<unsigned char>(Ch));
        const bool bDigit = std::isdigit(static_cast<unsigned char>(Ch));
        if (!bLower && !bDigit && Ch != '_')
        {
            return false;
        }
    }
    return true;
}

namespace Internal
{

bool FDefaultSaveSlotFilesystem::Exists(const std::filesystem::path& Path, bool& bOutExists, const std::string& Desc)
{
    (void)Desc;
    std::error_code Ec;
    bOutExists = std::filesystem::exists(Path, Ec);
    return !Ec;
}

bool FDefaultSaveSlotFilesystem::IsRegularFile(const std::filesystem::path& Path, bool& bOutIsRegularFile, const std::string& Desc)
{
    (void)Desc;
    std::error_code Ec;
    bOutIsRegularFile = std::filesystem::is_regular_file(Path, Ec);
    if (Ec)
    {
        if (Ec == std::errc::no_such_file_or_directory)
        {
            bOutIsRegularFile = false;
            return true;
        }
        bOutIsRegularFile = false;
        return false;
    }
    return true;
}

bool FDefaultSaveSlotFilesystem::ReadFile(const std::filesystem::path& Path, std::string& OutBytes, const std::string& Desc)
{
    (void)Desc;
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open())
    {
        return false;
    }
    std::ostringstream Buffer;
    Buffer << Stream.rdbuf();
    if (Stream.bad())
    {
        return false;
    }
    OutBytes = Buffer.str();
    return true;
}

bool FDefaultSaveSlotFilesystem::WriteFile(const std::filesystem::path& Path, const std::string& Bytes, const std::string& Desc)
{
    (void)Desc;
    std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
    if (!Stream.is_open())
    {
        return false;
    }
    Stream.write(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
    Stream.flush();
    return Stream.good() && !Stream.bad();
}

bool FDefaultSaveSlotFilesystem::Rename(const std::filesystem::path& From, const std::filesystem::path& To, const std::string& Desc)
{
    (void)Desc;
    std::error_code Ec;
    std::filesystem::rename(From, To, Ec);
    return !Ec;
}

bool FDefaultSaveSlotFilesystem::Remove(const std::filesystem::path& Path, const std::string& Desc)
{
    (void)Desc;
    std::error_code Ec;
    std::filesystem::remove(Path, Ec);
    return !Ec;
}

bool FDefaultSaveSlotFilesystem::ListDirectory(const std::filesystem::path& Path, std::vector<std::filesystem::path>& OutEntries, const std::string& Desc)
{
    (void)Desc;
    OutEntries.clear();
    std::error_code Ec;
    for (const auto& Entry : std::filesystem::directory_iterator(Path, Ec))
    {
        if (Ec)
        {
            return false;
        }
        OutEntries.push_back(Entry.path());
    }
    if (Ec)
    {
        return false;
    }
    std::sort(OutEntries.begin(), OutEntries.end());
    return true;
}

FInstrumentedSaveSlotFilesystem::FInstrumentedSaveSlotFilesystem(std::shared_ptr<ISaveSlotFilesystem> InUnderlying)
    : Underlying(InUnderlying ? std::move(InUnderlying) : std::make_shared<FDefaultSaveSlotFilesystem>())
{
}

void FInstrumentedSaveSlotFilesystem::Reset()
{
    Trace.clear();
    NextOrdinal = 1;
    InjectedFailureOrdinal = 0;
    CrashAtOrdinal = 0;
    OnOpPreHook = nullptr;
}

std::size_t FInstrumentedSaveSlotFilesystem::BeginOp(
    EFilesystemOpKind Kind,
    const std::string& Desc,
    const std::filesystem::path& P1,
    const std::filesystem::path& P2)
{
    const std::size_t Ord = NextOrdinal++;
    FFilesystemOpRecord Rec;
    Rec.Ordinal = Ord;
    Rec.Kind = Kind;
    Rec.Description = Desc;
    Rec.Path1 = P1;
    Rec.Path2 = P2;
    Trace.push_back(Rec);

    if (OnOpPreHook)
    {
        OnOpPreHook(Ord, Rec);
    }

    if (InjectedFailureOrdinal != 0 && InjectedFailureOrdinal == Ord)
    {
        return 0;
    }

    return Ord;
}

void FInstrumentedSaveSlotFilesystem::CheckPostOp(std::size_t Ord)
{
    if (CrashAtOrdinal != 0 && CrashAtOrdinal == Ord)
    {
        std::_Exit(42);
    }
}

bool FInstrumentedSaveSlotFilesystem::Exists(const std::filesystem::path& Path, bool& bOutExists, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::Exists, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->Exists(Path, bOutExists, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::IsRegularFile(const std::filesystem::path& Path, bool& bOutIsRegularFile, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::IsRegularFile, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->IsRegularFile(Path, bOutIsRegularFile, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::ReadFile(const std::filesystem::path& Path, std::string& OutBytes, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::ReadFile, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->ReadFile(Path, OutBytes, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::WriteFile(const std::filesystem::path& Path, const std::string& Bytes, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::WriteFile, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->WriteFile(Path, Bytes, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::Rename(const std::filesystem::path& From, const std::filesystem::path& To, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::Rename, Desc, From, To);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->Rename(From, To, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::Remove(const std::filesystem::path& Path, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::Remove, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->Remove(Path, Desc);
    CheckPostOp(Ord);
    return bOk;
}

bool FInstrumentedSaveSlotFilesystem::ListDirectory(const std::filesystem::path& Path, std::vector<std::filesystem::path>& OutEntries, const std::string& Desc)
{
    const std::size_t Ord = BeginOp(EFilesystemOpKind::ListDirectory, Desc, Path);
    if (Ord == 0)
    {
        return false;
    }
    const bool bOk = Underlying->ListDirectory(Path, OutEntries, Desc);
    CheckPostOp(Ord);
    return bOk;
}

} // namespace Internal

namespace
{

std::atomic<std::uint64_t> GUniqueTagCounter{1};

std::string MakeUniqueTag()
{
    const auto Now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto Ctr = GUniqueTagCounter.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(Now) + "_" + std::to_string(Ctr) + "_" + std::to_string(getpid());
}

bool IsValidGenerationFilename(const std::string& SlotId, const std::string& Filename)
{
    if (Filename.find('/') != std::string::npos || Filename.find('\\') != std::string::npos || Filename.find("..") != std::string::npos)
    {
        return false;
    }
    const std::string Prefix = SlotId + ".gen_";
    const std::string Suffix = ".save";
    if (Filename.size() <= Prefix.size() + Suffix.size())
    {
        return false;
    }
    if (Filename.rfind(Prefix, 0) != 0)
    {
        return false;
    }
    if (Filename.compare(Filename.size() - Suffix.size(), Suffix.size(), Suffix) != 0)
    {
        return false;
    }
    const std::string Middle = Filename.substr(Prefix.size(), Filename.size() - Prefix.size() - Suffix.size());
    if (Middle.empty())
    {
        return false;
    }
    for (const char Ch : Middle)
    {
        if (!std::isalnum(static_cast<unsigned char>(Ch)) && Ch != '_')
        {
            return false;
        }
    }
    return true;
}

} // namespace

namespace Internal
{

ESaveSlotResult ParseHeadDocument(
    const std::string& SlotId,
    const std::string& Content,
    FSlotHead& OutHead)
{
    GV2ContentCore::FParseLimits Limits;
    Limits.MaxFileSizeBytes = 8192;
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    auto Doc = GV2ContentCore::ParseJson5(Content, Limits, Diags);
    if (!Doc || !Doc->IsObject() || !Diags.empty())
    {
        return ESaveSlotResult::Unreadable;
    }

    // Strict bounded grammar: reject unknown fields
    for (const auto& [Key, Val] : Doc->AsObject())
    {
        if (Key != "version" && Key != "current" && Key != "previous")
        {
            return ESaveSlotResult::Unreadable;
        }
    }

    const auto* VersionVal = Doc->FindField("version");
    if (!VersionVal || !VersionVal->IsInteger() || VersionVal->AsInteger() != 1)
    {
        return ESaveSlotResult::Unreadable;
    }
    OutHead.Version = static_cast<std::int32_t>(VersionVal->AsInteger());

    const auto* CurrentVal = Doc->FindField("current");
    if (!CurrentVal || !CurrentVal->IsString() || !IsValidGenerationFilename(SlotId, CurrentVal->AsString()))
    {
        return ESaveSlotResult::Unreadable;
    }
    OutHead.CurrentGen = CurrentVal->AsString();

    const auto* PreviousVal = Doc->FindField("previous");
    if (PreviousVal != nullptr)
    {
        if (!PreviousVal->IsString() || !IsValidGenerationFilename(SlotId, PreviousVal->AsString()))
        {
            return ESaveSlotResult::Unreadable;
        }
        OutHead.PreviousGen = PreviousVal->AsString();
    }
    else
    {
        OutHead.PreviousGen.clear();
    }

    return ESaveSlotResult::Ok;
}

std::string SerializeHeadDocument(const FSlotHead& Head)
{
    std::string Out = "{\n";
    Out += "  \"version\": " + std::to_string(Head.Version) + ",\n";
    Out += "  \"current\": \"" + Head.CurrentGen + "\"";
    if (!Head.PreviousGen.empty())
    {
        Out += ",\n  \"previous\": \"" + Head.PreviousGen + "\"";
    }
    Out += "\n}\n";
    return Out;
}

} // namespace Internal

using Internal::FSlotHead;
using Internal::ParseHeadDocument;
using Internal::SerializeHeadDocument;

struct FFilesystemSaveSlotStorage::FImpl
{
    std::filesystem::path RootDir;
    int LockFd = -1;
    mutable std::mutex Mutex;
    std::shared_ptr<Internal::ISaveSlotFilesystem> Fs;

    FImpl(std::filesystem::path InRootDir, int InLockFd, std::shared_ptr<Internal::ISaveSlotFilesystem> InFs)
        : RootDir(std::move(InRootDir))
        , LockFd(InLockFd)
        , Fs(InFs ? std::move(InFs) : std::make_shared<Internal::FDefaultSaveSlotFilesystem>())
    {
    }

    ~FImpl()
    {
        if (LockFd >= 0)
        {
            struct flock Fl = {};
            Fl.l_type = F_UNLCK;
            Fl.l_whence = SEEK_SET;
            Fl.l_start = 0;
            Fl.l_len = 0;
            fcntl(LockFd, F_SETLK, &Fl);
            close(LockFd);
            LockFd = -1;
        }
    }

    void StartupCleanup()
    {
        bool bRootExists = false;
        if (!Fs->Exists(RootDir, bRootExists, "startup_cleanup_check_root") || !bRootExists)
        {
            return;
        }

        std::vector<std::filesystem::path> Entries;
        if (!Fs->ListDirectory(RootDir, Entries, "startup_cleanup_list_root"))
        {
            return;
        }

        // Clean up unreferenced files for slots with valid heads
        for (const auto& EntryPath : Entries)
        {
            bool bIsRegular = false;
            if (!Fs->IsRegularFile(EntryPath, bIsRegular, "startup_cleanup_check_file") || !bIsRegular)
            {
                continue;
            }
            const std::string Filename = EntryPath.filename().string();
            const std::string HeadSuffix = ".head";
            if (Filename.size() > HeadSuffix.size() &&
                Filename.compare(Filename.size() - HeadSuffix.size(), HeadSuffix.size(), HeadSuffix) == 0)
            {
                const std::string SlotId = Filename.substr(0, Filename.size() - HeadSuffix.size());
                if (!IsValidSaveSlotId(SlotId))
                {
                    continue;
                }

                std::string HeadBytes;
                if (!Fs->ReadFile(EntryPath, HeadBytes, "startup_cleanup_read_head"))
                {
                    continue;
                }
                FSlotHead Head;
                if (ParseHeadDocument(SlotId, HeadBytes, Head) != ESaveSlotResult::Ok)
                {
                    continue;
                }

                // Delete any unreferenced generation/temp files for this slot
                const std::string GenPrefix = SlotId + ".gen_";
                const std::string TmpPrefix = SlotId + ".tmp_";
                const std::string HeadTmpPrefix = SlotId + ".head.tmp";

                for (const auto& SubPath : Entries)
                {
                    const std::string SubName = SubPath.filename().string();
                    if (SubName.rfind(GenPrefix, 0) == 0)
                    {
                        if (SubName != Head.CurrentGen && SubName != Head.PreviousGen)
                        {
                            Fs->Remove(SubPath, "startup_cleanup_remove_unreferenced");
                        }
                    }
                    else if (SubName.rfind(TmpPrefix, 0) == 0 || SubName.rfind(HeadTmpPrefix, 0) == 0)
                    {
                        Fs->Remove(SubPath, "startup_cleanup_remove_temp");
                    }
                }
            }
        }
    }
};

FFilesystemSaveSlotStorage::FFilesystemSaveSlotStorage(std::unique_ptr<FImpl> InImpl)
    : Impl(std::move(InImpl))
{
}

FFilesystemSaveSlotStorage::~FFilesystemSaveSlotStorage() = default;

const std::filesystem::path& FFilesystemSaveSlotStorage::GetRootDir() const
{
    return Impl->RootDir;
}

FSaveSlotStorageOpenResult FFilesystemSaveSlotStorage::Open(const std::filesystem::path& InRootDir)
{
    return Testing::FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(InRootDir, nullptr);
}

FSaveSlotReadResult FFilesystemSaveSlotStorage::ReadSlot(
    const std::string& SlotId,
    ESaveSlotRevision Revision) const
{
    if (!IsValidSaveSlotId(SlotId))
    {
        return {ESaveSlotResult::Unreadable, {}};
    }

    std::lock_guard<std::mutex> Lock(Impl->Mutex);

    const std::filesystem::path HeadPath = Impl->RootDir / (SlotId + ".head");
    bool bHeadExists = false;
    if (!Impl->Fs->Exists(HeadPath, bHeadExists, "read_check_head_exists"))
    {
        return {ESaveSlotResult::Unreadable, {}};
    }

    if (bHeadExists)
    {
        bool bHeadIsRegular = false;
        if (!Impl->Fs->IsRegularFile(HeadPath, bHeadIsRegular, "read_check_head_is_regular") || !bHeadIsRegular)
        {
            return {ESaveSlotResult::Unreadable, {}};
        }

        std::string HeadBytes;
        if (!Impl->Fs->ReadFile(HeadPath, HeadBytes, "read_head"))
        {
            return {ESaveSlotResult::Unreadable, {}};
        }

        FSlotHead Head;
        if (ParseHeadDocument(SlotId, HeadBytes, Head) != ESaveSlotResult::Ok)
        {
            return {ESaveSlotResult::Unreadable, {}};
        }

        if (Revision == ESaveSlotRevision::Current)
        {
            const std::filesystem::path GenPath = Impl->RootDir / Head.CurrentGen;
            bool bGenExists = false;
            if (!Impl->Fs->Exists(GenPath, bGenExists, "read_check_current_gen_exists") || !bGenExists)
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            bool bGenIsRegular = false;
            if (!Impl->Fs->IsRegularFile(GenPath, bGenIsRegular, "read_check_current_gen_is_regular") || !bGenIsRegular)
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            std::string Payload;
            if (!Impl->Fs->ReadFile(GenPath, Payload, "read_current_generation"))
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            return {ESaveSlotResult::Ok, std::move(Payload)};
        }
        else // Revision == Previous
        {
            if (Head.PreviousGen.empty())
            {
                return {ESaveSlotResult::NotFound, {}};
            }
            const std::filesystem::path GenPath = Impl->RootDir / Head.PreviousGen;
            bool bGenExists = false;
            if (!Impl->Fs->Exists(GenPath, bGenExists, "read_check_previous_gen_exists") || !bGenExists)
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            bool bGenIsRegular = false;
            if (!Impl->Fs->IsRegularFile(GenPath, bGenIsRegular, "read_check_previous_gen_is_regular") || !bGenIsRegular)
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            std::string Payload;
            if (!Impl->Fs->ReadFile(GenPath, Payload, "read_previous_generation"))
            {
                return {ESaveSlotResult::Unreadable, {}};
            }
            return {ESaveSlotResult::Ok, std::move(Payload)};
        }
    }

    // No head: check legacy single-current slot
    const std::filesystem::path LegacyPath = Impl->RootDir / (SlotId + ".save");
    bool bLegacyExists = false;
    if (!Impl->Fs->Exists(LegacyPath, bLegacyExists, "read_check_legacy_exists"))
    {
        return {ESaveSlotResult::Unreadable, {}};
    }
    if (!bLegacyExists)
    {
        return {ESaveSlotResult::NotFound, {}};
    }
    bool bLegacyIsRegular = false;
    if (!Impl->Fs->IsRegularFile(LegacyPath, bLegacyIsRegular, "read_check_legacy_is_regular") || !bLegacyIsRegular)
    {
        return {ESaveSlotResult::Unreadable, {}};
    }

    if (Revision == ESaveSlotRevision::Current)
    {
        std::string Payload;
        if (!Impl->Fs->ReadFile(LegacyPath, Payload, "read_legacy_slot"))
        {
            return {ESaveSlotResult::Unreadable, {}};
        }
        return {ESaveSlotResult::Ok, std::move(Payload)};
    }
    else
    {
        // Legacy single-current slot has no previous generation
        return {ESaveSlotResult::NotFound, {}};
    }
}

FSaveSlotWriteResult FFilesystemSaveSlotStorage::WriteSlot(
    const std::string& SlotId,
    const std::string& Bytes)
{
    if (!IsValidSaveSlotId(SlotId))
    {
        return {ESaveSlotResult::Failure};
    }

    std::lock_guard<std::mutex> Lock(Impl->Mutex);

    std::error_code Ec;
    std::filesystem::create_directories(Impl->RootDir, Ec);

    const std::filesystem::path HeadPath = Impl->RootDir / (SlotId + ".head");
    const std::filesystem::path LegacyPath = Impl->RootDir / (SlotId + ".save");

    std::string OldCurrentGenName;
    bool bLegacyMigration = false;
    std::string LegacyBytes;

    bool bHeadExists = false;
    if (!Impl->Fs->Exists(HeadPath, bHeadExists, "check_head_exists"))
    {
        return {ESaveSlotResult::Failure};
    }

    if (bHeadExists)
    {
        bool bHeadIsRegular = false;
        if (!Impl->Fs->IsRegularFile(HeadPath, bHeadIsRegular, "check_head_is_regular") || !bHeadIsRegular)
        {
            return {ESaveSlotResult::Failure};
        }
        std::string HeadBytes;
        if (!Impl->Fs->ReadFile(HeadPath, HeadBytes, "write_read_head"))
        {
            return {ESaveSlotResult::Failure};
        }
        FSlotHead Head;
        if (ParseHeadDocument(SlotId, HeadBytes, Head) != ESaveSlotResult::Ok)
        {
            return {ESaveSlotResult::Failure};
        }
        OldCurrentGenName = Head.CurrentGen;
    }
    else
    {
        bool bLegacyExists = false;
        if (!Impl->Fs->Exists(LegacyPath, bLegacyExists, "check_legacy_exists"))
        {
            return {ESaveSlotResult::Failure};
        }
        if (bLegacyExists)
        {
            bool bLegacyIsRegular = false;
            if (!Impl->Fs->IsRegularFile(LegacyPath, bLegacyIsRegular, "check_legacy_is_regular") || !bLegacyIsRegular)
            {
                return {ESaveSlotResult::Failure};
            }
            if (!Impl->Fs->ReadFile(LegacyPath, LegacyBytes, "read_legacy_for_migration"))
            {
                return {ESaveSlotResult::Failure};
            }
            bLegacyMigration = true;
        }
    }

    const std::string Tag = MakeUniqueTag();

    // 1. If legacy migration: copy legacy bytes to an immutable generation
    std::string LegacyGenName;
    std::filesystem::path LegacyGenPath;
    if (bLegacyMigration)
    {
        const std::string LegacyTmpName = SlotId + ".tmp_leg_" + Tag + ".save";
        LegacyGenName = SlotId + ".gen_leg_" + Tag + ".save";
        const std::filesystem::path LegacyTmpPath = Impl->RootDir / LegacyTmpName;
        LegacyGenPath = Impl->RootDir / LegacyGenName;

        if (!Impl->Fs->WriteFile(LegacyTmpPath, LegacyBytes, "write_legacy_temp_generation"))
        {
            Impl->Fs->Remove(LegacyTmpPath, "cleanup_failed_legacy_tmp");
            return {ESaveSlotResult::Failure};
        }
        if (!Impl->Fs->Rename(LegacyTmpPath, LegacyGenPath, "commit_legacy_generation"))
        {
            Impl->Fs->Remove(LegacyTmpPath, "cleanup_failed_legacy_tmp");
            return {ESaveSlotResult::Failure};
        }
        OldCurrentGenName = LegacyGenName;
    }

    // 2. Write new payload to unique temp generation
    const std::string NewTmpName = SlotId + ".tmp_new_" + Tag + ".save";
    const std::string NewGenName = SlotId + ".gen_" + Tag + ".save";
    const std::filesystem::path NewTmpPath = Impl->RootDir / NewTmpName;
    const std::filesystem::path NewGenPath = Impl->RootDir / NewGenName;

    if (!Impl->Fs->WriteFile(NewTmpPath, Bytes, "write_new_temp_generation"))
    {
        Impl->Fs->Remove(NewTmpPath, "cleanup_failed_new_tmp");
        if (bLegacyMigration)
        {
            Impl->Fs->Remove(LegacyGenPath, "cleanup_legacy_gen_on_failure");
        }
        return {ESaveSlotResult::Failure};
    }

    // 3. Rename temp generation to immutable generation
    if (!Impl->Fs->Rename(NewTmpPath, NewGenPath, "commit_new_generation"))
    {
        Impl->Fs->Remove(NewTmpPath, "cleanup_failed_new_tmp");
        if (bLegacyMigration)
        {
            Impl->Fs->Remove(LegacyGenPath, "cleanup_legacy_gen_on_failure");
        }
        return {ESaveSlotResult::Failure};
    }

    // 4. Write temp head (new_generation, old_current)
    FSlotHead NewHead;
    NewHead.Version = 1;
    NewHead.CurrentGen = NewGenName;
    NewHead.PreviousGen = OldCurrentGenName;
    const std::string NewHeadContent = SerializeHeadDocument(NewHead);

    const std::string TempHeadName = SlotId + ".head.tmp_" + Tag;
    const std::filesystem::path TempHeadPath = Impl->RootDir / TempHeadName;

    if (!Impl->Fs->WriteFile(TempHeadPath, NewHeadContent, "write_temp_head"))
    {
        Impl->Fs->Remove(TempHeadPath, "cleanup_failed_temp_head");
        Impl->Fs->Remove(NewGenPath, "cleanup_new_gen_on_failure");
        if (bLegacyMigration)
        {
            Impl->Fs->Remove(LegacyGenPath, "cleanup_legacy_gen_on_failure");
        }
        return {ESaveSlotResult::Failure};
    }

    // 5. ATOMIC COMMIT POINT: rename temp head to published head
    if (!Impl->Fs->Rename(TempHeadPath, HeadPath, "commit_head"))
    {
        Impl->Fs->Remove(TempHeadPath, "cleanup_failed_temp_head");
        Impl->Fs->Remove(NewGenPath, "cleanup_new_gen_on_failure");
        if (bLegacyMigration)
        {
            Impl->Fs->Remove(LegacyGenPath, "cleanup_legacy_gen_on_failure");
        }
        return {ESaveSlotResult::Failure};
    }

    // 6. Post-commit cleanup: remove legacy file and obsolete generation files
    if (bLegacyMigration)
    {
        Impl->Fs->Remove(LegacyPath, "cleanup_migrated_legacy_file");
    }

    // Cleanup obsolete generations for this slot
    const std::string GenPrefix = SlotId + ".gen_";
    const std::string TmpPrefix = SlotId + ".tmp_";
    const std::string HeadTmpPrefix = SlotId + ".head.tmp";

    std::vector<std::filesystem::path> CleanupEntries;
    if (Impl->Fs->ListDirectory(Impl->RootDir, CleanupEntries, "cleanup_list_root"))
    {
        for (const auto& SubPath : CleanupEntries)
        {
            const std::string SubName = SubPath.filename().string();
            if (SubName.rfind(GenPrefix, 0) == 0)
            {
                if (SubName != NewGenName && SubName != OldCurrentGenName)
                {
                    Impl->Fs->Remove(SubPath, "cleanup_obsolete_generation");
                }
            }
            else if (SubName.rfind(TmpPrefix, 0) == 0 || SubName.rfind(HeadTmpPrefix, 0) == 0)
            {
                Impl->Fs->Remove(SubPath, "cleanup_leftover_temp");
            }
        }
    }

    return {ESaveSlotResult::Ok};
}

namespace Testing
{

FSaveSlotStorageOpenResult FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(
    const std::filesystem::path& RootDir,
    std::shared_ptr<Internal::ISaveSlotFilesystem> Filesystem)
{
    std::error_code Ec;
    std::filesystem::create_directories(RootDir, Ec);
    if (Ec)
    {
        return {ESaveSlotResult::Failure, nullptr};
    }

    const std::filesystem::path LockPath = RootDir / ".storage.lock";
    int LockFd = open(LockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (LockFd < 0)
    {
        return {ESaveSlotResult::Failure, nullptr};
    }

    if (flock(LockFd, LOCK_EX | LOCK_NB) < 0)
    {
        close(LockFd);
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)
        {
            return {ESaveSlotResult::Busy, nullptr};
        }
        return {ESaveSlotResult::Failure, nullptr};
    }

    auto Impl = std::make_unique<FFilesystemSaveSlotStorage::FImpl>(RootDir, LockFd, std::move(Filesystem));
    Impl->StartupCleanup();

    return {ESaveSlotResult::Ok, std::unique_ptr<FFilesystemSaveSlotStorage>(new FFilesystemSaveSlotStorage(std::move(Impl)))};
}

} // namespace Testing

} // namespace GV2RuntimeCore
