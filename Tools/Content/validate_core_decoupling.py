#!/usr/bin/env python3
"""Validates that engine/core code (Scripts/, GameData/core/, Source/)
contains no reverse dependencies / back-references to any non-core game package namespaces.
Rule ID: CORE_DECOUPLING_RULE

Implements TSR-10 (Plan TestSuiteRestructuring):
  1. Forbidden package namespaces are dynamically derived from GameData/*/package.json5
     (where namespace != 'core'). Adding a new game package automatically expands the
     rejection pattern without modifying this script.
  2. Test code (Source/**/Tests and Source/GV2ContentEditor/Private/Testing) is governed
     by the two-category test boundary and ratchet gate in validate_test_content_coupling.py
     per ADR-0046 (TSR-02 / TSR-10).
  3. Comments are stripped during scanning to prevent false positives from documentation.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Optional, Set

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TARGET_SUBDIRECTORIES = [
    Path("Scripts"),
    Path("GameData") / "core",
    Path("Source"),
]

EXCLUDED_DIR_NAMES = {"Tests", "Testing"}

CHECKED_EXTENSIONS = {
    ".lua",
    ".json5",
    ".json",
    ".po",
    ".pot",
    ".h",
    ".hpp",
    ".cpp",
    ".c",
    ".cs",
    ".py",
    ".txt",
    ".md",
}


def strip_json5_comments(text: str) -> str:
    """Strip single-line and multi-line comments while preserving line count."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.DOTALL)
    text = re.sub(r"(?://|#)[^\n]*", "", text)
    return text


def parse_json5_simple(text: str) -> dict:
    """Parse minimal json5 manifest content."""
    text = strip_json5_comments(text)
    text = re.sub(r"'([^'\\]*(?:\\.[^'\\]*)*)'", r'"\1"', text)
    text = re.sub(r'(?<=[{,\n])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:', r'"\1":', text)
    text = re.sub(r',\s*([}\]])', r'\1', text)
    return json.loads(text)


def discover_forbidden_namespaces(repo_root: Path) -> Set[str]:
    """Dynamically discover all non-core game package namespaces from GameData/*/package.json5."""
    gamedata_dir = repo_root / "GameData"
    forbidden: Set[str] = set()

    if not gamedata_dir.exists():
        return forbidden

    for manifest in sorted(gamedata_dir.glob("*/package.json5")):
        try:
            content = manifest.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        package_id = ""
        namespace = ""
        try:
            data = parse_json5_simple(content)
            if isinstance(data, dict):
                package_id = str(data.get("package_id", data.get("id", manifest.parent.name)))
                namespace = str(data.get("namespace", package_id))
        except Exception:
            pid_m = re.search(r'["\']?(?:package_id|id)["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', content)
            package_id = pid_m.group(1) if pid_m else manifest.parent.name
            ns_m = re.search(r'["\']?namespace["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', content)
            namespace = ns_m.group(1) if ns_m else package_id

        if namespace and namespace != "core" and package_id != "core":
            forbidden.add(namespace)

    return forbidden


def build_forbidden_pattern(namespaces: Set[str]) -> Optional[re.Pattern]:
    """Build compiled regex pattern matching forbidden namespace prefixes."""
    if not namespaces:
        return None
    sorted_ns = sorted(namespaces, key=len, reverse=True)
    pattern_str = r"\b(" + "|".join(re.escape(ns) for ns in sorted_ns) + r"):"
    return re.compile(pattern_str)


def strip_code_comments(source: str, suffix: str) -> str:
    """Preserve layout and line count while stripping comments."""
    if suffix in {".h", ".hpp", ".cpp", ".c", ".cs"}:
        source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.DOTALL)
        return re.sub(r"//[^\n]*", "", source)
    elif suffix == ".lua":
        source = re.sub(r"--\[\[.*?\]\]", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.DOTALL)
        return re.sub(r"--[^\n]*", "", source)
    elif suffix in {".json5", ".json"}:
        source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.DOTALL)
        return re.sub(r"(?://|#)[^\n]*", "", source)
    elif suffix == ".py":
        return re.sub(r"#[^\n]*", "", source)
    return source


def scan_directory(dir_path: Path, forbidden_pattern: re.Pattern, repo_root: Path) -> list[str]:
    """Scan directory for forbidden namespace references outside test directories."""
    violations: list[str] = []
    if not dir_path.exists():
        return violations

    for file_path in sorted(dir_path.rglob("*")):
        if not file_path.is_file():
            continue
        if file_path.suffix.lower() not in CHECKED_EXTENSIONS:
            continue
        if any(part in EXCLUDED_DIR_NAMES for part in file_path.parts):
            continue

        try:
            content = file_path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        stripped_content = strip_code_comments(content, file_path.suffix.lower())
        for line_num, line in enumerate(stripped_content.splitlines(), start=1):
            m = forbidden_pattern.search(line)
            if m:
                matched_ns = m.group(1)
                rel_path = file_path.relative_to(repo_root) if file_path.is_relative_to(repo_root) else file_path
                violations.append(
                    f"{rel_path}:{line_num}: error [CORE_DECOUPLING_RULE]: "
                    f"Engine/Core must not reference game package '{matched_ns}:' namespace. Found: {line.strip()}"
                )
    return violations


