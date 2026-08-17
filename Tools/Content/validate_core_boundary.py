#!/usr/bin/env python3
"""Validates that GameData/core contains no gameplay definitions or gameplay schemas.
Core owns framework mechanisms (screens, texts, resources, minimal actor schema).
Concrete game entities (items, locations, actor instances, gameplay schemas) must reside in gameplay packages (e.g. GameData/rh).
Rule ID: CORE_GAMEPLAY_BOUNDARY_RULE
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CORE_DEFINITIONS_DIR = REPO_ROOT / "GameData" / "core" / "definitions"
CORE_SCHEMAS_DIR = REPO_ROOT / "GameData" / "core" / "schemas"

FORBIDDEN_DEFINITION_KINDS = {"actor", "item", "location"}
FORBIDDEN_SCHEMA_KINDS = {"item", "location"}
FORBIDDEN_ACTOR_SCHEMA_FIELDS = {"base_hp", "name_text_id", "inventory", "price", "gold"}

ID_PATTERN = re.compile(r'id:\s*["\']([a-z0-9_]+):([a-z0-9_]+)\.([a-z0-9_.]+)["\']')
TYPE_PATTERN = re.compile(r'type:\s*["\']([a-z0-9_]+)["\']')
DEF_TYPE_PATTERN = re.compile(r'definition_type:\s*["\']([a-z0-9_]+)["\']')
FIELD_KEY_PATTERN = re.compile(r'([a-z0-9_]+):\s*\{')


def scan_definitions(defs_dir: Path) -> list[str]:
    violations: list[str] = []
    if not defs_dir.exists():
        return violations

    for file_path in sorted(defs_dir.glob("*.json5")):
        try:
            content = file_path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        rel_path = file_path.relative_to(REPO_ROOT) if file_path.is_relative_to(REPO_ROOT) else file_path

        for line_num, line in enumerate(content.splitlines(), start=1):
            type_match = TYPE_PATTERN.search(line)
            if type_match:
                kind = type_match.group(1)
                if kind in FORBIDDEN_DEFINITION_KINDS:
                    violations.append(
                        f"{rel_path}:{line_num}: error [CORE_GAMEPLAY_BOUNDARY_RULE]: "
                        f"GameData/core/definitions/ must not contain definition file with type '{kind}'."
                    )

            id_match = ID_PATTERN.search(line)
            if id_match:
                ns, kind, path = id_match.groups()
                full_id = f"{ns}:{kind}.{path}"
                if kind in FORBIDDEN_DEFINITION_KINDS:
                    violations.append(
                        f"{rel_path}:{line_num}: error [CORE_GAMEPLAY_BOUNDARY_RULE]: "
                        f"GameData/core/definitions/ must not declare definition '{full_id}' of forbidden gameplay kind '{kind}'."
                    )

    return violations


def scan_schemas(schemas_dir: Path) -> list[str]:
    violations: list[str] = []
    if not schemas_dir.exists():
        return violations

    for file_path in sorted(schemas_dir.glob("*.schema.json5")):
        try:
            content = file_path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        rel_path = file_path.relative_to(REPO_ROOT) if file_path.is_relative_to(REPO_ROOT) else file_path

        is_actor_schema = False
        for line_num, line in enumerate(content.splitlines(), start=1):
            def_type_match = DEF_TYPE_PATTERN.search(line)
            if def_type_match:
                def_type = def_type_match.group(1)
                if def_type in FORBIDDEN_SCHEMA_KINDS:
                    violations.append(
                        f"{rel_path}:{line_num}: error [CORE_GAMEPLAY_BOUNDARY_RULE]: "
                        f"GameData/core/schemas/ must not contain gameplay schema for definition_type '{def_type}'."
                    )
                if def_type == "actor":
                    is_actor_schema = True

            if is_actor_schema:
                field_match = FIELD_KEY_PATTERN.search(line)
                if field_match:
                    field_name = field_match.group(1)
                    if field_name in FORBIDDEN_ACTOR_SCHEMA_FIELDS:
                        violations.append(
                            f"{rel_path}:{line_num}: error [CORE_GAMEPLAY_BOUNDARY_RULE]: "
                            f"GameData/core/schemas/actor_v1 must not contain gameplay field '{field_name}' (use extension schemas in gameplay packages)."
                        )

    return violations


def run_validation(defs_dir: Path, schemas_dir: Path) -> list[str]:
    violations: list[str] = []
    violations.extend(scan_definitions(defs_dir))
    violations.extend(scan_schemas(schemas_dir))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_core_boundary self-test...")

    # 1. Positive test on live repo
    actual_violations = run_validation(CORE_DEFINITIONS_DIR, CORE_SCHEMAS_DIR)
    if actual_violations:
        print("FAILED: expected clean core definitions and schemas, found violations:\n" + "\n".join(actual_violations))
        return False

    # 2. Negative test: forbidden item definition in core
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_defs = Path(tmpdir) / "definitions"
        fake_defs.mkdir(parents=True)
        fake_item_def = fake_defs / "items.json5"
        fake_item_def.write_text(
            '{\n  schema_version: 1,\n  type: "item",\n  definitions: [\n    { id: "core:item.weapon.sword", data: {} }\n  ]\n}\n',
            encoding="utf-8",
        )
        fake_schemas = Path(tmpdir) / "schemas"
        fake_schemas.mkdir(parents=True)

        violations = run_validation(fake_defs, fake_schemas)
        if not violations:
            print("FAILED: self-test did not catch forbidden item definition in core")
            return False
        if not any("CORE_GAMEPLAY_BOUNDARY_RULE" in v and "core:item.weapon.sword" in v for v in violations):
            print(f"FAILED: violation message mismatch for forbidden item definition: {violations}")
            return False

    # 3. Negative test: forbidden actor definition in core
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_defs = Path(tmpdir) / "definitions"
        fake_defs.mkdir(parents=True)
        fake_actor_def = fake_defs / "actors.json5"
        fake_actor_def.write_text(
            '{\n  schema_version: 1,\n  type: "actor",\n  definitions: [\n    { id: "core:actor.npc.guard", data: { discriminator: "npc" } }\n  ]\n}\n',
            encoding="utf-8",
        )
        fake_schemas = Path(tmpdir) / "schemas"
        fake_schemas.mkdir(parents=True)

        violations = run_validation(fake_defs, fake_schemas)
        if not violations:
            print("FAILED: self-test did not catch forbidden actor definition in core")
            return False
        if not any("CORE_GAMEPLAY_BOUNDARY_RULE" in v and "core:actor.npc.guard" in v for v in violations):
            print(f"FAILED: violation message mismatch for forbidden actor definition: {violations}")
            return False

    # 4. Negative test: forbidden item schema in core
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_defs = Path(tmpdir) / "definitions"
        fake_defs.mkdir(parents=True)
        fake_schemas = Path(tmpdir) / "schemas"
        fake_schemas.mkdir(parents=True)
        fake_item_schema = fake_schemas / "item_v1.schema.json5"
        fake_item_schema.write_text(
            '{\n  id: "core:schema.definition.item.v1",\n  definition_type: "item",\n  schema_version: 1,\n  root: { kind: "object", fields: {} }\n}\n',
            encoding="utf-8",
        )

        violations = run_validation(fake_defs, fake_schemas)
        if not violations:
            print("FAILED: self-test did not catch forbidden item schema in core")
            return False
        if not any("CORE_GAMEPLAY_BOUNDARY_RULE" in v and "item" in v for v in violations):
            print(f"FAILED: violation message mismatch for forbidden item schema: {violations}")
            return False

    # 5. Negative test: forbidden actor schema fields in core
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_defs = Path(tmpdir) / "definitions"
        fake_defs.mkdir(parents=True)
        fake_schemas = Path(tmpdir) / "schemas"
        fake_schemas.mkdir(parents=True)
        fake_actor_schema = fake_schemas / "actor_v1.schema.json5"
        fake_actor_schema.write_text(
            '{\n  id: "core:schema.definition.actor.v1",\n  definition_type: "actor",\n  schema_version: 1,\n  root: {\n    kind: "object",\n    fields: {\n      discriminator: { kind: "string", required: true },\n      base_hp: { kind: "int64", required: true }\n    }\n  }\n}\n',
            encoding="utf-8",
        )

        violations = run_validation(fake_defs, fake_schemas)
        if not violations:
            print("FAILED: self-test did not catch forbidden base_hp field in core actor schema")
            return False
        if not any("CORE_GAMEPLAY_BOUNDARY_RULE" in v and "base_hp" in v for v in violations):
            print(f"FAILED: violation message mismatch for forbidden actor schema field: {violations}")
            return False

    print("  PASS: validate_core_boundary self-test succeeded.")
    return True


def main() -> int:
    if "--self-test" in sys.argv:
        return 0 if run_self_test() else 1

    violations = run_validation(CORE_DEFINITIONS_DIR, CORE_SCHEMAS_DIR)
    if violations:
        print(f"Core gameplay boundary validation failed ({len(violations)} violations):", file=sys.stderr)
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1

    print("Core gameplay boundary validation passed: no gameplay definitions or schemas in GameData/core/.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
