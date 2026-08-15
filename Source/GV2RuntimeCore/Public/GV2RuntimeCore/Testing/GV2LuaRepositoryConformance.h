#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host Lua repository access conformance suite (PCC-46).
 * Covers happy path get/require/exists/list, typed error conventions, tombstones,
 * redirects, detached deep copy immutability, provenance absence, value limits,
 * and active session isolation against unrelated repository rebuild/republish.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunLuaRepositoryAccessConformance();
}