def run_validation(repo_root: Path) -> list[str]:
    """Execute core decoupling validation for the given repository root."""
    namespaces = discover_forbidden_namespaces(repo_root)
    pattern = build_forbidden_pattern(namespaces)
    if pattern is None:
        return []

    all_violations: list[str] = []
    for subpath in TARGET_SUBDIRECTORIES:
        full_dir = repo_root / subpath
        all_violations.extend(scan_directory(full_dir, pattern, repo_root))
    return all_violations


def run_self_test() -> bool:
    print("[*] Running validate_core_decoupling self-test...")

    # 1. Positive test against actual codebase
    actual_violations = run_validation(REPO_ROOT)
    if actual_violations:
        print(f"FAILED: expected clean codebase, found violations:\n" + "\n".join(actual_violations))
        return False
    print("  PASS: Actual repository clean against discovered game package namespaces.")

    # 2. Dynamic discovery and negative test on added package in GameData
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)

        # Create core package manifest (exempt)
        fake_core = fake_root / "GameData" / "core"
        fake_core.mkdir(parents=True)
        (fake_core / "package.json5").write_text(
            '{\n  package_id: "core",\n  namespace: "core"\n}\n',
            encoding="utf-8",
        )

        # Initial check: no game packages -> no violations
        fake_scripts = fake_root / "Scripts"
        fake_scripts.mkdir(parents=True)
        probe_file = fake_scripts / "probe.lua"
        probe_file.write_text('local target = "synthetic_game:location.hub"\n', encoding="utf-8")

        init_violations = run_validation(fake_root)
        if init_violations:
            print(f"FAILED: expected 0 violations before game package was defined, got: {init_violations}")
            return False

        # Add new game package dynamically to GameData/
        fake_game = fake_root / "GameData" / "synthetic_pkg"
        fake_game.mkdir(parents=True)
        (fake_game / "package.json5").write_text(
            '{\n  package_id: "synthetic_pkg",\n  namespace: "synthetic_game"\n}\n',
            encoding="utf-8",
        )

        # Verify dynamic namespace discovery without script modification
        discovered = discover_forbidden_namespaces(fake_root)
        if "synthetic_game" not in discovered:
            print(f"FAILED: did not dynamically discover 'synthetic_game' from manifest: {discovered}")
            return False

        violations_after_add = run_validation(fake_root)
        if not violations_after_add:
            print("FAILED: scanner did not reject 'synthetic_game:' reference after package was added to GameData/")
            return False
        if "synthetic_game" not in violations_after_add[0] or "probe.lua:1:" not in violations_after_add[0]:
            print(f"FAILED: violation format mismatch: {violations_after_add[0]}")
            return False
        print("  PASS: Dynamic package discovery and negative rejection verified for added package.")

        # Verify test directory exclusion: test files are governed by validate_test_content_coupling
        fake_test_dir = fake_root / "Source" / "GV2" / "Private" / "Tests"
        fake_test_dir.mkdir(parents=True)
        test_file = fake_test_dir / "SmokeTest.cpp"
        test_file.write_text('const char* id = "synthetic_game:location.hub";\n', encoding="utf-8")

        violations_with_test = run_validation(fake_root)
        # Should still be only 1 violation (the one from probe.lua), not 2
        if len(violations_with_test) != 1:
            print(f"FAILED: test file under Tests/ was not excluded from core decoupling: {violations_with_test}")
            return False
        print("  PASS: Tests directory correctly excluded (delegated to validate_test_content_coupling).")

    print("ALL CORE DECOUPLING SELF-TESTS PASSED SUCCESSFULLY!")
    return True


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate that engine/core code contains no reverse dependencies to game packages."
    )
    parser.add_argument("--self-test", action="store_true", help="Run self-tests and negative validation tests.")
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT, help="Override repository root path.")
    args = parser.parse_args(argv)

    if args.self_test:
        return 0 if run_self_test() else 1

    violations = run_validation(args.repo_root)
    if violations:
        print(f"Core decoupling validation failed ({len(violations)} violations):", file=sys.stderr)
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        return 1

    namespaces = discover_forbidden_namespaces(args.repo_root)
    ns_list = ", ".join(sorted(namespaces))
    print(f"Core decoupling validation passed: no back-references to game namespaces ({ns_list}) in engine/core code.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
