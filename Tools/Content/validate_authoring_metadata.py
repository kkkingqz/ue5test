#!/usr/bin/env python3
"""Validates authoring UI metadata files (*.ui.json5).

Ensures that UI metadata files match their corresponding schema definitions,
contain no stale/unresolved fields, no unknown properties, and valid types.
Includes a --self-test suite for CI validation.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple


def strip_json_comments(text: str) -> str:
    """Strip single-line and multi-line comments from JSON5/JSON text."""
    # Multi-line comments /* ... */
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    # Single-line comments // ... or # ...
    text = re.sub(r"(?://|#)[^\n]*", "", text)
    return text


def json5_to_json(content: str) -> str:
    """Normalize simple JSON5 to standard JSON."""
    text = strip_json_comments(content)
    # Convert single-quoted strings to double-quoted
    text = re.sub(r"'([^'\\]*(?:\\.[^'\\]*)*)'", r'"\1"', text)
    # Quote unquoted object keys
    text = re.sub(r"(?<=[{,\n])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:", r'"\1":', text)
    # Remove trailing commas
    text = re.sub(r",\s*([}\]])", r"\1", text)
    return text


def parse_json5_file(path: Path) -> Tuple[Optional[Any], Optional[str]]:
    """Reads and parses a JSON5 file into a Python object."""
    try:
        raw = path.read_text(encoding="utf-8")
    except Exception as e:
        return None, f"failed to read file: {e}"
    return parse_json5_str(raw)


def parse_json5_str(raw: str) -> Tuple[Optional[Any], Optional[str]]:
    """Parses a JSON5 string into a Python object."""
    try:
        normalized = json5_to_json(raw)
        return json.loads(normalized), None
    except Exception as e:
        return None, f"JSON5 syntax error: {e}"


ALLOWED_ROOT_KEYS = {"fields", "schema_id", "schema_version", "definition_type"}
ALLOWED_FIELD_KEYS = {"label", "description", "category", "order", "widget_hint"}


def validate_ui_metadata_document(
    ui_doc: Any,
    schema_doc: Any,
    ui_file_label: str = "<ui_metadata>",
) -> List[str]:
    """Validates a UI metadata document against its schema document."""
    errors: List[str] = []

    if not isinstance(ui_doc, dict):
        errors.append(f"{ui_file_label}: root must be an object")
        return errors

    for key in ui_doc:
        if key not in ALLOWED_ROOT_KEYS:
            errors.append(f"{ui_file_label}: unknown root property '{key}'")

    if "fields" not in ui_doc:
        errors.append(f"{ui_file_label}: missing required 'fields' object")
        return errors

    fields_obj = ui_doc["fields"]
    if not isinstance(fields_obj, dict):
        errors.append(f"{ui_file_label}: 'fields' must be an object")
        return errors

    if not isinstance(schema_doc, dict):
        errors.append(f"{ui_file_label}: associated schema root is not an object")
        return errors

    schema_root = schema_doc.get("root")
    if not isinstance(schema_root, dict) or schema_root.get("kind") != "object":
        errors.append(f"{ui_file_label}: associated schema does not declare an object root")
        return errors

    schema_fields = schema_root.get("fields", {})
    if not isinstance(schema_fields, dict):
        schema_fields = {}

    for field_name, field_meta in fields_obj.items():
        if field_name not in schema_fields:
            errors.append(
                f"{ui_file_label}: field '{field_name}' declared in UI metadata does not exist in schema"
            )
            continue

        if not isinstance(field_meta, dict):
            errors.append(f"{ui_file_label}: metadata for field '{field_name}' must be an object")
            continue

        for prop_name, prop_val in field_meta.items():
            if prop_name not in ALLOWED_FIELD_KEYS:
                errors.append(
                    f"{ui_file_label}: unknown property '{prop_name}' in field metadata for '{field_name}'"
                )
                continue

            if prop_name in ("label", "description", "category", "widget_hint"):
                if not isinstance(prop_val, str):
                    errors.append(
                        f"{ui_file_label}: field '{field_name}' property '{prop_name}' must be a string"
                    )
            elif prop_name == "order":
                # bool is a subclass of int in Python, so check bool explicitly
                if isinstance(prop_val, bool) or not isinstance(prop_val, int):
                    errors.append(
                        f"{ui_file_label}: field '{field_name}' property 'order' must be an integer"
                    )

    return errors


def find_matching_schema(ui_path: Path) -> Optional[Path]:
    """Finds the corresponding schema file for a .ui.json5 file."""
    parent = ui_path.parent
    name = ui_path.name
    # Strip .ui.json5
    if name.endswith(".ui.json5"):
        stem = name[:-9]
    else:
        return None

    candidate1 = parent / f"{stem}.schema.json5"
    if candidate1.is_file():
        return candidate1

    candidate2 = parent / f"{stem}.json5"
    if candidate2.is_file():
        return candidate2

    return None


def validate_package_ui_metadata(package_root: Path) -> List[str]:
    """Scans and validates all UI metadata files in a package directory."""
    errors: List[str] = []
    schemas_dir = package_root / "schemas"
    if not schemas_dir.is_dir():
        return errors

    for ui_file in sorted(schemas_dir.glob("*.ui.json5")):
        schema_file = find_matching_schema(ui_file)
        if not schema_file:
            errors.append(
                f"{ui_file}: orphan UI metadata file; matching schema ({ui_file.stem}.schema.json5) not found"
            )
            continue

        ui_doc, ui_err = parse_json5_file(ui_file)
        if ui_err:
            errors.append(f"{ui_file}: {ui_err}")
            continue

        schema_doc, schema_err = parse_json5_file(schema_file)
        if schema_err:
            errors.append(f"{schema_file}: {schema_err}")
            continue

        file_errors = validate_ui_metadata_document(ui_doc, schema_doc, str(ui_file))
        errors.extend(file_errors)

    return errors


def run_self_test() -> bool:
    """Executes self-test test cases."""
    print("[*] Running validate_authoring_metadata self-test...")

    schema_json = """
    {
      id: "test:schema.definition.item.v1",
      definition_type: "item",
      schema_version: 1,
      root: {
        kind: "object",
        fields: {
          price: { kind: "int64", required: true, min: 0 },
          name: { kind: "string", required: true },
        },
      },
    }
    """
    schema_doc, err = parse_json5_str(schema_json)
    assert schema_doc is not None, f"Mock schema parse failed: {err}"

    # 1. Valid case
    valid_ui = """
    {
      fields: {
        price: {
          label: "Price in Gold",
          description: "Cost in gold",
          category: "Economy",
          order: 1,
          widget_hint: "number",
        },
        name: {
          label: "Name",
          order: 2,
        },
      },
    }
    """
    ui_doc, err = parse_json5_str(valid_ui)
    assert ui_doc is not None, f"Valid UI parse failed: {err}"
    errors = validate_ui_metadata_document(ui_doc, schema_doc, "valid_case")
    if errors:
        print(f"Self-test failed on valid case: {errors}", file=sys.stderr)
        return False

    # 2. Stale / Unresolved field
    stale_ui = """
    {
      fields: {
        price: { label: "Price" },
        stale_field: { label: "Stale" },
      },
    }
    """
    ui_doc, _ = parse_json5_str(stale_ui)
    errors = validate_ui_metadata_document(ui_doc, schema_doc, "stale_case")
    if not any("stale_field" in e and "does not exist in schema" in e for e in errors):
        print(f"Self-test failed: expected unresolved field error, got {errors}", file=sys.stderr)
        return False

    # 3. Unknown root property
    unknown_root_ui = """
    {
      unknown_root: 123,
      fields: {
        price: { label: "Price" },
      },
    }
    """
    ui_doc, _ = parse_json5_str(unknown_root_ui)
    errors = validate_ui_metadata_document(ui_doc, schema_doc, "unknown_root_case")
    if not any("unknown root property 'unknown_root'" in e for e in errors):
        print(f"Self-test failed: expected unknown root property error, got {errors}", file=sys.stderr)
        return False

    # 4. Unknown field property
    unknown_field_prop_ui = """
    {
      fields: {
        price: {
          label: "Price",
          bad_prop: "invalid",
        },
      },
    }
    """
    ui_doc, _ = parse_json5_str(unknown_field_prop_ui)
    errors = validate_ui_metadata_document(ui_doc, schema_doc, "unknown_field_prop_case")
    if not any("unknown property 'bad_prop'" in e for e in errors):
        print(f"Self-test failed: expected unknown field property error, got {errors}", file=sys.stderr)
        return False

    # 5. Invalid property type
    invalid_type_ui = """
    {
      fields: {
        price: {
          order: "not_a_number",
        },
      },
    }
    """
    ui_doc, _ = parse_json5_str(invalid_type_ui)
    errors = validate_ui_metadata_document(ui_doc, schema_doc, "invalid_type_case")
    if not any("property 'order' must be an integer" in e for e in errors):
        print(f"Self-test failed: expected invalid type error, got {errors}", file=sys.stderr)
        return False

    print("[+] All validate_authoring_metadata self-tests passed.")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate authoring UI metadata (*.ui.json5).")
    parser.add_argument("package_roots", nargs="*", type=Path, help="Paths to package roots to validate.")
    parser.add_argument("--self-test", action="store_true", help="Run internal self-tests.")

    args = parser.parse_args()

    if args.self_test:
        return 0 if run_self_test() else 1

    roots = args.package_roots
    if not roots:
        # Default roots
        repo_root = Path(__file__).resolve().parent.parent.parent
        roots = [
            repo_root / "GameData" / "core",
            repo_root / "GameData" / "rh",
            repo_root / "Tests" / "Fixtures" / "PortableContentCore" / "valid" / "core",
        ]

    all_errors: List[str] = []
    validated_count = 0

    for root in roots:
        if not root.is_dir():
            all_errors.append(f"Directory not found: {root}")
            continue
        errors = validate_package_ui_metadata(root)
        all_errors.extend(errors)
        validated_count += 1

    if all_errors:
        print(f"Authoring metadata validation failed with {len(all_errors)} error(s):", file=sys.stderr)
        for err in all_errors:
            print(f"  ERROR: {err}", file=sys.stderr)
        return 1

    print(f"Authoring metadata validation passed across {validated_count} package(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
