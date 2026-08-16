#!/usr/bin/env python3
"""Integration tests for gv2-headless --check-scripts (CAT-04, CAT-05)."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run_headless(headless_bin: str, args: list[str], cwd: str) -> subprocess.CompletedProcess[str]:
    abs_bin = str(Path(headless_bin).resolve())
    return subprocess.run(
        [abs_bin] + args,
        cwd=cwd,
        capture_output=True,
        text=True,
    )


def test_positive_check_scripts(headless_bin: str, repo_root: Path):
    print("Testing positive --check-scripts on valid repository...")
    proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(repo_root))
    if proc.returncode != 0:
        print(f"FAILED: expected returncode 0, got {proc.returncode}")
        print("stdout:", proc.stdout)
        print("stderr:", proc.stderr)
        sys.exit(1)
    if '"status":"ok"' not in proc.stdout or '"modules_checked":' not in proc.stdout:
        print(f"FAILED: unexpected stdout: {proc.stdout}")
        sys.exit(1)
    print("  PASS: positive --check-scripts")


def test_negative_missing_source(headless_bin: str, repo_root: Path):
    print("Testing negative: missing source (LuaModuleSourceMissing)...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        scripts_tmp = tmp / "Scripts"
        shutil.copytree(repo_root / "Scripts", scripts_tmp)
        gamedata_tmp = tmp / "GameData"
        shutil.copytree(repo_root / "GameData", gamedata_tmp)

        # Remove an existing module source
        os.remove(scripts_tmp / "runtime" / "world.lua")

        proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(tmp))
        if proc.returncode != 1:
            print(f"FAILED: expected returncode 1, got {proc.returncode}")
            sys.exit(1)
        if "LuaModuleSourceMissing" not in proc.stderr:
            print(f"FAILED: expected 'LuaModuleSourceMissing' in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "core:module.runtime.world" not in proc.stderr:
            print(f"FAILED: expected module ID in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "@core/runtime/world.lua" not in proc.stderr:
            print(f"FAILED: expected relative path in stderr, got:\n{proc.stderr}")
            sys.exit(1)
    print("  PASS: LuaModuleSourceMissing")


def test_negative_unlisted_source(headless_bin: str, repo_root: Path):
    print("Testing negative: unlisted source (LuaModuleSourceUnlisted)...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        scripts_tmp = tmp / "Scripts"
        shutil.copytree(repo_root / "Scripts", scripts_tmp)
        gamedata_tmp = tmp / "GameData"
        shutil.copytree(repo_root / "GameData", gamedata_tmp)

        # Create an unlisted lua file
        unlisted = scripts_tmp / "runtime" / "unlisted.lua"
        unlisted.write_text("return {}\n", encoding="utf-8")

        proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(tmp))
        if proc.returncode != 1:
            print(f"FAILED: expected returncode 1, got {proc.returncode}")
            sys.exit(1)
        if "LuaModuleSourceUnlisted" not in proc.stderr:
            print(f"FAILED: expected 'LuaModuleSourceUnlisted' in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "@core/runtime/unlisted.lua" not in proc.stderr:
            print(f"FAILED: expected relative source path in stderr, got:\n{proc.stderr}")
            sys.exit(1)
    print("  PASS: LuaModuleSourceUnlisted")


def test_negative_missing_dependency(headless_bin: str, repo_root: Path):
    print("Testing negative: missing dependency (LuaModuleDependencyMissing)...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        scripts_tmp = tmp / "Scripts"
        shutil.copytree(repo_root / "Scripts", scripts_tmp)
        gamedata_tmp = tmp / "GameData"
        shutil.copytree(repo_root / "GameData", gamedata_tmp)

        manifest_file = scripts_tmp / "bootstrap" / "manifest.lua"
        content = manifest_file.read_text(encoding="utf-8")
        # Add a missing dependency
        content = content.replace(
            'dependencies = {\n                "core:module.runtime.mutation_window",',
            'dependencies = {\n                "core:module.nonexistent.missing",\n                "core:module.runtime.mutation_window",'
        )
        manifest_file.write_text(content, encoding="utf-8")

        proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(tmp))
        if proc.returncode != 1:
            print(f"FAILED: expected returncode 1, got {proc.returncode}")
            sys.exit(1)
        if "LuaModuleDependencyMissing" not in proc.stderr:
            print(f"FAILED: expected 'LuaModuleDependencyMissing' in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "core:module.nonexistent.missing" not in proc.stderr:
            print(f"FAILED: expected missing dependency ID in stderr, got:\n{proc.stderr}")
            sys.exit(1)
    print("  PASS: LuaModuleDependencyMissing")


def test_negative_dependency_cycle(headless_bin: str, repo_root: Path):
    print("Testing negative: dependency cycle (LuaModuleDependencyCycle)...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        scripts_tmp = tmp / "Scripts"
        shutil.copytree(repo_root / "Scripts", scripts_tmp)
        gamedata_tmp = tmp / "GameData"
        shutil.copytree(repo_root / "GameData", gamedata_tmp)

        manifest_file = scripts_tmp / "bootstrap" / "manifest.lua"
        content = manifest_file.read_text(encoding="utf-8")
        # Make a cycle: state_validator -> mutation_window -> state_validator
        content = content.replace(
            'module_id = "core:module.runtime.mutation_window",\n            source = "runtime/mutation_window.lua",\n            dependencies = {},',
            'module_id = "core:module.runtime.mutation_window",\n            source = "runtime/mutation_window.lua",\n            dependencies = {"core:module.runtime.command_dispatcher"},'
        )
        manifest_file.write_text(content, encoding="utf-8")

        proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(tmp))
        if proc.returncode != 1:
            print(f"FAILED: expected returncode 1, got {proc.returncode}")
            sys.exit(1)
        if "LuaModuleDependencyCycle" not in proc.stderr:
            print(f"FAILED: expected 'LuaModuleDependencyCycle' in stderr, got:\n{proc.stderr}")
            sys.exit(1)
    print("  PASS: LuaModuleDependencyCycle")


def test_negative_syntax_error(headless_bin: str, repo_root: Path):
    print("Testing negative: syntax error (LuaModuleSyntaxError)...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        scripts_tmp = tmp / "Scripts"
        shutil.copytree(repo_root / "Scripts", scripts_tmp)
        gamedata_tmp = tmp / "GameData"
        shutil.copytree(repo_root / "GameData", gamedata_tmp)

        # Inject syntax error
        target = scripts_tmp / "runtime" / "world.lua"
        target.write_text("local foo = function( %%% syntax error %%%\n", encoding="utf-8")

        proc = run_headless(headless_bin, ["--check-scripts"], cwd=str(tmp))
        if proc.returncode != 1:
            print(f"FAILED: expected returncode 1, got {proc.returncode}")
            sys.exit(1)
        if "LuaModuleSyntaxError" not in proc.stderr:
            print(f"FAILED: expected 'LuaModuleSyntaxError' in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "core:module.runtime.world" not in proc.stderr:
            print(f"FAILED: expected module ID in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if "runtime/world.lua" not in proc.stderr:
            print(f"FAILED: expected relative path in stderr, got:\n{proc.stderr}")
            sys.exit(1)
        if ":1:" not in proc.stderr and "line 1" not in proc.stderr:
            print(f"FAILED: expected line number in stderr, got:\n{proc.stderr}")
            sys.exit(1)
    print("  PASS: LuaModuleSyntaxError")


def main():
    if len(sys.argv) < 2:
        print("Usage: test_script_feedback.py <path-to-gv2-headless>")
        sys.exit(1)

    headless_bin = sys.argv[1]
    repo_root = Path(__file__).resolve().parent.parent.parent

    test_positive_check_scripts(headless_bin, repo_root)
    test_negative_missing_source(headless_bin, repo_root)
    test_negative_unlisted_source(headless_bin, repo_root)
    test_negative_missing_dependency(headless_bin, repo_root)
    test_negative_dependency_cycle(headless_bin, repo_root)
    test_negative_syntax_error(headless_bin, repo_root)

    print("\nALL SCRIPT FEEDBACK TESTS PASSED!")


if __name__ == "__main__":
    main()
