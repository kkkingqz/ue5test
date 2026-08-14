#!/usr/bin/env python3
"""Validate the shared PortableContentCore fixture inventory."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path, PurePosixPath


def fail(message: str) -> int:
    print(f"ERROR: {message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixture_root", type=Path)
    arguments = parser.parse_args()
    root = arguments.fixture_root.resolve()
    index_path = root / "fixtures.index"
    if not index_path.is_file():
        return fail(f"fixture index is missing: {index_path}")

    try:
        index_text = index_path.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        return fail(f"fixture index is not valid UTF-8: {error}")

    entries = [
        line.strip()
        for line in index_text.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]
    if not entries:
        return fail("fixture index is empty")
    if entries != sorted(entries):
        return fail("fixture index entries are not bytewise sorted")
    if len(entries) != len(set(entries)):
        return fail("fixture index contains duplicate paths")

    indexed = set(entries)
    for entry in entries:
        relative = PurePosixPath(entry)
        if relative.is_absolute() or ".." in relative.parts or relative.suffix != ".json5":
            return fail(f"invalid fixture path: {entry}")
        target = (root / Path(*relative.parts)).resolve()
        try:
            target.relative_to(root)
        except ValueError:
            return fail(f"fixture path escapes root: {entry}")
        if not target.is_file():
            return fail(f"indexed fixture does not exist: {entry}")
        try:
            target.read_text(encoding="utf-8")
        except UnicodeDecodeError as error:
            return fail(f"fixture is not valid UTF-8 ({entry}): {error}")

    discovered = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*.json5")
        if path.is_file()
    }
    missing_from_index = sorted(discovered - indexed)
    missing_from_tree = sorted(indexed - discovered)
    if missing_from_index:
        return fail(f"unindexed fixture files: {', '.join(missing_from_index)}")
    if missing_from_tree:
        return fail(f"missing fixture files: {', '.join(missing_from_tree)}")

    required_groups = ("valid/core/", "valid/test_mod/", "invalid/")
    for group in required_groups:
        if not any(entry.startswith(group) for entry in entries):
            return fail(f"fixture group has no indexed sources: {group}")
    if not (root / "valid/empty_core").is_dir():
        return fail("empty core fixture directory is missing")

    forbidden_expectations = tuple(root.rglob("expected*"))
    if forbidden_expectations:
        rendered = ", ".join(path.relative_to(root).as_posix() for path in forbidden_expectations)
        return fail(f"PCC-01 must not freeze expected diagnostics/snapshots/hashes: {rendered}")

    print(f"PortableContentCore fixture validation passed: {len(entries)} shared sources.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
