#!/usr/bin/env python3
"""Compiles PO localization catalogs from <package-root>/localization/<locale>.po
into deterministic Unreal Engine String Table CSV format.
"""

from __future__ import annotations

import argparse
import csv
import io
import os
import re
import sys
from pathlib import Path


def parse_po_file(po_path: Path) -> dict[str, str]:
    """Parses a PO file into a dictionary of text_id -> translated_string."""
    content = po_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    entries: dict[str, str] = {}
    current_ctxt: str | None = None
    current_id: str | None = None
    current_str: str | None = None
    target_field: str | None = None

    def decode_str(s: str) -> str:
        s = s.strip()
        if s.startswith('"') and s.endswith('"'):
            s = s[1:-1]
        res = []
        i = 0
        while i < len(s):
            if s[i] == '\\' and i + 1 < len(s):
                esc = s[i + 1]
                if esc == 'n':
                    res.append('\n')
                elif esc == 'r':
                    res.append('\r')
                elif esc == 't':
                    res.append('\t')
                elif esc == '"':
                    res.append('"')
                elif esc == '\\':
                    res.append('\\')
                elif esc == "'":
                    res.append("'")
                else:
                    res.append(esc)
                i += 2
            else:
                res.append(s[i])
                i += 1
        return ''.join(res)

    def commit_entry():
        nonlocal current_ctxt, current_id, current_str, target_field
        if current_id is not None and current_str is not None:
            # Header check: empty msgid and empty msgctxt
            if current_id == "" and (current_ctxt is None or current_ctxt == ""):
                pass
            else:
                key = current_ctxt if current_ctxt else current_id
                val = current_str if current_str else current_id
                entries[key] = val
        current_ctxt = None
        current_id = None
        current_str = None
        target_field = None

    for line in lines:
        stripped = line.strip()
        if not stripped:
            commit_entry()
            continue
        if stripped.startswith("#"):
            continue
        if stripped.startswith("msgctxt"):
            commit_entry()
            current_ctxt = decode_str(stripped[7:])
            target_field = "ctxt"
        elif stripped.startswith("msgid"):
            if current_id is not None:
                commit_entry()
            current_id = decode_str(stripped[5:])
            target_field = "id"
        elif stripped.startswith("msgstr"):
            pos = stripped.find('"')
            if pos != -1:
                current_str = decode_str(stripped[pos:])
            else:
                current_str = ""
            target_field = "str"
        elif stripped.startswith('"'):
            continuation = decode_str(stripped)
            if target_field == "ctxt" and current_ctxt is not None:
                current_ctxt += continuation
            elif target_field == "id" and current_id is not None:
                current_id += continuation
            elif target_field == "str" and current_str is not None:
                current_str += continuation

    commit_entry()
    return entries


def export_string_table_csv(entries: dict[str, str]) -> str:
    """Exports entries as deterministic sorted CSV for Unreal Engine String Table."""
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(["Key", "SourceString"])
    for key in sorted(entries.keys()):
        writer.writerow([key, entries[key]])
    return output.getvalue()


def compile_package_localization(package_root: Path, output_dir: Path | None = None, locale: str | None = None) -> list[Path]:
    loc_dir = package_root / "localization"
    if not loc_dir.is_dir():
        return []

    if output_dir is None:
        output_dir = loc_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    package_id = package_root.name
    generated_files: list[Path] = []

    po_files = sorted(loc_dir.glob("*.po"))
    if locale:
        po_files = [p for p in po_files if p.stem == locale]

    for po_file in po_files:
        loc = po_file.stem
        entries = parse_po_file(po_file)
        csv_content = export_string_table_csv(entries)
        out_csv = output_dir / f"{package_id}_{loc}.csv"
        out_csv.write_text(csv_content, encoding="utf-8")
        generated_files.append(out_csv)

    return generated_files


def main():
    parser = argparse.ArgumentParser(description="Compile PO catalogs to UE String Table CSVs.")
    parser.add_argument("package_root", type=Path, help="Path to package root")
    parser.add_argument("--locale", type=str, default=None, help="Specific locale to compile")
    parser.add_argument("--output-dir", type=Path, default=None, help="Output directory for CSV files")

    args = parser.parse_args()
    if not args.package_root.is_dir():
        print(f"Error: {args.package_root} is not a directory", file=sys.stderr)
        sys.exit(2)

    generated = compile_package_localization(args.package_root, args.output_dir, args.locale)
    for g in generated:
        print(f"Compiled: {g}")


if __name__ == "__main__":
    main()
