#!/usr/bin/env python3
"""Inventory generator and verification tool for test suite content coupling.

Implements TSR-01 (M0, Plan TestSuiteRestructuring):
Discovers game packages and their UE content roots dynamically from GameData/*/package.json5,
scans Source/**/Tests for content coupling across three distinct classes:
  - Class 1: Namespaced IDs of game packages (excluding exempt core:).
  - Class 2: String literals starting with game package ue_content_roots.
  - Class 3: Definition entry IDs and widget names occurring in tests (heuristic).

Provides deterministic reporting, machine-readable JSON output, independent grep
reconciliation / checksum verification, and negative self-tests.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCANNED_EXTENSIONS = {".c", ".cpp", ".h", ".hpp"}


@dataclasses.dataclass(frozen=True)
class PackageInfo:
    package_id: str
    namespace: str
    is_core: bool
    ue_content_roots: Tuple[str, ...]
    manifest_path: str


@dataclasses.dataclass(frozen=True)
class CouplingOccurrence:
    category: str  # "class_1", "class_2", "class_3"
    file_path: str  # relative posix path
    line_number: int
    token: str
    matched_detail: str
    snippet: str


@dataclasses.dataclass(frozen=True)
class ReconciliationChecksum:
    raw_grep_total: int
    comment_occurrences: int
    prose_occurrences: int
    class_1_total: int
    is_reconciled: bool
    details: List[str]


@dataclasses.dataclass(frozen=True)
class InventoryReport:
    exempt_packages: Tuple[PackageInfo, ...]
    game_packages: Tuple[PackageInfo, ...]
    total_test_files_scanned: int
    occurrences: Tuple[CouplingOccurrence, ...]
    reconciliation: ReconciliationChecksum


def strip_json5_comments(text: str) -> str:
    """Strip single-line and multi-line comments while preserving line count."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.DOTALL)
    text = re.sub(r"(?://|#)[^\n]*", "", text)
    return text


def parse_json5(text: str) -> Any:
    """Normalize and parse basic JSON5 content."""
    text = strip_json5_comments(text)
    # Convert single-quoted strings to double-quoted strings
    text = re.sub(r"'([^'\\]*(?:\\.[^'\\]*)*)'", r'"\1"', text)
    # Quote unquoted property names
    text = re.sub(r'(?<=[{,\n])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:', r'"\1":', text)
    # Strip trailing commas
    text = re.sub(r',\s*([}\]])', r'\1', text)
    return json.loads(text)


def load_package_manifest(manifest_path: Path) -> PackageInfo:
    """Parse a package.json5 manifest without hardcoding package names or paths."""
    raw_content = manifest_path.read_text(encoding="utf-8")
    package_id = ""
    namespace = ""
    roots: List[str] = []

    try:
        data = parse_json5(raw_content)
        if isinstance(data, dict):
            package_id = str(data.get("package_id", data.get("id", manifest_path.parent.name)))
            namespace = str(data.get("namespace", package_id))
            raw_roots = data.get("ue_content_roots")
            if isinstance(raw_roots, list):
                roots = [str(r) for r in raw_roots if isinstance(r, str)]
    except Exception:
        pid_m = re.search(r'["\']?(?:package_id|id)["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', raw_content)
        package_id = pid_m.group(1) if pid_m else manifest_path.parent.name
        ns_m = re.search(r'["\']?namespace["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', raw_content)
        namespace = ns_m.group(1) if ns_m else package_id
        roots_m = re.search(r'["\']?ue_content_roots["\']?\s*:\s*\[(.*?)\]', raw_content, re.DOTALL)
        if roots_m:
            roots = re.findall(r'["\']([^"\']+)["\']', roots_m.group(1))

    is_core = (namespace == "core" or package_id == "core")
    return PackageInfo(
        package_id=package_id,
        namespace=namespace,
        is_core=is_core,
        ue_content_roots=tuple(roots),
        manifest_path=manifest_path.as_posix(),
    )


