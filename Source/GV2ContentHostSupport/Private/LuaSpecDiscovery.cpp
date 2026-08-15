#include "GV2ContentHostSupport/LuaSpecDiscovery.h"

#include <algorithm>
#include <fstream>
#include <optional>

namespace GV2ContentHostSupport
{
namespace
{
std::optional<std::string> ReadFileToString(const std::filesystem::path& FilePath)
{
    std::ifstream Stream(FilePath, std::ios::binary);
    if (!Stream)
    {
        return std::nullopt;
    }
    return std::string(
        (std::istreambuf_iterator<char>(Stream)),
        std::istreambuf_iterator<char>());
}

std::string ToForwardSlashRelativePath(
    const std::filesystem::path& Root,
    const std::filesystem::path& FilePath)
{
    std::error_code Ec;
    std::filesystem::path Relative = std::filesystem::relative(FilePath, Root, Ec);
    if (Ec)
    {
        Relative = FilePath.filename();
    }
    std::string Result = Relative.generic_string();
    return Result;
}
} // namespace

std::vector<FLuaSpecFile> DiscoverLuaSpecFiles(const std::filesystem::path& Root)
{
    std::vector<FLuaSpecFile> Files;

    std::error_code Ec;
    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        return Files;
    }

    std::vector<std::filesystem::path> Entries;
    for (const auto& Entry :
         std::filesystem::recursive_directory_iterator(Root, std::filesystem::directory_options::skip_permission_denied, Ec))
    {
        if (Ec) break;
        std::error_code IsFileEc;
        if (Entry.is_regular_file(IsFileEc) && !IsFileEc && Entry.path().extension() == ".lua")
        {
            Entries.push_back(Entry.path());
        }
    }

    std::vector<FLuaSpecFile> Result;
    Result.reserve(Entries.size());
    for (const std::filesystem::path& EntryPath : Entries)
    {
        FLuaSpecFile File;
        File.RelativePath = ToForwardSlashRelativePath(Root, EntryPath);
        const std::optional<std::string> Source = ReadFileToString(EntryPath);
        if (!Source.has_value())
        {
            continue;
        }
        File.Source = *Source;
        Result.push_back(std::move(File));
    }

    std::sort(Result.begin(), Result.end(), [](const FLuaSpecFile& A, const FLuaSpecFile& B) {
        return A.RelativePath < B.RelativePath;
    });
    return Result;
}
} // namespace GV2ContentHostSupport
