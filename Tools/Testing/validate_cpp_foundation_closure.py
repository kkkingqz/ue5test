#!/usr/bin/env python3
"""CFC-13 inventory gate for plan evidence, mutations, and new native enums.

The plan itself is the actual task/Done enumerator.  The tables below are an
independent review oracle: adding a task, a Done assertion, or a CFC native enum
value without adding evidence makes the standard CTest pipeline fail closed.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import namedtuple
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PLAN_DIR = REPO_ROOT / "Docs" / "Plans" / "CppFoundationClosure"

TaskEvidence = namedtuple(
    "TaskEvidence",
    "done_count ctests ue_tests enumerator oracle production_path",
)


def evidence(
    done_count: int,
    ctests: tuple[str, ...],
    ue_tests: tuple[str, ...],
    enumerator: str,
    oracle: str,
    production_path: str,
) -> TaskEvidence:
    return TaskEvidence(done_count, ctests, ue_tests, enumerator, oracle, production_path)


# Independent mapping reviewed in CFC-13.  done_count binds every individual
# bullet under **Done:**; the check sets may cover several bullets of one task.
TASK_EVIDENCE: dict[str, TaskEvidence] = {
    "CFC-01": evidence(4, ("documentation_contract",), ("GV2.Runtime.ContentCore.SharedFixtureCorpus",), "owner-contract and accepted-ADR links", "normative ownership/lifecycle rules", "documentation validator plus compiled shared fixture"),
    "CFC-02": evidence(5, ("ue_test_report_contract", "mcp_transport_contract", "ue_acceptance_runner_contract"), ("GV2.Runtime.ModuleIdentity",), "UE discovery records and runtime identity", "runner-owned expected revision/fingerprint", "fresh-process UE runner"),
    "CFC-02A": evidence(5, ("test_fixture_ownership_contract", "test_fixture_ownership_negative_contract"), ("GV2.UI.CapabilityObservabilityCollectionForgery",), "actual root and mode-storage token sites", "scoped-owner allowlist and foreign initial mode", "automation fixture construction and teardown"),
    "CFC-03": evidence(4, ("presentation_apply_module_graph_contract", "presentation_apply_module_graph_negative_contract"), ("GV2.Runtime.ModuleIdentity",), "parsed Build.cs plus CMake codemodel", "module/dependency allowlist", "CMake and UBT compiler-negative probes"),
    "CFC-03A": evidence(5, ("headless_hash_field_inventory_contract", "headless_hash_field_inventory_negative_contract", "gv2_headless_self_test"), ("GV2.Runtime.ContentCore.ValueModel", "GV2.Runtime.Session.RunDigest"), "Value constructors and actual manifest/digest fields", "canonical +0.0 and hash-domain fixtures", "public constructors/codecs in both hosts"),
    "CFC-04": evidence(5, ("ui_schema_authority_contract", "ui_schema_authority_negative_contract"), ("GV2.Runtime.Session.UiSchemaSnapshotIsolation",), "materializer declarations and call sites", "independent A/B schemas", "production Prepare context"),
    "CFC-04A": evidence(7, ("session_snapshot_ownership_contract", "session_snapshot_ownership_negative_contract"), ("GV2.Runtime.Session.ScreenRegistrySnapshotIsolation",), "snapshot members and compiled registry keys/placements", "independent A/B registry descriptors", "production snapshot Resolve after failed/successful B"),
    "CFC-04B": evidence(6, ("session_snapshot_ownership_contract", "presentation_apply_field_inventory_contract"), ("GV2.Runtime.Session.PreparedCommitAndFailureInjection",), "prepared owning pointer/member inventory", "expected fields and GC weak references", "Prepare/Commit/Rollback through production widgets"),
    "CFC-05": evidence(6, ("registry_lifecycle_ownership_contract", "registry_lifecycle_ownership_negative_contract", "gv2_headless_self_test"), ("GV2.Runtime.Lifecycle.RegistryLifecycleConformance",), "bootstrap DESCRIPTOR registry participants", "per-participant fault fixtures", "public FRuntimeSession::Start in both hosts"),
    "CFC-05A": evidence(5, ("state_composition_ownership_contract", "state_composition_ownership_negative_contract", "gv2_headless_self_test"), ("GV2.Runtime.Lua.SpecRunnerHost",), "actual native canonical-state access sites", "Lua state-composition specs", "production session bootstrap in both hosts"),
    "CFC-06": evidence(6, ("session_replacement_ownership_contract", "session_replacement_ownership_negative_contract"), ("GV2.Runtime.Session.SequentialSessionsDoNotShareAuthorities", "GV2.Runtime.Session.PreservesProjectionWhenCandidateFails"), "production projection publication/teardown sites", "independent A/B authorities and viewport state", "two real coordinator/subsystem starts"),
    "CFC-07": evidence(6, ("session_transition_ownership_contract", "session_transition_ownership_negative_contract", "gv2_headless_self_test"), ("GV2.Session.Transition.OracleMatrix", "GV2.Session.Transition.SingleVmInvariantAndSequentialLifecycle"), "closed request/phase enums and transition calls", "contract transition matrix", "public lifecycle request processing"),
    "CFC-07A": evidence(6, ("state_composition_ownership_contract", "gv2_headless_self_test"), ("GV2.Runtime.Session.ManifestReplayReproducesSessionPrng", "GV2.Runtime.Session.NewGameSessionsProduceDistinctSeedsAndPrng"), "actual session-start and replay callers", "fixed PRNG vectors and explicit seed fixtures", "UE/headless/replay session starts"),
    "CFC-08": evidence(5, ("save_slot_crash_contract", "save_slot_crash_negative_contract", "gv2_headless_self_test"), ("GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance",), "actual filesystem operation trace", "independent stage/byte-pair tables", "filesystem storage and process crash helper"),
    "CFC-09": evidence(8, ("state_composition_ownership_contract", "gv2_headless_self_test"), ("GV2.Runtime.SaveAndLoad.ProductionRequestSave", "GV2.Runtime.SaveAndLoad.UiAuthoredSaveButton", "GV2.Runtime.SaveAndLoad.CommandRefusalDiscardsSave"), "actual start/composition and outbound-control sites", "Lua state hash and on-disk opaque bytes", "semantic input to safe-point storage write"),
    "CFC-10": evidence(7, ("session_replacement_ownership_contract", "session_transition_ownership_contract", "gv2_headless_self_test"), ("GV2.Runtime.SaveAndLoad.ProductionRequestLoad", "GV2.Runtime.SaveAndLoad.CapturedBytesImmunityToFileOverwrite", "GV2.Runtime.SaveAndLoad.LoadAnotherSaveAndRestart"), "request/phase enums and captured buffer", "independent saved state/hash fixtures", "semantic input through replacement and continued command"),
    "CFC-11": evidence(4, ("documentation_contract", "gv2_headless_self_test"), ("GV2.Runtime.Presentation.LocationSceneDiagnostic", "GV2.Runtime.Presentation.ScreenFieldClosedSchemaRejection"), "schema required-property set", "positive/negative v2 scene fixtures", "production presentation Prepare"),
    "CFC-12": evidence(5, ("gv2_headless_self_test", "gv2_headless_check_scripts"), ("GV2.Runtime.SaveAndLoad.GameplaySlice", "GV2.Runtime.SaveAndLoad.LifecycleStress100"), "Lua spec tiers and production package manifests", "independent expected state/events/bindings", "semantic input, save/load, continued command"),
    "CFC-13": evidence(5, ("cpp_foundation_closure_contract", "cpp_foundation_closure_negative_contract", "documentation_contract"), ("GV2.Runtime.SaveAndLoad.GameplaySlice", "GV2.Runtime.ModuleIdentity"), "actual task headings, Done bullets, CTest and UE registrations", "this independent evidence/mutation policy", "full portable and fresh-process UE runbook"),
}


@dataclass(frozen=True)
class MutationEvidence:
    check: str
    expected_cause: str
    kind: str = "ctest"


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
        ),
    ),
    EnumTestPolicy(
        "ESessionOperationOutcome",
        "Source/GV2/Public/Bridge/GV2BridgeTypes.h",
        (
            "Source/GV2/Private/Tests/GV2SessionTransitionTests.cpp",
            "Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp",
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
        ("Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp",),
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


TASK_HEADING = re.compile(r"^## (CFC-[0-9]+[A-Z]?) — (.+?)\s*$")
TASK_CHECKBOX = re.compile(r"^- \[([ xX])\] (CFC-[0-9]+[A-Z]?) — (.+?)\s*$")


def collect_plan_inventory(plan_dir: Path) -> tuple[dict[str, tuple[str, int]], list[str]]:
    tasks: dict[str, tuple[str, int]] = {}
    errors: list[str] = []
    for path in sorted(plan_dir.glob("*.md")):
        current_id: str | None = None
        current_title = ""
        done_count = 0
        in_done = False
        checkbox_seen = False

        def finish_task() -> None:
            nonlocal current_id, current_title, done_count, checkbox_seen
            if current_id is None:
                return
            if current_id in tasks:
                errors.append(f"duplicate task heading {current_id}")
            else:
                tasks[current_id] = (current_title, done_count)
            if not checkbox_seen:
                errors.append(f"{current_id}: missing matching task checkbox")

        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            heading = TASK_HEADING.match(line)
            if heading:
                finish_task()
                current_id, current_title = heading.group(1), heading.group(2)
                done_count = 0
                in_done = False
                checkbox_seen = False
                continue
            if current_id is None:
                continue
            checkbox = TASK_CHECKBOX.match(line)
            if checkbox:
                if checkbox.group(2) != current_id or checkbox.group(3) != current_title:
                    errors.append(f"{path}:{line_number}: checkbox does not match heading {current_id}")
                if checkbox_seen:
                    errors.append(f"{path}:{line_number}: duplicate checkbox for {current_id}")
                checkbox_seen = True
                continue
            if line == "**Done:**":
                in_done = True
                continue
            if in_done and line.startswith("**"):
                in_done = False
            elif in_done and line.startswith("- "):
                done_count += 1
        finish_task()
    return tasks, errors


def validate_plan_inventory(plan_dir: Path, evidence_map: dict[str, TaskEvidence]) -> list[str]:
    tasks, errors = collect_plan_inventory(plan_dir)
    for task_id, (_, done_count) in sorted(tasks.items()):
        evidence_row = evidence_map.get(task_id)
        if evidence_row is None:
            errors.append(f"unmapped task {task_id}")
            continue
        if evidence_row.done_count != done_count:
            errors.append(
                f"{task_id}: Done count is {done_count}, evidence maps {evidence_row.done_count}"
            )
        for field_name in ("enumerator", "oracle", "production_path"):
            if not getattr(evidence_row, field_name).strip():
                errors.append(f"{task_id}: empty {field_name}")
        if not evidence_row.ctests and not evidence_row.ue_tests:
            errors.append(f"{task_id}: no named executable checks")
    for task_id in sorted(set(evidence_map) - set(tasks)):
        errors.append(f"evidence row {task_id} has no actual task heading")
    return errors


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
    for task_id, row in TASK_EVIDENCE.items():
        for check in row.ctests:
            if check not in ctests:
                errors.append(f"{task_id}: unknown CTest check {check}")
        for check in row.ue_tests:
            if check not in ue_tests:
                errors.append(f"{task_id}: unknown UE check {check}")
    for mutation_id, row in TARGETED_MUTATIONS.items():
        known = ue_tests if row.kind == "ue" else ctests
        if row.check not in known:
            errors.append(f"mutation {mutation_id}: unknown {row.kind} check {row.check}")
        if not row.expected_cause.strip():
            errors.append(f"mutation {mutation_id}: empty expected cause")
    return errors


def validate_repository(repo_root: Path = REPO_ROOT) -> list[str]:
    errors = validate_plan_inventory(repo_root / "Docs" / "Plans" / "CppFoundationClosure", TASK_EVIDENCE)
    errors.extend(validate_named_checks(repo_root))
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

    tasks, inventory_errors = collect_plan_inventory(PLAN_DIR)
    if inventory_errors:
        errors.extend(f"self-test baseline: {error}" for error in inventory_errors)
    elif not tasks:
        errors.append("self-test baseline: plan task enumerator returned an empty set")
    else:
        dropped = dict(TASK_EVIDENCE)
        dropped.pop(next(iter(tasks)), None)
        diagnostics = validate_plan_inventory(PLAN_DIR, dropped)
        if not any("unmapped task" in diagnostic for diagnostic in diagnostics):
            errors.append("self-test: an unmapped task did not fail")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    errors = run_self_test() if args.self_test else validate_repository()
    if errors:
        print("CFC-13 foundation inventory validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    if args.self_test:
        print("SUCCESS: CFC-13 inventory self-test rejected unmapped task and enum mutations.")
    else:
        task_count = len(TASK_EVIDENCE)
        done_count = sum(row.done_count for row in TASK_EVIDENCE.values())
        print(
            f"SUCCESS: mapped {task_count} CFC tasks and {done_count} Done assertions; "
            f"classified {len(TARGETED_MUTATIONS)} targeted mutations and verified native enum dispatch."
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