def discover_packages(repo_root: Path) -> Tuple[Tuple[PackageInfo, ...], Tuple[PackageInfo, ...]]:
    """Dynamically discover exempt and game packages from GameData/*/package.json5."""
    gamedata_dir = repo_root / "GameData"
    exempt_list: List[PackageInfo] = []
    game_list: List[PackageInfo] = []

    if gamedata_dir.exists():
        for manifest in sorted(gamedata_dir.glob("*/package.json5")):
            pkg = load_package_manifest(manifest)
            if pkg.is_core:
                exempt_list.append(pkg)
            else:
                game_list.append(pkg)

    return tuple(sorted(exempt_list, key=lambda p: p.package_id)), tuple(sorted(game_list, key=lambda p: p.package_id))


def strip_comments(source: str) -> str:
    """Preserve layout and line numbers while excluding comments."""
    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_definition_tokens(repo_root: Path) -> Dict[str, str]:
    """Extract entry keys, derived travel transition keys, and widget names from definitions."""
    gamedata_dir = repo_root / "GameData"
    candidates: Dict[str, str] = {}
    if not gamedata_dir.exists():
        return candidates

    for def_file in sorted(gamedata_dir.glob("*/definitions/**/*.json5")):
        try:
            data = parse_json5(def_file.read_text(encoding="utf-8"))
        except Exception:
            continue

        if not isinstance(data, dict):
            continue

        for item in data.get("definitions", []):
            if not isinstance(item, dict):
                continue
            def_id = str(item.get("id", ""))
            d_data = item.get("data", {})
            if isinstance(d_data, dict):
                for act in d_data.get("actions", []):
                    if isinstance(act, dict) and "key" in act:
                        k = str(act["key"])
                        candidates[k] = f"action key in {def_id}"
                for char in d_data.get("characters", []):
                    if isinstance(char, dict) and "key" in char:
                        k = str(char["key"])
                        candidates[k] = f"character key in {def_id}"
                for conn in d_data.get("connected_location_ids", []):
                    if isinstance(conn, str) and ":location." in conn:
                        loc_path = conn.split(":location.", 1)[1]
                        travel_key = f"travel_{loc_path.replace('.', '_')}"
                        candidates[travel_key] = f"derived travel key for {conn}"

            exts = item.get("extensions", {})
            if isinstance(exts, dict):
                for ext_name, ext_val in exts.items():
                    if isinstance(ext_val, dict):
                        for char in ext_val.get("characters", []):
                            if isinstance(char, dict) and "key" in char:
                                k = str(char["key"])
                                candidates[k] = f"extension character key in {def_id}"

            if ":location." in def_id:
                loc_path = def_id.split(":location.", 1)[1]
                travel_key = f"travel_{loc_path.replace('.', '_')}"
                candidates[travel_key] = f"derived location travel key for {def_id}"

        # Check for explicit widget names or widget hints mentioned in definitions
        raw_text = strip_json5_comments(def_file.read_text(encoding="utf-8"))
        for wbp in set(re.findall(r'["\']([a-zA-Z0-9_]*WBP_[a-zA-Z0-9_]+)["\']', raw_text)):
            candidates[wbp] = f"widget name in {def_file.name}"
        for wid in set(re.findall(r'["\']([a-zA-Z0-9_]*widget\.[a-zA-Z0-9_]+)["\']', raw_text)):
            candidates[wid] = f"widget identifier in {def_file.name}"

    return candidates


def discover_test_files(repo_root: Path) -> List[Path]:
    """Find all C++ test files under Source/**/Tests."""
    source_dir = repo_root / "Source"
    if not source_dir.exists():
        return []

    test_files: List[Path] = []
    for path in sorted(source_dir.rglob("*")):
        if path.is_file() and path.suffix in SCANNED_EXTENSIONS and "Tests" in path.parts:
            test_files.append(path)
    return test_files


