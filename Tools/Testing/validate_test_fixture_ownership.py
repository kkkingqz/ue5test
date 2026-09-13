#!/usr/bin/env python3
"""Checks that automation test fixtures maintain lifetime isolation and scoped ownership.

CFC-02A (ADR-0040, CFC-AF-02, CFC-AF-10):
Fixtures must never leave rooted GameInstance or UWorld objects, or un-restored global
test settings (such as EGV2ForgeryMode) behind for subsequent tests.
1. AddToRoot and RemoveFromRoot in test sources are strictly restricted to approved scoped
   RAII owners (FScopedTestWorldContext, TScopedRootObject). File-level allowlists are prohibited;
   enclosing class and function scope are strictly verified.
2. Direct mutation of global test settings (e.g., ModeForNextInstance() = ...) is forbidden;
   mutations must use RAII scoped helpers (FScopedForgeryMode). Direct calls to SetModeForNextInstance
   are restricted to FScopedForgeryMode or the widget method definition.
"""

from __future__ import annotations

import bisect
import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source"

SCANNED_SUFFIXES = (".h", ".cpp")

ALLOWED_ROOT_CLASSES = {"FScopedTestWorldContext", "TScopedRootObject"}
ALLOWED_ROOT_FUNCS_PREFIX = ("FScopedTestWorldContext::", "TScopedRootObject::")

ALLOWED_SET_MODE_CLASSES = {"FScopedForgeryMode"}
ALLOWED_SET_MODE_FUNCS = {
    "FScopedForgeryMode::FScopedForgeryMode",
    "FScopedForgeryMode::~FScopedForgeryMode",
    "UGV2ForgeryEntryTestWidget::SetModeForNextInstance",
}


def strip_comments(source: str) -> str:
    """Preserve layout and line count while stripping comments."""
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if c == "\n" else " " for c in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, source)


def find_test_source_files(source_root: Path) -> list[Path]:
    """Find all C++ source and header files located within Tests directories."""
    test_files: list[Path] = []
    if not source_root.exists():
        return test_files

    for path in sorted(source_root.rglob("*")):
        if not path.is_file() or path.suffix not in SCANNED_SUFFIXES:
            continue
        parts = path.relative_to(source_root).parts
        if "Tests" in parts:
            test_files.append(path)
    return test_files


