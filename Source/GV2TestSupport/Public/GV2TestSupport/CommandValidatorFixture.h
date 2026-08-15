#pragma once

#include "GV2TestSupport/GV2TestSupport.h"

#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <filesystem>

// TAS-13: Tests/Lua/commands/*.lua specs exercise the command dispatcher's
// interaction with validators, which needs test-scoped validators/commands
// registered during the session's "register" phase, before the global
// validator registry freezes. The real production session cannot provide
// this — by the time any spec runs it is already fully booted, and its
// (currently empty) validator registry is already frozen. This helper
// builds the isolated fixture session instead, shared by both hosts so
// neither duplicates the bootstrap wiring.
namespace GV2TestSupport
{
/**
 * Starts a session using an empty pinned repository and a fixture-only
 * module tree: the four real Scripts/runtime/{mutation_window,stable_id,
 * validator_registry,command_dispatcher}.lua modules (read from ScriptsRoot
 * — never duplicated as embedded C++ string literals) plus the test-only
 * driver at FixtureRoot/driver.lua (Tests/Fixtures/CommandValidatorSpecs),
 * which registers the validators and commands the specs under
 * Tests/Lua/commands/ dispatch against.
 *
 * ScriptsRoot must be the real Scripts/ directory; FixtureRoot must be
 * Tests/Fixtures/CommandValidatorSpecs/. Each host resolves these paths the
 * way it already resolves every other path (this function does no
 * candidate-search of its own).
 */
GV2_TEST_SUPPORT_API bool StartCommandValidatorFixtureSession(
    const std::filesystem::path& ScriptsRoot,
    const std::filesystem::path& FixtureRoot,
    GV2RuntimeCore::FRuntimeSession& OutSession,
    GV2RuntimeCore::FRuntimeFault& OutFault);
}
