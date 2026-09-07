#pragma once

#include "CoreMinimal.h"

/**
 * PAH-08 (ADR-0042, INV-P5): observation half of the two-part gate.
 *
 * Every presentation authority -- UI schema resolution, screen resolution,
 * image resource resolution -- bumps one monotonic counter when it is asked to
 * resolve something. Nothing in production reads the counter and no production
 * behaviour depends on it; it exists so a test can observe the invariant
 * directly instead of scanning source for its syntactic traces.
 *
 * The phase distinction deliberately does NOT live here. Preparation and
 * application are already two separate public functions
 * (FGV2LayeredUiReconciler::PrepareReconcile / ::CommitReconcile), so a test
 * defines the phases simply by which one it brackets. Introducing an
 * "is a commit in progress" flag into production would put phase semantics into
 * the shipping build purely to serve a check -- the shape ADR-0042 rejects
 * elsewhere (a value computed for a checker rather than for behaviour).
 *
 * Measurement boundary matters and is not interchangeable: a test must bracket
 * CommitReconcile, never the enclosing Reconcile. Since PAH-07, Reconcile
 * responds to a failed compensating rollback by calling
 * PerformCatastrophicRecovery, which legitimately replays PrepareReconcile
 * against the last committed document -- a nested preparation, not a resolve
 * during application. Bracketing Reconcile would count it and produce a false
 * violation, and the natural "fix" for that false violation is to weaken the
 * assertion, which is how a check stops checking.
 */
namespace GV2PresentationAuthorityProbe
{
#if !UE_BUILD_SHIPPING
/** Total authority resolutions since process start. Test-only observation point. */
GV2_API uint64 GetResolveCount();

/** Called by each authority as it resolves. Not called from anywhere else. */
GV2_API void NoteResolve();
#endif
}

#if !UE_BUILD_SHIPPING
#define GV2_NOTE_AUTHORITY_RESOLVE() ::GV2PresentationAuthorityProbe::NoteResolve()
#else
#define GV2_NOTE_AUTHORITY_RESOLVE() do {} while (false)
#endif
