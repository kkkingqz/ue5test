#!/usr/bin/env python3
"""Validates that all FRunManifest, FRunAcceptedCommand, and FRunDigest fields have an explicit policy classification,
and that all hash fields are strictly validated with IsCanonicalSha256 in their codecs.

The expected side is the independent policy table below. The actual side is derived from
the struct declarations in GV2RunManifest.h and GV2RunDigest.h, along with their deserializers
in GV2RunManifest.cpp and GV2RunDigest.cpp. Adding, removing, or changing a field without
updating the independent policy table or omitting IsCanonicalSha256 validation will fail this gate.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
MANIFEST_HEADER_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Public" / "GV2RuntimeCore" / "GV2RunManifest.h"
DIGEST_HEADER_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Public" / "GV2RuntimeCore" / "GV2RunDigest.h"
MANIFEST_CPP_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Private" / "GV2RunManifest.cpp"
DIGEST_CPP_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Private" / "GV2RunDigest.cpp"
CODEC_HELPERS_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Private" / "GV2RunCodecHelpers.h"

# Independent expected field policy table:
# (struct_name, member_name, json_field_name): policy_dict
EXPECTED_FIELD_POLICY = {
    ("FRunManifest", "ManifestFormatVersion", "manifest_format_version"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Run manifest format version (integer)",
    },
    ("FRunManifest", "LuaReleaseNumber", "lua_release_num"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Lua release number (integer)",
    },
    ("FRunManifest", "RepositoryContentHash", "repository_content_hash"): {
        "is_hash": True,
        "allow_empty": False,
        "description": "Repository content snapshot SHA-256 hash",
    },
    ("FRunManifest", "ScriptSetHash", "script_set_hash"): {
        "is_hash": True,
        "allow_empty": False,
        "description": "Script set SHA-256 hash",
    },
    ("FRunManifest", "Seed", "seed"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Deterministic simulation seed (uint64)",
    },
    ("FRunManifest", "AcceptedCommands", "accepted_commands"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Array of accepted commands to replay",
    },
    ("FRunAcceptedCommand", "CommandId", "command_id"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Stable ID of accepted command (string)",
    },
    ("FRunAcceptedCommand", "Args", "args"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Arguments object of accepted command",
    },
    ("FRunAcceptedCommand", "Sequence", "sequence"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Sequence index of accepted command (int64)",
    },
    ("FRunDigest", "DigestFormatVersion", "digest_format_version"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Run digest format version (integer)",
    },
    ("FRunDigest", "DigestHash", "digest_hash"): {
        "is_hash": True,
        "allow_empty": False,
        "description": "Deterministic SHA-256 hash of entire run digest",
    },
    ("FRunDigest", "LuaReleaseNumber", "lua_release_num"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Lua release number (integer)",
    },
    ("FRunDigest", "RepositoryContentHash", "repository_content_hash"): {
        "is_hash": True,
        "allow_empty": False,
        "description": "Repository content snapshot SHA-256 hash",
    },
    ("FRunDigest", "ScriptSetHash", "script_set_hash"): {
        "is_hash": True,
        "allow_empty": False,
        "description": "Script set SHA-256 hash",
    },
    ("FRunDigest", "Seed", "seed"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Deterministic simulation seed (uint64)",
    },
    ("FRunDigest", "ExecutedCommandsCount", "executed_commands_count"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Count of executed commands (uint64)",
    },
    ("FRunDigest", "bSuccess", "success"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Whether run completed successfully (boolean)",
    },
    ("FRunDigest", "FinalScreenId", "final_screen_id"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Stable ID of final screen (string)",
    },
    ("FRunDigest", "StateHash", "state_hash"): {
        "is_hash": True,
        "allow_empty": True,
        "description": "Canonical state SHA-256 hash (or empty on early abort)",
    },
    ("FRunDigest", "FaultCode", "fault_code"): {
        "is_hash": False,
        "allow_empty": False,
        "description": "Fault code if run aborted (string)",
    },
}


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_struct_members(header_source: str, struct_name: str) -> list[str]:
    """Extracts non-function data member names from a struct definition."""
    stripped = strip_comments(header_source)
    pattern = rf"struct\s+(?:GV2_PORTABLE_API\s+)?{re.escape(struct_name)}\s*(?:final)?\s*\{{(.*?)\}};"
    match = re.search(pattern, stripped, re.DOTALL)
    if not match:
        return []

    body = match.group(1)
    members = []
    field_pattern = re.compile(
        r"^\s*(?:[A-Za-z_][\w:<>,*& ]+?)\s+(?P<name>[A-Za-z_]\w*)\s*(?:=\s*[^;]+)?;\s*$",
        re.MULTILINE,
    )
    for m in field_pattern.finditer(body):
        name = m.group("name")
        if not name.startswith("operator"):
            members.append(name)
    return members


def validate_codec_helpers(helpers_source: str) -> list[str]:
    """Validates that GV2RunCodecHelpers.h defines required and optional SHA-256 helpers that invoke IsCanonicalSha256."""
    violations = []
    stripped = strip_comments(helpers_source)
    for helper_name in ("ReadRequiredCanonicalSha256Field", "ReadOptionalCanonicalSha256Field"):
        fn_idx = stripped.find(helper_name)
        if fn_idx < 0:
            violations.append(f"GV2RunCodecHelpers.h is missing helper definition: {helper_name}")
            continue
        open_brace = stripped.find("{", fn_idx)
        if open_brace < 0:
            violations.append(f"GV2RunCodecHelpers.h: could not find body for {helper_name}")
            continue
        depth = 0
        end_brace = -1
        for i in range(open_brace, len(stripped)):
            if stripped[i] == "{":
                depth += 1
            elif stripped[i] == "}":
                depth -= 1
                if depth == 0:
                    end_brace = i
                    break
        if end_brace < 0:
            violations.append(f"GV2RunCodecHelpers.h: unbalanced braces for {helper_name}")
            continue
        body = stripped[open_brace : end_brace + 1]
        if "IsCanonicalSha256" not in body:
            violations.append(f"GV2RunCodecHelpers.h: {helper_name} does not invoke IsCanonicalSha256")
    return violations


def extract_queried_fields(cpp_source: str, function_name: str) -> list[str]:
    """Extracts field names queried via FindField(\"...\") or Read(Required|Optional)CanonicalSha256Field(...) in a given function."""
    stripped = strip_comments(cpp_source)
    fn_start = stripped.find(function_name)
    if fn_start < 0:
        return []

    open_brace = stripped.find("{", fn_start)
    if open_brace < 0:
        return []

    depth = 0
    end_brace = -1
    for i in range(open_brace, len(stripped)):
        if stripped[i] == "{":
            depth += 1
        elif stripped[i] == "}":
            depth -= 1
            if depth == 0:
                end_brace = i
                break

    if end_brace < 0:
        return []

    fn_body = stripped[open_brace : end_brace + 1]
    pattern = re.compile(
        r'(?:FindField\s*\(\s*|Read(?:Required|Optional)CanonicalSha256Field\s*\([^,]+,\s*)"([^"]+)"'
    )
    return pattern.findall(fn_body)


extract_find_fields = extract_queried_fields


def check_hash_validation(cpp_source: str, function_name: str, json_field: str, allow_empty: bool) -> bool:
    """Checks that the deserialization function invokes the exact helper for json_field."""
    stripped = strip_comments(cpp_source)
    fn_start = stripped.find(function_name)
    if fn_start < 0:
        return False

    open_brace = stripped.find("{", fn_start)
    if open_brace < 0:
        return False

    depth = 0
    end_brace = -1
    for i in range(open_brace, len(stripped)):
        if stripped[i] == "{":
            depth += 1
        elif stripped[i] == "}":
            depth -= 1
            if depth == 0:
                end_brace = i
                break

    if end_brace < 0:
        return False

    fn_body = stripped[open_brace : end_brace + 1]
    helper_name = "ReadOptionalCanonicalSha256Field" if allow_empty else "ReadRequiredCanonicalSha256Field"
    call_pattern = re.compile(
        rf'\b{helper_name}\s*\(\s*[^,]+,\s*"{re.escape(json_field)}"\s*,'
    )
    return bool(call_pattern.search(fn_body))


def validate_sources(
    manifest_header: str,
    digest_header: str,
    manifest_cpp: str,
    digest_cpp: str,
    codec_helpers: str | None = None,
) -> list[str]:
    violations = []

    if codec_helpers is None:
        codec_helpers = CODEC_HELPERS_PATH.read_text(encoding="utf-8")
    violations.extend(validate_codec_helpers(codec_helpers))

    # 1. Check FRunManifest and FRunAcceptedCommand members
    manifest_members = extract_struct_members(manifest_header, "FRunManifest")
    if not manifest_members:
        violations.append("FRunManifest: could not extract struct members from header")

    cmd_members = extract_struct_members(manifest_header, "FRunAcceptedCommand")
    if not cmd_members:
        violations.append("FRunAcceptedCommand: could not extract struct members from header")

    manifest_json_fields = extract_find_fields(manifest_cpp, "DeserializeRunManifest")
    if not manifest_json_fields:
        violations.append("DeserializeRunManifest: could not extract queried fields from cpp")

    # 2. Check FRunDigest members
    digest_members = extract_struct_members(digest_header, "FRunDigest")
    if not digest_members:
        violations.append("FRunDigest: could not extract struct members from header")

    digest_json_fields = extract_find_fields(digest_cpp, "DeserializeRunDigest")
    if not digest_json_fields:
        violations.append("DeserializeRunDigest: could not extract queried fields from cpp")

    # 3. Match against EXPECTED_FIELD_POLICY
    for (struct_name, member_name, json_field), policy in EXPECTED_FIELD_POLICY.items():
        if struct_name == "FRunManifest":
            actual_members = manifest_members
            actual_cpp = manifest_cpp
            deser_fn = "DeserializeRunManifest"
        elif struct_name == "FRunAcceptedCommand":
            actual_members = cmd_members
            actual_cpp = manifest_cpp
            deser_fn = "DeserializeRunManifest"
        else:
            actual_members = digest_members
            actual_cpp = digest_cpp
            deser_fn = "DeserializeRunDigest"

        if member_name not in actual_members:
            violations.append(
                f"{struct_name}::{member_name} declared in EXPECTED_FIELD_POLICY is missing in struct declaration"
            )

        if policy["is_hash"]:
            if not check_hash_validation(actual_cpp, deser_fn, json_field, policy["allow_empty"]):
                helper_expected = (
                    "ReadOptionalCanonicalSha256Field"
                    if policy["allow_empty"]
                    else "ReadRequiredCanonicalSha256Field"
                )
                violations.append(
                    f"{struct_name} hash field '{json_field}' ({member_name}) does not use {helper_expected} in {deser_fn}"
                )

    # 4. Check for unclassified struct members
    for member in manifest_members:
        matching = [p for p in EXPECTED_FIELD_POLICY if p[0] == "FRunManifest" and p[1] == member]
        if not matching:
            violations.append(
                f"FRunManifest::{member} is not classified in EXPECTED_FIELD_POLICY table in validate_headless_hash_fields.py"
            )

    for member in cmd_members:
        matching = [p for p in EXPECTED_FIELD_POLICY if p[0] == "FRunAcceptedCommand" and p[1] == member]
        if not matching:
            violations.append(
                f"FRunAcceptedCommand::{member} is not classified in EXPECTED_FIELD_POLICY table in validate_headless_hash_fields.py"
            )

    for member in digest_members:
        matching = [p for p in EXPECTED_FIELD_POLICY if p[0] == "FRunDigest" and p[1] == member]
        if not matching:
            violations.append(
                f"FRunDigest::{member} is not classified in EXPECTED_FIELD_POLICY table in validate_headless_hash_fields.py"
            )

    # 5. Check for unclassified queried fields
    for field in manifest_json_fields:
        matching = [p for p in EXPECTED_FIELD_POLICY if p[0] in ("FRunManifest", "FRunAcceptedCommand") and p[2] == field]
        if not matching:
            violations.append(
                f"DeserializeRunManifest queries '{field}', which is not in EXPECTED_FIELD_POLICY"
            )

    for field in digest_json_fields:
        matching = [p for p in EXPECTED_FIELD_POLICY if p[0] == "FRunDigest" and p[2] == field]
        if not matching:
            violations.append(
                f"DeserializeRunDigest queries '{field}', which is not in EXPECTED_FIELD_POLICY"
            )

    return violations


def validate_repository() -> list[str]:
    return validate_sources(
        MANIFEST_HEADER_PATH.read_text(encoding="utf-8"),
        DIGEST_HEADER_PATH.read_text(encoding="utf-8"),
        MANIFEST_CPP_PATH.read_text(encoding="utf-8"),
        DIGEST_CPP_PATH.read_text(encoding="utf-8"),
        CODEC_HELPERS_PATH.read_text(encoding="utf-8"),
    )


def run_self_test() -> bool:
    print("[*] Running validate_headless_hash_fields self-test...")
    repo_errors = validate_repository()
    if repo_errors:
        print("FAILED: current production code violates the gate:\n" + "\n".join(repo_errors))
        return False

    manifest_header = MANIFEST_HEADER_PATH.read_text(encoding="utf-8")
    digest_header = DIGEST_HEADER_PATH.read_text(encoding="utf-8")
    manifest_cpp = MANIFEST_CPP_PATH.read_text(encoding="utf-8")
    digest_cpp = DIGEST_CPP_PATH.read_text(encoding="utf-8")
    codec_helpers = CODEC_HELPERS_PATH.read_text(encoding="utf-8")

    # Mutation test: automatic enumerator over ALL is_hash fields in EXPECTED_FIELD_POLICY
    # Removes validation of each hash field in turn and asserts expected red
    hash_fields = [
        (struct_name, member_name, json_field, policy)
        for (struct_name, member_name, json_field), policy in EXPECTED_FIELD_POLICY.items()
        if policy["is_hash"]
    ]
    if not hash_fields:
        print("FAILED: no hash fields found in EXPECTED_FIELD_POLICY")
        return False

    for struct_name, member_name, json_field, policy in hash_fields:
        is_manifest = struct_name in ("FRunManifest", "FRunAcceptedCommand")
        target_cpp = manifest_cpp if is_manifest else digest_cpp
        helper_func = (
            "ReadOptionalCanonicalSha256Field"
            if policy["allow_empty"]
            else "ReadRequiredCanonicalSha256Field"
        )
        block_pattern = re.compile(
            rf'\s*if\s*\(\s*!Internal::{helper_func}\s*\(\s*[^,]+,\s*"{re.escape(json_field)}"[^;]+;\s*}}\n?',
            re.DOTALL,
        )
        mutated_cpp, count = block_pattern.subn("", target_cpp)
        if count != 1:
            print(f"FAILED: self-test could not locate exact validation block for {struct_name}::{member_name} ({json_field})")
            return False

        m_manifest = mutated_cpp if is_manifest else manifest_cpp
        m_digest = digest_cpp if is_manifest else mutated_cpp

        errors = validate_sources(manifest_header, digest_header, m_manifest, m_digest, codec_helpers)
        expected_msg = f"{struct_name} hash field '{json_field}' ({member_name}) does not use {helper_func}"
        if not any(expected_msg in e for e in errors):
            print(f"FAILED: removing validation for {struct_name}::{member_name} ({json_field}) did not trigger expected red:\n{errors}")
            return False
        print(f"  [+] Verified red on removal of {struct_name}::{member_name} ({json_field})")

    # Negative test: Wrong helper variant (e.g. required field calling ReadOptionalCanonicalSha256Field)
    synth_manifest_cpp = manifest_cpp.replace(
        'ReadRequiredCanonicalSha256Field(\n            Root,\n            "repository_content_hash"',
        'ReadOptionalCanonicalSha256Field(\n            Root,\n            "repository_content_hash"',
    )
    errors = validate_sources(manifest_header, digest_header, synth_manifest_cpp, digest_cpp, codec_helpers)
    if not any("repository_content_hash" in e and "does not use ReadRequiredCanonicalSha256Field" in e for e in errors):
        print(f"FAILED: negative test did not catch wrong helper variant: {errors}")
        return False

    # Negative test: Unclassified member in FRunManifest
    synth_manifest_header = manifest_header.replace(
        "struct GV2_PORTABLE_API FRunManifest final\n{",
        "struct GV2_PORTABLE_API FRunManifest final\n{\n    int UnclassifiedExtraField;\n",
    )
    errors = validate_sources(synth_manifest_header, digest_header, manifest_cpp, digest_cpp, codec_helpers)
    if not any("UnclassifiedExtraField" in e for e in errors):
        print(f"FAILED: negative test did not catch unclassified struct member: {errors}")
        return False

    # Negative test: Unclassified queried field
    synth_manifest_cpp = manifest_cpp.replace(
        'Root.FindField("seed")', 'Root.FindField("unclassified_seed_query")'
    )
    errors = validate_sources(manifest_header, digest_header, synth_manifest_cpp, digest_cpp, codec_helpers)
    if not any("unclassified_seed_query" in e for e in errors):
        print(f"FAILED: negative test did not catch unclassified query: {errors}")
        return False

    # Negative test: Helper in GV2RunCodecHelpers.h missing IsCanonicalSha256
    synth_codec_helpers = codec_helpers.replace(
        "GV2ContentCore::IsCanonicalSha256", "GV2ContentCore::IsDummyCheck"
    )
    errors = validate_sources(manifest_header, digest_header, manifest_cpp, digest_cpp, synth_codec_helpers)
    if not any("does not invoke IsCanonicalSha256" in e for e in errors):
        print(f"FAILED: negative test did not catch missing IsCanonicalSha256 in helper header: {errors}")
        return False

    print("SUCCESS: validate_headless_hash_fields passed all checks and negative self-tests")
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    errors = validate_repository()
    if errors:
        print("FAILED:\n" + "\n".join(errors))
        return 1

    print("SUCCESS: all FRunManifest and FRunDigest fields match expected policy and hash validators")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
