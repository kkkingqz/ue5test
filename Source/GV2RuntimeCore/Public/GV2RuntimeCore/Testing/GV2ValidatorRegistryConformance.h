#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host Command Validator Registry conformance
 * suite (GEW-01: game.commands.validators registry and stable order).
 *
 * Starts a minimal Lua session that loads the real
 * Scripts/runtime/validator_registry.lua module and:
 * 1. Runs its self-contained run_conformance() check (priority-then-
 *    registration-order sort, late-registration/invalid-id/duplicate
 *    rejection) against a throwaway local registry.
 * 2. Verifies the live game.commands.validators facade exists and is
 *    frozen once the host's "register" lifecycle phase has completed, and
 *    that registering into it afterwards is rejected.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunValidatorRegistryConformance();
}