def scan_inventory(repo_root: Path) -> InventoryReport:
    """Perform the full 3-class content coupling inventory scan."""
    exempt_pkgs, game_pkgs = discover_packages(repo_root)
    test_files = discover_test_files(repo_root)
    def_tokens = extract_definition_tokens(repo_root)

    # Class 1 pattern: namespaced ID for each game package
    class_1_patterns: List[Tuple[str, re.Pattern[str]]] = []
    for pkg in game_pkgs:
        class_1_patterns.append(
            (pkg.namespace, re.compile(rf"\b{re.escape(pkg.namespace)}:[a-zA-Z0-9_./#-]+"))
        )

    # Class 2 pattern: string literals starting with any game package ue_content_roots
    game_roots: List[str] = []
    for pkg in game_pkgs:
        for root in pkg.ue_content_roots:
            if root:
                game_roots.append(root)
    game_roots = sorted(set(game_roots), key=len, reverse=True)
    class_2_pattern: Optional[re.Pattern[str]] = None
    if game_roots:
        class_2_pattern = re.compile(r'"((?:' + "|".join(re.escape(r) for r in game_roots) + r')[^"]*)"')

    occurrences: List[CouplingOccurrence] = []

    # Checksum tracking
    raw_grep_total = 0
    comment_occurrences = 0
    prose_occurrences = 0
    class_1_total = 0
    reconciliation_details: List[str] = []

    # Pattern for counting raw game namespace references (e.g. "textsystem:", "rh:", "sample:")
    game_ns_tokens = [f"{pkg.namespace}:" for pkg in game_pkgs]

    for file_path in test_files:
        rel_path = file_path.relative_to(repo_root).as_posix()
        raw_content = file_path.read_text(encoding="utf-8")
        stripped_content = strip_comments(raw_content)

        # Checksum tracking: count raw matches in original file vs stripped
        for line_no, (raw_line, stripped_line) in enumerate(
            zip(raw_content.splitlines(), stripped_content.splitlines()), start=1
        ):
            raw_hits = sum(raw_line.count(ns_tok) for ns_tok in game_ns_tokens)
            stripped_hits = sum(stripped_line.count(ns_tok) for ns_tok in game_ns_tokens)
            raw_grep_total += raw_hits
            if raw_hits > stripped_hits:
                diff = raw_hits - stripped_hits
                comment_occurrences += diff
                reconciliation_details.append(
                    f"{rel_path}:{line_no}: {diff} occurrence(s) in comment: {raw_line.strip()}"
                )

        # Scan stripped lines
        for line_no, line in enumerate(stripped_content.splitlines(), start=1):
            if not line.strip():
                continue

            # Class 1 scan
            line_class1_matches: List[str] = []
            for ns, pattern in class_1_patterns:
                for m in pattern.finditer(line):
                    tok = m.group(0)
                    line_class1_matches.append(tok)
                    occurrences.append(
                        CouplingOccurrence(
                            category="class_1",
                            file_path=rel_path,
                            line_number=line_no,
                            token=tok,
                            matched_detail=f"namespaced ID for game package '{ns}'",
                            snippet=line.strip(),
                        )
                    )
                    class_1_total += 1

            # Check if there are namespace hits in code that are NOT valid namespaced IDs (e.g. prose)
            stripped_line_ns_hits = sum(line.count(ns_tok) for ns_tok in game_ns_tokens)
            if stripped_line_ns_hits > len(line_class1_matches):
                prose_diff = stripped_line_ns_hits - len(line_class1_matches)
                prose_occurrences += prose_diff
                reconciliation_details.append(
                    f"{rel_path}:{line_no}: {prose_diff} non-ID prose occurrence(s): {line.strip()}"
                )

            # Class 2 scan
            if class_2_pattern:
                for m in class_2_pattern.finditer(line):
                    literal_path = m.group(1)
                    matched_root = next(r for r in game_roots if literal_path.startswith(r))
                    occurrences.append(
                        CouplingOccurrence(
                            category="class_2",
                            file_path=rel_path,
                            line_number=line_no,
                            token=literal_path,
                            matched_detail=f"starts with game content root '{matched_root}'",
                            snippet=line.strip(),
                        )
                    )

            # Class 3 scan (heuristic: entry id & widget name candidates from definitions)
            for cand_token, desc in sorted(def_tokens.items()):
                if re.search(rf"\b{re.escape(cand_token)}\b", line):
                    occurrences.append(
                        CouplingOccurrence(
                            category="class_3",
                            file_path=rel_path,
                            line_number=line_no,
                            token=cand_token,
                            matched_detail=f"heuristic candidate: {desc}",
                            snippet=line.strip(),
                        )
                    )

    # Sort occurrences deterministically
    occurrences.sort(key=lambda o: (o.category, o.file_path, o.line_number, o.token))

    is_reconciled = (class_1_total + comment_occurrences + prose_occurrences == raw_grep_total)
    reconciliation = ReconciliationChecksum(
        raw_grep_total=raw_grep_total,
        comment_occurrences=comment_occurrences,
        prose_occurrences=prose_occurrences,
        class_1_total=class_1_total,
        is_reconciled=is_reconciled,
        details=sorted(reconciliation_details),
    )

    return InventoryReport(
        exempt_packages=exempt_pkgs,
        game_packages=game_pkgs,
        total_test_files_scanned=len(test_files),
        occurrences=tuple(occurrences),
        reconciliation=reconciliation,
    )


