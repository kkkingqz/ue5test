#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2RuntimeCore/GV2RunDigest.h"
#include "GV2RuntimeCore/GV2RunManifest.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2ContentCore/RepositorySnapshot.h"

#include <vector>

namespace GV2RuntimeCore
{
/**
 * Executes replay of a recorded run manifest on a portable runtime session.
 * Rejects replay if RepositoryContentHash or LuaReleaseNumber does not match.
 */
GV2_PORTABLE_API bool ReplayRunManifest(
    const FRunManifest& Manifest,
    const GV2ContentCore::FRepositoryReadHandle& RepositoryHandle,
    const std::vector<FRuntimeSource>& RuntimeSources,
    FRunResult& OutResult,
    FRuntimeFault& OutFault);

} // namespace GV2RuntimeCore
