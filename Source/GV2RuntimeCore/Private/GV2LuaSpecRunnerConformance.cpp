#include "GV2RuntimeCore/Testing/GV2LuaSpecRunnerConformance.h"

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

// A trivial production-style module, loaded by the real bootstrap, so specs
// have something real to require().
const char* LoadedMarkerSource = R"lua(
local M = {
    id = "core:module.test.loaded_marker",
    value = 42,
}
return M
)lua";

bool StartFixtureSession(FRuntimeSession& Session, FRuntimeFault& OutFault)
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    const std::vector<FRuntimeSource> Sources = {
        {
            "@Scripts/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.test.loaded_marker",
                modules = {
                    {
                        module_id = "core:module.test.loaded_marker",
                        source = "test/loaded_marker.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {"@Scripts/test/loaded_marker.lua", LoadedMarkerSource},
    };

    return Session.Start(1, RepoHandle, Sources, OutFault);
}
} // namespace

std::string RunLuaSpecRunnerConformance()
{
    // 1 + 7: pass/fail recorded, case failure does not abort remaining
    // cases, and case order is deterministic ascending by case id
    // regardless of source insertion order (cases below are declared
    // z_case, a_case, m_case).
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartFixtureSession(Session, Fault))
        {
            return "lua_spec_runner.fixture_session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }

        const std::string SpecSource = R"lua(
            return {
                z_case = function() end,
                a_case = function() error("boom") end,
                m_case = function() end,
            }
        )lua";

        std::vector<FLuaSpecCaseResult> Results;
        FRuntimeFault SpecFault;
        const bool Ok = Session.RunLuaSpec("@spec.lua", SpecSource, Results, SpecFault);
        Session.Stop();

        if (!Ok)
        {
            return "lua_spec_runner.well_formed_spec_reported_fault";
        }
        if (Results.size() != 3)
        {
            return "lua_spec_runner.all_cases_recorded";
        }
        if (Results[0].CaseId != "a_case" || Results[1].CaseId != "m_case" || Results[2].CaseId != "z_case")
        {
            return "lua_spec_runner.deterministic_case_order";
        }
        if (Results[0].Success)
        {
            return "lua_spec_runner.failing_case_recorded_as_failure";
        }
        if (!Results[1].Success || !Results[2].Success)
        {
            return "lua_spec_runner.passing_cases_recorded_as_success";
        }
    }

    // 3: require() succeeds for an already-loaded module.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartFixtureSession(Session, Fault))
        {
            return "lua_spec_runner.fixture_session_start_failed_require";
        }

        const std::string SpecSource = R"lua(
            return {
                requires_loaded_module = function()
                    local marker = require("core:module.test.loaded_marker")
                    assert(marker.value == 42, "require must return the already-loaded module")
                end,
            }
        )lua";

        std::vector<FLuaSpecCaseResult> Results;
        FRuntimeFault SpecFault;
        const bool Ok = Session.RunLuaSpec("@spec.lua", SpecSource, Results, SpecFault);
        Session.Stop();

        if (!Ok || Results.size() != 1 || !Results[0].Success)
        {
            return "lua_spec_runner.require_already_loaded_module";
        }
    }

    // 4: empty table is rejected as a fault, not zero passing cases.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartFixtureSession(Session, Fault))
        {
            return "lua_spec_runner.fixture_session_start_failed_empty";
        }

        std::vector<FLuaSpecCaseResult> Results;
        FRuntimeFault SpecFault;
        const bool Ok = Session.RunLuaSpec("@spec.lua", "return {}", Results, SpecFault);
        Session.Stop();

        if (Ok || SpecFault.Code != "LuaSpecEmpty")
        {
            return "lua_spec_runner.empty_table_rejected";
        }
    }

    // 5a: non-table return value is rejected.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartFixtureSession(Session, Fault))
        {
            return "lua_spec_runner.fixture_session_start_failed_non_table";
        }

        std::vector<FLuaSpecCaseResult> Results;
        FRuntimeFault SpecFault;
        const bool Ok = Session.RunLuaSpec("@spec.lua", "return 42", Results, SpecFault);
        Session.Stop();

        if (Ok || SpecFault.Code != "LuaSpecFormatInvalid")
        {
            return "lua_spec_runner.non_table_return_rejected";
        }
    }

    // 5b: a non-function value inside the table is rejected.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        if (!StartFixtureSession(Session, Fault))
        {
            return "lua_spec_runner.fixture_session_start_failed_non_function";
        }

        std::vector<FLuaSpecCaseResult> Results;
        FRuntimeFault SpecFault;
        const bool Ok = Session.RunLuaSpec("@spec.lua", "return { not_a_case = 1 }", Results, SpecFault);
        Session.Stop();

        if (Ok || SpecFault.Code != "LuaSpecFormatInvalid")
        {
            return "lua_spec_runner.non_function_value_rejected";
        }
    }

    return "";
}
} // namespace GV2RuntimeCore::Testing