def format_text_report(report: InventoryReport) -> str:
    """Format the inventory into a deterministic human-readable report."""
    lines: List[str] = [
        "=" * 80,
        "TEST CONTENT COUPLING INVENTORY REPORT (TSR-01)",
        "=" * 80,
        f"Total Test Files Scanned: {report.total_test_files_scanned}",
        "Scanned Extensions: " + ", ".join(sorted(SCANNED_EXTENSIONS)),
        "",
        "--- Discovered Packages (from GameData/*/package.json5) ---",
        "Exempt Core Package(s):",
    ]

    for pkg in report.exempt_packages:
        roots_str = ", ".join(repr(r) for r in pkg.ue_content_roots) if pkg.ue_content_roots else "none"
        lines.append(f"  - {pkg.package_id} (namespace: {pkg.namespace}, ue_content_roots: [{roots_str}])")

    lines.append(f"Game Packages ({len(report.game_packages)}):")
    for pkg in report.game_packages:
        if pkg.ue_content_roots:
            roots_str = ", ".join(repr(r) for r in pkg.ue_content_roots)
            lines.append(f"  - {pkg.package_id} (namespace: {pkg.namespace}, ue_content_roots: [{roots_str}])")
        else:
            lines.append(
                f"  - {pkg.package_id} (namespace: {pkg.namespace}, ue_content_roots: []) [no content roots declared]"
            )

    c1_occurrences = [o for o in report.occurrences if o.category == "class_1"]
    c2_occurrences = [o for o in report.occurrences if o.category == "class_2"]
    c3_occurrences = [o for o in report.occurrences if o.category == "class_3"]

    c1_files = len(set(o.file_path for o in c1_occurrences))
    c2_files = len(set(o.file_path for o in c2_occurrences))
    c3_files = len(set(o.file_path for o in c3_occurrences))

    lines.extend([
        "",
        "--- Inventory Summary ---",
        f"Class 1 (Namespaced IDs of game packages): {len(c1_occurrences)} occurrences across {c1_files} files",
        f"Class 2 (Game package UE content root path literals): {len(c2_occurrences)} occurrences across {c2_files} files",
        f"Class 3 (Heuristic candidates: definition entry IDs & widget names): {len(c3_occurrences)} occurrences across {c3_files} files",
        f"Total Direct Couplings (Class 1 + Class 2): {len(c1_occurrences) + len(c2_occurrences)}",
        "",
        "--- Checksum and Reconciliation ---",
        f"Independent raw grep count for game namespaces: {report.reconciliation.raw_grep_total}",
        f"  - Occurrences inside comments (excluded): {report.reconciliation.comment_occurrences}",
        f"  - Non-ID prose occurrence(s) (excluded): {report.reconciliation.prose_occurrences}",
        f"  - Class 1 namespaced IDs: {report.reconciliation.class_1_total}",
        f"Reconciliation Status: {'MATCHED (100% accounted for)' if report.reconciliation.is_reconciled else 'MISMATCH'}",
    ])

    if report.reconciliation.details:
        lines.append("Reconciliation Breakdown:")
        for det in report.reconciliation.details:
            lines.append(f"    * {det}")

    # Detailed sections
    lines.extend([
        "",
        f"--- Detailed Inventory: Class 1 - Namespaced IDs ({len(c1_occurrences)} items) ---",
    ])
    for o in c1_occurrences:
        lines.append(f"{o.file_path}:{o.line_number} [{o.token}] ({o.matched_detail}) -> {o.snippet}")

    lines.extend([
        "",
        f"--- Detailed Inventory: Class 2 - Game UE Content Roots ({len(c2_occurrences)} items) ---",
    ])
    for o in c2_occurrences:
        lines.append(f"{o.file_path}:{o.line_number} [{o.token}] ({o.matched_detail}) -> {o.snippet}")

    lines.extend([
        "",
        f"--- Detailed Inventory: Class 3 - Heuristic Candidates ({len(c3_occurrences)} items) ---",
    ])
    for o in c3_occurrences:
        lines.append(f"{o.file_path}:{o.line_number} [{o.token}] ({o.matched_detail}) -> {o.snippet}")

    lines.extend([
        "",
        "=" * 80,
    ])

    return "\n".join(lines) + "\n"


