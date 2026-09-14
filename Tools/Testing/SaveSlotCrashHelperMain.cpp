#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2SaveSlotStorageInternal.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[])
{
    std::string Action;
    std::string RootDir;
    std::string SlotId;
    std::string Payload;
    std::string RevisionStr = "current";
    std::size_t CrashAtStage = 0;
    int HoldMs = 1000;

    for (int i = 1; i < argc; ++i)
    {
        const std::string Arg = argv[i];
        if (Arg == "--action" && i + 1 < argc)
        {
            Action = argv[++i];
        }
        else if (Arg == "--root" && i + 1 < argc)
        {
            RootDir = argv[++i];
        }
        else if (Arg == "--slot" && i + 1 < argc)
        {
            SlotId = argv[++i];
        }
        else if (Arg == "--payload" && i + 1 < argc)
        {
            Payload = argv[++i];
        }
        else if (Arg == "--revision" && i + 1 < argc)
        {
            RevisionStr = argv[++i];
        }
        else if (Arg == "--crash-at-stage" && i + 1 < argc)
        {
            CrashAtStage = std::stoul(argv[++i]);
        }
        else if (Arg == "--hold-ms" && i + 1 < argc)
        {
            HoldMs = std::stoi(argv[++i]);
        }
    }

    if (RootDir.empty())
    {
        std::cerr << "Missing --root\n";
        return 1;
    }

    if (Action == "hold-lock")
    {
        auto OpenRes = GV2RuntimeCore::FFilesystemSaveSlotStorage::Open(RootDir);
        if (OpenRes.Result == GV2RuntimeCore::ESaveSlotResult::Busy)
        {
            std::cout << "BUSY\n" << std::flush;
            return 2;
        }
        if (OpenRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok || !OpenRes.Storage)
        {
            std::cout << "FAIL\n" << std::flush;
            return 1;
        }
        std::cout << "LOCKED\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(HoldMs));
        OpenRes.Storage.reset();
        std::cout << "UNLOCKED\n" << std::flush;
        return 0;
    }

    if (Action == "count-stages")
    {
        auto Fs = std::make_shared<GV2RuntimeCore::Internal::FInstrumentedSaveSlotFilesystem>();
        auto OpenRes = GV2RuntimeCore::Testing::FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(RootDir, Fs);
        if (OpenRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok || !OpenRes.Storage)
        {
            std::cerr << "Open failed for count-stages\n";
            return 1;
        }
        Fs->Reset();
        auto WriteRes = OpenRes.Storage->WriteSlot(SlotId, Payload);
        if (WriteRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok)
        {
            std::cerr << "Write failed for count-stages\n";
            return 1;
        }

        std::size_t CommitStage = 0;
        for (const auto& Rec : Fs->Trace)
        {
            if (Rec.Description == "commit_head")
            {
                CommitStage = Rec.Ordinal;
                break;
            }
        }
        std::cout << "STAGES " << Fs->Trace.size() << "\n";
        std::cout << "COMMIT_STAGE " << CommitStage << "\n";
        return 0;
    }

    if (Action == "write")
    {
        std::shared_ptr<GV2RuntimeCore::Internal::FInstrumentedSaveSlotFilesystem> Fs;
        GV2RuntimeCore::FSaveSlotStorageOpenResult OpenRes;

        if (CrashAtStage > 0)
        {
            Fs = std::make_shared<GV2RuntimeCore::Internal::FInstrumentedSaveSlotFilesystem>();
            OpenRes = GV2RuntimeCore::Testing::FGV2SaveSlotStorageTestAccess::OpenWithFilesystem(RootDir, Fs);
            if (OpenRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok || !OpenRes.Storage)
            {
                std::cout << "OPEN_FAILED\n";
                return 1;
            }
            Fs->Reset();
            Fs->CrashAtOrdinal = CrashAtStage;
        }
        else
        {
            OpenRes = GV2RuntimeCore::FFilesystemSaveSlotStorage::Open(RootDir);
            if (OpenRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok || !OpenRes.Storage)
            {
                std::cout << "OPEN_FAILED\n";
                return 1;
            }
        }

        auto WriteRes = OpenRes.Storage->WriteSlot(SlotId, Payload);
        if (WriteRes.Result == GV2RuntimeCore::ESaveSlotResult::Ok)
        {
            std::cout << "OK\n";
            return 0;
        }
        std::cout << "FAIL\n";
        return 1;
    }

    if (Action == "read")
    {
        auto OpenRes = GV2RuntimeCore::FFilesystemSaveSlotStorage::Open(RootDir);
        if (OpenRes.Result != GV2RuntimeCore::ESaveSlotResult::Ok || !OpenRes.Storage)
        {
            std::cout << "OPEN_FAILED\n";
            return 1;
        }

        GV2RuntimeCore::ESaveSlotRevision Revision = GV2RuntimeCore::ESaveSlotRevision::Current;
        if (RevisionStr == "previous")
        {
            Revision = GV2RuntimeCore::ESaveSlotRevision::Previous;
        }

        auto ReadRes = OpenRes.Storage->ReadSlot(SlotId, Revision);
        if (ReadRes.Result == GV2RuntimeCore::ESaveSlotResult::Ok)
        {
            std::cout << "OK " << ReadRes.Bytes << "\n";
            return 0;
        }
        if (ReadRes.Result == GV2RuntimeCore::ESaveSlotResult::NotFound)
        {
            std::cout << "NOT_FOUND\n";
            return 0;
        }
        if (ReadRes.Result == GV2RuntimeCore::ESaveSlotResult::Unreadable)
        {
            std::cout << "UNREADABLE\n";
            return 0;
        }
        std::cout << "FAIL\n";
        return 1;
    }

    std::cerr << "Unknown action: " << Action << "\n";
    return 1;
}
