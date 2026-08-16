#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2RuntimeCore/GV2RunManifest.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace GV2RuntimeCore
{
struct GV2_PORTABLE_API FRunResult final
{
    bool bSuccess = true;
    std::uint64_t ExecutedCommandsCount = 0;
    std::string FinalScreenId;
    FValue::FObject FinalScreenFields;
    std::string StateHash;
    std::string FaultCode;

    bool operator==(const FRunResult&) const = default;
};

struct GV2_PORTABLE_API FRunDigest final
{
    std::string DigestHash;
    std::int32_t LuaReleaseNumber = FRuntimeSession::LuaReleaseNumber;
    std::string RepositoryContentHash;
    std::string ScriptSetHash;
    std::uint64_t Seed = 0;
    std::uint64_t ExecutedCommandsCount = 0;
    bool bSuccess = true;
    std::string FinalScreenId;
    std::string StateHash;
    std::string FaultCode;

    bool operator==(const FRunDigest&) const = default;
};

/** Computes the deterministic run digest given a manifest and execution result. */
GV2_PORTABLE_API FRunDigest ComputeRunDigest(
    const FRunManifest& Manifest,
    const FRunResult& Result);

/** Serializes FRunDigest to canonical deterministic JSON. */
GV2_PORTABLE_API std::string SerializeRunDigest(const FRunDigest& Digest);

/** Deserializes FRunDigest from JSON/JSON5 string. Returns true on success, false on error with OutError. */
GV2_PORTABLE_API bool DeserializeRunDigest(
    std::string_view Json,
    FRunDigest& OutDigest,
    std::string& OutError);

} // namespace GV2RuntimeCore
