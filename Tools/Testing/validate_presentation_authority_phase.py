#!/usr/bin/env python3
"""Derives every presentation-authority access from production source and requires
its enclosing function to declare which phase it belongs to.

PAH-08 (ADR-0042, INV-P5): after a prepared plan exists, the application phase
must not resolve a registry entry, resolve a schema, decide package ownership or
read the filesystem. This is the structural half of the two-part gate; the
observational half is a runtime counter bracketed around PrepareReconcile and
CommitReconcile (GV2PresentationAuthorityProbe.h).

Granularity is the FUNCTION, not the translation unit. A translation unit
legitimately holds both phases: GV2PropertyConsumers.cpp contains the tab
consumer's Prepare, which resolves the screen registry as it must, and the same
consumer's Commit. A ban expressed as "these files may not include those
headers" would therefore fire on correct code the day it is written, and the
only way to keep it green would be an exception list -- a hand-written list
inside a gate written against hand-written lists.

Actual side: every occurrence of a small, fixed grammar of authority accessors
anywhere in Source/GV2/{Public,Private} production code. Expected side: a marker
comment immediately above the enclosing function's definition:

  PAH-08: phase=authority
      This function IS an authority entry point (its own definition), not a
      consumer of one.

  PAH-08: phase=prepare
      Reached only while a revision is being prepared, or outside a
      presentation transaction altogether (session start, registry build).
      Documented inline: by whom, and why that is the only caller.

Indirection is closed by propagation, not by scanning the commit path: a
function in the application phase cannot call an unmarked helper that resolves,
because that helper's own enclosing function would then need a marker, and a
marker of `prepare` on something reachable from a Commit root is a contradiction
this gate reports (see COMMIT_ROOT_PATTERN below).

The grammar is deliberately over-broad and fail-closed: `->Resolve(` matches any
call to any Resolve member, not only the two authorities that have one. A new
unrelated Resolve must therefore be classified too. That is noise in the
direction of safety; the opposite error -- a grammar too narrow to see a new
authority -- is the failure this whole plan exists to prevent.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOTS = [
    REPO_ROOT / "Source" / "GV2" / "Public",
    REPO_ROOT / "Source" / "GV2" / "Private",
]
EXCLUDED_DIR_NAME = "Tests"

# Authority accessors. Anchored on the identifiers that name the authorities
# themselves, so a rename shows up as an unclassified access rather than as a
# silently shrinking set.
AUTHORITY_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_])("
    r"GetCompiledSchema"
    r"|GetSchemaCache"
    r"|GetSessionCatalog"
    r"|FindOwningPackageForAssetPath"
    r"|GetPackageLoadOrderFromGameData"
    r"|ResolveContentRootOwnershipFromGameData"
    r")\s*\("
    r"|->\s*Resolve\s*\("
)

# Application-phase roots, same grammar the rollback-boundary gate derives its
# own set from (GBF-07): a definition whose name is Commit* or
# AttachScreenToLayer. A marker is not required on these -- their phase follows
# from the name -- but an authority access inside one is a violation.
COMMIT_ROOT_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_])(?:Commit[A-Z]\w*|Commit|AttachScreenToLayer)\s*\("
)

MARKER_PATTERN = re.compile(
    r"PAH-08:\s*phase=(?P<phase>authority|prepare|commit_resolve_deferred=STATUS-\d+)"
)

DEFERRED_PREFIX = "commit_resolve_deferred="

VALID_PHASES = {"authority", "prepare"}


def strip_comments_preserving_layout(source: str) -> str:
    """Blank out comment bodies without moving any line or column."""

    def blank_block(match: re.Match[str]) -> str:
        return "".join("\n" if ch == "\n" else " " for ch in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank_block, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", lambda m: " " * len(m.group(0)), source)


def iter_production_files() -> list[Path]:
    files: list[Path] = []
    for root in SOURCE_ROOTS:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix not in (".h", ".cpp"):
                continue
            if EXCLUDED_DIR_NAME in path.parts:
                continue
            files.append(path)
    return files


def find_function_definitions(code: str) -> list[tuple[int, int, str, int]]:
    """Return (start_offset, end_offset, name) for every brace-balanced definition.

    Derived from the source, not from a list of function names: a definition is
    an identifier followed by a parameter list and an opening brace at the start
    of a line, which is how every definition in this module is written.
    """
    definitions: list[tuple[int, int, str, int]] = []
    signature = re.compile(
        r"^[A-Za-z_][A-Za-z0-9_:<>,&*\s\[\]]*?"
        r"(?<![A-Za-z0-9_])(?P<name>[A-Za-z_]\w*)\s*\([^;{}]*?\)"
        r"[^;{}]*?\{",
        re.MULTILINE | re.DOTALL,
    )
    for match in signature.finditer(code):
        open_brace = code.index("{", match.end() - 1)
        depth = 0
        end = None
        for index in range(open_brace, len(code)):
            char = code[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = index
                    break
        if end is None:
            continue
        definitions.append((match.start(), end, match.group("name"), open_brace))
    return definitions


def marker_above(raw_source: str, definition_start: int) -> str | None:
    """Read the marker on the comment lines immediately above a definition."""
    head = raw_source[:definition_start]
    lines = head.split("\n")
    # Skip the (possibly empty) partial line the definition starts on.
    index = len(lines) - 2
    while index >= 0:
        line = lines[index].strip()
        if not line:
            break
        if not line.startswith("//") and not line.startswith("*") and not line.startswith("/*"):
            break
        found = MARKER_PATTERN.search(line)
        if found:
            return found.group("phase")
        index -= 1
    return None


def check_source(raw_source: str, path_label: str) -> list[str]:
    problems: list[str] = []
    code = strip_comments_preserving_layout(raw_source)
    definitions = find_function_definitions(code)

    for match in AUTHORITY_PATTERN.finditer(code):
        offset = match.start()
        line_no = raw_source.count("\n", 0, offset) + 1
        access = match.group(0).strip()

        # An occurrence inside a definition's own signature is the definition
        # naming itself, not a call to anything.
        if any(start <= offset < open_brace for start, _end, _name, open_brace in definitions):
            continue

        enclosing = None
        for start, end, name, open_brace in definitions:
            if open_brace < offset <= end:
                if enclosing is None or start > enclosing[0]:
                    enclosing = (start, end, name)
        if enclosing is None:
            if path_label.endswith(".h"):
                # A header occurrence outside any body is a declaration, not an
                # access. In a .cpp the same shape is namespace-scope
                # initialisation -- the very form PKG-R1 was about (a
                # function-local static built from a discovery call) -- so it
                # stays an error there.
                continue
            problems.append(
                f"{path_label}:{line_no}: authority access `{access}` sits at namespace "
                f"scope, outside any function body; a resolution performed during static "
                f"initialisation belongs to no phase and outlives every session"
            )
            continue

        start, _end, name = enclosing
        phase = marker_above(raw_source, start)

        if COMMIT_ROOT_PATTERN.fullmatch(name + "(") is not None:
            problems.append(
                f"{path_label}:{line_no}: `{name}` is an application-phase root "
                f"(Commit*/AttachScreenToLayer) and resolves an authority (`{access}`) -- "
                f"INV-P5 forbids registry/schema/ownership/filesystem resolution once a "
                f"prepared plan exists"
            )
            continue

        if phase is None:
            problems.append(
                f"{path_label}:{line_no}: `{name}` resolves an authority (`{access}`) but "
                f"carries no `PAH-08: phase=` marker above its definition; classify it as "
                f"`authority` (it is the authority) or `prepare` (documented: reached only "
                f"while preparing, or outside a presentation transaction)"
            )
        elif phase.startswith(DEFERRED_PREFIX):
            # A real violation, already recorded as a confirmed divergence with a
            # stable id. Not silently allowed and not silently fixed here: the
            # marker names the row a planner reads, so the deferral cannot rot
            # the way GBH-08's KeyPropertyName did.
            continue
        elif phase not in VALID_PHASES:
            problems.append(f"{path_label}:{line_no}: `{name}` has unknown phase `{phase}`")

    return problems


def run_self_test() -> int:
    """Each half of the rule must actually reject; a gate that only ever passes
    proves nothing about the property it names."""
    cases: list[tuple[str, str]] = [
        (
            "unmarked function resolving an authority",
            "bool SomeHelper(const FString& Id)\n"
            "{\n"
            "    return GetCompiledSchema(Id) != nullptr;\n"
            "}\n",
        ),
        (
            "application-phase root resolving an authority",
            "// PAH-08: phase=prepare\n"
            "bool FThing::CommitScreenFields(const FPlan& Plan)\n"
            "{\n"
            "    return GetSessionCatalog() != nullptr;\n"
            "}\n",
        ),
        (
            "marker removed from an authority definition",
            "GV2ContentCore::FCompiledUiFieldSpecPtr FCache::GetCompiledSchema(\n"
            "    const std::string& SchemaId) const\n"
            "{\n"
            "    return Resolver->Resolve(SchemaId);\n"
            "}\n",
        ),
    ]
    failures: list[str] = []
    for label, snippet in cases:
        with tempfile.TemporaryDirectory() as tmp:
            probe = Path(tmp) / "Probe.cpp"
            probe.write_text(snippet, encoding="utf-8")
            if not check_source(snippet, str(probe)):
                failures.append(f"self-test did not reject: {label}")

    reach_defs = {
        "CommitScreenFields": [("<probe>", 1, None, False)],
        "ApplyThing": [("<probe>", 5, None, False)],
        "ResolveThing": [("<probe>", 9, None, True)],
    }
    reach_calls = {
        "CommitScreenFields": {"ApplyThing"},
        "ApplyThing": {"ResolveThing"},
        "ResolveThing": set(),
    }
    if not check_commit_reachability(reach_defs, reach_calls):
        failures.append("self-test did not reject: authority two calls below a Commit root")

    accepted = (
        "// PAH-08: phase=prepare -- only reached from PrepareScreenFields.\n"
        "bool FThing::PrepareOne(const FString& Id)\n"
        "{\n"
        "    return GetCompiledSchema(Id) != nullptr;\n"
        "}\n"
    )
    if check_source(accepted, "<accepted>"):
        failures.append("self-test rejected a correctly marked prepare-phase function")

    if failures:
        for failure in failures:
            print(f"SELF-TEST FAILURE: {failure}", file=sys.stderr)
        return 1
    print("validate_presentation_authority_phase self-test: 5 cases passed")
    return 0


CALL_PATTERN = re.compile(r"(?<![A-Za-z0-9_.])(?P<name>[A-Za-z_]\w*)\s*\(")

# Control flow and common non-call constructs that syntactically look like calls.
CALL_NOISE = {
    "if", "for", "while", "switch", "return", "sizeof", "catch", "static_cast",
    "const_cast", "reinterpret_cast", "dynamic_cast", "Cast", "TEXT", "check",
    "checkf", "ensure", "ensureMsgf", "MoveTemp", "Forward", "FString", "FName",
    "TArray", "TMap", "TSet", "TOptional", "MakeShared", "MakeUnique", "NewObject",
}


def build_reachability_index() -> tuple[dict[str, list[tuple[str, int, str | None, bool]]], dict[str, set[str]]]:
    """Map every production function name to its definitions and its callees.

    Text-level, like every other gate in this project: a definition's body is
    scanned for identifiers in call position. Over-broad (a name that merely
    looks like a call is counted) and therefore fail-closed in the direction
    that matters -- it can report a path that does not exist, never miss one
    that does.
    """
    definitions_by_name: dict[str, list[tuple[str, int, str | None, bool]]] = {}
    callees_by_name: dict[str, set[str]] = {}

    for path in iter_production_files():
        raw = path.read_text(encoding="utf-8")
        code = strip_comments_preserving_layout(raw)
        label = str(path.relative_to(REPO_ROOT))
        for start, end, name, open_brace in find_function_definitions(code):
            body = code[open_brace : end + 1]
            line_no = raw.count("\n", 0, start) + 1
            phase = marker_above(raw, start)
            resolves = AUTHORITY_PATTERN.search(body) is not None
            definitions_by_name.setdefault(name, []).append((label, line_no, phase, resolves))
            callees = callees_by_name.setdefault(name, set())
            for call in CALL_PATTERN.finditer(body):
                callee = call.group("name")
                if callee != name and callee not in CALL_NOISE:
                    callees.add(callee)

    return definitions_by_name, callees_by_name


def check_commit_reachability(
    definitions_by_name: dict[str, list[tuple[str, int, str | None, bool]]],
    callees_by_name: dict[str, set[str]],
) -> list[str]:
    """Report any authority reachable from an application-phase root.

    This is the half that closes indirection. A scan that only looks inside
    Commit* bodies misses Commit() -> helper() -> service() -> lookup, which is
    exactly how the one real violation in this codebase hides: an image
    consumer's Commit reaches the resource catalog two calls down.
    """
    problems: list[str] = []
    roots = [name for name in definitions_by_name if COMMIT_ROOT_PATTERN.fullmatch(name + "(")]

    for root in sorted(roots):
        seen: set[str] = {root}
        queue: list[tuple[str, list[str]]] = [(root, [root])]
        reported: set[str] = set()
        while queue:
            current, chain = queue.pop(0)
            for callee in sorted(callees_by_name.get(current, set())):
                if callee in seen:
                    continue
                seen.add(callee)
                path = chain + [callee]
                for label, line_no, phase, resolves in definitions_by_name.get(callee, []):
                    if not resolves:
                        continue
                    if phase == "authority":
                        # The authority's own definition: reaching it is the point.
                        continue
                    if phase is not None and phase.startswith(DEFERRED_PREFIX):
                        continue
                    key = f"{label}:{line_no}"
                    if key in reported:
                        continue
                    reported.add(key)
                    problems.append(
                        f"{label}:{line_no}: `{callee}` resolves an authority and is reachable "
                        f"from application-phase root `{root}` via {' -> '.join(path)}; INV-P5 "
                        f"forbids resolution once a prepared plan exists -- the prepared value "
                        f"must already carry what application needs"
                    )
                queue.append((callee, path))

    return problems


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return run_self_test()

    problems: list[str] = []
    scanned = 0
    for path in iter_production_files():
        raw = path.read_text(encoding="utf-8")
        if AUTHORITY_PATTERN.search(strip_comments_preserving_layout(raw)) is None:
            continue
        scanned += 1
        problems.extend(check_source(raw, str(path.relative_to(REPO_ROOT))))

    definitions_by_name, callees_by_name = build_reachability_index()
    problems.extend(check_commit_reachability(definitions_by_name, callees_by_name))

    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        print(
            f"validate_presentation_authority_phase: {len(problems)} unclassified or "
            f"misplaced authority access(es)",
            file=sys.stderr,
        )
        return 1

    print(
        f"validate_presentation_authority_phase: every authority access in "
        f"{scanned} production file(s) is phase-classified"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
