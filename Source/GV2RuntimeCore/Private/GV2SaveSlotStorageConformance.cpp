#include "GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h"

#include "GV2RuntimeCore/GV2HostServices.h"

#include <chrono>
#include <fstream>
#include <string>

namespace GV2RuntimeCore::Testing
{
namespace
{
std::filesystem::path MakeUniqueTempDir()
{
    const auto Now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path Dir = std::filesystem::temp_directory_path()
        / ("gv2_save_slot_storage_conformance_" + std::to_string(Now));
    std::error_code Ec;
    std::filesystem::create_directories(Dir, Ec);
    return Dir;
}

struct FScopedTempDir
{
    std::filesystem::path Dir = MakeUniqueTempDir();
    ~FScopedTempDir()
    {
        std::error_code Ec;
        std::filesystem::remove_all(Dir, Ec);
    }
};
}

std::string RunSaveSlotStorageConformance()
{
    FScopedTempDir TempDir;
    FFilesystemSaveSlotStorage Storage(TempDir.Dir);

    // 1. Reading a slot that was never written is NotFound, not an error.
    {
        const FSaveSlotReadResult Result = Storage.ReadSlot("missing_slot");
        if (Result.Result != ESaveSlotResult::NotFound)
        {
            return "save_slot_storage_conformance.missing_slot_not_reported_as_not_found";
        }
        if (!Result.Bytes.empty())
        {
            return "save_slot_storage_conformance.missing_slot_returned_bytes";
        }
    }

    // 2. Write/read roundtrip with arbitrary bytes, including embedded NUL.
    {
        const std::string Payload = std::string("abc\0def", 7);
        const FSaveSlotWriteResult WriteResult = Storage.WriteSlot("slot_one", Payload);
        if (WriteResult.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.roundtrip_write_failed";
        }
        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("slot_one");
        if (ReadResult.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.roundtrip_read_failed";
        }
        if (ReadResult.Bytes != Payload)
        {
            return "save_slot_storage_conformance.roundtrip_bytes_mismatch";
        }
    }

    // 3. A second write atomically replaces the first — the slot never
    //    observably holds a mix of old and new content.
    {
        const FSaveSlotWriteResult WriteResult = Storage.WriteSlot("slot_one", "replacement");
        if (WriteResult.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.replace_write_failed";
        }
        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("slot_one");
        if (ReadResult.Result != ESaveSlotResult::Ok || ReadResult.Bytes != "replacement")
        {
            return "save_slot_storage_conformance.replace_read_mismatch";
        }
    }

    // 4. A slot whose path is occupied by something other than a regular
    //    file (here: a directory) is Unreadable, not NotFound and not a
    //    crash.
    {
        std::error_code Ec;
        std::filesystem::create_directories(TempDir.Dir / "slot_two.save", Ec);
        if (Ec)
        {
            return "save_slot_storage_conformance.setup_directory_slot_failed";
        }
        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("slot_two");
        if (ReadResult.Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.non_regular_file_not_reported_as_unreadable";
        }
    }

    // 5. A write that fails before it can publish leaves the previously
    //    published slot content untouched and still readable. Forced here
    //    by pre-occupying the temp-file path the write would use with a
    //    directory, so opening it for write fails before any rename.
    {
        const FSaveSlotWriteResult BaselineWrite = Storage.WriteSlot("slot_three", "baseline");
        if (BaselineWrite.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.interrupted_write_baseline_failed";
        }

        std::error_code Ec;
        std::filesystem::create_directories(TempDir.Dir / "slot_three.save.tmp", Ec);
        if (Ec)
        {
            return "save_slot_storage_conformance.setup_interrupted_write_failed";
        }

        const FSaveSlotWriteResult FailedWrite = Storage.WriteSlot("slot_three", "corrupted_attempt");
        if (FailedWrite.Result != ESaveSlotResult::Failure)
        {
            return "save_slot_storage_conformance.interrupted_write_did_not_fail";
        }

        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("slot_three");
        if (ReadResult.Result != ESaveSlotResult::Ok || ReadResult.Bytes != "baseline")
        {
            return "save_slot_storage_conformance.interrupted_write_corrupted_previous_slot";
        }

        std::filesystem::remove_all(TempDir.Dir / "slot_three.save.tmp", Ec);
    }

    // 6. Slot ids outside the allowed grammar (path separators, traversal,
    //    empty, uppercase) are rejected rather than resolving to a path
    //    that could escape RootDir.
    {
        for (const std::string& InvalidId : {std::string("../escape"), std::string("a/b"), std::string(""), std::string("Slot")})
        {
            if (IsValidSaveSlotId(InvalidId))
            {
                return "save_slot_storage_conformance.invalid_slot_id_accepted_by_grammar: " + InvalidId;
            }
            const FSaveSlotWriteResult WriteResult = Storage.WriteSlot(InvalidId, "x");
            if (WriteResult.Result != ESaveSlotResult::Failure)
            {
                return "save_slot_storage_conformance.invalid_slot_id_write_not_rejected: " + InvalidId;
            }
            const FSaveSlotReadResult ReadResult = Storage.ReadSlot(InvalidId);
            if (ReadResult.Result == ESaveSlotResult::Ok)
            {
                return "save_slot_storage_conformance.invalid_slot_id_read_not_rejected: " + InvalidId;
            }
        }

        const std::filesystem::path EscapedPath = TempDir.Dir.parent_path() / "escape.save";
        std::error_code Ec;
        if (std::filesystem::exists(EscapedPath, Ec))
        {
            return "save_slot_storage_conformance.invalid_slot_id_escaped_root";
        }
    }

    return "";
}
}
