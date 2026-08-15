#pragma once

#include "GV2TestSupport/GV2TestSupport.h"

#include "GV2ContentHostSupport/LuaSpecIdentity.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <cstddef>
#include <filesystem>
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
}
