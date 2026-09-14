#include "GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h"

#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2SaveSlotStorageInternal.h"

#include <chrono>
#include <fstream>
#include <memory>
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
    auto StorageOpen = FFilesystemSaveSlotStorage::Open(TempDir.Dir);
    if (StorageOpen.Result != ESaveSlotResult::Ok || !StorageOpen.Storage)
    {
        return "save_slot_storage_conformance.open_failed";
    }
    auto& Storage = *StorageOpen.Storage;

    // 1. Reading a slot that was never written is NotFound for both Current and Previous.
    {
        const FSaveSlotReadResult CurResult = Storage.ReadSlot("missing_slot", ESaveSlotRevision::Current);
        if (CurResult.Result != ESaveSlotResult::NotFound)
        {
            return "save_slot_storage_conformance.missing_slot_current_not_not_found";
        }
        if (!CurResult.Bytes.empty())
        {
            return "save_slot_storage_conformance.missing_slot_current_returned_bytes";
        }

        const FSaveSlotReadResult PrevResult = Storage.ReadSlot("missing_slot", ESaveSlotRevision::Previous);
        if (PrevResult.Result != ESaveSlotResult::NotFound)
        {
            return "save_slot_storage_conformance.missing_slot_previous_not_not_found";
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
        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("slot_one", ESaveSlotRevision::Current);
        if (ReadResult.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.roundtrip_read_failed";
        }
        if (ReadResult.Bytes != Payload)
        {
            return "save_slot_storage_conformance.roundtrip_bytes_mismatch";
        }
        // First write has no Previous: must return NotFound
        const FSaveSlotReadResult PrevReadResult = Storage.ReadSlot("slot_one", ESaveSlotRevision::Previous);
        if (PrevReadResult.Result != ESaveSlotResult::NotFound)
        {
            return "save_slot_storage_conformance.first_write_previous_not_not_found";
        }
    }

    // 3. Sequential writes maintain Current and Previous committed generations.
    {
        // Second write (overwrite of slot_one)
        const std::string PayloadB = "replacement_b";
        const FSaveSlotWriteResult WriteResultB = Storage.WriteSlot("slot_one", PayloadB);
        if (WriteResultB.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.sequential_write_b_failed";
        }
        const FSaveSlotReadResult ReadCurB = Storage.ReadSlot("slot_one", ESaveSlotRevision::Current);
        if (ReadCurB.Result != ESaveSlotResult::Ok || ReadCurB.Bytes != PayloadB)
        {
            return "save_slot_storage_conformance.sequential_read_current_b_mismatch";
        }
        const FSaveSlotReadResult ReadPrevB = Storage.ReadSlot("slot_one", ESaveSlotRevision::Previous);
        if (ReadPrevB.Result != ESaveSlotResult::Ok || ReadPrevB.Bytes != std::string("abc\0def", 7))
        {
            return "save_slot_storage_conformance.sequential_read_previous_b_mismatch";
        }

        // Third write (overwrite of slot_one)
        const std::string PayloadC = "replacement_c";
        const FSaveSlotWriteResult WriteResultC = Storage.WriteSlot("slot_one", PayloadC);
        if (WriteResultC.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.sequential_write_c_failed";
        }
        const FSaveSlotReadResult ReadCurC = Storage.ReadSlot("slot_one", ESaveSlotRevision::Current);
        if (ReadCurC.Result != ESaveSlotResult::Ok || ReadCurC.Bytes != PayloadC)
        {
            return "save_slot_storage_conformance.sequential_read_current_c_mismatch";
        }
        const FSaveSlotReadResult ReadPrevC = Storage.ReadSlot("slot_one", ESaveSlotRevision::Previous);
        if (ReadPrevC.Result != ESaveSlotResult::Ok || ReadPrevC.Bytes != PayloadB)
        {
            return "save_slot_storage_conformance.sequential_read_previous_c_mismatch";
        }
    }

    // 4. Legacy migration: single-current slot without head is read as Current;
    //    first overwrite migrates legacy bytes to Previous and writes new Current.
    {
        const std::string LegacySlotId = "legacy_slot";
        const std::string LegacyPayload = "legacy_save_content";
        {
            std::ofstream LegacyFile(TempDir.Dir / (LegacySlotId + ".save"), std::ios::binary);
            LegacyFile.write(LegacyPayload.data(), static_cast<std::streamsize>(LegacyPayload.size()));
        }

        // Before overwrite: Current = LegacyPayload, Previous = NotFound
        const FSaveSlotReadResult ReadCurLegacy = Storage.ReadSlot(LegacySlotId, ESaveSlotRevision::Current);
        if (ReadCurLegacy.Result != ESaveSlotResult::Ok || ReadCurLegacy.Bytes != LegacyPayload)
        {
            return "save_slot_storage_conformance.legacy_read_current_mismatch";
        }
        const FSaveSlotReadResult ReadPrevLegacy = Storage.ReadSlot(LegacySlotId, ESaveSlotRevision::Previous);
        if (ReadPrevLegacy.Result != ESaveSlotResult::NotFound)
        {
            return "save_slot_storage_conformance.legacy_read_previous_not_not_found";
        }

        // Overwrite
        const std::string OverwritePayload = "new_overwritten_content";
        const FSaveSlotWriteResult OverwriteRes = Storage.WriteSlot(LegacySlotId, OverwritePayload);
        if (OverwriteRes.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.legacy_overwrite_failed";
        }

        const FSaveSlotReadResult ReadPostCur = Storage.ReadSlot(LegacySlotId, ESaveSlotRevision::Current);
        if (ReadPostCur.Result != ESaveSlotResult::Ok || ReadPostCur.Bytes != OverwritePayload)
        {
            return "save_slot_storage_conformance.legacy_post_overwrite_current_mismatch";
        }
        const FSaveSlotReadResult ReadPostPrev = Storage.ReadSlot(LegacySlotId, ESaveSlotRevision::Previous);
        if (ReadPostPrev.Result != ESaveSlotResult::Ok || ReadPostPrev.Bytes != LegacyPayload)
        {
            return "save_slot_storage_conformance.legacy_post_overwrite_previous_mismatch";
        }
    }

    // 5. Malformed head rejection: invalid JSON, unknown versions, unknown fields,
    //    traversal paths, or wrong slot prefix result in Unreadable.
    {
        auto WriteRawHead = [&](const std::string& SlotId, const std::string& HeadContent) {
            std::ofstream F(TempDir.Dir / (SlotId + ".head"), std::ios::binary);
            F.write(HeadContent.data(), static_cast<std::streamsize>(HeadContent.size()));
        };

        // 5a. Corrupt JSON
        WriteRawHead("malformed_json_slot", "{ unclosed json");
        if (Storage.ReadSlot("malformed_json_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.malformed_json_head_not_unreadable";
        }

        // 5b. Unsupported version
        WriteRawHead("bad_ver_slot", "{\"version\": 99, \"current\": \"bad_ver_slot.gen_1.save\"}");
        if (Storage.ReadSlot("bad_ver_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.unsupported_version_head_not_unreadable";
        }

        // 5c. Unknown field
        WriteRawHead("unknown_field_slot", "{\"version\": 1, \"current\": \"unknown_field_slot.gen_1.save\", \"unexpected\": 123}");
        if (Storage.ReadSlot("unknown_field_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.unknown_field_head_not_unreadable";
        }

        // 5d. Path traversal in current
        WriteRawHead("traversal_slot", "{\"version\": 1, \"current\": \"../traversal_slot.gen_1.save\"}");
        if (Storage.ReadSlot("traversal_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.traversal_head_not_unreadable";
        }

        // 5e. Wrong slot prefix in current
        WriteRawHead("wrong_prefix_slot", "{\"version\": 1, \"current\": \"other_slot.gen_1.save\"}");
        if (Storage.ReadSlot("wrong_prefix_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.wrong_prefix_head_not_unreadable";
        }

        // 5f. Generation file missing on disk
        WriteRawHead("missing_gen_slot", "{\"version\": 1, \"current\": \"missing_gen_slot.gen_9999.save\"}");
        if (Storage.ReadSlot("missing_gen_slot").Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.missing_gen_file_head_not_unreadable";
        }
    }

    // 6. A slot whose path is occupied by something other than a regular file is Unreadable.
    {
        std::error_code Ec;
        std::filesystem::create_directories(TempDir.Dir / "dir_slot.save", Ec);
        if (Ec)
        {
            return "save_slot_storage_conformance.setup_directory_slot_failed";
        }
        const FSaveSlotReadResult ReadResult = Storage.ReadSlot("dir_slot");
        if (ReadResult.Result != ESaveSlotResult::Unreadable)
        {
            return "save_slot_storage_conformance.directory_slot_not_reported_as_unreadable";
        }
    }

    // 7. Process locking: second Open on same RootDir returns Busy.
    {
        auto SecondOpen = FFilesystemSaveSlotStorage::Open(TempDir.Dir);
        if (SecondOpen.Result != ESaveSlotResult::Busy || SecondOpen.Storage != nullptr)
        {
            return "save_slot_storage_conformance.concurrent_open_not_reported_as_busy";
        }

        // Close storage and reopen
        StorageOpen.Storage.reset();
        auto ThirdOpen = FFilesystemSaveSlotStorage::Open(TempDir.Dir);
        if (ThirdOpen.Result != ESaveSlotResult::Ok || ThirdOpen.Storage == nullptr)
        {
            return "save_slot_storage_conformance.reopen_after_close_failed";
        }
        StorageOpen = std::move(ThirdOpen);
    }

    // 8. Slot ID grammar validation.
    {
        for (const std::string& InvalidId : {std::string("../escape"), std::string("a/b"), std::string(""), std::string("Slot")})
        {
            if (IsValidSaveSlotId(InvalidId))
            {
                return "save_slot_storage_conformance.invalid_slot_id_accepted_by_grammar: " + InvalidId;
            }
            const FSaveSlotWriteResult WriteResult = StorageOpen.Storage->WriteSlot(InvalidId, "x");
            if (WriteResult.Result != ESaveSlotResult::Failure)
            {
                return "save_slot_storage_conformance.invalid_slot_id_write_not_rejected: " + InvalidId;
            }
            const FSaveSlotReadResult ReadResult = StorageOpen.Storage->ReadSlot(InvalidId);
            if (ReadResult.Result == ESaveSlotResult::Ok)
            {
                return "save_slot_storage_conformance.invalid_slot_id_read_not_rejected: " + InvalidId;
            }
        }
    }

    // Close storage before running instrumented fault injection
    StorageOpen.Storage.reset();

    // 9. Fault injection per-stage via internal filesystem adapter
    {
        FScopedTempDir FiDir;
        auto InstrumentedFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
        auto FiOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(FiDir.Dir, InstrumentedFs);
        if (FiOpen.Result != ESaveSlotResult::Ok || !FiOpen.Storage)
        {
            return "save_slot_storage_conformance.fault_injection_open_failed";
        }

        // Baseline first write
        const std::string FiSlot = "fi_slot";
        const std::string BaselineBytesA = "payload_a";
        InstrumentedFs->Reset();
        FSaveSlotWriteResult FiWriteA = FiOpen.Storage->WriteSlot(FiSlot, BaselineBytesA);
        if (FiWriteA.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.fi_baseline_write_a_failed";
        }
        const std::size_t FirstWriteOps = InstrumentedFs->Trace.size();

        // Baseline overwrite
        const std::string BaselineBytesB = "payload_b";
        InstrumentedFs->Reset();
        FSaveSlotWriteResult FiWriteB = FiOpen.Storage->WriteSlot(FiSlot, BaselineBytesB);
        if (FiWriteB.Result != ESaveSlotResult::Ok)
        {
            return "save_slot_storage_conformance.fi_baseline_write_b_failed";
        }
        const std::size_t OverwriteOps = InstrumentedFs->Trace.size();

        // Close storage to prepare clean directories for each injected failure ordinal
        FiOpen.Storage.reset();

        // 9a. Test fault injection on first write:
        // Identify the commit ordinal (when "commit_head" rename occurs)
        std::size_t FirstWriteCommitOrdinal = 0;
        {
            FScopedTempDir TmpTrace;
            auto TraceFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto TraceOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(TmpTrace.Dir, TraceFs);
            TraceFs->Reset();
            TraceOpen.Storage->WriteSlot("trace_slot", "data");
            for (const auto& Rec : TraceFs->Trace)
            {
                if (Rec.Description == "commit_head")
                {
                    FirstWriteCommitOrdinal = Rec.Ordinal;
                    break;
                }
            }
            TraceOpen.Storage.reset();
        }

        if (FirstWriteCommitOrdinal == 0)
        {
            return "save_slot_storage_conformance.commit_head_not_found_in_trace";
        }

        for (std::size_t Ord = 1; Ord <= FirstWriteOps; ++Ord)
        {
            FScopedTempDir StepDir;
            auto StepFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto StepOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(StepDir.Dir, StepFs);
            StepFs->Reset();
            StepFs->InjectedFailureOrdinal = Ord;

            const FSaveSlotWriteResult Res = StepOpen.Storage->WriteSlot("slot_test", "data");
            if (Ord <= FirstWriteCommitOrdinal)
            {
                // Failure before or at commit point must result in Failure and slot remains NotFound
                if (Res.Result != ESaveSlotResult::Failure)
                {
                    return "save_slot_storage_conformance.fault_injection_pre_commit_did_not_fail ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadRes = StepOpen.Storage->ReadSlot("slot_test", ESaveSlotRevision::Current);
                if (ReadRes.Result != ESaveSlotResult::NotFound)
                {
                    return "save_slot_storage_conformance.fault_injection_pre_commit_left_slot_observable ordinal=" + std::to_string(Ord);
                }
            }
            else
            {
                // Post-commit failure (cleanup phase) must not fail the write
                if (Res.Result != ESaveSlotResult::Ok)
                {
                    return "save_slot_storage_conformance.fault_injection_post_commit_cleanup_failed_write ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadRes = StepOpen.Storage->ReadSlot("slot_test", ESaveSlotRevision::Current);
                if (ReadRes.Result != ESaveSlotResult::Ok || ReadRes.Bytes != "data")
                {
                    return "save_slot_storage_conformance.fault_injection_post_commit_slot_corrupted ordinal=" + std::to_string(Ord);
                }
            }
            StepOpen.Storage.reset();
        }

        // 9b. Test fault injection on overwrite:
        std::size_t OverwriteCommitOrdinal = 0;
        {
            FScopedTempDir TmpTrace;
            auto TraceFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto TraceOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(TmpTrace.Dir, TraceFs);
            TraceOpen.Storage->WriteSlot("trace_slot", "prev");
            TraceFs->Reset();
            TraceOpen.Storage->WriteSlot("trace_slot", "new");
            for (const auto& Rec : TraceFs->Trace)
            {
                if (Rec.Description == "commit_head")
                {
                    OverwriteCommitOrdinal = Rec.Ordinal;
                    break;
                }
            }
            TraceOpen.Storage.reset();
        }

        for (std::size_t Ord = 1; Ord <= OverwriteOps; ++Ord)
        {
            FScopedTempDir StepDir;
            auto StepFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto StepOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(StepDir.Dir, StepFs);
            StepOpen.Storage->WriteSlot("slot_test", "initial_val");

            StepFs->Reset();
            StepFs->InjectedFailureOrdinal = Ord;

            const FSaveSlotWriteResult Res = StepOpen.Storage->WriteSlot("slot_test", "updated_val");
            if (Ord <= OverwriteCommitOrdinal)
            {
                if (Res.Result != ESaveSlotResult::Failure)
                {
                    return "save_slot_storage_conformance.overwrite_fault_pre_commit_did_not_fail ordinal=" + std::to_string(Ord);
                }
                // Previous committed state must remain intact
                const FSaveSlotReadResult ReadCur = StepOpen.Storage->ReadSlot("slot_test", ESaveSlotRevision::Current);
                if (ReadCur.Result != ESaveSlotResult::Ok || ReadCur.Bytes != "initial_val")
                {
                    return "save_slot_storage_conformance.overwrite_fault_pre_commit_corrupted_current ordinal=" + std::to_string(Ord);
                }
            }
            else
            {
                // Post-commit cleanup failure
                if (Res.Result != ESaveSlotResult::Ok)
                {
                    return "save_slot_storage_conformance.overwrite_fault_post_commit_failed_write ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadCur = StepOpen.Storage->ReadSlot("slot_test", ESaveSlotRevision::Current);
                if (ReadCur.Result != ESaveSlotResult::Ok || ReadCur.Bytes != "updated_val")
                {
                    return "save_slot_storage_conformance.overwrite_fault_post_commit_corrupted_current ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadPrev = StepOpen.Storage->ReadSlot("slot_test", ESaveSlotRevision::Previous);
                if (ReadPrev.Result != ESaveSlotResult::Ok || ReadPrev.Bytes != "initial_val")
                {
                    return "save_slot_storage_conformance.overwrite_fault_post_commit_corrupted_previous ordinal=" + std::to_string(Ord);
                }
            }
            StepOpen.Storage.reset();
        }

        // 9c. Test fault injection on legacy migration:
        std::size_t LegacyCommitOrdinal = 0;
        std::size_t LegacyOps = 0;
        {
            FScopedTempDir TmpTrace;
            const std::filesystem::path LegPath = TmpTrace.Dir / "legacy_slot.save";
            {
                std::ofstream LegFile(LegPath, std::ios::binary);
                LegFile << "legacy_payload";
            }

            auto TraceFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto TraceOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(TmpTrace.Dir, TraceFs);
            TraceFs->Reset();
            const FSaveSlotWriteResult LegWriteRes = TraceOpen.Storage->WriteSlot("legacy_slot", "migrated_payload");
            if (LegWriteRes.Result != ESaveSlotResult::Ok)
            {
                return "save_slot_storage_conformance.legacy_baseline_write_failed";
            }
            LegacyOps = TraceFs->Trace.size();
            for (const auto& Rec : TraceFs->Trace)
            {
                if (Rec.Description == "commit_head")
                {
                    LegacyCommitOrdinal = Rec.Ordinal;
                    break;
                }
            }
            TraceOpen.Storage.reset();
        }

        if (LegacyCommitOrdinal == 0)
        {
            return "save_slot_storage_conformance.legacy_commit_head_not_found_in_trace";
        }

        for (std::size_t Ord = 1; Ord <= LegacyOps; ++Ord)
        {
            FScopedTempDir StepDir;
            const std::filesystem::path LegPath = StepDir.Dir / "legacy_slot.save";
            {
                std::ofstream LegFile(LegPath, std::ios::binary);
                LegFile << "legacy_payload";
            }

            auto StepFs = std::make_shared<Internal::FInstrumentedSaveSlotFilesystem>();
            auto StepOpen = FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(StepDir.Dir, StepFs);
            StepFs->Reset();
            StepFs->InjectedFailureOrdinal = Ord;

            const FSaveSlotWriteResult Res = StepOpen.Storage->WriteSlot("legacy_slot", "migrated_payload");
            if (Ord <= LegacyCommitOrdinal)
            {
                if (Res.Result != ESaveSlotResult::Failure)
                {
                    return "save_slot_storage_conformance.legacy_fault_pre_commit_did_not_fail ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadCur = StepOpen.Storage->ReadSlot("legacy_slot", ESaveSlotRevision::Current);
                if (ReadCur.Result != ESaveSlotResult::Ok || ReadCur.Bytes != "legacy_payload")
                {
                    return "save_slot_storage_conformance.legacy_fault_pre_commit_corrupted_current ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadPrev = StepOpen.Storage->ReadSlot("legacy_slot", ESaveSlotRevision::Previous);
                if (ReadPrev.Result != ESaveSlotResult::NotFound)
                {
                    return "save_slot_storage_conformance.legacy_fault_pre_commit_previous_not_empty ordinal=" + std::to_string(Ord);
                }
            }
            else
            {
                if (Res.Result != ESaveSlotResult::Ok)
                {
                    return "save_slot_storage_conformance.legacy_fault_post_commit_failed_write ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadCur = StepOpen.Storage->ReadSlot("legacy_slot", ESaveSlotRevision::Current);
                if (ReadCur.Result != ESaveSlotResult::Ok || ReadCur.Bytes != "migrated_payload")
                {
                    return "save_slot_storage_conformance.legacy_fault_post_commit_corrupted_current ordinal=" + std::to_string(Ord);
                }
                const FSaveSlotReadResult ReadPrev = StepOpen.Storage->ReadSlot("legacy_slot", ESaveSlotRevision::Previous);
                if (ReadPrev.Result != ESaveSlotResult::Ok || ReadPrev.Bytes != "legacy_payload")
                {
                    return "save_slot_storage_conformance.legacy_fault_post_commit_corrupted_previous ordinal=" + std::to_string(Ord);
                }
            }
            StepOpen.Storage.reset();
        }
    }

    return "";
}
}
