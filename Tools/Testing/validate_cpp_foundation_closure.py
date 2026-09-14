#!/usr/bin/env python3
"""Native surface gate for the accepted C++ foundation baseline.

The plan that produced this baseline is archived; what survives it is the
guarantee, not the bookkeeping.  Three claims are enforced here: the named
regression checks the baseline was accepted on still exist, every value of a
CFC-introduced native enum reaches exhaustive production dispatch and a test
inventory, and the repository Lua callback cannot longjmp over C++ RAII.

Actual sets come from the tree: CTest registrations are parsed out of every
CMakeLists.txt, UE test names out of the automation sources, enum values out of
their declarations.  The tables below are the independent expected side —
deleting or renaming a check without touching them fails the standard CTest
pipeline closed.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# Named checks the accepted baseline relies on.  Every entry is verified against
# the actual CTest and automation registrations, so a deleted or silently
# renamed regression check is a gate failure rather than a quiet loss.
BASELINE_CTESTS: tuple[str, ...] = (
    "cpp_foundation_closure_contract",
    "cpp_foundation_closure_negative_contract",
    "documentation_contract",
    "gv2_headless_check_scripts",
    "gv2_headless_self_test",
    "headless_hash_field_inventory_contract",
    "headless_hash_field_inventory_negative_contract",
    "mcp_transport_contract",
    "presentation_apply_field_inventory_contract",
    "presentation_apply_module_graph_contract",
    "presentation_apply_module_graph_negative_contract",
    "registry_lifecycle_ownership_contract",
    "registry_lifecycle_ownership_negative_contract",
    "save_slot_crash_contract",
    "save_slot_crash_negative_contract",
    "session_replacement_ownership_contract",
    "session_replacement_ownership_negative_contract",
    "session_snapshot_ownership_contract",
    "session_snapshot_ownership_negative_contract",
    "session_transition_ownership_contract",
    "session_transition_ownership_negative_contract",
    "state_composition_ownership_contract",
    "state_composition_ownership_negative_contract",
    "test_fixture_ownership_contract",
    "test_fixture_ownership_negative_contract",
    "ue_acceptance_runner_contract",
    "ue_test_report_contract",
    "ui_schema_authority_contract",
    "ui_schema_authority_negative_contract",
)

BASELINE_UE_TESTS: tuple[str, ...] = (
    "GV2.Runtime.ContentCore.SharedFixtureCorpus",
    "GV2.Runtime.ContentCore.ValueModel",
    "GV2.Runtime.Lifecycle.RegistryLifecycleConformance",
    "GV2.Runtime.Lua.SpecRunnerHost",
    "GV2.Runtime.ModuleIdentity",
    "GV2.Runtime.Presentation.LocationSceneDiagnostic",
    "GV2.Runtime.Presentation.ScreenFieldClosedSchemaRejection",
    "GV2.Runtime.SaveAndLoad.CapturedBytesImmunityToFileOverwrite",
    "GV2.Runtime.SaveAndLoad.CommandRefusalDiscardsSave",
    "GV2.Runtime.SaveAndLoad.GameplaySlice",
    "GV2.Runtime.SaveAndLoad.LifecycleStress100",
    "GV2.Runtime.SaveAndLoad.LoadAnotherSaveAndRestart",
    "GV2.Runtime.SaveAndLoad.ProductionRequestLoad",
    "GV2.Runtime.SaveAndLoad.ProductionRequestSave",
    "GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance",
    "GV2.Runtime.SaveAndLoad.UiAuthoredSaveButton",
    "GV2.Runtime.Session.ManifestReplayReproducesSessionPrng",
    "GV2.Runtime.Session.NewGameSessionsProduceDistinctSeedsAndPrng",
    "GV2.Runtime.Session.PreparedCommitAndFailureInjection",
    "GV2.Runtime.Session.PreservesProjectionWhenCandidateFails",
    "GV2.Runtime.Session.RunDigest",
    "GV2.Runtime.Session.ScreenRegistrySnapshotIsolation",
    "GV2.Runtime.Session.SequentialSessionsDoNotShareAuthorities",
    "GV2.Runtime.Session.UiSchemaSnapshotIsolation",
    "GV2.Session.Transition.OracleMatrix",
    "GV2.Session.Transition.SingleVmInvariantAndSequentialLifecycle",
    "GV2.UI.CapabilityObservabilityCollectionForgery",
)


@dataclass(frozen=True)
class MutationEvidence:
    check: str
    expected_cause: str
    kind: str = "ctest"


# Each accepted regression cause and the check that must reject it.  The table
# records what the baseline was actually mutation-tested against; it does not
# replace running those mutations on a revision under review.
TARGETED_MUTATIONS: dict[str, MutationEvidence] = {
    "second_schema_source": MutationEvidence("ui_schema_authority_negative_contract", "forbidden schema authority/discovery"),
    "shared_mutable_screen_registry": MutationEvidence("session_snapshot_ownership_negative_contract", "mutable or shared registry authority"),
    "stale_candidate": MutationEvidence("session_replacement_ownership_negative_contract", "stale/ambient candidate access"),
    "early_host_teardown": MutationEvidence("session_replacement_ownership_negative_contract", "projection teardown outside owner protocol"),
    "ignored_freeze_result": MutationEvidence("registry_lifecycle_ownership_negative_contract", "ignored registry sealing result"),
    "disconnected_storage": MutationEvidence("GV2.Runtime.SaveAndLoad.ProductionRequestSave", "production storage capability unavailable", "ue"),
    "lost_previous_copy": MutationEvidence("save_slot_crash_contract", "Current/Previous byte-pair mismatch"),
    "forbidden_dependency_statement": MutationEvidence("presentation_apply_module_graph_negative_contract", "forbidden or unparsed Build.cs dependency"),
    "not_run": MutationEvidence("ue_test_report_contract", "non-success NotRun state"),
    "missing_ue_record": MutationEvidence("ue_test_report_contract", "discovered/completed set mismatch"),
    "untraced_owning_pointer": MutationEvidence("session_snapshot_ownership_negative_contract", "unclassified owning pointer/member"),
    "leaked_fixture_state": MutationEvidence("test_fixture_ownership_negative_contract", "unscoped root or global mode writer"),
    "native_state_merge": MutationEvidence("state_composition_ownership_negative_contract", "native semantic state access/merge"),
    "ignored_seed": MutationEvidence("GV2.Runtime.Session.ManifestReplayReproducesSessionPrng", "seeded replay divergence", "ue"),
    "signed_zero": MutationEvidence("gv2_headless_self_test", "canonical Number zero/hash mismatch"),
    "len_only_hash": MutationEvidence("headless_hash_field_inventory_negative_contract", "missing IsCanonicalSha256 validation"),
}


@dataclass(frozen=True)
class EnumDispatchPolicy:
    enum_name: str
    header: str
    consumers: tuple[str, ...]


ENUM_DISPATCH_POLICIES = (
    EnumDispatchPolicy(
        "ESessionStartMode",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        ("Source/GV2/Private/Application/GV2SessionTransition.h",),
    ),
    EnumDispatchPolicy(
        "ERuntimeLifecyclePhase",
        "Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h",
        ("Source/GV2/Private/Application/GV2SessionCoordinator.cpp",),
    ),
)


@dataclass(frozen=True)
class EnumTestPolicy:
    enum_name: str
    header: str
    tests: tuple[str, ...]


ENUM_TEST_POLICIES = (
    EnumTestPolicy(
        "ESessionStartMode",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        (
            "Source/GV2/Private/Tests/GV2SessionTransitionTests.cpp",
            "Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp",
            "Source/GV2/Private/Tests/GV2SaveAndLoadTests.cpp",
        ),
    ),
    EnumTestPolicy(
        "ESessionOperationOutcome",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        (
            "Source/GV2/Private/Tests/GV2SessionTransitionTests.cpp",
            "Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp",
            "Source/GV2/Private/Tests/GV2SaveAndLoadTests.cpp",
        ),
    ),
    EnumTestPolicy(
        "ESessionCancellationResult",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        ("Source/GV2/Private/Tests/GV2SessionTransitionTests.cpp",),
    ),
    EnumTestPolicy(
        "EGV2SaveSlotRevision",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        (
            "Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp",
            "Source/GV2/Private/Tests/GV2SaveAndLoadTests.cpp",
        ),
    ),
    EnumTestPolicy(
        "ERuntimeLifecyclePhase",
        "Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h",
        ("Source/GV2RuntimeCore/Private/GV2ColdStartLoadConformance.cpp",),
    ),
    EnumTestPolicy(
        "ERuntimePhaseResultKind",
        "Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h",
        ("Source/GV2RuntimeCore/Private/GV2ColdStartLoadConformance.cpp",),
    ),
    EnumTestPolicy(
        "ESaveSlotRevision",
        "Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2HostServices.h",
        ("Source/GV2RuntimeCore/Private/GV2SaveSlotStorageConformance.cpp",),
    ),
    EnumTestPolicy(
        "ESaveSlotResult",
        "Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2HostServices.h",
        ("Source/GV2RuntimeCore/Private/GV2SaveSlotStorageConformance.cpp",),
    ),
)


def collect_ctest_names(repo_root: Path) -> set[str]:
    names: set[str] = set()
    pattern = re.compile(r"\badd_test\s*\(\s*(?:NAME\s+)?([^\s\)]+)", re.MULTILINE)
    headless_contract = re.compile(
        r"\bgv2_add_headless_json_contract_test\s*\(\s*([^\s\)]+)",
        re.MULTILINE,
    )
    for path in repo_root.rglob("CMakeLists.txt"):
        if any(part.startswith("cmake-build") for part in path.parts):
            continue
        text = path.read_text(encoding="utf-8")
        names.update(pattern.findall(text))
        names.update(headless_contract.findall(text))
    return names


def collect_ue_test_names(repo_root: Path) -> set[str]:
    names: set[str] = set()
    pattern = re.compile(r'"(GV2\.[A-Za-z0-9_.-]+)"')
    test_root = repo_root / "Source" / "GV2" / "Private" / "Tests"
    for path in test_root.rglob("*.cpp"):
        names.update(pattern.findall(path.read_text(encoding="utf-8")))
    return names


def strip_cpp_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//.*", "", source)


def parse_enum_values(enum_name: str, header: str) -> list[str]:
    clean = strip_cpp_comments(header)
    match = re.search(rf"\benum\s+class\s+{re.escape(enum_name)}\b[^{{]*\{{(.*?)\}}\s*;", clean, re.DOTALL)
    if match is None:
        return []
    values: list[str] = []
    for item in match.group(1).split(","):
        token = item.strip()
        if not token:
            continue
        token = token.split("=", 1)[0].strip()
        token = re.sub(r"\bUMETA\s*\(.*\)\s*$", "", token).strip()
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", token):
            values.append(token)
    return values


def validate_enum_dispatch(enum_name: str, header: str, consumers: dict[str, str]) -> list[str]:
    values = parse_enum_values(enum_name, header)
    if not values:
        return [f"{enum_name}: enum declaration is missing or empty"]
    all_consumers = "\n".join(strip_cpp_comments(text) for text in consumers.values())
    relevant_switches = [
        match.group(1)
        for match in re.finditer(r"\bswitch\s*\([^)]*\)\s*\{(.*?)\n\s*\}", all_consumers, re.DOTALL)
        if re.search(rf"\bcase\s+(?:[A-Za-z_][A-Za-z0-9_]*::)*{re.escape(enum_name)}::", match.group(1))
    ]
    relevant_text = "\n".join(relevant_switches)
    cases = set(re.findall(rf"\bcase\s+(?:[A-Za-z_][A-Za-z0-9_]*::)*{re.escape(enum_name)}::([A-Za-z_][A-Za-z0-9_]*)\s*:", relevant_text))
    errors: list[str] = []
    for value in values:
        if value not in cases:
            errors.append(f"{enum_name}: value {value} is missing from exhaustive production dispatch")
    extra = cases - set(values)
    for value in sorted(extra):
        errors.append(f"{enum_name}: dispatch contains unknown value {value}")
    if re.search(r"\bdefault\s*:", relevant_text):
        errors.append(f"{enum_name}: exhaustive production dispatch contains forbidden default label")
    return errors


def validate_enum_test_inventory(enum_name: str, header: str, tests: dict[str, str]) -> list[str]:
    values = parse_enum_values(enum_name, header)
    if not values:
        return [f"{enum_name}: enum declaration is missing or empty"]
    test_text = "\n".join(strip_cpp_comments(text) for text in tests.values())
    errors: list[str] = []
    for value in values:
        if re.search(
            rf"\b(?:[A-Za-z_][A-Za-z0-9_]*::)*{re.escape(enum_name)}::{re.escape(value)}\b",
            test_text,
        ) is None:
            errors.append(f"{enum_name}: value {value} is missing from test inventory")
    return errors


def validate_lua_callback_error_boundary(source: str) -> list[str]:
    """Keep repository.require's Lua longjmp outside all non-trivial C++ locals."""
    clean = strip_cpp_comments(source)
    errors: list[str] = []
    body_start = clean.find("static int RepositoryRequireBody")
    section_end = clean.find("static int RepositoryList", max(body_start, 0))
    if body_start == -1 or section_end == -1:
        suffix = "; direct luaL_error is forbidden" if "luaL_error" in clean else ""
        return [f"RepositoryRequire must use a body plus trivial Lua-error trampoline{suffix}"]

    section = clean[body_start:section_end]
    if "luaL_error" in section:
        errors.append(
            "RepositoryRequire contains luaL_error; Lua longjmp would bypass C++ RAII destructors"
        )

    trampoline = re.compile(
        r"static\s+int\s+RepositoryRequire\s*\(\s*lua_State\s*\*\s*InState\s*\)\s*\{"
        r"\s*const\s+int\s+Result\s*=\s*RepositoryRequireBody\s*\(\s*InState\s*\)\s*;"
        r"\s*if\s*\(\s*Result\s*==\s*LuaErrorPending\s*\)\s*\{"
        r"\s*return\s+lua_error\s*\(\s*InState\s*\)\s*;\s*\}"
        r"\s*return\s+Result\s*;\s*\}",
        re.DOTALL,
    )
    if trampoline.search(section) is None:
        errors.append(
            "RepositoryRequire must raise only from the trivial trampoline after RepositoryRequireBody returns"
        )
    return errors


