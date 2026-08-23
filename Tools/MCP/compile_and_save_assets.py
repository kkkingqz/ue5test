#!/usr/bin/env python3
"""Compile Blueprint/UMG assets and save dirty assets via Unreal MCP.

Usage:
    # Compile and save a specific asset (recommended, ~16s)
    python3 Tools/MCP/compile_and_save_assets.py --path "/Game/UI/Widgets/WBP_Button"

    # Save multiple assets without compiling
    python3 Tools/MCP/compile_and_save_assets.py --save-only --path "/Game/TextSystem/UI/Styles/DA_UITheme_Default"

    # Batch compile and save all Widget Blueprints (takes ~7 minutes due to 16s MCP cycle per asset)
    python3 Tools/MCP/compile_and_save_assets.py --all
"""

from __future__ import annotations

import argparse
import datetime
import os
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from mcp_client import UnrealMcpClient


def log_step(msg: str) -> None:
    now = datetime.datetime.now().strftime("%H:%M:%S")
    print(f"[{now}] {msg}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile and save assets in Unreal Editor via MCP.")
    parser.add_argument(
        "--path",
        "-p",
        action="append",
        help="Specific asset path(s) to compile and save (e.g. /Game/UI/Widgets/WBP_Button). Can be specified multiple times."
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Batch compile and save all Widget Blueprints in /Game/ (Warning: takes ~7 minutes)."
    )
    parser.add_argument(
        "--save-only",
        action="store_true",
        help="Only save assets without running compile_blueprint."
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="Per-request timeout in seconds (default: 60.0)."
    )
    parser.add_argument(
        "--url",
        default=os.environ.get("UNREAL_MCP_URL", "http://127.0.0.1:8000/mcp"),
        help="MCP server endpoint URL."
    )
    args = parser.parse_args()

    if not args.path and not args.all:
        print("ERROR: Please specify --path <asset_path> or --all to compile/save all assets.", file=sys.stderr)
        print("Example: python3 Tools/MCP/compile_and_save_assets.py --path /Game/UI/Widgets/WBP_Button", file=sys.stderr)
        return 1

    try:
        client = UnrealMcpClient(url=args.url, timeout=args.timeout)
    except Exception as e:
        print(f"ERROR: Could not connect to Unreal Editor MCP server: {e}", file=sys.stderr)
        print("Please ensure Unreal Editor is running with ModelContextProtocol enabled on port 8000.", file=sys.stderr)
        return 1

    targets = []
    if args.path:
        targets = args.path
    elif args.all:
        log_step("Finding all Widget Blueprints in /Game/...")
        assets_res = client.call_tool("editor_toolset.toolsets.asset.AssetTools", "find_assets", {
            "folder_path": "/Game/",
            "name": "WBP_",
            "recursive": True
        })
        targets = assets_res.get("returnValue", []) if isinstance(assets_res, dict) else []
        log_step(f"Found {len(targets)} Widget Blueprints. Estimated time: ~{len(targets) * 16 // 60} min.")

    failed_compiles = []
    if not args.save_only:
        log_step(f"Starting compilation of {len(targets)} asset(s)...")
        for i, asset_path in enumerate(targets, 1):
            asset_name = asset_path.split("/")[-1]
            bp_ref = f"{asset_path}.{asset_name}"
            log_step(f"[{i}/{len(targets)}] Compiling {asset_name}...")
            t0 = time.time()
            try:
                comp_res = client.call_tool(
                    "editor_toolset.toolsets.blueprint.BlueprintTools",
                    "compile_blueprint",
                    {"blueprint": {"refPath": bp_ref}}
                )
                elapsed = time.time() - t0
                log_step(f"[{i}/{len(targets)}] [OK] {asset_name} compiled in {elapsed:.2f}s")
            except Exception as err:
                elapsed = time.time() - t0
                log_step(f"[{i}/{len(targets)}] [FAIL] {asset_name} compilation error ({elapsed:.2f}s): {err}")
                failed_compiles.append(asset_path)

    log_step(f"Saving {len(targets)} asset(s)...")
    t0 = time.time()
    try:
        save_res = client.call_tool(
            "editor_toolset.toolsets.asset.AssetTools",
            "save_assets",
            {"asset_paths": targets}
        )
        elapsed = time.time() - t0
        save_ok = save_res.get("returnValue", False) if isinstance(save_res, dict) else False
        if save_ok:
            log_step(f"[OK] Assets saved successfully in {elapsed:.2f}s.")
        else:
            log_step(f"[WARN] Save assets returned: {save_res}")
    except Exception as err:
        log_step(f"[FAIL] Error saving assets: {err}")
        return 1

    return 0 if not failed_compiles else 1


if __name__ == "__main__":
    sys.exit(main())
