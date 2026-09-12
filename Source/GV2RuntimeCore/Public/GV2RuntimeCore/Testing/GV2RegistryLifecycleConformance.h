#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host conformance suite for mandatory registry
 * sealing phase (CFC-05, plan SessionLifecycle, M1).
 *
 * Exercises FRuntimeSession::Start lifecycle orchestration when sealing fails:
 * - A throwing freeze fails Start(), records phase=SealingRegistries, fault code,
 *   and registry path, and guarantees neither state build nor start hooks execute.
 * - An is_frozen predicate returning false or non-boolean fails Start() with fault.
 * - A missing required participant or missing seal method fails Start() with fault.
 * - Protected-call failure restores the Lua/native stack/context without corruption.
 * - A correctly implemented lifecycle seals successfully and Start() proceeds.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunRegistryLifecycleConformance();
}