def find_violations_in_file(path: Path) -> list[str]:
    raw_text = path.read_text(encoding="utf-8")
    clean_text = strip_comments(raw_text)

    token_pattern = re.compile(
        r'(?P<STRING>"(?:[^"\\]|\\.)*")|'
        r"(?P<CHAR>'(?:[^'\\]|\\.)*')|"
        r"(?P<IDENT>(?:[A-Za-z_][A-Za-z0-9_]*::)*~?[A-Za-z_][A-Za-z0-9_]*)|"
        r"(?P<PUNCT>[{}();,=])|"
        r"(?P<OTHER>[^\sA-Za-z0-9_{}();,\"':]+)"
    )

    lines = raw_text.splitlines(keepends=True)
    line_offsets = [0]
    for line in lines:
        line_offsets.append(line_offsets[-1] + len(line))

    def get_line(pos: int) -> int:
        return bisect.bisect_right(line_offsets, pos)

    tokens = []
    for match in token_pattern.finditer(clean_text):
        kind = match.lastgroup
        val = match.group()
        pos = match.start()
        line_no = get_line(pos)
        tokens.append((kind, val, line_no, pos))

    scope_stack: list[dict[str, str]] = []
    stmt_tokens: list[tuple[str, str, int, int]] = []
    violations: list[str] = []

    idx = 0
    total = len(tokens)

    while idx < total:
        kind, val, line_no, pos = tokens[idx]

        # Check AddToRoot and RemoveFromRoot
        if kind == "IDENT" and val in ("AddToRoot", "RemoveFromRoot"):
            if stmt_tokens and stmt_tokens[-1][1] == "->":
                call_name = val
                enclosing_classes = [s["name"] for s in scope_stack if s["type"] == "class"]
                enclosing_funcs = [s["name"] for s in scope_stack if s["type"] == "func"]

                is_permitted = any(c in ALLOWED_ROOT_CLASSES for c in enclosing_classes) or any(
                    any(f.startswith(p) for p in ALLOWED_ROOT_FUNCS_PREFIX) for f in enclosing_funcs
                )

                if not is_permitted:
                    scope_desc = " > ".join(f"{s['type']}:{s['name']}" for s in scope_stack) or "global"
                    violations.append(
                        f"{path}:{line_no}: raw '{call_name}()' in test fixture is prohibited outside RAII owners "
                        f"(FScopedTestWorldContext, TScopedRootObject). Scope: [{scope_desc}]"
                    )

        # Check SetModeForNextInstance calls
        if kind == "IDENT" and val.endswith("SetModeForNextInstance"):
            if idx + 1 < total and tokens[idx + 1][1] == "(":
                # Check if it is a declaration/definition rather than a call:
                is_definition = bool(stmt_tokens and stmt_tokens[-1][1] == "void")

                if not is_definition:
                    enclosing_classes = [s["name"] for s in scope_stack if s["type"] == "class"]
                    enclosing_funcs = [s["name"] for s in scope_stack if s["type"] == "func"]

                    is_permitted = any(c in ALLOWED_SET_MODE_CLASSES for c in enclosing_classes) or any(
                        f in ALLOWED_SET_MODE_FUNCS for f in enclosing_funcs
                    )

                    if not is_permitted:
                        scope_desc = " > ".join(f"{s['type']}:{s['name']}" for s in scope_stack) or "global"
                        violations.append(
                            f"{path}:{line_no}: direct call to '{val}()' is prohibited outside "
                            f"FScopedForgeryMode or UGV2ForgeryEntryTestWidget definition. Scope: [{scope_desc}]"
                        )

        # Check direct assignment to ModeForNextInstance
        if kind == "IDENT" and val.endswith("ModeForNextInstance"):
            look = idx + 1
            if look < total and tokens[look][1] == "(":
                paren_depth = 1
                look += 1
                while look < total and paren_depth > 0:
                    if tokens[look][1] == "(":
                        paren_depth += 1
                    elif tokens[look][1] == ")":
                        paren_depth -= 1
                    look += 1
                if look < total and tokens[look][1] == "=":
                    violations.append(
                        f"{path}:{line_no}: direct assignment to ModeForNextInstance is prohibited. "
                        "Use FScopedForgeryMode to guarantee RAII restoration of test settings."
                    )

        # Structure / Braces
        if val == "{":
            scope_type = "block"
            scope_name = "block"

            stmt_idents = [t[1] for t in stmt_tokens if t[0] == "IDENT"]
            if "namespace" in stmt_idents:
                scope_type = "namespace"
                idx_ns = stmt_idents.index("namespace")
                scope_name = stmt_idents[idx_ns + 1] if idx_ns + 1 < len(stmt_idents) else "anonymous"
            elif "class" in stmt_idents or "struct" in stmt_idents:
                kw = "class" if "class" in stmt_idents else "struct"
                idx_kw = stmt_idents.index(kw)
                for cand in stmt_idents[idx_kw + 1:]:
                    if cand in ("final",) or cand.endswith("_API"):
                        continue
                    scope_type = "class"
                    scope_name = cand
                    break
            else:
                first_paren_idx = -1
                for i, t in enumerate(stmt_tokens):
                    if t[1] == "(":
                        first_paren_idx = i
                        break
                if first_paren_idx > 0:
                    prev = stmt_tokens[first_paren_idx - 1]
                    if prev[0] == "IDENT":
                        if prev[1] in ("if", "for", "while", "switch", "catch"):
                            scope_type = "control"
                            scope_name = prev[1]
                        else:
                            scope_type = "func"
                            scope_name = prev[1]

            scope_stack.append({"type": scope_type, "name": scope_name, "line": line_no})
            stmt_tokens = []
        elif val == "}":
            if scope_stack:
                scope_stack.pop()
            stmt_tokens = []
        elif val == ";":
            stmt_tokens = []
        else:
            stmt_tokens.append((kind, val, line_no, pos))

        idx += 1

    return violations


