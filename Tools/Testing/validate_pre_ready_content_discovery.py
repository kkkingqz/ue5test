#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if not (REPO_ROOT / "Source" / "GV2").exists():
    REPO_ROOT = Path("/home/king/ue5/GV2")

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
    r"|GV2ContentHostSupport::ResolvePackageSet\w+"
    r"|GV2PackageClosure::DiscoverFromGameData"
)

MARKER_BASE_PATTERN = re.compile(r"PAH-04:\s*pre_ready_discovery\b(?!_deferred)")
MARKER_WITH_CALLERS_PATTERN = re.compile(
    r"PAH-04:\s*pre_ready_discovery\s+callers=(?P<callers>[A-Za-z0-9_:,]+)(?:\s|$)"
)
DEFERRED_MARKER_PATTERN = re.compile(
    r"PAH-04:\s*pre_ready_discovery_deferred=(?P<task>[A-Za-z0-9_-]+)"
)

STATEMENT_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "return", "sizeof",
    "static_assert", "decltype", "explicit", "noexcept", "alignof",
    "static_cast", "reinterpret_cast", "dynamic_cast", "const_cast",
    "UCLASS", "USTRUCT", "UENUM", "UINTERFACE", "GENERATED_BODY", "UPROPERTY", "UFUNCTION",
}
FUNCTION_NAME_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_])(?P<scope>(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*::)?)(?P<name>[A-Za-z_]\w*)\s*\("
)


def mask_comments_and_literals(source: str) -> str:
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


def count_arguments(args_str: str) -> int:
    depth = 0
    commas = 0
    has_token = False
    for ch in args_str:
        if ch in "([<":
            depth += 1
        elif ch in ")]>":
            depth -= 1
        elif ch == "," and depth == 0:
            commas += 1
        elif not ch.isspace() and depth == 0:
            has_token = True
    return (commas + 1) if has_token else 0


def is_function_definition(source: str, open_paren: int) -> tuple[bool, int | None, int | None]:
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
    if suffix < len(source) and source[suffix] in (")", ","):
        return False, None, None
    while suffix < len(source) and source[suffix : suffix + 5] == "const":
        suffix += 5
        while suffix < len(source) and source[suffix].isspace():
            suffix += 1

    next_brace = source.find("{", suffix)
    next_semicolon = source.find(";", suffix)
    if next_brace < 0 or (0 <= next_semicolon < next_brace):
        return False, None, None

    decl_tokens = set(re.findall(r"\b\w+\b", source[suffix:next_brace]))
    if decl_tokens & {"class", "struct", "enum", "union", "namespace"}:
        return False, None, None

    body_end = matching_brace(source, next_brace)
    if body_end is None:
        return False, None, None
    return True, next_brace, body_end


class FunctionDef:
    def __init__(
        self,
        path: Path,
        full_name: str,
        scope: str,
        name: str,
        param_count: int,
        sig_start: int,
        body_start: int,
        body_end: int,
    ):
        self.path = path
        self.full_name = full_name
        self.scope = scope
        self.class_name = scope.rstrip(":")
        self.name = name
        self.param_count = param_count
        self.sig_start = sig_start
        self.body_start = body_start
        self.body_end = body_end


def marker_before(source: str, signature_start: int) -> tuple[str, any] | None:
    line_start = source.rfind("\n", 0, signature_start) + 1
    for line in reversed(source[:line_start].splitlines()):
        stripped = line.strip()
        if not stripped:
            continue
        deferred = DEFERRED_MARKER_PATTERN.search(stripped)
        if deferred:
            return ("deferred", deferred.group("task"))
        with_callers = MARKER_WITH_CALLERS_PATTERN.search(stripped)
        if with_callers:
            raw = [c.strip() for c in with_callers.group("callers").split(",") if c.strip()]
            declared = set() if raw == ["none"] else set(raw)
            return ("pre_ready", declared)
        if MARKER_BASE_PATTERN.search(stripped):
            return ("missing_callers", None)
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
        return None
    return None


