#include "GV2RuntimeCore/Testing/GV2PresentationEffectConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <string>
#include <vector>

namespace GV2RuntimeCore::Testing
{
namespace
{
GV2ContentCore::FRepositoryReadHandle MakeEmptyRepository()
{
    const GV2ContentCore::FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    GV2ContentCore::FBuildOptions Options;
    GV2ContentCore::FBuildResult Result = GV2ContentCore::BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return {};
    }
    return Result.GetCandidate().GetReadHandle();
}

// Deliberately trivial stand-ins, not copies of the real Scripts/ modules of the same
// id -- this suite is about FRuntimeSession's C++ orchestration of the effect queue,
// not any gameplay-shaped state rule (GV2ColdStartLoadConformance.cpp's own precedent).
const char* MigrateStubSource = R"lua(
local M = {
    id = "core:module.runtime.migrate",
}
M.CURRENT_SECTION_VERSIONS = { meta = 1, data = 1 }
function M.plan_migrations(saved_section_versions)
    return {}, nil
end
function M.verify_complete()
    return nil
end
return M
)lua";

const char* StateValidatorStubSource = R"lua(
local M = {
    id = "core:module.runtime.state_validator",
}
local SECTIONS = { "meta", "data" }
M.canonical_sections = SECTIONS
M.definition_reference_fields = {}
function M.is_canonical_section(name)
    for _, s in ipairs(SECTIONS) do
        if s == name then return true end
    end
    return false
end
function M.create_empty_canonical_state()
    return { meta = { save_version = 1 }, data = {} }
end
function M.validate_state_tree(tree)
    return true
end
return M
)lua";

const char* StateCompositionStubSource = R"lua(
local state_validator = require("core:module.runtime.state_validator")
local M = {
    id = "core:module.runtime.state_composition",
}
function M.compose_default_state(ctx, modules)
    return state_validator.create_empty_canonical_state()
end
return M
)lua";

// PEP-03 (ADR-0047): a synthetic game.ui.publish_effect/take_pending_effects, NOT a
// copy of the real Scripts/boundary/outbound.lua -- this suite exercises the HOST
// side (ReadPresentationEffect, TakePendingEffects, sequence stamping), not
// outbound.lua's own light validation or staged/committed/rollback lifecycle, which
// Tests/Lua/presentation/effect_queue.lua covers against the real module instead.
const char* DriverSource = R"lua(
local M = {
    id = "core:module.test.presentation_effect_driver",
}

local pending_effects = {}

-- Top-level (not wrapped in a lifecycle hook): this module is required as part of
-- bootstrap (it is entry_module_id), so this runs exactly once, immediately, the
-- same way requiring any module executes its own chunk body.
if not _G.game then _G.game = {} end
_G.game.ui = _G.game.ui or {}
_G.game.ui.publish_effect = function(effect)
    table.insert(pending_effects, effect)
    return true
end
_G.game.ui.take_pending_effects = function()
    if #pending_effects == 0 then
        return nil
    end
    local result = pending_effects
    pending_effects = {}
    return result
end

return M
)lua";

std::vector<FRuntimeSource> MakeConformanceSources()
{
    const std::string Manifest = R"lua(
return {
    entry_module_id = "core:module.test.presentation_effect_driver",
    modules = {
        {
            module_id = "core:module.runtime.state_validator",
            source = "runtime/state_validator.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.state_composition",
            source = "runtime/state_composition.lua",
            dependencies = { "core:module.runtime.state_validator" },
        },
        {
            module_id = "core:module.runtime.migrate",
            source = "runtime/migrate.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.test.presentation_effect_driver",
            source = "test/driver.lua",
            dependencies = {
                "core:module.runtime.state_composition",
                "core:module.runtime.migrate",
            },
        },
    },
}
)lua";

    return {
        {"@core/bootstrap/manifest.lua", Manifest},
        {"@core/runtime/state_validator.lua", StateValidatorStubSource},
        {"@core/runtime/state_composition.lua", StateCompositionStubSource},
        {"@core/runtime/migrate.lua", MigrateStubSource},
        {"@core/test/driver.lua", DriverSource},
    };
}

bool StartConformanceSession(FRuntimeSession& Session, FRuntimeFault& OutFault)
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    if (!RepoHandle.IsValid())
    {
        OutFault = {"ConformanceSetupFailed", "empty repository build failed"};
        return false;
    }
    FSessionStartInputs Inputs;
    Inputs.SessionGeneration = 1;
    Inputs.SeedHex = "0123456789abcdef";
    return Session.Start(Inputs, RepoHandle, MakeConformanceSources(), OutFault);
}
}

