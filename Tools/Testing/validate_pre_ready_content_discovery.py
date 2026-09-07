#!/usr/bin/env python3
"""Derives every content-discovery call site from production source and requires
each to be classified as reachable only before a session reaches Ready.

PAH-04A (ADR-0042, INV-P1): after a session's Status.bIsReady is set, presentation
code must not discover content -- no directory walk, no raw file read, no package
scan. The actual side is every occurrence of a small, fixed grammar of filesystem/
discovery primitives (std::filesystem directory iteration, IFileManager::FindFiles*,
FFileHelper::LoadFileTo*, std::ifstream, GV2ContentHostSupport::Discover*,
GV2PackageClosure::DiscoverFromGameData) anywhere in Source/GV2/{Public,Private}
production code. Each occurrence's enclosing function must carry a PAH-04 marker
comment immediately above its definition:

  PAH-04: pre_ready_discovery
      Every caller of this function is, transitively, only ever reached before a
      session reaches Ready (documented inline: which caller, why).

  PAH-04: pre_ready_discovery_deferred=<TASK-ID>
      A real, currently post-Ready-reachable discovery call, already named as a
      defect for a specific later task in this plan (e.g. PAH-04B) -- not silently
      allowed, not silently fixed here.

An unmarked occurrence is a violation: either a new discovery call that was never
classified, or an existing one whose marker was removed.
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
SCANNED_SUFFIXES = (".h", ".cpp")

DISCOVERY_GRAMMAR = re.compile(
    r"std::filesystem::recursive_directory_iterator"
    r"|std::filesystem::directory_iterator"
    r"|IFileManager::Get\(\)\s*(?:\.|->)\s*FindFiles\w*"
    r"|FFileHelper::LoadFileToArray"
    r"|FFileHelper::LoadFileToString"
    r"|std::ifstream"
    r"|GV2ContentHostSupport::Discover\w+"
    r"|GV2PackageClosure::DiscoverFromGameData"
)

MARKER_PATTERN = re.compile(r"PAH-04:\s*pre_ready_discovery\b(?!_deferred)")
DEFERRED_MARKER_PATTERN = re.compile(r"PAH-04:\s*pre_ready_discovery_deferred=(?P<task>[A-Za-z0-9_-]+)")

# A definition: an identifier (optionally scoped, e.g. Foo::Bar) followed by '(',
# a parameter list, and eventually a '{' before the next ';' -- mirrors the
# established idiom in validate_ui_rollback_boundaries.py. Excludes C++ control-flow
# and operator keywords, which have the exact same "name(...) {" shape as a function
# definition (an `if (...)  { ... }` block is indistinguishable from one by that
# shape alone) and would otherwise be misidentified as enclosing function signatures.
STATEMENT_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "return", "sizeof",
    "static_assert", "decltype", "explicit", "noexcept", "alignof",
    "static_cast", "reinterpret_cast", "dynamic_cast", "const_cast",
}
FUNCTION_NAME_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_])(?P<scope>(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*::)?)(?P<name>[A-Za-z_]\w*)\s*\("
)


def mask_comments_and_literals(source: str) -> str:
    """Preserve offsets while removing syntax that may contain fake matches."""
    masked = list(source)
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index)
            if end < 0:
                end = len(source)
            for masked_index in range(index, end):
                masked[masked_index] = " "
            index = end
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = len(source) if end < 0 else end + 2
            for masked_index in range(index, end):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        if source[index] in {'"', "'"}:
            quote = source[index]
            end = index + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                    continue
                if source[end] == quote:
                    end += 1
                    break
                end += 1
            for masked_index in range(index, min(end, len(source))):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        index += 1
    return "".join(masked)


def matching_brace(source: str, open_brace: int) -> int | None:
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def is_function_definition(source: str, open_paren: int) -> tuple[bool, int | None, int | None]:
    """Mirrors validate_ui_rollback_boundaries.py's is_function_definition, but also
    returns the [start, end) body span so a discovery call site can be attributed to
    the function whose body actually contains it, not just the nearest signature."""
    depth = 0
    close_paren = None
    for index in range(open_paren, len(source)):
        if source[index] == "(":
            depth += 1
        elif source[index] == ")":
            depth -= 1
            if depth == 0:
                close_paren = index
                break
    if close_paren is None:
        return False, None, None

    suffix = close_paren + 1
    while suffix < len(source) and source[suffix].isspace():
        suffix += 1
    # A ')' or ',' immediately after this candidate's own close paren means it's a
    # nested call inside an outer expression (e.g. `for (... : Outer(Inner(x))) {`
    # or `if (Object->Commit())`), not a declaration -- mirrors
    # validate_ui_rollback_boundaries.py's is_function_definition guard.
    if suffix < len(source) and source[suffix] in (")", ","):
        return False, None, None
    # Skip a trailing qualifier like 'const' between ')' and '{'.
    while suffix < len(source) and source[suffix : suffix + 5] == "const":
        suffix += 5
        while suffix < len(source) and source[suffix].isspace():
            suffix += 1

    next_brace = source.find("{", suffix)
    next_semicolon = source.find(";", suffix)
    if next_brace < 0 or (0 <= next_semicolon < next_brace):
        return False, None, None

    body_end = matching_brace(source, next_brace)
    if body_end is None:
        return False, None, None
    return True, next_brace, body_end


def iter_function_bodies(masked_source: str):
    """Yields (signature_start, body_start, body_end) for every production function
    definition found in masked_source, outermost first (no dedup of nested ones --
    callers pick the innermost span that contains a given position)."""
    for match in FUNCTION_NAME_PATTERN.finditer(masked_source):
        if not match.group("scope") and match.group("name") in STATEMENT_KEYWORDS:
            continue
        open_paren = masked_source.find("(", match.start(), match.end())
        if open_paren < 0:
            continue
        is_def, body_start, body_end = is_function_definition(masked_source, open_paren)
        if is_def:
            yield match.start(), body_start, body_end


def enclosing_function_signature_start(masked_source: str, position: int) -> int | None:
    """The innermost function whose body span contains position, by signature start."""
    best: int | None = None
    best_body_start = -1
    for signature_start, body_start, body_end in iter_function_bodies(masked_source):
        if body_start <= position < body_end and body_start > best_body_start:
            best = signature_start
            best_body_start = body_start
    return best


def marker_before(source: str, signature_start: int) -> str | None:
    line_start = source.rfind("\n", 0, signature_start) + 1
    for line in reversed(source[:line_start].splitlines()):
        stripped = line.strip()
        if not stripped:
            continue
        if MARKER_PATTERN.search(stripped):
            return "pre_ready_discovery"
        deferred = DEFERRED_MARKER_PATTERN.search(stripped)
        if deferred:
            return f"pre_ready_discovery_deferred={deferred.group('task')}"
        if stripped.startswith("//"):
            continue
        return None
    return None


def find_violations(source_files: dict[Path, str]) -> list[str]:
    violations: list[str] = []
    for path, source in source_files.items():
        masked = mask_comments_and_literals(source)
        for match in DISCOVERY_GRAMMAR.finditer(masked):
            signature_start = enclosing_function_signature_start(masked, match.start())
            line_number = source.count("\n", 0, match.start()) + 1
            if signature_start is None:
                violations.append(
                    f"{path}:{line_number}: content-discovery call '{match.group(0)}' is not inside "
                    "any recognized function definition -- add a PAH-04 marker to its enclosing function"
                )
                continue
            marker = marker_before(source, signature_start)
            if marker is None:
                func_line = source.count("\n", 0, signature_start) + 1
                violations.append(
                    f"{path}:{line_number}: content-discovery call '{match.group(0)}' "
                    f"(enclosing function at line {func_line}) has no PAH-04 marker -- add "
                    "'// PAH-04: pre_ready_discovery' (with justification) or "
                    "'// PAH-04: pre_ready_discovery_deferred=<TASK-ID>'"
                )
    return violations


def production_sources() -> dict[Path, str]:
    sources: dict[Path, str] = {}
    for root in SOURCE_ROOTS:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if not path.is_file() or path.suffix not in SCANNED_SUFFIXES:
                continue
            if EXCLUDED_DIR_NAME in path.relative_to(root).parts:
                continue
            sources[path] = path.read_text(encoding="utf-8")
    return sources


def validate_repository() -> list[str]:
    return find_violations(production_sources())


def run_self_test() -> bool:
    print("[*] Running validate_pre_ready_content_discovery self-test...")
    if errors := validate_repository():
        print("FAILED: current production source violates the gate:\n" + "\n".join(errors))
        return False

    sources = production_sources()
    with tempfile.TemporaryDirectory() as tmpdir:
        synthetic_path = Path(tmpdir) / "SyntheticDiscovery.cpp"

        sources[synthetic_path] = (
            "void FSynthetic::ScanUnmarked()\n"
            "{\n"
            "    std::vector<std::filesystem::path> Roots;\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Roots[0])) {}\n"
            "}\n"
        )
        errors = find_violations(sources)
        if not any("ScanUnmarked" in error or "recursive_directory_iterator" in error for error in errors):
            print(f"FAILED: gate accepted an unmarked discovery call: {errors}")
            return False

        sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery -- synthetic, self-test only.\n"
            "void FSynthetic::ScanMarked()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(sources)
        if any("ScanMarked" in error for error in errors):
            print(f"FAILED: gate rejected a properly marked discovery call: {errors}")
            return False

        sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery_deferred=PAH-99\n"
            "void FSynthetic::ScanDeferred()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(sources)
        if any("ScanDeferred" in error for error in errors):
            print(f"FAILED: gate rejected a properly deferred-marked discovery call: {errors}")
            return False

        # A comment mentioning the grammar must not itself trip the gate.
        sources[synthetic_path] = (
            "void FSynthetic::JustAComment()\n"
            "{\n"
            "    // std::filesystem::recursive_directory_iterator is mentioned here only in prose.\n"
            "    DoSomethingElse();\n"
            "}\n"
        )
        errors = find_violations(sources)
        if any("JustAComment" in error for error in errors):
            print(f"FAILED: gate flagged a discovery-grammar mention inside a comment: {errors}")
            return False

        sources.pop(synthetic_path)

    print("SUCCESS: every content-discovery call site is classified pre-Ready or explicitly deferred")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_pre_ready_content_discovery.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every content-discovery call site is classified pre-Ready or explicitly deferred")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
