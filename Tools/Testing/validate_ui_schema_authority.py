#!/usr/bin/env python3
"""Validates that the session content snapshot is the sole UI schema authority (CFC-04).

Rules enforced:
1. FGV2UiSchemaCache instantiation is prohibited outside FGV2SessionContentCandidate::Build
   in Source/GV2/Private/Application/GV2SessionContentSnapshot.cpp.
2. Retired process-global methods and variables are completely banned:
   - GSessionSchemaCache
   - RebuildSchemaCacheForSession
   - ReleaseSchemaCacheForSession
3. All calls and declarations of GV2ScreenFieldMaterializer entry points:
   - PrepareBindingDefinitions
   - BuildFields
   - GetCompiledSchema
   must explicitly take/pass FGV2PresentationPrepareContext.
4. FGV2UiSchemaCache constructor and CompileAll must remain private in GV2UiSchemaCache.h.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PRODUCTION_ROOTS = [
    REPO_ROOT / "Source" / "GV2" / "Public",
    REPO_ROOT / "Source" / "GV2" / "Private",
]
EXCLUDED_DIR_NAMES = {"Tests"}

BANNED_GLOBAL_PATTERNS = [
    (re.compile(r"\bGSessionSchemaCache\b"), "GSessionSchemaCache is deleted; schemas are snapshot-scoped"),
    (re.compile(r"\bRebuildSchemaCacheForSession\b"), "RebuildSchemaCacheForSession is deleted; schemas are snapshot-scoped"),
    (re.compile(r"\bReleaseSchemaCacheForSession\b"), "ReleaseSchemaCacheForSession is deleted; schemas are snapshot-scoped"),
]

SCHEMA_CACHE_INSTANTIATION_PATTERN = re.compile(
    r"\b(?:new\s+FGV2UiSchemaCache|MakeShared<FGV2UiSchemaCache>|MakeUnique<FGV2UiSchemaCache>)\b"
)

MATERIALIZER_CALL_PATTERN = re.compile(
    r"GV2ScreenFieldMaterializer::(?P<func>PrepareBindingDefinitions|BuildFields|GetCompiledSchema)\s*\((?P<args>[^;)]*)\)",
    re.DOTALL
)


def mask_comments_and_literals(source: str) -> str:
    """Preserve offsets and newlines while masking comment and literal contents."""
    masked = list(source)
    index = 0
    length = len(source)
    while index < length:
        if source.startswith("//", index):
            end = source.find("\n", index)
            if end < 0:
                end = length
            for masked_index in range(index, end):
                masked[masked_index] = " "
            index = end
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = length if end < 0 else end + 2
            for masked_index in range(index, end):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        if source[index] in {'"', "'"}:
            quote = source[index]
            end = index + 1
            while end < length:
                if source[end] == "\\":
                    end += 2
                    continue
                if source[end] == quote:
                    end += 1
                    break
                end += 1
            for masked_index in range(index, min(end, length)):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        index += 1
    return "".join(masked)


def collect_production_sources(repo_root: Path | None = None) -> dict[Path, str]:
    root = repo_root or REPO_ROOT
    sources: dict[Path, str] = {}
    for sub in ["Public", "Private"]:
        base = root / "Source" / "GV2" / sub
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in (".h", ".cpp"):
                continue
            if any(part in EXCLUDED_DIR_NAMES for part in path.parts):
                continue
            sources[path] = path.read_text(encoding="utf-8")
    return sources


def validate_sources(sources: dict[Path, str], root: Path | None = None) -> list[str]:
    base_root = root or REPO_ROOT
    violations: list[str] = []

    snapshot_cpp = base_root / "Source" / "GV2" / "Private" / "Application" / "GV2SessionContentSnapshot.cpp"
    cache_header = base_root / "Source" / "GV2" / "Private" / "UI" / "GV2UiSchemaCache.h"

    for path, text in sources.items():
        try:
            rel_path = path.relative_to(base_root).as_posix()
        except ValueError:
            rel_path = path.as_posix()
        masked = mask_comments_and_literals(text)

        # 1. Banned globals / retired methods
        for pattern, reason in BANNED_GLOBAL_PATTERNS:
            for match in pattern.finditer(masked):
                line_no = masked.count("\n", 0, match.start()) + 1
                violations.append(f"{rel_path}:{line_no}: {reason}")

        # 2. Construction of FGV2UiSchemaCache outside FGV2SessionContentSnapshot.cpp
        for match in SCHEMA_CACHE_INSTANTIATION_PATTERN.finditer(masked):
            if path.resolve() != snapshot_cpp.resolve():
                line_no = masked.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{rel_path}:{line_no}: FGV2UiSchemaCache instantiation is forbidden outside FGV2SessionContentCandidate::Build"
                )

        # 3. GV2ScreenFieldMaterializer calls missing PrepareContext
        for match in MATERIALIZER_CALL_PATTERN.finditer(masked):
            args = match.group("args")
            func_name = match.group("func")
            line_no = masked.count("\n", 0, match.start()) + 1
            if "PrepareContext" not in args:
                violations.append(
                    f"{rel_path}:{line_no}: GV2ScreenFieldMaterializer::{func_name} call does not pass PrepareContext"
                )

    # 4. FGV2UiSchemaCache header check: constructor & CompileAll must be private
    if cache_header in sources:
        header_text = sources[cache_header]
        masked_header = mask_comments_and_literals(header_text)
        priv_idx = masked_header.find("private:")
        ctor_match = re.search(r"\bexplicit\s+FGV2UiSchemaCache\b", masked_header)
        if ctor_match:
            if priv_idx == -1 or ctor_match.start() < priv_idx:
                violations.append(
                    f"{(cache_header.relative_to(base_root).as_posix() if cache_header.is_relative_to(base_root) else cache_header.as_posix())}: FGV2UiSchemaCache constructor must be private"
                )
        compile_match = re.search(r"\bbool\s+CompileAll\b", masked_header)
        if compile_match:
            if priv_idx == -1 or compile_match.start() < priv_idx:
                violations.append(
                    f"{(cache_header.relative_to(base_root).as_posix() if cache_header.is_relative_to(base_root) else cache_header.as_posix())}: FGV2UiSchemaCache::CompileAll must be private"
                )

    return violations


def run_self_test() -> bool:
    print("Running validate_ui_schema_authority self-test...")
    real_sources = collect_production_sources()
    real_violations = validate_sources(real_sources)
    if real_violations:
        print("FAILED: Clean repository check failed:\n  " + "\n  ".join(real_violations), file=sys.stderr)
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)

        # Mutation 1: Banned RebuildSchemaCacheForSession
        mut_sources = dict(real_sources)
        dummy_file = tmp_path / "DummyCoordinator.cpp"
        mut_sources[dummy_file] = "void Foo() { RebuildSchemaCacheForSession({}); }"
        viols = validate_sources(mut_sources)
        if not any("RebuildSchemaCacheForSession" in v for v in viols):
            print("FAILED: self-test did not catch RebuildSchemaCacheForSession mutation", file=sys.stderr)
            return False

        # Mutation 2: Banned GSessionSchemaCache
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { if (GSessionSchemaCache) {} }"
        viols = validate_sources(mut_sources)
        if not any("GSessionSchemaCache" in v for v in viols):
            print("FAILED: self-test did not catch GSessionSchemaCache mutation", file=sys.stderr)
            return False

        # Mutation 3: new FGV2UiSchemaCache outside snapshot builder
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { auto X = new FGV2UiSchemaCache(Roots); }"
        viols = validate_sources(mut_sources)
        if not any("FGV2UiSchemaCache instantiation is forbidden" in v for v in viols):
            print("FAILED: self-test did not catch new FGV2UiSchemaCache mutation", file=sys.stderr)
            return False

        # Mutation 4: Materializer call without PrepareContext
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { GV2ScreenFieldMaterializer::PrepareBindingDefinitions(Req, Defs); }"
        viols = validate_sources(mut_sources)
        if not any("GV2ScreenFieldMaterializer::PrepareBindingDefinitions call does not pass PrepareContext" in v for v in viols):
            print("FAILED: self-test did not catch Materializer call missing PrepareContext", file=sys.stderr)
            return False

        # Mutation 5: Materializer BuildFields without PrepareContext
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { GV2ScreenFieldMaterializer::BuildFields(Req, Handles, Fields); }"
        viols = validate_sources(mut_sources)
        if not any("GV2ScreenFieldMaterializer::BuildFields call does not pass PrepareContext" in v for v in viols):
            print("FAILED: self-test did not catch Materializer BuildFields missing PrepareContext", file=sys.stderr)
            return False

        # Mutation 6: Materializer GetCompiledSchema without PrepareContext
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { GV2ScreenFieldMaterializer::GetCompiledSchema(SchemaId, Err); }"
        viols = validate_sources(mut_sources)
        if not any("GV2ScreenFieldMaterializer::GetCompiledSchema call does not pass PrepareContext" in v for v in viols):
            print("FAILED: self-test did not catch Materializer GetCompiledSchema missing PrepareContext", file=sys.stderr)
            return False

        # Legitimate call with PrepareContext in dummy file must NOT trigger violation
        mut_sources = dict(real_sources)
        mut_sources[dummy_file] = "void Foo() { GV2ScreenFieldMaterializer::BuildFields(PrepareContext, Req, Handles, Fields); }"
        viols = validate_sources(mut_sources)
        if any(dummy_file.as_posix() in v for v in viols):
            print(f"FAILED: self-test rejected legitimate materializer call: {viols}", file=sys.stderr)
            return False

    print("SUCCESS: validate_ui_schema_authority self-test passed all negative mutations.")
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    sources = collect_production_sources()
    violations = validate_sources(sources)
    if violations:
        print("UI schema authority violations found:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print("SUCCESS: UI schema authority invariants verified across production sources.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
