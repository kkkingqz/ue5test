#!/usr/bin/env python3
"""
Text Collector Tool (DLA-20 / ADR-0027)

Scans Lua scripts in a package root for literal text("...") and fail("...") calls,
identifies missing text definitions in definitions/ and PO entries in localization/,
and appends them idempotently without touching existing translations.

Usage:
    python3 Tools/Content/collect_texts.py <package_root> [--dry-run]
"""

import os
import re
import sys
import argparse
from pathlib import Path


def extract_package_id(package_root: Path) -> str:
    pkg_json5 = package_root / "package.json5"
    if pkg_json5.exists():
        content = pkg_json5.read_text(encoding="utf-8")
        m = re.search(r'["\']?package_id["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', content)
        if m:
            return m.group(1)
        m = re.search(r'["\']?id["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', content)
        if m:
            return m.group(1)
    return package_root.name


def scan_lua_files_for_texts(package_root: Path, package_id: str):
    scripts_dir = package_root / "scripts"
    if not scripts_dir.exists():
        return set()

    found_texts = set()

    # Regex patterns for literal text("...") and fail("...")
    # text("foo") or M.text("foo")
    text_pattern = re.compile(r'(?:\btext|\bM\.text)\s*\(\s*["\']([a-zA-Z0-9_.]+)["\']')
    # fail("foo") or M.fail("foo")
    fail_pattern = re.compile(r'(?:\bfail|\bM\.fail)\s*\(\s*["\']([a-zA-Z0-9_.]+)["\']')

    for root, _, files in os.walk(scripts_dir):
        for f in files:
            if f.endswith(".lua"):
                filepath = Path(root) / f
                content = filepath.read_text(encoding="utf-8")

                for m in text_pattern.finditer(content):
                    key = m.group(1)
                    if key.startswith(f"{package_id}:text."):
                        found_texts.add(key)
                    elif ":" in key:
                        # Other package text ID, ignore or keep if from same package
                        if key.startswith(f"{package_id}:"):
                            found_texts.add(key)
                    else:
                        found_texts.add(f"{package_id}:text.{key}")

                for m in fail_pattern.finditer(content):
                    key = m.group(1)
                    # Convention: fail("travel.insufficient_stamina") -> <pkg>:text.error.travel.insufficient_stamina
                    if key.startswith(f"{package_id}:error."):
                        sub = key[len(f"{package_id}:error."):]
                        found_texts.add(f"{package_id}:text.error.{sub}")
                    elif key.startswith("error."):
                        sub = key[len("error."):]
                        found_texts.add(f"{package_id}:text.error.{sub}")
                    elif ":" not in key:
                        found_texts.add(f"{package_id}:text.error.{key}")

    return found_texts


def get_existing_definition_text_ids(package_root: Path) -> set:
    defs_dir = package_root / "definitions"
    if not defs_dir.exists():
        return set()

    existing_ids = set()
    id_pattern = re.compile(r'["\']?id["\']?\s*:\s*["\']([a-zA-Z0-9_]+:text\.[a-zA-Z0-9_.]+)["\']')

    for root, _, files in os.walk(defs_dir):
        for f in files:
            if f.endswith(".json5") or f.endswith(".json"):
                filepath = Path(root) / f
                content = filepath.read_text(encoding="utf-8")
                for m in id_pattern.finditer(content):
                    existing_ids.add(m.group(1))

    return existing_ids


def get_existing_po_contexts(po_path: Path) -> set:
    if not po_path.exists():
        return set()
    content = po_path.read_text(encoding="utf-8")
    ctxt_pattern = re.compile(r'^msgctxt\s*["\']([^"\']+)["\']', re.MULTILINE)
    return set(ctxt_pattern.findall(content))


def humanize_key(text_id: str) -> str:
    # Extracts the last path tokens and converts to title case
    # e.g. rh:text.location.market.title -> Market Title
    # e.g. rh:text.error.travel.insufficient_stamina -> Travel Insufficient Stamina
    parts = text_id.split(":", 1)[-1].split(".", 1)[-1].split(".")
    return " ".join(p.replace("_", " ").capitalize() for p in parts)


