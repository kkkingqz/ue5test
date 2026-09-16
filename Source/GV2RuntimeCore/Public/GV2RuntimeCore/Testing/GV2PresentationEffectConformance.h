#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
/**
 * Executes the portable conformance suite for PEP-03 (ADR-0047): the one-shot
 * presentation effect DTO, queue, and typed-rejection resolution. Exercises
 * FRuntimeSession::PublishHostLocalEffect/TakePendingEffects and the free function
 * ResolveEffectTarget end to end against a minimal embedded module set (not
 * GameData/core) that defines its own synthetic game.ui.publish_effect/
 * take_pending_effects -- the REAL Scripts/boundary/outbound.lua's own light
 * validation and staged/committed/rollback lifecycle is a Lua-level rule and is
 * covered separately by Tests/Lua/presentation/effect_queue.lua (ADR-0024: a rule
 * expressed in Lua belongs to a Lua spec, not a C++ conformance fixture). This
 * module cannot link GV2ContentHostSupport (ADR-0018/0019: GV2RuntimeCore stays
 * VM-boundary-only), so it cannot load the real Scripts/ tree from disk itself --
 * only GV2TestSupport-level and host-level tests can.
 *
 * Covers: ResolveEffectTarget's four outcomes (None for an untargeted effect,
 * WrongSessionGeneration, StaleTarget, StaleRevision, and a matching target
 * resolving None); PublishHostLocalEffect stamping Sequence and
 * TargetSessionGeneration itself, never trusting the caller's values;
 * TakePendingEffects pulling a Lua-committed batch, assigning Sequence in array
 * order, and draining both sources' effects together; Sequence monotonicity
 * across an alternating host-local/Lua-published/host-local sequence of calls; an
 * unknown top-level or target field rejected before the effect is enqueued (the
 * queue's own length proves nothing was added); args read as an object and
 * rejected when given a non-object.
 *
 * Returns empty string on success, or a diagnostic error message on failure.
 */
GV2_PORTABLE_API std::string RunPresentationEffectConformance();
}
