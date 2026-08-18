#!/usr/bin/env python3
"""
Module Discovery and Manifest Generator (SAS-06..09 / ADR-0028)

Discovers Lua scripts in a package root, derives module IDs for non-replaceable
modules, statically extracts dependencies from literal require("...") calls,
validates graph invariants (no cycles, no dynamic require, explicit IDs for
replaceable modules), and generates a canonical, byte-deterministic manifest.lua.

Usage:
    python3 Tools/Content/generate_manifest.py <package_root> [--check] [--output=<path>] [--format=text|json]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple


def strip_json_comments(text: str) -> str:
    """Strip single-line and multi-line comments from JSON5/JSON text."""
    # Multi-line comments /* ... */
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Single-line comments // ... or # ...
    text = re.sub(r'(?://|#)[^\n]*', '', text)
    return text


def json5_to_json(content: str) -> str:
    """Normalize simple JSON5 to standard JSON."""
    text = strip_json_comments(content)
    # Convert single-quoted strings to double-quoted
    text = re.sub(r"'([^'\\]*(?:\\.[^'\\]*)*)'", r'"\1"', text)
    # Quote unquoted object keys
    text = re.sub(r'(?<=[{,\n])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:', r'"\1":', text)
    # Remove trailing commas
    text = re.sub(r',\s*([}\]])', r'\1', text)
    return text


def parse_package_metadata(package_root: Path) -> Tuple[str, Dict[str, Any]]:
    """Extract package_id and optional modules metadata from package.json5."""
    pkg_json5 = package_root / "package.json5"
    if not pkg_json5.exists():
        return package_root.name, {}

    content = pkg_json5.read_text(encoding="utf-8")
    json_text = json5_to_json(content)

    data: Dict[str, Any] = {}
    try:
        data = json.loads(json_text)
    except Exception:
        # Fallback simple extractor if custom JSON5 constructs are used
        pkg_id_m = re.search(r'["\']?(?:package_id|id)["\']?\s*:\s*["\']([a-zA-Z0-9_]+)["\']', content)
        pkg_id = pkg_id_m.group(1) if pkg_id_m else package_root.name
        return pkg_id, {}

    pkg_id = data.get("package_id") or data.get("id") or package_root.name
    return pkg_id, data


def strip_lua_comments(code: str) -> str:
    """Strip multi-line and single-line Lua comments from source."""
    # Multi-line --[[ ... ]] or --[=[ ... ]=]
    code = re.sub(r'--\[(=*)\[.*?\]\1\]', '', code, flags=re.DOTALL)
    # Single-line -- ...
    code = re.sub(r'--[^\n]*', '', code)
    return code


def scan_dependencies_and_validate(code: str, source_rel: str) -> List[str]:
    """
    Extract dependencies from literal require calls and reject dynamic require.
    SAS-08: static scan collects dependencies from literal require("...");
    dynamic require(variable) in discovered module is rejected.
    """
    clean_code = strip_lua_comments(code)

    # Check for dynamic require
    # Matches require(...) where argument does not start with quote
    dynamic_pattern = re.compile(r'\brequire\s*\(\s*([^"\'\s\)][^\)]*)\)')
    for match in dynamic_pattern.finditer(clean_code):
        arg = match.group(1).strip()
        # If argument is not a string literal
        if not (arg.startswith('"') or arg.startswith("'")):
            raise ValueError(
                f"DynamicRequireDisallowed: dynamic require({arg}) call in module '{source_rel}' "
                f"is forbidden; dependencies must be string literals"
            )

    # Check for require "string" or require 'string' without parentheses
    dynamic_no_paren = re.compile(r'\brequire\s+([a-zA-Z_][a-zA-Z0-9_.]*)')
    for match in dynamic_no_paren.finditer(clean_code):
        arg = match.group(1).strip()
        raise ValueError(
            f"DynamicRequireDisallowed: dynamic require {arg} call in module '{source_rel}' "
            f"is forbidden; dependencies must be string literals"
        )

    # Extract literal require("...") and require('...')
    literal_pattern = re.compile(r'\brequire\s*\(?\s*["\']([^"\']+)["\']\s*\)?')
    deps: Set[str] = set()
    for match in literal_pattern.finditer(clean_code):
        dep = match.group(1).strip()
        if dep:
            deps.add(dep)

    return sorted(deps)


def derive_module_id_from_path(package_id: str, relative_path: str) -> str:
    """
    SAS-07: derive module ID from path for non-replaceable modules:
    scripts/runtime/actors.lua -> <package_id>:module.runtime.actors
    """
    path_without_ext = os.path.splitext(relative_path)[0]
    dotted_path = path_without_ext.replace("/", ".").replace("\\", ".")
    return f"{package_id}:module.{dotted_path}"


def check_cycles(modules_by_id: Dict[str, Dict[str, Any]]) -> None:
    """Check for circular dependencies within the discovered module set."""
    visited: Dict[str, int] = {}  # 0: visiting, 1: visited

    def dfs(mod_id: str, path: List[str]) -> None:
        visited[mod_id] = 0
        mod = modules_by_id.get(mod_id)
        if mod:
            for dep in mod.get("dependencies", []):
                if dep in modules_by_id:
                    if visited.get(dep) == 0:
                        cycle_str = " -> ".join(path + [dep])
                        raise ValueError(f"CircularDependencyDetected: dependency cycle detected: {cycle_str}")
                    elif dep not in visited:
                        dfs(dep, path + [dep])
        visited[mod_id] = 1

    for mod_id in modules_by_id:
        if mod_id not in visited:
            dfs(mod_id, [mod_id])


def build_package_manifest(package_root: Path) -> Tuple[Dict[str, Any], str]:
    """
    Discovers all scripts in package_root/scripts/ and produces the manifest table and Lua text.
    """
    package_id, pkg_meta = parse_package_metadata(package_root)

    scripts_dir = package_root / "scripts"
    if not scripts_dir.exists():
        scripts_dir = package_root / "Scripts"
    if not scripts_dir.exists():
        raise FileNotFoundError(f"Scripts directory not found in package root: {package_root}")

    # Gather explicit module configurations from package.json5
    modules_meta: Dict[str, Dict[str, Any]] = {}
    for m in pkg_meta.get("modules", []):
        src = m.get("source")
        if src:
            normalized_src = str(Path(src)).replace("\\", "/")
            modules_meta[normalized_src] = m

    discovered_files: List[Tuple[str, Path]] = []
    for root, _, files in os.walk(scripts_dir):
        for f in files:
            if f.endswith(".lua"):
                full_path = Path(root) / f
                rel_path = full_path.relative_to(scripts_dir).as_posix()
                # Skip manifest files themselves
                if rel_path in ("manifest.lua", "bootstrap/manifest.lua"):
                    continue
                discovered_files.append((rel_path, full_path))

    # Sort files lexicographically for deterministic processing
    discovered_files.sort(key=lambda x: x[0])

    module_records: List[Dict[str, Any]] = []
    seen_module_ids: Set[str] = set()
    modules_by_id: Dict[str, Dict[str, Any]] = {}

    for rel_path, full_path in discovered_files:
        content = full_path.read_text(encoding="utf-8")
        extracted_deps = scan_dependencies_and_validate(content, rel_path)

        meta = modules_meta.get(rel_path, {})
        b_replaceable = meta.get("replaceable", False)
        explicit_id = meta.get("module_id")

        if b_replaceable and not explicit_id:
            # SAS-07: Replaceable module must declare explicit ID
            raise ValueError(
                f"ReplaceableModuleRequiresExplicitId: module at '{rel_path}' is marked replaceable "
                f"but does not declare an explicit module_id"
            )

        if explicit_id:
            module_id = explicit_id
        else:
            module_id = derive_module_id_from_path(package_id, rel_path)

        if module_id in seen_module_ids:
            raise ValueError(f"DuplicateModuleId: duplicate module ID '{module_id}' from source '{rel_path}'")
        seen_module_ids.add(module_id)

        # Merge dependencies: literal requires + any explicit deps
        all_deps = set(extracted_deps)
        for dep in meta.get("dependencies", []):
            all_deps.add(dep)
        # Filter self dependencies
        all_deps.discard(module_id)
        sorted_deps = sorted(all_deps)

        b_authoring = meta.get("authoring", False)
        if not b_authoring and package_id != "core" and rel_path.startswith("authoring/"):
            b_authoring = True

        rec: Dict[str, Any] = {
            "module_id": module_id,
            "source": rel_path,
            "dependencies": sorted_deps,
            "replaceable": b_replaceable,
        }
        if b_authoring:
            rec["authoring"] = True

        module_records.append(rec)
        modules_by_id[module_id] = rec

    # Validate graph cycles
    check_cycles(modules_by_id)

    # Sort module records deterministically by module_id
    module_records.sort(key=lambda x: x["module_id"])

    manifest_data: Dict[str, Any] = {
        "modules": module_records,
    }

    if package_id == "core":
        manifest_data["entry_module_id"] = pkg_meta.get("entry_module_id", "core:module.bootstrap.main")

    # Generate canonical Lua code
    lines = ["return {"]
    if "entry_module_id" in manifest_data:
        lines.append(f'    entry_module_id = "{manifest_data["entry_module_id"]}",')

    lines.append("    modules = {")
    for rec in module_records:
        lines.append("        {")
        lines.append(f'            module_id = "{rec["module_id"]}",')
        lines.append(f'            source = "{rec["source"]}",')
        if rec.get("dependencies"):
            lines.append("            dependencies = {")
            for dep in rec["dependencies"]:
                lines.append(f'                "{dep}",')
            lines.append("            },")
        else:
            lines.append("            dependencies = {},")

        lines.append(f'            replaceable = {"true" if rec["replaceable"] else "false"},')
        if rec.get("authoring"):
            lines.append("            authoring = true,")
        lines.append("        },")
    lines.append("    },")
    lines.append("}")
    lines.append("")  # trailing newline

    lua_text = "\n".join(lines)
    return manifest_data, lua_text


def determine_manifest_target_path(package_root: Path) -> Path:
    scripts_dir = package_root / "scripts"
    if not scripts_dir.exists():
        scripts_dir = package_root / "Scripts"
    if (scripts_dir / "bootstrap").is_dir():
        return scripts_dir / "bootstrap" / "manifest.lua"
    return scripts_dir / "manifest.lua"


def main():
    parser = argparse.ArgumentParser(description="GV2 Module Discovery and Manifest Generator")
    parser.add_argument("package_root", help="Path to package root directory")
    parser.add_argument("--check", action="store_true", help="Verify that existing manifest is up to date")
    parser.add_argument("--output", help="Optional explicit output path for manifest.lua")
    parser.add_argument("--format", choices=["text", "json"], default="text", help="Output format")

    args = parser.parse_args()
    package_root = Path(args.package_root).resolve()

    try:
        manifest_data, lua_text = build_package_manifest(package_root)
        target_path = Path(args.output).resolve() if args.output else determine_manifest_target_path(package_root)

        if args.check:
            if not target_path.exists():
                err_msg = f"ManifestMissing: manifest file not found at {target_path}"
                if args.format == "json":
                    print(json.dumps({"status": "error", "code": "ManifestMissing", "message": err_msg, "target": str(target_path)}))
                else:
                    sys.stderr.write(f"gv2-manifest check failed: {err_msg}\n")
                sys.exit(1)

            existing_text = target_path.read_text(encoding="utf-8")
            if existing_text != lua_text:
                err_msg = f"ManifestMismatch: manifest at {target_path} is out of date with discovered scripts"
                if args.format == "json":
                    print(json.dumps({
                        "status": "error",
                        "code": "ManifestMismatch",
                        "message": err_msg,
                        "target": str(target_path),
                        "modules_count": len(manifest_data["modules"]),
                    }))
                else:
                    sys.stderr.write(f"gv2-manifest check failed: {err_msg}\n")
                sys.exit(1)

            if args.format == "json":
                print(json.dumps({
                    "status": "ok",
                    "code": "ManifestUpToDate",
                    "message": "Manifest is up to date",
                    "target": str(target_path),
                    "modules_count": len(manifest_data["modules"]),
                }))
            else:
                print(f"Manifest at {target_path} is up to date ({len(manifest_data['modules'])} modules).")
            sys.exit(0)

        else:
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(lua_text, encoding="utf-8")

            if args.format == "json":
                print(json.dumps({
                    "status": "ok",
                    "code": "ManifestGenerated",
                    "message": "Manifest generated successfully",
                    "target": str(target_path),
                    "modules_count": len(manifest_data["modules"]),
                }))
            else:
                print(f"Generated manifest at {target_path} ({len(manifest_data['modules'])} modules).")
            sys.exit(0)

    except Exception as ex:
        err_code = type(ex).__name__
        if "Disallowed" in str(ex) or "ExplicitId" in str(ex) or "Detected" in str(ex) or "Duplicate" in str(ex):
            # Extract typed error code if present
            m = re.match(r'([a-zA-Z0-9_]+):', str(ex))
            if m:
                err_code = m.group(1)

        if args.format == "json":
            print(json.dumps({"status": "error", "code": err_code, "message": str(ex)}))
        else:
            sys.stderr.write(f"gv2-manifest error: {str(ex)}\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