std::string RunPresentationEffectConformance()
{
    // 1. ResolveEffectTarget: pure function, no session required.
    {
        FPresentationEffect Untargeted;
        Untargeted.EffectId = "core:effect.test.untargeted";
        if (ResolveEffectTarget(Untargeted, 7, "ui@7:1", 3) != EPresentationEffectRejectReason::None)
        {
            return "presentation_effect_conformance.untargeted_effect_not_none";
        }

        FPresentationEffect Targeted;
        Targeted.EffectId = "core:effect.test.targeted";
        Targeted.bHasTarget = true;
        Targeted.TargetSessionGeneration = 7;
        Targeted.TargetUiInstanceId = "ui@7:1";
        Targeted.TargetRevision = 3;

        if (ResolveEffectTarget(Targeted, 7, "ui@7:1", 3) != EPresentationEffectRejectReason::None)
        {
            return "presentation_effect_conformance.matching_target_not_none";
        }
        if (ResolveEffectTarget(Targeted, 8, "ui@7:1", 3) != EPresentationEffectRejectReason::WrongSessionGeneration)
        {
            return "presentation_effect_conformance.wrong_generation_not_detected";
        }
        if (ResolveEffectTarget(Targeted, 7, "ui@7:2", 3) != EPresentationEffectRejectReason::StaleTarget)
        {
            return "presentation_effect_conformance.stale_target_not_detected";
        }
        if (ResolveEffectTarget(Targeted, 7, "ui@7:1", 4) != EPresentationEffectRejectReason::StaleRevision)
        {
            return "presentation_effect_conformance.stale_revision_not_detected";
        }
    }

    // 2. PublishHostLocalEffect stamps Sequence and TargetSessionGeneration itself,
    //    ignoring whatever the caller put there.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartConformanceSession(Session, Fault))
        {
            return "presentation_effect_conformance.session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }

        FPresentationEffect Effect;
        Effect.EffectId = "core:effect.test.host_local";
        Effect.Sequence = 999; // must be overwritten
        Effect.bHasTarget = true;
        Effect.TargetSessionGeneration = 42; // must be overwritten with the real generation
        Effect.TargetUiInstanceId = "ui@1:1";
        Effect.TargetRevision = 1;

        if (!Session.PublishHostLocalEffect(Effect, Fault))
        {
            Session.Stop();
            return "presentation_effect_conformance.publish_host_local_failed: " + Fault.Code;
        }

        std::vector<FPresentationEffect> Drained;
        if (!Session.TakePendingEffects(Drained, Fault))
        {
            Session.Stop();
            return "presentation_effect_conformance.take_pending_after_host_local_failed: " + Fault.Code;
        }
        if (Drained.size() != 1)
        {
            Session.Stop();
            return "presentation_effect_conformance.host_local_not_enqueued";
        }
        if (Drained[0].Sequence != 1)
        {
            Session.Stop();
            return "presentation_effect_conformance.host_local_sequence_not_stamped: " + std::to_string(Drained[0].Sequence);
        }
        if (Drained[0].TargetSessionGeneration != 1)
        {
            Session.Stop();
            return "presentation_effect_conformance.host_local_generation_not_stamped: " + std::to_string(Drained[0].TargetSessionGeneration);
        }

        // Draining again must yield nothing left over.
        std::vector<FPresentationEffect> SecondDrain;
        if (!Session.TakePendingEffects(SecondDrain, Fault) || !SecondDrain.empty())
        {
            Session.Stop();
            return "presentation_effect_conformance.queue_not_cleared_after_drain";
        }

        Session.Stop();
    }

    // 3. TakePendingEffects pulls a Lua-committed batch, assigning Sequence in array
    //    order, and rejects an unknown field BEFORE enqueueing anything from that pull.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartConformanceSession(Session, Fault))
        {
            return "presentation_effect_conformance.session_start_failed_case3: " + Fault.Code;
        }

        const char* PublishTwoChunk = R"lua(
return {
    publish_two = function()
        game.ui.publish_effect({ effect_id = "core:effect.test.lua_one" })
        game.ui.publish_effect({ effect_id = "core:effect.test.lua_two", args = { duration_ms = 250 } })
        return true
    end,
}
)lua";
        std::vector<FLuaSpecCaseResult> CaseResults;
        if (!Session.RunLuaSpec("presentation_effect_conformance_publish_two", PublishTwoChunk, CaseResults, Fault)
            || CaseResults.size() != 1 || !CaseResults[0].Success)
        {
            Session.Stop();
            return "presentation_effect_conformance.publish_two_spec_failed: " + Fault.Code;
        }

        std::vector<FPresentationEffect> Drained;
        if (!Session.TakePendingEffects(Drained, Fault))
        {
            Session.Stop();
            return "presentation_effect_conformance.take_pending_after_lua_failed: " + Fault.Code;
        }
        if (Drained.size() != 2)
        {
            Session.Stop();
            return "presentation_effect_conformance.lua_batch_wrong_count: " + std::to_string(Drained.size());
        }
        if (Drained[0].EffectId != "core:effect.test.lua_one" || Drained[0].Sequence != 1)
        {
            Session.Stop();
            return "presentation_effect_conformance.lua_first_effect_wrong";
        }
        if (Drained[1].EffectId != "core:effect.test.lua_two" || Drained[1].Sequence != 2)
        {
            Session.Stop();
            return "presentation_effect_conformance.lua_second_effect_wrong";
        }
        if (Drained[1].Args.find("duration_ms") == Drained[1].Args.end())
        {
            Session.Stop();
            return "presentation_effect_conformance.lua_args_not_read";
        }

        // Unknown field: rejected before enqueue -- the queue must be empty afterward,
        // not just the call reporting failure.
        const char* PublishUnknownFieldChunk = R"lua(