def validate_named_checks(repo_root: Path) -> list[str]:
    ctests = collect_ctest_names(repo_root)
    ue_tests = collect_ue_test_names(repo_root)
    errors: list[str] = []
    for check in BASELINE_CTESTS:
        if check not in ctests:
            errors.append(f"baseline CTest check {check} is no longer registered")
    for check in BASELINE_UE_TESTS:
        if check not in ue_tests:
            errors.append(f"baseline UE check {check} is no longer registered")
    for mutation_id, row in TARGETED_MUTATIONS.items():
        known = BASELINE_UE_TESTS if row.kind == "ue" else BASELINE_CTESTS
        if row.check not in known:
            errors.append(f"mutation {mutation_id}: {row.kind} check {row.check} is outside the baseline inventory")
        if not row.expected_cause.strip():
            errors.append(f"mutation {mutation_id}: empty expected cause")
    return errors


def validate_repository(repo_root: Path = REPO_ROOT) -> list[str]:
    errors = validate_named_checks(repo_root)
    for policy in ENUM_DISPATCH_POLICIES:
        header_path = repo_root / policy.header
        consumers = {
            relative: (repo_root / relative).read_text(encoding="utf-8")
            for relative in policy.consumers
        }
        errors.extend(
            validate_enum_dispatch(
                policy.enum_name,
                header_path.read_text(encoding="utf-8"),
                consumers,
            )
        )
    for policy in ENUM_TEST_POLICIES:
        header_path = repo_root / policy.header
        tests = {
            relative: (repo_root / relative).read_text(encoding="utf-8")
            for relative in policy.tests
        }
        errors.extend(
            validate_enum_test_inventory(
                policy.enum_name,
                header_path.read_text(encoding="utf-8"),
                tests,
            )
        )
    runtime_session = (
        repo_root / "Source" / "GV2RuntimeCore" / "Private" / "GV2RuntimeSession.cpp"
    ).read_text(encoding="utf-8")
    errors.extend(validate_lua_callback_error_boundary(runtime_session))
    return errors