def derive_actual_callers(
    target: FunctionDef,
    source_files: dict[Path, str],
    file_masked: dict[Path, str],
    file_funcs: dict[Path, list[FunctionDef]],
) -> set[str]:
    call_pattern = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(target.name)}\s*\(")
    callers = set()
    for path, masked in file_masked.items():
        funcs = file_funcs[path]
        for m in call_pattern.finditer(masked):
            call_pos = m.start()
            if path == target.path and call_pos == target.sig_start:
                continue

            enclosing = None
            for f in funcs:
                if f.body_start <= call_pos < f.body_end:
                    if enclosing is None or f.body_start > enclosing.body_start:
                        enclosing = f
            if enclosing is None:
                continue

            open_p = masked.find("(", call_pos)
            depth = 0
            close_p = None
            for idx in range(open_p, len(masked)):
                if masked[idx] == "(":
                    depth += 1
                elif masked[idx] == ")":
                    depth -= 1
                    if depth == 0:
                        close_p = idx
                        break
            if close_p is not None:
                arg_count = count_arguments(masked[open_p + 1 : close_p])
                if arg_count != target.param_count:
                    continue

            prefix = masked[:call_pos].rstrip()
            if target.class_name:
                if enclosing.class_name == target.class_name:
                    callers.add(enclosing.full_name)
                else:
                    if prefix.endswith("->") or prefix.endswith(".") or prefix.endswith(target.class_name + "::"):
                        callers.add(enclosing.full_name)
            else:
                if not (prefix.endswith("->") or prefix.endswith(".")):
                    callers.add(enclosing.full_name)
    return callers


def find_violations(source_files: dict[Path, str]) -> list[str]:
    violations: list[str] = []
    file_masked: dict[Path, str] = {}
    file_funcs: dict[Path, list[FunctionDef]] = {}

    for path, source in source_files.items():
        masked = mask_comments_and_literals(source)
        file_masked[path] = masked
        funcs: list[FunctionDef] = []
        for match in FUNCTION_NAME_PATTERN.finditer(masked):
            if not match.group("scope") and match.group("name") in STATEMENT_KEYWORDS:
                continue
            open_paren = masked.find("(", match.start(), match.end())
            if open_paren < 0:
                continue
            is_def, b_start, b_end = is_function_definition(masked, open_paren)
            if is_def:
                full_name = (match.group("scope") or "") + match.group("name")
                scope = match.group("scope") or ""
                name = match.group("name")
                close_paren = masked.rfind(")", open_paren, b_start)
                param_str = masked[open_paren + 1 : close_paren] if close_paren > open_paren else ""
                funcs.append(
                    FunctionDef(
                        path=path,
                        full_name=full_name,
                        scope=scope,
                        name=name,
                        param_count=count_arguments(param_str),
                        sig_start=match.start(),
                        body_start=b_start,
                        body_end=b_end,
                    )
                )
        file_funcs[path] = funcs

    # 1. Discovery grammar check
    for path, source in source_files.items():
        masked = file_masked[path]
        funcs = file_funcs[path]
        for match in DISCOVERY_GRAMMAR.finditer(masked):
            line_number = source.count("\n", 0, match.start()) + 1
            enclosing = None
            for f in funcs:
                if f.body_start <= match.start() < f.body_end:
                    if enclosing is None or f.body_start > enclosing.body_start:
                        enclosing = f
            if enclosing is None:
                violations.append(
                    f"{path}:{line_number}: content-discovery call '{match.group(0)}' is not inside "
                    "any recognized function definition -- add a PAH-04 marker to its enclosing function"
                )
                continue
            marker = marker_before(source, enclosing.sig_start)
            if marker is None:
                func_line = source.count("\n", 0, enclosing.sig_start) + 1
                violations.append(
                    f"{path}:{line_number}: content-discovery call '{match.group(0)}' "
                    f"(enclosing function '{enclosing.full_name}' at line {func_line}) has no PAH-04 marker -- add "
                    "'// PAH-04: pre_ready_discovery callers=...' (with justification) or "
                    "'// PAH-04: pre_ready_discovery_deferred=<TASK-ID>'"
                )

    # 2. Check every marked function for machine-readable callers agreement
    for path, funcs in file_funcs.items():
        source = source_files[path]
        for f in funcs:
            marker = marker_before(source, f.sig_start)
            if marker is None:
                continue
            marker_kind, marker_data = marker
            if marker_kind == "deferred":
                continue
            func_line = source.count("\n", 0, f.sig_start) + 1
            if marker_kind == "missing_callers":
                violations.append(
                    f"{path}:{func_line}: PAH-04 marker on '{f.full_name}' lacks machine-readable callers "
                    "declaration -- specify 'callers=<caller1>,...' or 'callers=none'"
                )
                continue
            if marker_kind == "pre_ready":
                declared_callers = marker_data
                actual_callers = derive_actual_callers(f, source_files, file_masked, file_funcs)
                spurious = declared_callers - actual_callers
                if spurious:
                    spurious_str = ", ".join(sorted(spurious))
                    violations.append(
                        f"{path}:{func_line}: PAH-04 marker on '{f.full_name}' declares non-existent "
                        f"caller(s): {spurious_str}"
                    )
                missing = actual_callers - declared_callers
                if missing:
                    missing_str = ", ".join(sorted(missing))
                    violations.append(
                        f"{path}:{func_line}: PAH-04 marker on '{f.full_name}' omits actual "
                        f"caller(s): {missing_str}"
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


def run_self_test(base_sources: dict[Path, str] | None = None) -> bool:
    print("[*] Running validate_pre_ready_content_discovery self-test...")
    test_sources = dict(base_sources) if base_sources is not None else production_sources()

    with tempfile.TemporaryDirectory() as tmpdir:
        synthetic_path = Path(tmpdir) / "SyntheticDiscovery.cpp"

        # 1. Unmarked discovery call must be rejected
        test_sources[synthetic_path] = (
            "void FSynthetic::ScanUnmarked()\n"
            "{\n"
            "    std::vector<std::filesystem::path> Roots;\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Roots[0])) {}\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if not any("ScanUnmarked" in error or "recursive_directory_iterator" in error for error in errors):
            print(f"FAILED: gate accepted an unmarked discovery call: {errors}")
            return False

        # 2. Properly marked discovery call with callers=none must pass
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery callers=none\n"
            "// Synthetic, self-test only.\n"
            "void FSynthetic::ScanMarked()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if any("ScanMarked" in error for error in errors):
            print(f"FAILED: gate rejected a properly marked discovery call: {errors}")
            return False

        # 3. Properly deferred-marked discovery call must pass
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery_deferred=PAH-99\n"
            "void FSynthetic::ScanDeferred()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if any("ScanDeferred" in error for error in errors):
            print(f"FAILED: gate rejected a properly deferred-marked discovery call: {errors}")
            return False

        # 4. Mention inside a comment must not trip the gate
        test_sources[synthetic_path] = (
            "void FSynthetic::JustAComment()\n"
            "{\n"
            "    // std::filesystem::recursive_directory_iterator is mentioned here only in prose.\n"
            "    DoSomethingElse();\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if any("JustAComment" in error for error in errors):
            print(f"FAILED: gate flagged a discovery-grammar mention inside a comment: {errors}")
            return False

        # 5. Marker lacking machine-readable callers= declaration must be rejected
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery -- missing callers declaration\n"
            "void FSynthetic::ScanMissingCallers()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if not any("ScanMissingCallers" in error and "lacks machine-readable callers" in error for error in errors):
            print(f"FAILED: gate accepted a marker without machine-readable callers: {errors}")
            return False

        # 6. Marker declaring non-existent caller must be rejected
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery callers=NonExistentCaller\n"
            "void FSynthetic::ScanSpurious()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if not any("ScanSpurious" in error and "declares non-existent caller" in error for error in errors):
            print(f"FAILED: gate accepted a marker with non-existent caller: {errors}")
            return False

        # 7. Marker omitting an actual caller must be rejected
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery callers=none\n"
            "void FSynthetic::ScanOmitted()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
            "void FSynthetic::ActualCaller()\n"
            "{\n"
            "    ScanOmitted();\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if not any("ScanOmitted" in error and "omits actual caller" in error for error in errors):
            print(f"FAILED: gate accepted a marker omitting an actual caller: {errors}")
            return False

        # 8. Marker with correct declared caller must pass
        test_sources[synthetic_path] = (
            "// PAH-04: pre_ready_discovery callers=FSynthetic::ActualCaller\n"
            "void FSynthetic::ScanCorrectCaller()\n"
            "{\n"
            "    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root)) {}\n"
            "}\n"
            "void FSynthetic::ActualCaller()\n"
            "{\n"
            "    ScanCorrectCaller();\n"
            "}\n"
        )
        errors = find_violations(test_sources)
        if any("ScanCorrectCaller" in error or "ActualCaller" in error for error in errors):
            print(f"FAILED: gate rejected a correctly declared caller: {errors}")
            return False

        test_sources.pop(synthetic_path)

    print("SUCCESS: every content-discovery call site is classified pre-Ready or explicitly deferred, and all callers verified")
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
    print("SUCCESS: every content-discovery call site is classified pre-Ready or explicitly deferred, and all callers verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

