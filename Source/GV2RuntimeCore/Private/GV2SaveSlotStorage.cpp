#include "GV2RuntimeCore/GV2HostServices.h"

#include <cctype>
#include <fstream>
#include <sstream>

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

namespace
{
std::optional<std::filesystem::path> ResolveSlotPath(
    const std::filesystem::path& RootDir,
    const std::string& SlotId)
{
    if (!IsValidSaveSlotId(SlotId))
    {
        return std::nullopt;
    }
    return RootDir / (SlotId + ".save");
}
}

FFilesystemSaveSlotStorage::FFilesystemSaveSlotStorage(std::filesystem::path InRootDir)
    : RootDir(std::move(InRootDir))
{
}

FSaveSlotReadResult FFilesystemSaveSlotStorage::ReadSlot(const std::string& SlotId) const
{
    const std::optional<std::filesystem::path> SlotPath = ResolveSlotPath(RootDir, SlotId);
    if (!SlotPath)
    {
        return {ESaveSlotResult::Unreadable, {}};
    }

    std::error_code Ec;
    const bool bExists = std::filesystem::exists(*SlotPath, Ec);
    if (Ec || !bExists)
    {
        return {ESaveSlotResult::NotFound, {}};
    }
    if (!std::filesystem::is_regular_file(*SlotPath, Ec) || Ec)
    {
        return {ESaveSlotResult::Unreadable, {}};
    }

    std::ifstream Stream(*SlotPath, std::ios::binary);
    if (!Stream.is_open())
    {
        return {ESaveSlotResult::Unreadable, {}};
    }
    std::ostringstream Buffer;
    Buffer << Stream.rdbuf();
    if (Stream.bad())
    {
        return {ESaveSlotResult::Unreadable, {}};
    }
    return {ESaveSlotResult::Ok, Buffer.str()};
}

FSaveSlotWriteResult FFilesystemSaveSlotStorage::WriteSlot(const std::string& SlotId, const std::string& Bytes)
{
    const std::optional<std::filesystem::path> SlotPath = ResolveSlotPath(RootDir, SlotId);
    if (!SlotPath)
    {
        return {ESaveSlotResult::Failure};
    }

    std::error_code Ec;
    std::filesystem::create_directories(RootDir, Ec);

    const std::filesystem::path TempPath = SlotPath->string() + ".tmp";
    {
        std::ofstream Stream(TempPath, std::ios::binary | std::ios::trunc);
        if (!Stream.is_open())
        {
            return {ESaveSlotResult::Failure};
        }
        Stream.write(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
        Stream.flush();
        if (Stream.bad())
        {
            std::filesystem::remove(TempPath, Ec);
            return {ESaveSlotResult::Failure};
        }
    }

    std::filesystem::rename(TempPath, *SlotPath, Ec);
    if (Ec)
    {
        std::filesystem::remove(TempPath, Ec);
        return {ESaveSlotResult::Failure};
    }
    return {ESaveSlotResult::Ok};
}
}
