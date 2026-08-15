#!/usr/bin/env python3
"""
generate_vscode_snippets.py

Generates VS Code code-snippets for Stable ID autocompletion and schema scaffolding
from 'gv2-content index <package-root> --format=json'.
"""

import argparse
import json
import os
import subprocess
import sys


def extract_snippets_from_index(index_doc: dict) -> dict:
    snippets = {}
    kinds = index_doc.get("kinds", {})
    for kind, ids in kinds.items():
        for stable_id in ids:
            # Full Stable ID snippet
            snippets[f"ID: {stable_id}"] = {
                "prefix": [stable_id, stable_id.split(":", 1)[-1]],
                "body": [f'"{stable_id}"'],
                "description": f"[{kind}] Active Stable ID: {stable_id}",
                "scope": "json,jsonc"
            }

    # Redirects
    for redirect in index_doc.get("redirects", []):
        src = redirect.get("source_id", "")
        dst = redirect.get("target_id", "")
        if src and dst:
            snippets[f"Redirect: {src}"] = {
                "prefix": [src, src.split(":", 1)[-1]],
                "body": [f'"{dst}"'],
                "description": f"[redirect] Redirected ID {src} -> {dst}",
                "scope": "json,jsonc"
            }

    return snippets


def main():
    parser = argparse.ArgumentParser(description="Generate VS Code snippets from gv2-content index")
    parser.add_argument("gv2_content", help="Path to gv2-content executable")
    parser.add_argument("package_root", help="Path to package root (e.g. GameData/core)")
    parser.add_argument("--output", "-o", default=".vscode/gv2-content.code-snippets", help="Target output file")
    args = parser.parse_args()

    cmd = [args.gv2_content, "index", args.package_root, "--format=json"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running gv2-content index: {e.stderr}", file=sys.stderr)
        sys.exit(e.returncode)

    try:
        index_doc = json.loads(proc.stdout)
    except Exception as e:
        print(f"Failed to parse gv2-content JSON output: {e}\nOutput was:\n{proc.stdout}", file=sys.stderr)
        sys.exit(1)

    snippets = extract_snippets_from_index(index_doc)

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(snippets, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"Successfully generated {len(snippets)} snippets to {args.output}")


if __name__ == "__main__":
    main()
