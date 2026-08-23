#!/usr/bin/env python3
"""Validates the structural invariants of Tools/Content/Source/main.cpp.

Invariants:
- main.cpp is strictly a CLI argument router and top-level dispatcher.
- main.cpp does not contain command implementations, repository building,
  JSON5 parsing/document manipulation, or direct filesystem mutation.
- All command business logic lives in Tools/Content/Source/Commands/.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path
from typing import List, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
MAIN_CPP_PATH = REPO_ROOT / "Tools" / "Content" / "Source" / "main.cpp"

FORBIDDEN_INCLUDES = [
    (re.compile(r'#include\s+["<]GV2ContentCore/(?!Testing/)[^">]+[">]'), "direct core header (use Commands/ or Support/ instead)"),
    (re.compile(r'#include\s+["<]GV2ContentAuthoring/(?!Testing/)[^">]+[">]'), "direct authoring header (use Commands/ or Support/ instead)"),
    (re.compile(r'#include\s+["<]GV2ContentHostSupport/[^">]+[">]'), "direct host support header (use Commands/ or Support/ instead)"),
    (re.compile(r'#include\s+["<]fstream[">]'), "direct file stream include (file I/O belongs in Commands/ or Support/)"),
]

FORBIDDEN_PATTERNS = [
    (re.compile(r'\b(BuildRepository|BuildFromPackageRoots|BuildFromPackageRoot)\b'), "direct repository building call"),
    (re.compile(r'\b(ParseJson5Document|ParseDefinitionFileEnvelope|ParseSchemaResource)\b'), "direct JSON5 / envelope parsing call"),
    (re.compile(r'\b(CreateDefinition|SetField|BatchSetFields|DeleteDefinition|RenameDefinition|DuplicateDefinition)\b'), "direct AuthoringService mutation call"),
    (re.compile(r'\bstd::(ofstream|basic_ofstream)\b'), "direct file write stream"),
    (re.compile(r'\bstd::filesystem::(create_directories|remove|rename|copy)\b'), "direct filesystem mutation call"),
]


def validate_main_cpp_content(content: str, filename: str = "main.cpp") -> List[str]:
    """Validates structural rules on the content of main.cpp."""
    violations: List[str] = []
    lines = content.splitlines()

    for line_idx, line in enumerate(lines, 1):
        # Ignore comments
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue

        for pattern, reason in FORBIDDEN_INCLUDES:
            if pattern.search(line):
                violations.append(f"{filename}:{line_idx}: forbidden include [{reason}]: {stripped}")

        for pattern, reason in FORBIDDEN_PATTERNS:
            if pattern.search(line):
                violations.append(f"{filename}:{line_idx}: forbidden logic [{reason}]: {stripped}")

    return violations


def run_self_test() -> bool:
    print("[*] Running validate_main_router self-test...")

    # 1. Positive test against actual main.cpp
    if not MAIN_CPP_PATH.is_file():
        print(f"FAILED: main.cpp not found at {MAIN_CPP_PATH}", file=sys.stderr)
        return False

    actual_content = MAIN_CPP_PATH.read_text(encoding="utf-8")
    actual_violations = validate_main_cpp_content(actual_content, str(MAIN_CPP_PATH))
    if actual_violations:
        print(f"FAILED: actual main.cpp has structural violations:\n" + "\n".join(actual_violations), file=sys.stderr)
        return False

    # 2. Negative test 1: Forbidden include (e.g. Json5Parser.h)
    mock_bad_include = """#include "Commands/ValidateCommand.h"
#include "GV2ContentCore/Json5Parser.h"
int main() { return 0; }
"""
    v1 = validate_main_cpp_content(mock_bad_include, "mock_bad_include.cpp")
    if not any("forbidden include" in e for e in v1):
        print(f"FAILED: expected forbidden include violation, got {v1}", file=sys.stderr)
        return False

    # 3. Negative test 2: Forbidden direct repository building
    mock_bad_build = """#include "Commands/ValidateCommand.h"
int main() {
    auto res = BuildRepository(descriptors, options);
    return 0;
}
"""
    v2 = validate_main_cpp_content(mock_bad_build, "mock_bad_build.cpp")
    if not any("direct repository building call" in e for e in v2):
        print(f"FAILED: expected repository building violation, got {v2}", file=sys.stderr)
        return False

    # 4. Negative test 3: Forbidden direct file write
    mock_bad_io = """#include "Commands/ValidateCommand.h"
int main() {
    std::ofstream out("file.txt");
    return 0;
}
"""
    v3 = validate_main_cpp_content(mock_bad_io, "mock_bad_io.cpp")
    if not any("direct file write" in e for e in v3):
        print(f"FAILED: expected direct file write violation, got {v3}", file=sys.stderr)
        return False

    # 5. Negative test 4: Forbidden direct AuthoringService call
    mock_bad_authoring = """#include "Commands/ValidateCommand.h"
int main() {
    FAuthoringService::CreateDefinition(params);
    return 0;
}
"""
    v4 = validate_main_cpp_content(mock_bad_authoring, "mock_bad_authoring.cpp")
    if not any("direct AuthoringService mutation call" in e for e in v4):
        print(f"FAILED: expected authoring mutation violation, got {v4}", file=sys.stderr)
        return False

    print("[+] All validate_main_router self-tests passed successfully.")
    return True


def main() -> int:
    if "--self-test" in sys.argv:
        return 0 if run_self_test() else 1

    target_path = MAIN_CPP_PATH
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        target_path = Path(sys.argv[1])

    if not target_path.is_file():
        print(f"Error: target file not found: {target_path}", file=sys.stderr)
        return 1

    content = target_path.read_text(encoding="utf-8")
    violations = validate_main_cpp_content(content, str(target_path))

    if violations:
        print(f"Structural validation failed for {target_path} ({len(violations)} violation(s)):", file=sys.stderr)
        for v in violations:
            print(f"  ERROR: {v}", file=sys.stderr)
        return 1

    print(f"Structural validation passed: {target_path} is a clean CLI router.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
