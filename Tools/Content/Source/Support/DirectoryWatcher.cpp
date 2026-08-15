#include "Support/DirectoryWatcher.h"

#include <atomic>
#include <csignal>
#include <system_error>
#include <thread>

namespace GV2ContentCli
{

namespace
{
static std::atomic<bool> g_StopRequested{false};

void SignalHandler(int)
{
    g_StopRequested = true;
}
} // namespace

void InstallSignalHandlers()
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
}

bool IsStopRequested()
{
    return g_StopRequested.load();
}

void RequestStop()
{
    g_StopRequested.store(true);
}

void ResetStopRequested()
{
    g_StopRequested.store(false);
}

FDirectorySnapshot TakeDirectorySnapshot(const std::filesystem::path& Root)
{
    FDirectorySnapshot Snapshot;
    std::error_code Ec;
    if (!std::filesystem::exists(Root, Ec) || !std::filesystem::is_directory(Root, Ec))
    {
        return Snapshot;
    }
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(
             Root, std::filesystem::directory_options::skip_permission_denied, Ec))
    {
        if (Ec) break;
        if (Entry.is_regular_file(Ec))
        {
            const std::string Rel = std::filesystem::relative(Entry.path(), Root, Ec).generic_string();
            const auto MTime = Entry.last_write_time(Ec);
            const auto Size = Entry.file_size(Ec);
            Snapshot.Files[Rel] = { MTime, Size };
        }
    }
    return Snapshot;
}

int RunDirectoryWatchLoop(
    const std::filesystem::path& Root,
    const FWatchOptions& Options,
    const std::function<int(std::size_t Iteration)>& OnIteration)
{
    InstallSignalHandlers();
    ResetStopRequested();

    std::size_t Iteration = 1;
    int LastExitCode = OnIteration(Iteration);
    FDirectorySnapshot LastSnapshot = TakeDirectorySnapshot(Root);

    while (!IsStopRequested())
    {
        if (Options.MaxIterations > 0 && Iteration >= Options.MaxIterations)
        {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(Options.PollIntervalMs > 0 ? Options.PollIntervalMs : 500));
        if (IsStopRequested())
        {
            break;
        }

        FDirectorySnapshot CurrentSnapshot = TakeDirectorySnapshot(Root);
        if (!(CurrentSnapshot == LastSnapshot))
        {
            LastSnapshot = std::move(CurrentSnapshot);
            Iteration++;
            LastExitCode = OnIteration(Iteration);
        }
    }

    return LastExitCode;
}

} // namespace GV2ContentCli