return {
    publish_bad = function()
        game.ui.publish_effect({ effect_id = "core:effect.test.bad", bogus_field = 1 })
        return true
    end,
}
)lua";
        if (!Session.RunLuaSpec("presentation_effect_conformance_publish_bad", PublishUnknownFieldChunk, CaseResults, Fault)
            || CaseResults.size() != 1 || !CaseResults[0].Success)
        {
            Session.Stop();
            return "presentation_effect_conformance.publish_bad_spec_failed: " + Fault.Code;
        }

        std::vector<FPresentationEffect> RejectedDrain;
        FRuntimeFault DrainFault;
        if (Session.TakePendingEffects(RejectedDrain, DrainFault))
        {
            Session.Stop();
            return "presentation_effect_conformance.unknown_field_was_accepted";
        }
        if (DrainFault.Code != "LuaPresentationEffectInvalid")
        {
            Session.Stop();
            return "presentation_effect_conformance.unknown_field_wrong_fault_code: " + DrainFault.Code;
        }

        Session.Stop();
    }

    // 4. Sequence monotonicity across an alternating host-local/Lua/host-local/Lua
    //    stream, draining after each publish -- Sequence must increase strictly
    //    across all four drains, regardless of which source produced which effect.
    //    (A Lua-published effect's Sequence is only assigned when TakePendingEffects
    //    actually pulls it, NOT at the moment Lua calls publish_effect -- an effect
    //    left sitting in Lua's own committed queue across an intervening
    //    PublishHostLocalEffect call is expected to sort AFTER it, by design; this
    //    test drains eagerly specifically so it observes true call-order instead.)
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartConformanceSession(Session, Fault))
        {
            return "presentation_effect_conformance.session_start_failed_case4: " + Fault.Code;
        }

        std::int64_t LastSequence = 0;
        std::vector<std::string> ObservedOrder;
        auto DrainAndRecord = [&](const char* StepLabel) -> std::string
        {
            std::vector<FPresentationEffect> Drained;
            FRuntimeFault DrainFault;
            if (!Session.TakePendingEffects(Drained, DrainFault))
            {
                return std::string("presentation_effect_conformance.alt_drain_failed_") + StepLabel;
            }
            for (const FPresentationEffect& Effect : Drained)
            {
                if (Effect.Sequence <= LastSequence)
                {
                    return std::string("presentation_effect_conformance.alt_sequence_not_monotonic_") + StepLabel;
                }
                LastSequence = Effect.Sequence;
                ObservedOrder.push_back(Effect.EffectId);
            }
            return "";
        };

        FPresentationEffect First;
        First.EffectId = "core:effect.test.alt_one";
        if (!Session.PublishHostLocalEffect(First, Fault))
        {
            Session.Stop();
            return "presentation_effect_conformance.alt_publish_one_failed: " + Fault.Code;
        }
        if (const std::string Err = DrainAndRecord("after_one"); !Err.empty())
        {
            Session.Stop();
            return Err;
        }

        const char* PublishOneChunk = R"lua(
return {
    publish_one = function()
        game.ui.publish_effect({ effect_id = "core:effect.test.alt_two" })
        return true
    end,
}
)lua";
        std::vector<FLuaSpecCaseResult> CaseResults;
        if (!Session.RunLuaSpec("presentation_effect_conformance_alt", PublishOneChunk, CaseResults, Fault)
            || CaseResults.size() != 1 || !CaseResults[0].Success)
        {
            Session.Stop();
            return "presentation_effect_conformance.alt_lua_spec_failed: " + Fault.Code;
        }
        if (const std::string Err = DrainAndRecord("after_two"); !Err.empty())
        {
            Session.Stop();
            return Err;
        }

        FPresentationEffect Third;
        Third.EffectId = "core:effect.test.alt_three";
        if (!Session.PublishHostLocalEffect(Third, Fault))
        {
            Session.Stop();
            return "presentation_effect_conformance.alt_publish_three_failed: " + Fault.Code;
        }
        if (const std::string Err = DrainAndRecord("after_three"); !Err.empty())
        {
            Session.Stop();
            return Err;
        }

        const std::vector<std::string> Expected = {
            "core:effect.test.alt_one",
            "core:effect.test.alt_two",
            "core:effect.test.alt_three",
        };
        if (ObservedOrder != Expected)
        {
            Session.Stop();
            return "presentation_effect_conformance.alt_order_wrong";
        }

        Session.Stop();
    }

    return "";
}
}
