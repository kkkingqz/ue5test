#pragma once

#include "GV2TestSupport/GV2TestSupport.h"

#include "GV2ContentHostSupport/LuaSpecIdentity.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// GV2TestSupport is the one module both hosts call for TAS-04 ("Оба host-а
// вызывают один и тот же runner", ADR-0024). It exists only because the
// runner needs BOTH filesystem discovery (GV2ContentHostSupport,
// ADR-0018/0019: kept out of the VM-boundary-only GV2RuntimeCore) and the
// Lua VM (GV2RuntimeCore, TAS-02's RunLuaSpec): no existing module already
// depends on both. Test-only: no gameplay host links this module.
namespace GV2TestSupport
{
struct FLuaSpecRunResult
{
    std::size_t SpecsExecuted = 0;
    std::size_t CasesExecuted = 0;
    std::vector<GV2ContentHostSupport::FLuaSpecFailure> Failures;
};

/**
 * Discovers every `*.lua` spec under SpecRoot (TAS-02), runs each against
 * the already-started Session (TAS-02's `RunLuaSpec`), and aggregates
 * failures using the TAS-03 identity contract. A SpecRoot that does not
 * exist yields zero specs, zero failures — success, not an error.
 *
 * Adding a spec file under SpecRoot changes what this call covers without
 * any change to this function, to Session, or to the calling host: that is
 * the whole point of ADR-0024.
 *
 * Returns OutResult.Failures.empty().
 */
GV2_TEST_SUPPORT_API bool RunLuaSpecs(
    const std::filesystem::path& SpecRoot,
    GV2RuntimeCore::FRuntimeSession& Session,
    FLuaSpecRunResult& OutResult);

/**
 * Single source of truth for which immediate subdirectories of TestsLuaRoot
 * (normally "Tests/Lua") run on the shared production session, discovered
 * from the filesystem rather than hardcoded per host. Excludes the
 * subtrees in GetFixtureSessionSubtreeNames() below, which need an
 * isolated fixture session instead (TAS-13: Tests/Lua/commands).
 *
 * Before this function existed, Headless/Source/main.cpp and
 * GV2LuaSpecRunnerHostTests.cpp each hardcoded their own copy of this
 * subtree list — a new production-session subtree (e.g.
 * Tests/Lua/inventory/) silently ran in only one host if either copy was
 * forgotten, eroding ADR-0024's "adding a spec extends both hosts' cover­
 * age with zero C++" promise at the subtree granularity (the promise held
 * only *within* an already-declared subtree). Both hosts now call this one
 * function instead, so adding a subtree directory extends both hosts
 * automatically — no C++ change, and nothing to keep in sync by hand.
 *
 * Returns an empty vector (not an error) if TestsLuaRoot does not exist,
 * matching RunLuaSpecs' own "missing directory is not an error" contract.
 * Sorted for deterministic iteration order.
 */
GV2_TEST_SUPPORT_API std::vector<std::string> DiscoverProductionSessionSubtreeNames(
    const std::filesystem::path& TestsLuaRoot);

/**
 * Tests/Lua/ subtrees that need an isolated fixture session instead of the
 * shared production session (TAS-13) — the one place this exception is
 * declared. Read by DiscoverProductionSessionSubtreeNames() above; a host
 * wiring up a new fixture-session subtree still adds its own dedicated
 * call site (the session itself is necessarily host-specific setup), but
 * must exclude the name here so the production-session loop skips it.
 */
GV2_TEST_SUPPORT_API const std::vector<std::string>& GetFixtureSessionSubtreeNames();
}
