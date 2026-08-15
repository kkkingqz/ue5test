#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <utility>

namespace GV2ContentCli
{

struct FDirectorySnapshot
{
    std::map<std::string, std::pair<std::filesystem::file_time_type, std::uintmax_t>> Files;

    bool operator==(const FDirectorySnapshot& Other) const = default;
};

FDirectorySnapshot TakeDirectorySnapshot(const std::filesystem::path& Root);

void InstallSignalHandlers();
bool IsStopRequested();
void RequestStop();
void ResetStopRequested();

struct FWatchOptions
{
    std::uint32_t PollIntervalMs = 500;
    std::uint32_t MaxIterations = 0;
};

int RunDirectoryWatchLoop(
    const std::filesystem::path& Root,
    const FWatchOptions& Options,
    const std::function<int(std::size_t Iteration)>& OnIteration);

} // namespace GV2ContentCli
