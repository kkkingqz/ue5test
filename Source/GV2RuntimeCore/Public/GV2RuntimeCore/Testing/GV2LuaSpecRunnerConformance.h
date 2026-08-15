#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host conformance suite for the Lua spec
 * runner mechanism (TAS-02: `FRuntimeSession::RunLuaSpec`).
 *
 * This tests the C++ MECHANISM that loads and executes a `Tests/Lua`-format
 * spec chunk, not a Lua gameplay rule (ADR-0024's carve-out: mechanism is a
 * legitimate C++ conformance target, gameplay rules are not).
 *
 * Verifies:
 * 1. A spec with a passing and a failing case reports both outcomes in
 *    OutCaseResults; a case failure does not stop other cases from running
 *    and is not reported as OutFault.
 * 2. Case order in OutCaseResults is deterministic ascending by case id,
 *    independent of source/table insertion order.
 * 3. A case may `require()` any module the production bootstrap already
 *    loaded.
 * 4. A spec returning an empty table is rejected as OutFault
 *    "LuaSpecEmpty".
 * 5. A spec returning a non-table value, or a table with a non-function
 *    value, is rejected as OutFault "LuaSpecFormatInvalid".
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunLuaSpecRunnerConformance();
}