def run_self_test() -> list[str]:
    errors: list[str] = []
    synthetic_header = "enum class ERuntimeLifecyclePhase { Registering, Starting, Recovering };"
    synthetic_consumer = {
        "consumer.cpp": "switch (Phase) { case ERuntimeLifecyclePhase::Registering: break; case ERuntimeLifecyclePhase::Starting: break; }"
    }
    diagnostics = validate_enum_dispatch("ERuntimeLifecyclePhase", synthetic_header, synthetic_consumer)
    if not any("Recovering" in diagnostic for diagnostic in diagnostics):
        errors.append("self-test: an unhandled enum value did not fail")

    unsafe_lua_callback = """
    static int RepositoryRequire(lua_State* State) {
        std::string Code = "not_found";
        return luaL_error(State, "%s", Code.c_str());
    }
    static int RepositoryList(lua_State*) { return 0; }
    """
    diagnostics = validate_lua_callback_error_boundary(unsafe_lua_callback)
    if not diagnostics:
        errors.append("self-test: Lua error longjmp over repository RAII did not fail")

    baseline_diagnostics = validate_named_checks(REPO_ROOT)
    if baseline_diagnostics:
        errors.extend(f"self-test baseline: {error}" for error in baseline_diagnostics)
    else:
        with tempfile.TemporaryDirectory() as temp_dir:
            empty_root = Path(temp_dir)
            (empty_root / "CMakeLists.txt").write_text("", encoding="utf-8")
            diagnostics = validate_named_checks(empty_root)
        if not any("no longer registered" in diagnostic for diagnostic in diagnostics):
            errors.append("self-test: a removed baseline check did not fail")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    errors = run_self_test() if args.self_test else validate_repository()
    if errors:
        print("C++ foundation baseline validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    if args.self_test:
        print("SUCCESS: baseline self-test rejected a removed check and enum/RAII mutations.")
    else:
        print(
            f"SUCCESS: confirmed {len(BASELINE_CTESTS)} CTest and {len(BASELINE_UE_TESTS)} UE baseline checks, "
            f"{len(TARGETED_MUTATIONS)} classified targeted mutations and native enum dispatch."
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
