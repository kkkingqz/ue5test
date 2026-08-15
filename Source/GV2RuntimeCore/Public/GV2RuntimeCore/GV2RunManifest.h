#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace GV2RuntimeCore
{
struct GV2_PORTABLE_API FRunAcceptedCommand final
{
    std::string CommandId;
    FValue::FObject Args;
    std::int64_t Sequence = 0;

    bool operator==(const FRunAcceptedCommand&) const = default;
};

struct GV2_PORTABLE_API FRunManifest final
{
    std::int32_t LuaReleaseNumber = FRuntimeSession::LuaReleaseNumber;
    std::string RepositoryContentHash;
    std::uint64_t Seed = 0;
    std::vector<FRunAcceptedCommand> AcceptedCommands;

    bool operator==(const FRunManifest&) const = default;
};

/** Serializes FRunManifest to canonical deterministic JSON. */
GV2_PORTABLE_API std::string SerializeRunManifest(const FRunManifest& Manifest);

/** Deserializes FRunManifest from JSON/JSON5 string. Returns true on success, false on error with OutError. */
GV2_PORTABLE_API bool DeserializeRunManifest(
    std::string_view Json,
    FRunManifest& OutManifest,
    std::string& OutError);

} // namespace GV2RuntimeCore