def validate_repository(source_root: Path | None = None) -> list[str]:
    root = source_root or SOURCE_ROOT
    violations: list[str] = []
    test_files = find_test_source_files(root)
    for test_file in test_files:
        violations.extend(find_violations_in_file(test_file))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_test_fixture_ownership self-test...")

    # 1. Repository check
    actual = validate_repository()
    if actual:
        print(
            "FAILED: current repository violates test fixture ownership contract:\n"
            + "\n".join(actual)
        )
        return False

    # 2. Synthetic negative & positive test fixtures targeting previously allowed files
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_root = Path(tmpdir)
        test_dir = tmp_root / "Source" / "GV2" / "Private" / "Tests"
        test_dir.mkdir(parents=True)

        # Negative Mutation 1: GV2PresentationTestFixtures.h with unapproved raw AddToRoot in a free helper
        bad_fixtures_h = test_dir / "GV2PresentationTestFixtures.h"
        bad_fixtures_h.write_text(
            "namespace GV2PresentationTestFixtures {\n"
            "    class FScopedTestWorldContext {\n"
            "        void Init() { GameInstance->AddToRoot(); }\n"
            "    };\n"
            "    void UnapprovedHelper() {\n"
            "        UObject* RawObj = nullptr;\n"
            "        RawObj->AddToRoot();\n"
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        v1 = validate_repository(tmp_root / "Source")
        if not any("GV2PresentationTestFixtures.h" in v and "raw 'AddToRoot()' in test fixture is prohibited" in v and "UnapprovedHelper" in v for v in v1):
            print(f"FAILED: gate did not flag raw AddToRoot in GV2PresentationTestFixtures.h outside approved classes: {v1}")
            return False
        bad_fixtures_h.unlink()

        # Negative Mutation 2: GV2RuntimeSubsystemTests.cpp with raw AddToRoot and RemoveFromRoot in a test
        bad_runtime_tests = test_dir / "GV2RuntimeSubsystemTests.cpp"
        bad_runtime_tests.write_text(
            "bool FGV2LeakyTest::RunTest(const FString& Parameters) {\n"
            "    UGameInstance* GI = NewObject<UGameInstance>();\n"
            "    GI->AddToRoot();\n"
            "    GI->RemoveFromRoot();\n"
            "    return true;\n"
            "}\n",
            encoding="utf-8",
        )
        v2 = validate_repository(tmp_root / "Source")
        has_add = any("GV2RuntimeSubsystemTests.cpp" in v and "raw 'AddToRoot()'" in v for v in v2)
        has_remove = any("GV2RuntimeSubsystemTests.cpp" in v and "raw 'RemoveFromRoot()'" in v for v in v2)
        if not (has_add and has_remove):
            print(f"FAILED: gate did not flag raw AddToRoot/RemoveFromRoot in GV2RuntimeSubsystemTests.cpp: {v2}")
            return False
        bad_runtime_tests.unlink()

        # Negative Mutation 3: GV2ForgeryTestWidgets.cpp with call to SetModeForNextInstance outside FScopedForgeryMode
        bad_forgery_cpp = test_dir / "GV2ForgeryTestWidgets.cpp"
        bad_forgery_cpp.write_text(
            "void RogueHelper() {\n"
            "    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(EGV2ForgeryMode::NoOpConsumer);\n"
            "}\n",
            encoding="utf-8",
        )
        v3 = validate_repository(tmp_root / "Source")
        if not any("GV2ForgeryTestWidgets.cpp" in v and "direct call to 'UGV2ForgeryEntryTestWidget::SetModeForNextInstance()' is prohibited" in v for v in v3):
            print(f"FAILED: gate did not flag unapproved SetModeForNextInstance in GV2ForgeryTestWidgets.cpp: {v3}")
            return False
        bad_forgery_cpp.unlink()

        # Negative Mutation 4: GV2ForgeryTestWidgets.h with direct assignment to ModeForNextInstance
        bad_forgery_h = test_dir / "GV2ForgeryTestWidgets.h"
        bad_forgery_h.write_text(
            "inline void RogueInline() {\n"
            "    UGV2ForgeryEntryTestWidget::ModeForNextInstance() = EGV2ForgeryMode::DetachedRenderer;\n"
            "}\n",
            encoding="utf-8",
        )
        v4 = validate_repository(tmp_root / "Source")
        if not any("GV2ForgeryTestWidgets.h" in v and "direct assignment to ModeForNextInstance is prohibited" in v for v in v4):
            print(f"FAILED: gate did not flag direct assignment to ModeForNextInstance in GV2ForgeryTestWidgets.h: {v4}")
            return False
        bad_forgery_h.unlink()

        # Positive Fixtures: approved RAII owners in the exact previously allowed files
        good_fixtures_h = test_dir / "GV2PresentationTestFixtures.h"
        good_fixtures_h.write_text(
            "namespace GV2PresentationTestFixtures {\n"
            "    class FScopedTestWorldContext final {\n"
            "    public:\n"
            "        FScopedTestWorldContext() { GameInstance->AddToRoot(); }\n"
            "        ~FScopedTestWorldContext() { GameInstance->RemoveFromRoot(); }\n"
            "    private:\n"
            "        UGameInstance* GameInstance = nullptr;\n"
            "    };\n"
            "    template <typename T = UObject>\n"
            "    class TScopedRootObject final {\n"
            "    public:\n"
            "        explicit TScopedRootObject(T* InObj) : Object(InObj) { if (Object) Object->AddToRoot(); }\n"
            "        ~TScopedRootObject() { if (Object) Object->RemoveFromRoot(); }\n"
            "    private:\n"
            "        T* Object = nullptr;\n"
            "    };\n"
            "}\n",
            encoding="utf-8",
        )
        good_runtime_cpp = test_dir / "GV2RuntimeSubsystemTests.cpp"
        good_runtime_cpp.write_text(
            "bool FGV2GoodTest::RunTest(const FString& Parameters) {\n"
            "    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;\n"
            "    GV2PresentationTestFixtures::TScopedRootObject<UUserWidget> ScopedWidget(Widget);\n"
            "    return true;\n"
            "}\n",
            encoding="utf-8",
        )
        good_forgery_h = test_dir / "GV2ForgeryTestWidgets.h"
        good_forgery_h.write_text(
            "class UGV2ForgeryEntryTestWidget {\n"
            "private:\n"
            "    friend class FScopedForgeryMode;\n"
            "    static void SetModeForNextInstance(EGV2ForgeryMode InMode);\n"
            "};\n"
            "class FScopedForgeryMode final {\n"
            "public:\n"
            "    explicit FScopedForgeryMode(EGV2ForgeryMode InMode);\n"
            "    ~FScopedForgeryMode();\n"
            "};\n",
            encoding="utf-8",
        )
        good_forgery_cpp = test_dir / "GV2ForgeryTestWidgets.cpp"
        good_forgery_cpp.write_text(
            "FScopedForgeryMode::FScopedForgeryMode(EGV2ForgeryMode InMode) {\n"
            "    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(InMode);\n"
            "}\n"
            "FScopedForgeryMode::~FScopedForgeryMode() {\n"
            "    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(PreviousMode);\n"
            "}\n"
            "void UGV2ForgeryEntryTestWidget::SetModeForNextInstance(EGV2ForgeryMode NewMode) {\n"
            "    GForgeryModeForNextInstance = NewMode;\n"
            "}\n",
            encoding="utf-8",
        )

        v_good = validate_repository(tmp_root / "Source")
        if v_good:
            print(f"FAILED: gate falsely flagged valid scoped fixtures: {v_good}")
            return False

        good_fixtures_h.unlink()
        good_runtime_cpp.unlink()
        good_forgery_h.unlink()
        good_forgery_cpp.unlink()

    print("SUCCESS: test fixture ownership validator correctly flags violations and accepts scoped owners.")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_test_fixture_ownership.py [--self-test]", file=sys.stderr)
        return 2

    violations = validate_repository()
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("SUCCESS: all test fixtures adhere to scoped ownership contract.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
