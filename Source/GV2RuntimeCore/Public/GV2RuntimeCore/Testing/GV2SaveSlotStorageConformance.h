#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable cross-host conformance suite for
 * FFilesystemSaveSlotStorage (SAV-07, plan SaveAndLoad). Roots itself in a
 * fresh temporary directory it creates and removes internally, so both
 * hosts call this with no arguments and no host-specific setup — matching
 * every other conformance suite in this namespace.
 *
 * Covers: write/read roundtrip with arbitrary bytes (including embedded
 * NUL); reading a slot that was never written; reading a slot whose path
 * is occupied by something other than a regular file; a write that fails
 * before publishing leaves the previously-published slot content valid and
 * readable; and slot ids outside the allowed grammar are rejected instead
 * of resolving to an escaping path.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunSaveSlotStorageConformance();
}
