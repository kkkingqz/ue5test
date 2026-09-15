#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PACKAGE_DISCOVERY_SOURCE = (
    REPO_ROOT / "Source" / "GV2ContentHostSupport" / "Private" / "PackageDiscovery.cpp"
)
WRAPPER_NAME = "ReadManifestFileToString"
OWNER_NAME = "DiscoverPackageArtifactFromDirectory"


def _matching_brace(source: str, open_brace: int) -> int | None:
    depth = 0
    in_string: str | None = None
    escaped = False
    index = open_brace
    while index < len(source):
        char = source[index]
        if in_string is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            index += 1
            continue
        if source.startswith("//", index):
            newline = source.find("\n", index + 2)
            index = len(source) if newline < 0 else newline + 1
            continue
        if source.startswith("/*", index):
            comment_end = source.find("*/", index + 2)
            index = len(source) if comment_end < 0 else comment_end + 2
            continue
        if char in {'"', "'"}:
            in_string = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return None


def _function_body(source: str, function_name: str) -> str | None:
    match = re.search(rf"\b{re.escape(function_name)}\s*\([^;{{]*\)\s*\{{", source)
    if match is None:
        return None
    open_brace = source.find("{", match.start(), match.end())
    close_brace = _matching_brace(source, open_brace)
    if close_brace is None:
        return None
    return source[open_brace + 1 : close_brace]


def find_violations(source: str) -> list[str]:
    violations: list[str] = []

    wrapper_body = _function_body(source, WRAPPER_NAME)
    owner_body = _function_body(source, OWNER_NAME)
    if wrapper_body is None:
        violations.append(f"missing {WRAPPER_NAME} definition")
    if owner_body is None:
        violations.append(f"missing {OWNER_NAME} definition")

    # One definition plus exactly one call from the artifact owner is the closed
    # manifest-read surface. A second call is a second read opportunity.
    wrapper_occurrences = len(re.findall(rf"\b{WRAPPER_NAME}\s*\(", source))
    if wrapper_occurrences != 2:
        violations.append(
            f"expected exactly one manifest read call plus its definition; found {wrapper_occurrences - 1} calls"
        )
    if owner_body is not None and len(re.findall(rf"\b{WRAPPER_NAME}\s*\(", owner_body)) != 1:
        violations.append(f"{OWNER_NAME} must own exactly one manifest read call")

    direct_literal_read = re.search(
        r"(?:\bReadFileToString\s*\(|\bstd::ifstream\s+\w+\s*\()[^;)]*package\.json5",
        source,
        re.DOTALL,
    )
    if direct_literal_read is not None:
        violations.append(f"direct package.json5 read bypass; route through {WRAPPER_NAME}")

    manifest_path_vars = set(
        re.findall(
            r"\b([A-Za-z_]\w*)\s*=\s*[^;\n]*[\"']package\.json5[\"']\s*;",
            source,
        )
    )
    for variable_name in sorted(manifest_path_vars):
        if re.search(rf"\bReadFileToString\s*\(\s*{re.escape(variable_name)}\s*\)", source):
            violations.append(
                f"direct package.json5 read bypass through {variable_name}; route through {WRAPPER_NAME}"
            )

    if wrapper_body is not None and len(re.findall(r"\bReadFileToString\s*\(", wrapper_body)) != 1:
        violations.append(f"{WRAPPER_NAME} must perform exactly one low-level file read")

    return violations


def main(argv: list[str]) -> int:
    if argv:
        print("usage: validate_manifest_single_read.py", file=sys.stderr)
        return 2
    if not PACKAGE_DISCOVERY_SOURCE.is_file():
        print(f"missing source: {PACKAGE_DISCOVERY_SOURCE}", file=sys.stderr)
        return 2
    violations = find_violations(PACKAGE_DISCOVERY_SOURCE.read_text(encoding="utf-8"))
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("Manifest single-read gate passed: one typed read owned by package artifact discovery.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
