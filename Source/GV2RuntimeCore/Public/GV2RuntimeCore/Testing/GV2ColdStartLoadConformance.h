#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host conformance suite for cold-start load
 * (SAV-12/13/17, plan SaveAndLoad, M4). Exercises FRuntimeSession's
 * StartFromSave() end to end against a minimal embedded module set (not
 * GameData/core — SAV-15/16's redirect/referential-integrity logic is
 * covered separately by Tests/Lua/save/load_path.lua against the real
 * repository, using core:module.runtime.load.resolve_definition_id's
 * injectable repository_get).
 *
 * Covers: a session started with Start() (NewGame) writes a real save
 * through game.save_slots.write; a second session started with
 * StartFromSave() against the same slot decodes it, calls the
 * "restore_instances" module hook (proven via a marker module), and ends
 * up with a canonical state hash identical to the first session's; a
 * corrupt container is rejected without ever assigning state; a save slot
 * that was never written is rejected before any Lua VM is created.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunColdStartLoadConformance();
}