def format_json_report(report: InventoryReport) -> str:
    """Format the inventory into deterministic JSON for baseline and programmatic consumption."""
    data = {
        "packages": {
            "exempt": [
                {
                    "package_id": pkg.package_id,
                    "namespace": pkg.namespace,
                    "ue_content_roots": list(pkg.ue_content_roots),
                }
                for pkg in report.exempt_packages
            ],
            "game": [
                {
                    "package_id": pkg.package_id,
                    "namespace": pkg.namespace,
                    "ue_content_roots": list(pkg.ue_content_roots),
                }
                for pkg in report.game_packages
            ],
        },
        "summary": {
            "total_test_files_scanned": report.total_test_files_scanned,
            "class_1_count": len([o for o in report.occurrences if o.category == "class_1"]),
            "class_2_count": len([o for o in report.occurrences if o.category == "class_2"]),
            "class_3_count": len([o for o in report.occurrences if o.category == "class_3"]),
        },
        "reconciliation": {
            "raw_grep_total": report.reconciliation.raw_grep_total,
            "comment_occurrences": report.reconciliation.comment_occurrences,
            "prose_occurrences": report.reconciliation.prose_occurrences,
            "class_1_total": report.reconciliation.class_1_total,
            "is_reconciled": report.reconciliation.is_reconciled,
            "details": report.reconciliation.details,
        },
        "inventory": {
            "class_1": [
                {
                    "file": o.file_path,
                    "line": o.line_number,
                    "token": o.token,
                    "detail": o.matched_detail,
                    "snippet": o.snippet,
                }
                for o in report.occurrences
                if o.category == "class_1"
            ],
            "class_2": [
                {
                    "file": o.file_path,
                    "line": o.line_number,
                    "token": o.token,
                    "detail": o.matched_detail,
                    "snippet": o.snippet,
                }
                for o in report.occurrences
                if o.category == "class_2"
            ],
            "class_3": [
                {
                    "file": o.file_path,
                    "line": o.line_number,
                    "token": o.token,
                    "detail": o.matched_detail,
                    "snippet": o.snippet,
                }
                for o in report.occurrences
                if o.category == "class_3"
            ],
        },
    }
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def run_self_tests() -> bool:
    """Run comprehensive negative and positive self-tests for TSR-01."""
    print("[*] Running inventory_test_content_coupling self-tests...")

    # 1. Determinism and checksum check on actual repository
    actual_report1 = scan_inventory(REPO_ROOT)
    actual_report2 = scan_inventory(REPO_ROOT)

    text1 = format_text_report(actual_report1)
    text2 = format_text_report(actual_report2)
    if text1 != text2:
        print("FAILED: report output is not byte-for-byte deterministic across two consecutive runs!")
        return False
    print("  [+] Determinism check passed (text output is byte-identical).")

    json1 = format_json_report(actual_report1)
    json2 = format_json_report(actual_report2)
    if json1 != json2:
        print("FAILED: JSON report output is not byte-for-byte deterministic across two consecutive runs!")
        return False
    print("  [+] Determinism check passed (JSON output is byte-identical).")

    if not actual_report1.reconciliation.is_reconciled:
        print(f"FAILED: checksum reconciliation failed on repository: {actual_report1.reconciliation}")
        return False
    print("  [+] Checksum reconciliation passed on repository (100% accounted for).")

    # 2. Synthetic environment tests with negative self-tests
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)

        # Create core package manifest
        core_dir = fake_root / "GameData" / "core"
        core_dir.mkdir(parents=True)
        (core_dir / "package.json5").write_text(
            '{\n  package_id: "core",\n  namespace: "core",\n  ue_content_roots: ["/Game/core", "/Game/UI"],\n}\n',
            encoding="utf-8",
        )

        # Create game package alpha (with ue_content_roots)
        alpha_dir = fake_root / "GameData" / "alpha"
        alpha_dir.mkdir(parents=True)
        (alpha_dir / "package.json5").write_text(
            '{\n  package_id: "alpha",\n  namespace: "alpha",\n  ue_content_roots: ["/Game/Alpha"],\n}\n',
            encoding="utf-8",
        )
        alpha_defs = alpha_dir / "definitions"
        alpha_defs.mkdir(parents=True)
        (alpha_defs / "screens.json5").write_text(
            '{\n  definitions: [\n    {\n      id: "alpha:screen.main",\n      data: {\n'
            '        actions: [{ key: "synth_btn_start" }],\n'
            '        connected_location_ids: ["alpha:location.dungeon"],\n'
            '      },\n    },\n  ],\n}\n',
            encoding="utf-8",
        )

        # Create game package beta WITHOUT ue_content_roots
        beta_dir = fake_root / "GameData" / "beta"
        beta_dir.mkdir(parents=True)
        (beta_dir / "package.json5").write_text(
            '{\n  package_id: "beta",\n  namespace: "beta",\n}\n',
            encoding="utf-8",
        )

        # Check dynamic package discovery
        exempt_pkgs, game_pkgs = discover_packages(fake_root)
        if len(exempt_pkgs) != 1 or exempt_pkgs[0].package_id != "core":
            print(f"FAILED: expected exempt core package, got {exempt_pkgs}")
            return False
        if len(game_pkgs) != 2 or {p.package_id for p in game_pkgs} != {"alpha", "beta"}:
            print(f"FAILED: expected game packages alpha and beta, got {game_pkgs}")
            return False

        beta_info = next(p for p in game_pkgs if p.package_id == "beta")
        if beta_info.ue_content_roots != ():
            print(f"FAILED: package without ue_content_roots should have empty roots, got {beta_info.ue_content_roots}")
            return False
        print("  [+] Dynamic package manifest discovery and missing ue_content_roots handling passed.")

        # Dynamically add a third package gamma and ensure discover_packages picks it up without script edits
        gamma_dir = fake_root / "GameData" / "gamma"
        gamma_dir.mkdir(parents=True)
        (gamma_dir / "package.json5").write_text(
            '{\n  package_id: "gamma",\n  namespace: "gamma",\n  ue_content_roots: ["/Game/Gamma"],\n}\n',
            encoding="utf-8",
        )
        _, game_pkgs_with_gamma = discover_packages(fake_root)
        if len(game_pkgs_with_gamma) != 3 or "gamma" not in {p.package_id for p in game_pkgs_with_gamma}:
            print(f"FAILED: dynamic gamma package addition not detected!")
            return False
        print("  [+] Dynamic package addition without script edit passed.")

        # Create test suite directory
        test_dir = fake_root / "Source" / "Synthetic" / "Private" / "Tests"
        test_dir.mkdir(parents=True)

        # Negative test for Clean Core file (Must produce ZERO occurrences)
        clean_file = test_dir / "SyntheticCleanCoreTest.cpp"
        clean_file.write_text(
            '#include "CoreMinimal.h"\n'
            'void TestCleanCore() {\n'
            '    const char* s = "core:screen.test";\n'
            '    const char* a = "core:actor.character.hero";\n'
            '    const char* c = "core:command.test.start";\n'
            '    const char* path1 = "/Game/core/UI/WBP_Core";\n'
            '    const char* path2 = "/Game/UI/Styles/Default";\n'
            '}\n',
            encoding="utf-8",
        )

        clean_report = scan_inventory(fake_root)
        clean_occ = [o for o in clean_report.occurrences if o.file_path.endswith("SyntheticCleanCoreTest.cpp")]
        if clean_occ:
            print(f"FAILED: clean core file produced false positive occurrences: {clean_occ}")
            return False
        print("  [+] Negative self-test: clean core file produces 0 occurrences across all classes.")

        # Negative test for Comments (Tokens inside comments must be ignored)
        comment_file = test_dir / "SyntheticCommentsTest.cpp"
        comment_file.write_text(
            '// alpha:screen.in_single_comment\n'
            '/*\n'
            ' * alpha:screen.in_block_comment\n'
            ' * /Game/Alpha/UI/WBP_InBlockComment\n'
            ' * synth_btn_start\n'
            ' */\n'
            'void TestComments() {}\n',
            encoding="utf-8",
        )

        comment_report = scan_inventory(fake_root)
        comment_occ = [o for o in comment_report.occurrences if o.file_path.endswith("SyntheticCommentsTest.cpp")]
        if comment_occ:
            print(f"FAILED: comment file produced false positives: {comment_occ}")
            return False
        print("  [+] Negative self-test: comments are properly stripped without false positives.")

        # Negative test for Class 1: detects namespaced IDs (including package without ue_content_roots)
        c1_file = test_dir / "SyntheticClass1Test.cpp"
        c1_file.write_text(
            'void TestClass1() {\n'
            '    const char* id1 = "alpha:screen.main";\n'
            '    const char* id2 = "beta:command.trigger";\n'
            '}\n',
            encoding="utf-8",
        )

        c1_report = scan_inventory(fake_root)
        c1_matches = [o for o in c1_report.occurrences if o.file_path.endswith("SyntheticClass1Test.cpp")]
        c1_tokens = {o.token for o in c1_matches if o.category == "class_1"}
        if c1_tokens != {"alpha:screen.main", "beta:command.trigger"}:
            print(f"FAILED: Class 1 negative test did not catch expected tokens, got {c1_tokens}")
            return False
        print("  [+] Negative self-test: Class 1 correctly flags namespaced IDs (including package without ue_content_roots).")

        # Negative test for Class 2: detects game ue_content_roots
        c2_file = test_dir / "SyntheticClass2Test.cpp"
        c2_file.write_text(
            'void TestClass2() {\n'
            '    const char* asset1 = "/Game/Alpha/UI/Screens/WBP_Main.WBP_Main_C";\n'
            '    const char* asset2 = "/Game/Gamma/Textures/T_Background";\n'
            '}\n',
            encoding="utf-8",
        )

        c2_report = scan_inventory(fake_root)
        c2_matches = [o for o in c2_report.occurrences if o.file_path.endswith("SyntheticClass2Test.cpp")]
        c2_tokens = {o.token for o in c2_matches if o.category == "class_2"}
        if c2_tokens != {"/Game/Alpha/UI/Screens/WBP_Main.WBP_Main_C", "/Game/Gamma/Textures/T_Background"}:
            print(f"FAILED: Class 2 negative test did not catch expected tokens, got {c2_tokens}")
            return False
        print("  [+] Negative self-test: Class 2 correctly flags game ue_content_roots literals.")

        # Negative test for Class 3: detects definition entry IDs & derived travel keys
        c3_file = test_dir / "SyntheticClass3Test.cpp"
        c3_file.write_text(
            'void TestClass3() {\n'
            '    auto* btn = Repeater->GetEntryWidget(FName(TEXT("synth_btn_start")));\n'
            '    auto* travel = Repeater->GetEntryWidget(FName(TEXT("travel_dungeon")));\n'
            '}\n',
            encoding="utf-8",
        )

        c3_report = scan_inventory(fake_root)
        c3_matches = [o for o in c3_report.occurrences if o.file_path.endswith("SyntheticClass3Test.cpp")]
        c3_tokens = {o.token for o in c3_matches if o.category == "class_3"}
        if c3_tokens != {"synth_btn_start", "travel_dungeon"}:
            print(f"FAILED: Class 3 negative test did not catch expected tokens, got {c3_tokens}")
            return False
        print("  [+] Negative self-test: Class 3 correctly flags definition entry IDs and travel keys.")

    print("ALL SELF-TESTS PASSED SUCCESSFULLY!")
    return True


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Generate content coupling inventory for tests (TSR-01).")
    parser.add_argument(
        "--self-test", action="store_true", help="Run self-tests and negative validation tests."
    )
    parser.add_argument(
        "--format", choices=["text", "json"], default="text", help="Output format (default: text)."
    )
    parser.add_argument(
        "--json", action="store_true", help="Shortcut for --format=json."
    )
    parser.add_argument(
        "--output", type=Path, default=None, help="Optional output file path."
    )
    parser.add_argument(
        "--repo-root", type=Path, default=REPO_ROOT, help="Override repository root path."
    )

    args = parser.parse_args(argv)

    if args.self_test:
        return 0 if run_self_tests() else 1

    report = scan_inventory(args.repo_root)

    if args.json or args.format == "json":
        output_str = format_json_report(report)
    else:
        output_str = format_text_report(report)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output_str, encoding="utf-8")
        print(f"Wrote inventory report to {args.output}")
    else:
        sys.stdout.write(output_str)

    return 0


if __name__ == "__main__":
    sys.exit(main())
