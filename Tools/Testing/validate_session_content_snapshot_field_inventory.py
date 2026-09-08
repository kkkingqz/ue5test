#!/usr/bin/env python3
"""Checks that every FGV2SessionContentSnapshot member has an explicit role classification.

The expected side is the independent table below. The actual side is derived from the
struct's own C++ declaration, so adding or deleting a member cannot silently make this
pass by updating the same source the check reads from.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SNAPSHOT_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionContentSnapshot.h"
)
STRUCT_SIGNATURE = "class FGV2SessionContentSnapshot"

# PSC-04 (ADR-0043 D1): the independent classification table -- what role each snapshot
# member plays in the fixed composition Snapshot.md's "Состав snapshot" names, or why a
# member exists that isn't in that list (an identity hash the same section also names).
CLASSIFIED_MEMBERS = {
    "Repository": "content: FRepositoryReadHandle",
    "OrderedPackageIds": "content: ordered package identities",
    "LuaSources": "content: loaded Lua source set",
    "ScriptSetHash": "identity: script_set_hash",
    "SchemaCache": "content: eagerly compiled UI schema set",
    "ScreenRegistry": "presentation: FGV2ResolvedScreenRegistry",
    "ImageCatalog": "presentation: FGV2ResolvedImageCatalog",
    "Theme": "presentation: FGV2ResolvedUiTheme",
    "GameShellClass": "presentation: resolved GameShell class",
    "RepositoryContentHash": "identity: repository_content_hash",
    "PackageSetFingerprint": "identity: package_set_fingerprint",
    "PresentationHash": "identity: presentation_hash",
    "SessionContentId": "identity: session_content_id (combines the other three identities)",
}


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_struct_body(source: str) -> str | None:
    start = source.find(STRUCT_SIGNATURE)
    if start < 0:
        return None

    open_brace = source.find("{", start)
    if open_brace < 0:
        return None

    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace : index + 1]
    return None


def extract_private_section(struct_body: str) -> str:
    """Member data only lives after the `private:` label in this struct."""
    marker = struct_body.find("private:")
    if marker < 0:
        return ""
    return struct_body[marker:]


# A field declaration: some type text, then an identifier, then ';' -- simple enough
# (no function pointers/arrays in this struct) that a straight line-based scan is more
# robust here than a brace/anchor-based regex.
FIELD_LINE_PATTERN = re.compile(r"^\s*(?P<type>[\w:<>,\*&]+(?:\s+[\w:<>,\*&]+)*)\s+(?P<name>[A-Za-z_]\w*)\s*;\s*$")


def extract_member_names(section: str) -> list[str]:
    names = []
    for line in section.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("friend "):
            continue
        match = FIELD_LINE_PATTERN.match(line)
        if match is None:
            continue
        names.append(match.group("name"))
    return names


def find_violations(header_source: str) -> list[str]:
    stripped = strip_comments(header_source)
    struct_body = extract_struct_body(stripped)
    if struct_body is None:
        return [f"could not find '{STRUCT_SIGNATURE}' in the given source"]

    private_section = extract_private_section(struct_body)
    members = extract_member_names(private_section)
    if not members:
        return ["could not find any private data member in FGV2SessionContentSnapshot -- extraction is broken"]

    violations = []
    seen = set()
    for member in members:
        seen.add(member)
        if member not in CLASSIFIED_MEMBERS:
            violations.append(
                f"FGV2SessionContentSnapshot::{member} has no entry in CLASSIFIED_MEMBERS -- "
                "add its role classification to validate_session_content_snapshot_field_inventory.py"
            )

    for classified_name in CLASSIFIED_MEMBERS:
        if classified_name not in seen:
            violations.append(
                f"CLASSIFIED_MEMBERS names '{classified_name}', which is no longer a member of "
                "FGV2SessionContentSnapshot -- remove the stale classification entry"
            )

    return violations


def validate_repository() -> list[str]:
    return find_violations(SNAPSHOT_HEADER_PATH.read_text(encoding="utf-8"))


def run_self_test() -> bool:
    print("[*] Running validate_session_content_snapshot_field_inventory self-test...")
    if errors := validate_repository():
        print("FAILED: current production header violates the gate:\n" + "\n".join(errors))
        return False

    synthetic_source = (
        "class FGV2SessionContentSnapshot\n"
        "{\n"
        "public:\n"
        "    int GetX() const { return X; }\n"
        "private:\n"
        "    friend class FGV2SessionContentCandidate;\n"
        "    int X;\n"
        "    FString SyntheticUnclassifiedField;\n"
        "};\n"
    )
    errors = find_violations(synthetic_source)
    if not any("SyntheticUnclassifiedField" in error for error in errors):
        print(f"FAILED: gate accepted an unclassified synthetic member: {errors}")
        return False

    print("SUCCESS: every FGV2SessionContentSnapshot member has an explicit role classification")
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    if errors := validate_repository():
        print("FAILED:\n" + "\n".join(errors))
        return 1

    print("SUCCESS: every FGV2SessionContentSnapshot member has an explicit role classification")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