def update_texts_json5(package_root: Path, missing_ids: list, dry_run: bool) -> bool:
    if not missing_ids:
        return False

    defs_dir = package_root / "definitions"
    defs_dir.mkdir(parents=True, exist_ok=True)

    texts_file = defs_dir / "texts.json5"
    if not texts_file.exists():
        # Check for any .json5 file with type: "text"
        for f in defs_dir.glob("*.json5"):
            if 'type: "text"' in f.read_text(encoding="utf-8") or "type: 'text'" in f.read_text(encoding="utf-8"):
                texts_file = f
                break

    new_entries = []
    for tid in sorted(missing_ids):
        source_msg = humanize_key(tid)
        new_entries.append(f'    {{\n      id: "{tid}",\n      data: {{ source_message: "{source_msg}" }},\n    }},')

    if not texts_file.exists():
        content = '{\n  schema_version: 1,\n  type: "text",\n  definitions: [\n' + "\n".join(new_entries) + '\n  ],\n}\n'
        if not dry_run:
            texts_file.write_text(content, encoding="utf-8")
        return True

    orig_content = texts_file.read_text(encoding="utf-8")
    # Find the closing bracket of definitions array
    close_bracket_pos = orig_content.rfind("]")
    if close_bracket_pos != -1:
        prefix = orig_content[:close_bracket_pos].rstrip()
        suffix = orig_content[close_bracket_pos:]
        if not prefix.endswith(","):
            prefix += "\n"
        else:
            prefix += "\n"
        updated_content = prefix + "\n".join(new_entries) + "\n  " + suffix.lstrip()
        if not dry_run:
            texts_file.write_text(updated_content, encoding="utf-8")
        return True

    return False


def update_po_files(package_root: Path, missing_ids: list, dry_run: bool) -> bool:
    if not missing_ids:
        return False

    loc_dir = package_root / "localization"
    loc_dir.mkdir(parents=True, exist_ok=True)

    po_files = list(loc_dir.glob("*.po"))
    if not po_files:
        po_files = [loc_dir / "en.po", loc_dir / "ru.po"]

    modified_any = False
    for po_file in po_files:
        existing_contexts = get_existing_po_contexts(po_file)
        to_add = [tid for tid in sorted(missing_ids) if tid not in existing_contexts]
        if not to_add:
            continue

        entries = []
        for tid in to_add:
            source_msg = humanize_key(tid)
            entries.append(f'msgctxt "{tid}"\nmsgid "{source_msg}"\nmsgstr ""\n')

        append_text = "\n" + "\n".join(entries)
        if po_file.exists():
            orig_content = po_file.read_text(encoding="utf-8")
            if not dry_run:
                po_file.write_text(orig_content.rstrip() + "\n" + append_text, encoding="utf-8")
        else:
            header = 'msgid ""\nmsgstr ""\n"MIME-Version: 1.0\\n"\n"Content-Type: text/plain; charset=UTF-8\\n"\n\n'
            if not dry_run:
                po_file.write_text(header + append_text.lstrip(), encoding="utf-8")
        modified_any = True

    return modified_any


def main():
    parser = argparse.ArgumentParser(description="Collect literal text references and generate definitions/PO entries.")
    parser.add_argument("package_root", type=Path, help="Path to package root (e.g. GameData/rh)")
    parser.add_argument("--dry-run", action="store_true", help="Report missing texts without modifying files")
    args = parser.parse_args()

    package_root = args.package_root.resolve()
    if not package_root.exists() or not package_root.is_dir():
        print(f"Error: package root '{package_root}' does not exist or is not a directory", file=sys.stderr)
        sys.exit(1)

    package_id = extract_package_id(package_root)
    found_texts = scan_lua_files_for_texts(package_root, package_id)

    existing_defs = get_existing_definition_text_ids(package_root)
    missing_defs = [t for t in found_texts if t not in existing_defs]

    loc_dir = package_root / "localization"
    missing_po = set()
    for po_file in loc_dir.glob("*.po"):
        ctxs = get_existing_po_contexts(po_file)
        for t in found_texts:
            if t not in ctxs:
                missing_po.add(t)

    total_missing = set(missing_defs).union(missing_po)

    if not total_missing:
        print(f"[{package_id}] All {len(found_texts)} texts found in Lua scripts are already defined and present in PO catalogs.")
        sys.exit(0)

    print(f"[{package_id}] Found {len(total_missing)} missing text references:")
    for tid in sorted(total_missing):
        print(f"  - {tid}")

    if args.dry_run:
        print("[dry-run] No files modified.")
        sys.exit(0)

    updated_defs = update_texts_json5(package_root, missing_defs, dry_run=False)
    updated_po = update_po_files(package_root, list(total_missing), dry_run=False)

    print(f"[{package_id}] Successfully updated texts (definitions: {updated_defs}, PO catalogs: {updated_po}).")


if __name__ == "__main__":
    main()
