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

enum class ELuaSpecTier
{
    Core,
    TextSystem,
    FullGame,
    FixtureCommands,
};

struct FSubtreeTierConfig
{
    std::string SubtreeName;
    ELuaSpecTier Tier;
};

/**
 * Returns the canonical list of all configured subtrees and their assigned test tier.
 */
GV2_TEST_SUPPORT_API const std::vector<FSubtreeTierConfig>& GetAllSubtreeTierConfigs();

/**
 * Returns the declared tier for a subtree name, or std::nullopt if unregistered.
 */
GV2_TEST_SUPPORT_API std::optional<ELuaSpecTier> GetSubtreeTier(const std::string& SubtreeName);

/**
 * Returns all declared subtree names mapped to a specific tier (sorted).
 */
GV2_TEST_SUPPORT_API std::vector<std::string> GetSubtreesForTier(ELuaSpecTier Tier);

/**
 * EGC-07: returns the actual filesystem roots for a tier's packages --
 * GameDataRoot/<name> for every ordinary package (Core -> {core};
 * TextSystem -> {core, textsystem, sample}; FullGame -> {core, textsystem,
 * rh}), plus, for FullGame only, the mode-lifecycle fixture package nested
 * at ProjectRoot/Tests/Fixtures/SyntheticMechanicalFixture/synthetic (not
 * under GameData/, so it never collides with that fixture's own
 * pre-existing core:-namespaced definitions, read directly by unrelated
 * C++ contract tests).
 *
 * Replaces a prior GetPackageNamesForTier(): both hosts used to take that
 * name list and glue it to GameDataRoot/<name> themselves, independently --
 * a root added to one host's own glue (such as the fixture package above)
 * could silently diverge from the other host's. This is the one shared
 * place both hosts call instead.
 */
GV2_TEST_SUPPORT_API std::vector<std::filesystem::path> GetPackageRootsForTier(
    ELuaSpecTier Tier,
    const std::filesystem::path& GameDataRoot,
    const std::filesystem::path& ProjectRoot);

/**
 * Validates that every immediate subdirectory under TestsLuaRoot is declared
 * in the subtree tier map. Returns false and populates OutUnregisteredSubtrees
 * if any directory is undeclared (TSL-15).
 */
GV2_TEST_SUPPORT_API bool ValidateAllSubtreesRegistered(
    const std::filesystem::path& TestsLuaRoot,
    std::vector<std::string>& OutUnregisteredSubtrees);

/**
 * Single source of truth for which immediate subdirectories of TestsLuaRoot
 * (normally "Tests/Lua") run on production sessions, discovered
 * from the filesystem rather than hardcoded per host. Excludes the
 * subtrees in GetFixtureSessionSubtreeNames() below, which need an
 * isolated fixture session instead (TAS-13: Tests/Lua/commands).
 */
GV2_TEST_SUPPORT_API std::vector<std::string> DiscoverProductionSessionSubtreeNames(
    const std::filesystem::path& TestsLuaRoot);

/**
 * Tests/Lua/ subtrees that need an isolated fixture session instead of the
 * standard session (TAS-13).
 */
GV2_TEST_SUPPORT_API const std::vector<std::string>& GetFixtureSessionSubtreeNames();
}
