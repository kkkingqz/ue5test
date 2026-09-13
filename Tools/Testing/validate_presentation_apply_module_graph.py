#!/usr/bin/env python3
"""Validates GV2PresentationApply's own Build.cs dependency list against the exact
allowlist ADR-0043 D2/D4 names, and that GV2 declares the one allowed forward edge onto
it.

PSC-09A (ADR-0043 D2/D4, Payload.md M3): the dependency list in
Source/GV2PresentationApply/GV2PresentationApply.Build.cs IS the primary guarantee that
physical Apply cannot reach a content/authority type -- a module that never lists
GV2/GV2ContentCore/GV2ContentHostSupport/GV2RuntimeCore/DeveloperSettings/AssetRegistry/
ImageCore or any filesystem/content-authoring module cannot link code from it, whatever a
future authority type is named. Source-scanning stays a second, weaker rubric (ADR-0043's
own words -- the exact anti-pattern PAH-08's hand-listed authority accessors already
proved insufficient once, see AuditFindings.md). This gate reads the declaration
directly, not a hand-maintained expectation of what it "should" contain beyond the
allowlist itself.
"""

from __future__ import annotations

import argparse
import bisect
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source"
APPLY_BUILD_CS = SOURCE_ROOT / "GV2PresentationApply" / "GV2PresentationApply.Build.cs"
GV2_BUILD_CS = SOURCE_ROOT / "GV2" / "GV2.Build.cs"
APPLY_MODULE = "GV2PresentationApply"

CMAKE_IGNORED_ROOTS = {
    ".git", ".claude", ".idea", ".vscode", "Binaries", "Build", "DerivedDataCache",
    "Intermediate", "Saved", "build", "cmake-build-debug", "cmake-build-release",
}
CMAKE_GRAPH_COMMANDS = {
    "add_executable", "add_library", "add_subdirectory", "target_link_libraries", "target_sources",
}
FORBIDDEN_CMAKE_LINK_ITEMS = {
    "commonui", "coreuobject", "engine", "gv2", "gv2presentationapply", "slate", "slatecore", "ue", "umg", "unreal",
}

# PSC-11: the CMake side of the same claim. The portable/Headless build must neither compile
# nor link this module -- it is a UE-only physical layer, and a portable target that pulled
# it in would make the Headless run depend on UMG.
# ADR-0043 D2: "Разрешённый allowlist зависимостей -- ровно Core, CoreUObject, Engine,
# UMG, CommonUI, Slate, SlateCore".
ALLOWED_MODULES = {
    "Core",
    "CoreUObject",
    "Engine",
    "UMG",
    "CommonUI",
    "Slate",
    "SlateCore",
}

# Explicit denylist named in ADR-0043 D2 and SystemContextAndComponents.md -- reported by
# name when found, even though "not in ALLOWED_MODULES" alone would already catch it, so
# a violation message names the actual rule broken instead of just "not permitted".
NAMED_DENYLIST = {
    "GV2",
    "GV2ContentCore",
    "GV2ContentHostSupport",
    "GV2RuntimeCore",
    "DeveloperSettings",
    "AssetRegistry",
    "ImageCore",
}

DEPENDENCY_LIST_PATTERN = re.compile(
    r'(Public|Private)DependencyModuleNames\s*\.\s*(?:AddRange\s*\(\s*new\s*(?:string\s*\[\s*\]|\[\s*\])\s*\{(?P<range>[^}]*)\}|Add\s*\(\s*(?P<single>"[^"]*")\s*\))',
    re.DOTALL,
)
STRING_LITERAL_PATTERN = re.compile(r'"([^"]*)"')


def extract_dependency_modules(source: str) -> list[str]:
    modules: list[str] = []
    for match in DEPENDENCY_LIST_PATTERN.finditer(source):
        body = match.group("range") if match.group("range") is not None else match.group("single")
        modules.extend(STRING_LITERAL_PATTERN.findall(body))
    return modules


class Token:
    def __init__(self, kind: str, value: str, line: int):
        self.kind = kind
        self.value = value
        self.line = line

    def __repr__(self) -> str:
        return f"Token({self.kind}, {self.value!r}, line={self.line})"


def strip_comments(source: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if c == "\n" else " " for c in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, source)


def tokenize(source: str) -> list[Token]:
    stripped = strip_comments(source)
    tokens: list[Token] = []

    token_spec = [
        ("STRING", r'"(?:[^"\\]|\\.)*"'),
        ("IDENT", r'[A-Za-z_][A-Za-z0-9_]*'),
        ("PUNCT", r'[{}()\[\];,:.=]'),
        ("OTHER", r'[^ \t\r\n]'),
    ]
    tok_regex = "|".join(f"(?P<{name}>{pattern})" for name, pattern in token_spec)

    line_starts = [0]
    for m in re.finditer(r"\n", stripped):
        line_starts.append(m.end())

    def get_line(pos: int) -> int:
        return bisect.bisect_right(line_starts, pos)

    for m in re.finditer(tok_regex, stripped):
        kind = m.lastgroup
        val = m.group()
        line = get_line(m.start())
        if kind == "STRING":
            val = val[1:-1]
        tokens.append(Token(kind, val, line))
    return tokens


def parse_apply_build_cs(source: str, label: str) -> tuple[list[str], list[str]]:
    """Strictly parses GV2PresentationApply.Build.cs declarative grammar.

    Returns:
        (violations, extracted_dependencies)
    """
    violations: list[str] = []
    modules: list[str] = []
    tokens = tokenize(source)
    pos = 0
    total = len(tokens)

    def peek(offset: int = 0) -> Token | None:
        idx = pos + offset
        return tokens[idx] if idx < total else None

    def match(kind: str, val: str | None = None) -> Token | None:
        nonlocal pos
        tok = peek()
        if tok and tok.kind == kind and (val is None or tok.value == val):
            pos += 1
            return tok
        return None

    # 1. Top-level: zero or more 'using' directives
    # Grammar: only 'using UnrealBuildTool;' is allowed. Any alias or other namespace is rejected.
    while peek() and peek().kind == "IDENT" and peek().value == "using":
        using_tok = tokens[pos]
        pos += 1
        next_tok = peek()
        if not next_tok:
            violations.append(f"{label}:{using_tok.line}: incomplete using directive")
            return violations, modules

        if next_tok.kind == "IDENT" and next_tok.value == "UnrealBuildTool" and peek(1) and peek(1).kind == "PUNCT" and peek(1).value == ";":
            pos += 2
            continue

        disallowed_expr = []
        while peek() and not (peek().kind == "PUNCT" and peek().value == ";"):
            disallowed_expr.append(tokens[pos].value)
            pos += 1
        if peek() and peek().kind == "PUNCT" and peek().value == ";":
            pos += 1
        expr_str = " ".join(disallowed_expr)
        if "=" in expr_str:
            violations.append(f"{label}:{using_tok.line}: alias directive 'using {expr_str};' is prohibited")
        else:
            violations.append(f"{label}:{using_tok.line}: disallowed using directive 'using {expr_str};' -- only 'using UnrealBuildTool;' is permitted")

    # 2. Class definition: public class GV2PresentationApply : ModuleRules { ... }
    class_tok = peek()
    if not class_tok:
        violations.append(f"{label}: no class declaration found")
        return violations, modules

    if not match("IDENT", "public"):
        violations.append(f"{label}:{class_tok.line}: class visibility must be 'public'")
    if not match("IDENT", "class"):
        violations.append(f"{label}:{peek().line if peek() else class_tok.line}: expected 'class' keyword")
        return violations, modules

    name_tok = peek()
    if not name_tok or name_tok.kind != "IDENT":
        violations.append(f"{label}: missing class name")
        return violations, modules
    pos += 1
    if name_tok.value != "GV2PresentationApply":
        violations.append(f"{label}:{name_tok.line}: class name must be 'GV2PresentationApply', got '{name_tok.value}'")

    colon_tok = match("PUNCT", ":")
    if not colon_tok:
        violations.append(f"{label}:{name_tok.line}: missing base class inheritance (: ModuleRules)")
        return violations, modules

    base_tok = peek()
    if not base_tok or base_tok.kind != "IDENT":
        violations.append(f"{label}:{colon_tok.line}: expected base class name")
        return violations, modules
    pos += 1
    if base_tok.value != "ModuleRules":
        violations.append(f"{label}:{base_tok.line}: custom base class '{base_tok.value}' is prohibited -- must inherit directly from 'ModuleRules'")

    if not match("PUNCT", "{"):
        violations.append(f"{label}:{base_tok.line}: expected '{{' to open class body")
        return violations, modules

    # 3. Class body: exactly one constructor
    ctor_tok = peek()
    if not ctor_tok:
        violations.append(f"{label}: empty class body")
        return violations, modules

    if not match("IDENT", "public"):
        violations.append(f"{label}:{ctor_tok.line}: constructor must be 'public'")

    ctor_name = match("IDENT", "GV2PresentationApply")
    if not ctor_name:
        violations.append(f"{label}:{peek().line if peek() else ctor_tok.line}: expected constructor 'GV2PresentationApply'")
        return violations, modules

    if not match("PUNCT", "("):
        violations.append(f"{label}:{ctor_name.line}: expected '(' in constructor declaration")
        return violations, modules

    if not match("IDENT", "ReadOnlyTargetRules"):
        violations.append(f"{label}:{ctor_name.line}: constructor parameter type must be 'ReadOnlyTargetRules'")
    target_param = match("IDENT")
    if not target_param:
        violations.append(f"{label}:{ctor_name.line}: missing constructor parameter name")
    if not match("PUNCT", ")"):
        violations.append(f"{label}:{ctor_name.line}: expected ')' in constructor declaration")

    if not match("PUNCT", ":"):
        violations.append(f"{label}:{ctor_name.line}: missing ': base(...)' constructor initializer")
    if not match("IDENT", "base"):
        violations.append(f"{label}:{ctor_name.line}: expected 'base' in constructor initializer")
    if not match("PUNCT", "("):
        violations.append(f"{label}:{ctor_name.line}: expected '(' after base")
    match("IDENT")
    if not match("PUNCT", ")"):
        violations.append(f"{label}:{ctor_name.line}: expected ')' after base argument")

    if not match("PUNCT", "{"):
        violations.append(f"{label}:{ctor_name.line}: expected '{{' to open constructor body")
        return violations, modules

    # 4. Constructor body statements until '}'
    dependency_statements_count = 0

    while peek() and not (peek().kind == "PUNCT" and peek().value == "}"):
        stmt_start = peek()
        stmt_line = stmt_start.line

        # Check for conditional/loop keywords
        if stmt_start.kind == "IDENT" and stmt_start.value in {"if", "switch", "while", "for", "foreach"}:
            violations.append(f"{label}:{stmt_line}: conditional/loop statement '{stmt_start.value}' is prohibited -- graph must be declarative and unconditional")
            pos += 1
            while peek() and not (peek().kind == "PUNCT" and peek().value in {";", "}"}):
                pos += 1
            if peek() and peek().value == ";":
                pos += 1
            continue

        # Allowed assignment 1: PCHUsage = PCHUsageMode.<Value>;
        if stmt_start.kind == "IDENT" and stmt_start.value == "PCHUsage":
            pos += 1
            if not match("PUNCT", "="):
                violations.append(f"{label}:{stmt_line}: expected '=' after PCHUsage")
            if not match("IDENT", "PCHUsageMode"):
                violations.append(f"{label}:{stmt_line}: expected PCHUsageMode enum type")
            if not match("PUNCT", "."):
                violations.append(f"{label}:{stmt_line}: expected '.' after PCHUsageMode")
            val_tok = match("IDENT")
            if not val_tok:
                violations.append(f"{label}:{stmt_line}: expected PCHUsageMode value identifier")
            if not match("PUNCT", ";"):
                violations.append(f"{label}:{stmt_line}: expected ';' after PCHUsage statement")
            continue

        # Allowed assignment 2: CppStandard = CppStandardVersion.<Value>;
        if stmt_start.kind == "IDENT" and stmt_start.value == "CppStandard":
            pos += 1
            if not match("PUNCT", "="):
                violations.append(f"{label}:{stmt_line}: expected '=' after CppStandard")
            if not match("IDENT", "CppStandardVersion"):
                violations.append(f"{label}:{stmt_line}: expected CppStandardVersion enum type")
            if not match("PUNCT", "."):
                violations.append(f"{label}:{stmt_line}: expected '.' after CppStandardVersion")
            val_tok = match("IDENT")
            if not val_tok:
                violations.append(f"{label}:{stmt_line}: expected CppStandardVersion value identifier")
            if not match("PUNCT", ";"):
                violations.append(f"{label}:{stmt_line}: expected ';' after CppStandard statement")
            continue

        # Allowed assignment 3: bUseUnity = (false | true);
        if stmt_start.kind == "IDENT" and stmt_start.value == "bUseUnity":
            pos += 1
            if not match("PUNCT", "="):
                violations.append(f"{label}:{stmt_line}: expected '=' after bUseUnity")
            val_tok = match("IDENT")
            if not val_tok or val_tok.value not in {"true", "false"}:
                violations.append(f"{label}:{stmt_line}: expected boolean literal for bUseUnity")
            if not match("PUNCT", ";"):
                violations.append(f"{label}:{stmt_line}: expected ';' after bUseUnity statement")
            continue

        # Allowed dependency statement: (Public|Private)DependencyModuleNames.(AddRange|Add)(...);
        if stmt_start.kind == "IDENT" and stmt_start.value in {"PublicDependencyModuleNames", "PrivateDependencyModuleNames"}:
            col_name = stmt_start.value
            pos += 1
            if not match("PUNCT", "."):
                violations.append(f"{label}:{stmt_line}: expected '.' after {col_name}")
            method_tok = match("IDENT")
            if not method_tok or method_tok.value not in {"AddRange", "Add"}:
                violations.append(f"{label}:{stmt_line}: expected AddRange or Add on {col_name}, got '{method_tok.value if method_tok else None}'")
            if not match("PUNCT", "("):
                violations.append(f"{label}:{stmt_line}: expected '(' after method call")

            dependency_statements_count += 1

            if method_tok and method_tok.value == "AddRange":
                if not match("IDENT", "new"):
                    violations.append(f"{label}:{stmt_line}: AddRange requires literal array expression (new string[] {{ ... }} or new[] {{ ... }})")
                    while peek() and not (peek().kind == "PUNCT" and peek().value in {";", "}"}):
                        pos += 1
                    match("PUNCT", ";")
                    continue

                # Optional 'string'
                match("IDENT", "string")
                if not match("PUNCT", "[") or not match("PUNCT", "]"):
                    violations.append(f"{label}:{stmt_line}: expected '[]' in array creation expression")
                if not match("PUNCT", "{"):
                    violations.append(f"{label}:{stmt_line}: expected '{{' opening literal array body")

                while peek() and not (peek().kind == "PUNCT" and peek().value == "}"):
                    tok = peek()
                    if tok.kind == "STRING":
                        modules.append(tok.value)
                        pos += 1
                        match("PUNCT", ",")
                    else:
                        violations.append(f"{label}:{tok.line}: only string literals are permitted in dependency list, got '{tok.value}'")
                        pos += 1

                if not match("PUNCT", "}"):
                    violations.append(f"{label}:{stmt_line}: expected '}}' closing literal array body")
                if not match("PUNCT", ")"):
                    violations.append(f"{label}:{stmt_line}: expected ')' closing AddRange")
                if not match("PUNCT", ";"):
                    violations.append(f"{label}:{stmt_line}: expected ';' terminating AddRange statement")
                continue

            elif method_tok and method_tok.value == "Add":
                tok = peek()
                if tok and tok.kind == "STRING":
                    modules.append(tok.value)
                    pos += 1
                else:
                    violations.append(f"{label}:{stmt_line}: Add requires string literal module name")
                if not match("PUNCT", ")"):
                    violations.append(f"{label}:{stmt_line}: expected ')' closing Add")
                if not match("PUNCT", ";"):
                    violations.append(f"{label}:{stmt_line}: expected ';' terminating Add statement")
                continue

        # Disallowed statement or helper call
        disallowed_tokens = []
        while peek() and not (peek().kind == "PUNCT" and peek().value in {";", "}"}):
            disallowed_tokens.append(tokens[pos].value)
            pos += 1
        match("PUNCT", ";")
        stmt_text = " ".join(disallowed_tokens)
        violations.append(f"{label}:{stmt_line}: disallowed statement or helper call: '{stmt_text}'")

    # Close constructor body
    if not match("PUNCT", "}"):
        violations.append(f"{label}: expected '}}' closing constructor body")

    # Check class body for any additional members
    if peek() and not (peek().kind == "PUNCT" and peek().value == "}"):
        extra_tok = peek()
        violations.append(f"{label}:{extra_tok.line}: extra member or token '{extra_tok.value}' in class body -- only the single constructor is permitted")
        while peek() and not (peek().kind == "PUNCT" and peek().value == "}"):
            pos += 1

    # Close class body
    if not match("PUNCT", "}"):
        violations.append(f"{label}: expected '}}' closing class body")

    # Trailing tokens
    if peek():
        trailing = peek()
        violations.append(f"{label}:{trailing.line}: trailing tokens after class definition: '{trailing.value}'")

    if dependency_statements_count == 0:
        violations.append(f"{label}: no PublicDependencyModuleNames/PrivateDependencyModuleNames declaration found")

    for mod in modules:
        if mod in ALLOWED_MODULES:
            continue
        if mod in NAMED_DENYLIST:
            violations.append(
                f"{label}: '{mod}' is on ADR-0043 D2's explicit denylist -- "
                "GV2PresentationApply must never depend on it"
            )
        else:
            violations.append(
                f"{label}: '{mod}' is not in ADR-0043 D2's allowlist "
                f"({sorted(ALLOWED_MODULES)}) -- classify it there or remove the dependency"
            )

    return violations, modules


def find_violations(build_cs_text: str, build_cs_label: str) -> list[str]:
    violations, _ = parse_apply_build_cs(build_cs_text, build_cs_label)
    return violations


def find_forward_edge_violation(gv2_build_cs_text: str) -> list[str]:
    modules = extract_dependency_modules(gv2_build_cs_text)
    if "GV2PresentationApply" not in modules:
        return [
            f"{GV2_BUILD_CS}: GV2.Build.cs does not declare a dependency on "
            "'GV2PresentationApply' -- ADR-0043 D2's one allowed forward edge (GV2 -> "
            "GV2PresentationApply) is missing"
        ]
    return []


def all_build_cs() -> dict[str, str]:
    """Every module declaration in the tree, by directory walk -- the actual UBT graph, not
    a list of the two files this gate used to read."""
    return {
        path.name[: -len(".Build.cs")]: path.read_text(encoding="utf-8")
        for path in sorted(SOURCE_ROOT.rglob("*.Build.cs"))
    }


def find_conditional_dependency_violations(source: str, label: str) -> list[str]:
    """A dependency added inside a conditional is a dependency the graph does not state.

    ADR-0043 D2's guarantee is that UBT CANNOT link an authority type here. An edge added
    only for Editor targets, or behind any other condition, turns that into a claim about
    which configuration was inspected.
    """
    errors: list[str] = []
    stripped = strip_comments(source)
    for match in re.finditer(r"\b(?:if|else|switch|\?)\b", stripped):
        tail = stripped[match.start():]
        block_end = tail.find("\n        }")
        block = tail[: block_end if block_end > 0 else 400]
        if "DependencyModuleNames" in block:
            line = stripped.count("\n", 0, match.start()) + 1
            errors.append(f"{label}:{line}: conditional dependency edge -- the graph must be unconditional")
    return errors


def canonical_cmake_sources() -> dict[Path, str]:
    """Every canonical CMake declaration in the repository, excluding generated/copy trees."""
    result: dict[Path, str] = {}
    for path in sorted(REPO_ROOT.rglob("CMakeLists.txt")):
        relative = path.relative_to(REPO_ROOT)
        if any(
            part in CMAKE_IGNORED_ROOTS
            or part.startswith("cmake-build-")
            or part == "CMakeFiles"
            for part in relative.parts
        ):
            continue
        result[path] = path.read_text(encoding="utf-8")
    return result


def cmake_commands(source: str) -> list[tuple[str, str, int]]:
    """Return canonical graph command name, argument body and source line."""
    stripped = re.sub(r"#[^\n]*", "", source)
    commands: list[tuple[str, str, int]] = []
    pattern = re.compile(r"\b(" + "|".join(sorted(CMAKE_GRAPH_COMMANDS)) + r")\s*\(", re.IGNORECASE)
    for match in pattern.finditer(stripped):
        depth = 1
        index = match.end()
        quote: str | None = None
        while index < len(stripped) and depth:
            char = stripped[index]
            if quote is not None:
                if char == "\\":
                    index += 2
                    continue
                if char == quote:
                    quote = None
            elif char in {'"', "'"}:
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        if depth == 0:
            commands.append((match.group(1).lower(), stripped[match.end():index - 1], stripped.count("\n", 0, match.start()) + 1))
    return commands


def cmake_arguments(body: str) -> list[str]:
    try:
        return shlex.split(body, comments=False, posix=True)
    except ValueError:
        return re.findall(r'"[^"]*"|\S+', body)


def forbidden_cmake_link_item(item: str) -> bool:
    """Classify plain, imported-target and generator-expression spellings."""
    normalized = item.strip('"').lower()
    raw_tokens = re.findall(r"[a-z][a-z0-9_]*", normalized)
    candidates = {
        normalized,
        normalized.rsplit("::", 1)[-1],
        *raw_tokens,
    }
    for t in list(candidates):
        if t.startswith("lib") and len(t) > 3:
            candidates.add(t[3:])
        if t.startswith("l") and len(t) > 1:
            candidates.add(t[1:])
    return bool(candidates & FORBIDDEN_CMAKE_LINK_ITEMS) or "gv2presentationapply" in normalized


def find_cmake_violations(cmake_sources: dict[Path, str] | None = None) -> list[str]:
    """Reject Unreal sources or link items in the complete portable CMake graph."""
    errors: list[str] = []
    if cmake_sources is None:
        cmake_sources = canonical_cmake_sources()
    if not cmake_sources:
        return ["the canonical CMake-file enumerator produced an empty set; the derivation is broken"]
    for path, source in cmake_sources.items():
        try:
            label = path.relative_to(REPO_ROOT).as_posix()
        except ValueError:
            label = path.as_posix()
        for command, body, line in cmake_commands(source):
            args = cmake_arguments(body)
            if command == "target_link_libraries":
                for arg in args[1:]:
                    if forbidden_cmake_link_item(arg):
                        errors.append(
                            f"{label}:{line}: portable CMake target links Unreal item '{arg}'"
                        )
            if command in {"add_executable", "add_library", "add_subdirectory", "target_sources"}:
                for arg in args:
                    normalized = arg.strip('"').replace("\\", "/")
                    lower = normalized.lower()
                    if ("gv2presentationapply" in lower
                            or re.search(r"(?:^|/)source/gv2/", lower)
                            or re.search(r"(?:^|/)gv2/private/", lower)
                            or lower.endswith((".build.cs", ".target.cs"))):
                        errors.append(
                            f"{label}:{line}: portable CMake graph compiles/includes UE source '{normalized}'"
                        )
    return errors


REQUIRED_PORTABLE_TARGETS = {
    "gv2_content_core",
    "gv2_content_host_support",
    "gv2_runtime_core",
    "gv2_test_support",
    "gv2_content_authoring",
    "gv2_content_editor",
    "gv2_content_editor_conformance",
    "gv2-headless",
    "gv2-content",
}


def ensure_cmake_query(build_dir: Path) -> Path:
    """Pre-create the CMake File API query before configure so replies are emitted on first pass."""
    query_dir = build_dir / ".cmake" / "api" / "v1" / "query"
    query_dir.mkdir(parents=True, exist_ok=True)
    query_file = query_dir / "codemodel-v2"
    query_file.touch(exist_ok=True)
    return query_file


def find_cmake_reply_dirs() -> list[Path]:
    dirs: list[Path] = []
    for candidate in sorted(REPO_ROOT.glob("cmake-build*")):
        reply = candidate / ".cmake" / "api" / "v1" / "reply"
        if reply.is_dir() and list(reply.glob("codemodel-v2-*.json")):
            dirs.append(reply)
    for candidate in sorted(REPO_ROOT.glob("build*")):
        reply = candidate / ".cmake" / "api" / "v1" / "reply"
        if reply.is_dir() and list(reply.glob("codemodel-v2-*.json")):
            dirs.append(reply)
    return dirs


def find_cmake_codemodel_violations(
    reply_dirs: list[Path] | None = None,
    required_targets: set[str] | None = REQUIRED_PORTABLE_TARGETS,
) -> list[str]:
    """Inspect evaluated CMake File API codemodel to ensure no target links or compiles UE items,
    and fail closed if codemodel is missing or incomplete."""
    errors: list[str] = []
    if reply_dirs is None:
        reply_dirs = find_cmake_reply_dirs()
        if not reply_dirs and shutil.which("cmake"):
            with tempfile.TemporaryDirectory() as tmp_build:
                tmp_build_path = Path(tmp_build)
                ensure_cmake_query(tmp_build_path)
                res = subprocess.run(
                    ["cmake", "-S", str(REPO_ROOT), "-B", str(tmp_build_path)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                if res.returncode == 0:
                    tmp_reply = tmp_build_path / ".cmake" / "api" / "v1" / "reply"
                    if tmp_reply.is_dir() and list(tmp_reply.glob("codemodel-v2-*.json")):
                        reply_dirs = [tmp_reply]

    if not reply_dirs:
        return ["CMake File API codemodel reply is missing: no valid codemodel found in build directories or via configure"]

    for reply_dir in reply_dirs:
        if not reply_dir.is_dir():
            errors.append(f"CMake File API reply directory '{reply_dir}' does not exist")
            continue
        codemodel_files = sorted(reply_dir.glob("codemodel-v2-*.json"))
        if not codemodel_files:
            errors.append(f"CMake File API reply directory '{reply_dir}' contains no codemodel-v2 reply")
            continue

        try:
            cm = json.loads(codemodel_files[0].read_text(encoding="utf-8"))
        except Exception as ex:
            errors.append(f"{reply_dir}: failed to read codemodel JSON: {ex}")
            continue

        if cm.get("kind") != "codemodel" or cm.get("version", {}).get("major") != 2:
            errors.append(f"{reply_dir}: unexpected codemodel schema (expected kind='codemodel', version.major=2)")
            continue

        configs = cm.get("configurations", [])
        if not configs:
            errors.append(f"{reply_dir}: codemodel contains zero configurations")
            continue

        for config in configs:
            targets_list = config.get("targets", [])
            if not targets_list:
                errors.append(f"{reply_dir}: configuration '{config.get('name', '')}' has no targets")
                continue

            target_names = {t.get("name", "") for t in targets_list}
            if required_targets:
                missing_required = sorted(required_targets - target_names)
                if missing_required:
                    errors.append(
                        f"{reply_dir}: codemodel is incomplete: required portable targets missing: {missing_required}"
                    )

            for target_ref in targets_list:
                json_file = target_ref.get("jsonFile")
                if not json_file:
                    errors.append(f"{reply_dir}: target reference missing 'jsonFile'")
                    continue
                t_path = reply_dir / json_file
                if not t_path.is_file():
                    errors.append(
                        f"{reply_dir}: codemodel is incomplete: target JSON file '{t_path}' does not exist"
                    )
                    continue
                try:
                    target = json.loads(t_path.read_text(encoding="utf-8"))
                except Exception as ex:
                    errors.append(f"{t_path}: failed to read target json: {ex}")
                    continue

                name = target.get("name", target_ref.get("name", ""))
                label = f"CMake codemodel target '{name}'"

                for s in target.get("sources", []):
                    path = s.get("path", "").replace("\\", "/")
                    lower = path.lower()
                    if (
                        "gv2presentationapply" in lower
                        or re.search(r"(?:^|/)source/gv2/", lower)
                        or re.search(r"(?:^|/)gv2/private/", lower)
                        or lower.endswith((".build.cs", ".target.cs"))
                    ):
                        errors.append(
                            f"{label}: portable target compiles/includes UE source '{path}'"
                        )

                all_deps = (
                    target.get("compileDependencies", [])
                    + target.get("linkLibraries", [])
                    + target.get("interfaceLinkLibraries", [])
                    + target.get("dependencies", [])
                )
                for dep in all_deps:
                    dep_id = dep.get("id", "").split("::")[0]
                    if dep_id and forbidden_cmake_link_item(dep_id):
                        errors.append(
                            f"{label}: portable target links forbidden dependency '{dep_id}'"
                        )
                    frag = dep.get("fragment", "")
                    if frag:
                        for arg in cmake_arguments(frag):
                            if forbidden_cmake_link_item(arg):
                                errors.append(
                                    f"{label}: portable target links forbidden item '{arg}'"
                                )

                link_info = target.get("link", {})
                for frag in link_info.get("commandFragments", []):
                    for arg in cmake_arguments(frag.get("fragment", "")):
                        if forbidden_cmake_link_item(arg):
                            errors.append(
                                f"{label}: portable target links forbidden item '{arg}'"
                            )

    return errors


def find_reverse_edge_violations(modules: dict[str, str]) -> list[str]:
    """No module the Apply module is forbidden to depend on may be reachable FROM it, and
    the Apply module itself must be depended on by GV2 alone -- a second consumer would be
    a second place the boundary has to hold."""
    errors: list[str] = []
    consumers = [
        name for name, source in modules.items()
        if name != APPLY_MODULE and APPLY_MODULE in extract_dependency_modules(source)
    ]
    if consumers != ["GV2"]:
        errors.append(
            f"{APPLY_MODULE} must be consumed by GV2 alone; actual consumers: {sorted(consumers)}"
        )
    return errors


def validate_repository(reply_dir: Path | None = None) -> list[str]:
    violations: list[str] = []
    if not APPLY_BUILD_CS.exists():
        return [f"{APPLY_BUILD_CS}: not found"]
    violations.extend(find_violations(APPLY_BUILD_CS.read_text(encoding="utf-8"), str(APPLY_BUILD_CS)))

    if not GV2_BUILD_CS.exists():
        violations.append(f"{GV2_BUILD_CS}: not found")
    else:
        violations.extend(find_forward_edge_violation(GV2_BUILD_CS.read_text(encoding="utf-8")))

    # PSC-11: the actual graph, derived from every module declaration in the tree.
    modules = all_build_cs()
    if not modules:
        return violations + ["the module enumerator produced an empty set; the derivation is broken"]
    violations.extend(find_conditional_dependency_violations(modules[APPLY_MODULE], str(APPLY_BUILD_CS)))
    violations.extend(find_reverse_edge_violations(modules))
    violations.extend(find_cmake_violations())
    reply_dirs = [reply_dir] if reply_dir else None
    violations.extend(find_cmake_codemodel_violations(reply_dirs))
    return violations


def run_self_test(reply_dir: Path | None = None, ue_root: Path | None = None) -> bool:
    print("[*] Running validate_presentation_apply_module_graph self-test...")
    if errors := validate_repository(reply_dir):
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir) / "Synthetic.Build.cs"

        # 1. Denylisted module with new string[] must be flagged.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new string[] { "Core", "GV2ContentCore" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("GV2ContentCore" in error and "denylist" in error for error in errors):
            print(f"FAILED: gate did not flag a denylisted dependency (new string[]): {errors}")
            return False

        # 2. Denylisted module with new[] (inferred array syntax, PSC-AF-06 bypass) must be flagged.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core", "GV2ContentCore" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("GV2ContentCore" in error and "denylist" in error for error in errors):
            print(f"FAILED: gate did not flag a denylisted dependency (new[]): {errors}")
            return False

        # 3. An unknown, unlisted module must also be flagged with new[].
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core", "SomeFutureModule" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("SomeFutureModule" in error for error in errors):
            print(f"FAILED: gate did not flag an unclassified dependency: {errors}")
            return False

        # 4. A single .Add("X") call (not AddRange) must be parsed too.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            '        PrivateDependencyModuleNames.Add("AssetRegistry");\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("AssetRegistry" in error and "denylist" in error for error in errors):
            print(f"FAILED: gate did not flag a denylisted single .Add(...) dependency: {errors}")
            return False

        # 5. Helper method call must be rejected (fail-closed against procedural logic).
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            "        AddCustomDependencies();\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("disallowed statement or helper call" in error and "AddCustomDependencies" in error for error in errors):
            print(f"FAILED: gate did not flag a helper method call: {errors}")
            return False

        # 6. Disallowed include path statement must be rejected.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicIncludePaths.Add("Secret/Include/Path");\n'
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("disallowed statement or helper call" in error and "PublicIncludePaths" in error for error in errors):
            print(f"FAILED: gate did not flag a disallowed include path statement: {errors}")
            return False

        # 7. Custom base class inheritance must be rejected.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : CustomModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("custom base class 'CustomModuleRules' is prohibited" in error for error in errors):
            print(f"FAILED: gate did not flag a custom base class: {errors}")
            return False

        # 8. Disallowed using directive must be rejected.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "using System.IO;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("disallowed using directive" in error and "System . IO" in error for error in errors):
            print(f"FAILED: gate did not flag a disallowed using directive: {errors}")
            return False

        # 9. Disallowed using alias must be rejected.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "using Rules = UnrealBuildTool.ModuleRules;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("alias directive" in error for error in errors):
            print(f"FAILED: gate did not flag a using alias directive: {errors}")
            return False

        # 10. Conditional/loop statement inside constructor must be rejected.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            "        if (Target.bBuildEditor) {\n"
            '            PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
            "        }\n"
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("conditional/loop statement 'if' is prohibited" in error for error in errors):
            print(f"FAILED: gate did not flag a conditional statement: {errors}")
            return False

        # 11. Valid allowlist using new string[] must produce zero violations.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            "        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n"
            "        CppStandard = CppStandardVersion.Cpp20;\n"
            "        bUseUnity = false;\n"
            "        PublicDependencyModuleNames.AddRange(new string[]\n"
            "        {\n"
            '            "Core",\n'
            '            "CoreUObject",\n'
            '            "Engine",\n'
            '            "UMG",\n'
            '            "CommonUI",\n'
            '            "Slate",\n'
            '            "SlateCore"\n'
            "        });\n"
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if errors:
            print(f"FAILED: gate rejected an entirely allowlisted dependency set (new string[]): {errors}")
            return False

        # 12. Valid allowlist using new[] must produce zero violations.
        tmp_path.write_text(
            "using UnrealBuildTool;\n"
            "public class GV2PresentationApply : ModuleRules {\n"
            "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
            "        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n"
            "        CppStandard = CppStandardVersion.Cpp20;\n"
            "        bUseUnity = false;\n"
            "        PublicDependencyModuleNames.AddRange(new[]\n"
            "        {\n"
            '            "Core",\n'
            '            "CoreUObject",\n'
            '            "Engine",\n'
            '            "UMG",\n'
            '            "CommonUI",\n'
            '            "Slate",\n'
            '            "SlateCore"\n'
            "        });\n"
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if errors:
            print(f"FAILED: gate rejected an entirely allowlisted dependency set (new[]): {errors}")
            return False

        # 13. Forward edge checks on GV2.Build.cs
        errors = find_forward_edge_violation('PublicDependencyModuleNames.AddRange(new string[] { "Core" });\n')
        if not any("GV2PresentationApply" in error for error in errors):
            print(f"FAILED: gate did not flag a missing GV2 -> GV2PresentationApply forward edge: {errors}")
            return False

        errors = find_forward_edge_violation(
            'PublicDependencyModuleNames.AddRange(new[] { "Core", "GV2PresentationApply" });\n'
        )
        if errors:
            print(f"FAILED: gate rejected a GV2.Build.cs that does declare the forward edge: {errors}")
            return False

    # 14. Reverse edge checks: a second consumer must be flagged.
    valid_apply = (
        "using UnrealBuildTool;\n"
        "public class GV2PresentationApply : ModuleRules {\n"
        "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target) {\n"
        '        PublicDependencyModuleNames.AddRange(new[] { "Core" });\n'
        "    }\n"
        "}\n"
    )
    two_consumers = {
        "GV2PresentationApply": valid_apply,
        "GV2": 'PublicDependencyModuleNames.AddRange(new string[] { "GV2PresentationApply" });',
        "GV2ContentEditor": 'PublicDependencyModuleNames.AddRange(new[] { "GV2PresentationApply" });',
    }
    if not find_reverse_edge_violations(two_consumers):
        print("FAILED: gate accepted a second consumer of the Apply module")
        return False
    one_consumer = dict(two_consumers)
    del one_consumer["GV2ContentEditor"]
    if find_reverse_edge_violations(one_consumer):
        print("FAILED: gate flagged the single allowed consumer")
        return False

    # 15. Static CMake oracle checks
    synthetic_cmake = {
        Path("Headless/CMakeLists.txt"): (
            "add_executable(gv2-headless Source/main.cpp)\n"
            "target_link_libraries(gv2-headless PRIVATE gv2_runtime_core UMG Unreal::CommonUI)\n"
        ),
        Path("FutureHost/CMakeLists.txt"): (
            "target_sources(gv2-headless PRIVATE ../Source/GV2/Private/UI/Future.cpp)\n"
        ),
    }
    cmake_errors = find_cmake_violations(synthetic_cmake)
    if not any("UMG" in error for error in cmake_errors):
        print(f"FAILED: CMake graph gate accepted an Unreal/UMG link edge: {cmake_errors}")
        return False
    if not any("Unreal::CommonUI" in error for error in cmake_errors):
        print(f"FAILED: CMake graph gate accepted a namespaced Unreal/CommonUI link edge: {cmake_errors}")
        return False
    if not any("Source/GV2/" in error for error in cmake_errors):
        print(f"FAILED: CMake graph gate accepted a UE source edge: {cmake_errors}")
        return False

    # 16. Configured CMake File API codemodel mutations via real macro() and function()
    if shutil.which("cmake"):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir_path = Path(tmpdir)
            # Test 16a: Real macro() mutating target_link_libraries with forbidden item
            macro_proj = tmpdir_path / "macro_proj"
            macro_proj.mkdir()
            (macro_proj / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\n"
                "project(MacroMutation LANGUAGES CXX)\n"
                "macro(inject_forbidden_link tgt)\n"
                "    target_link_libraries(${tgt} PRIVATE UMG)\n"
                "endmacro()\n"
                "add_library(macro_tgt STATIC dummy.cpp)\n"
                "inject_forbidden_link(macro_tgt)\n",
                encoding="utf-8",
            )
            (macro_proj / "dummy.cpp").write_text("int f() { return 0; }\n", encoding="utf-8")
            macro_build = tmpdir_path / "macro_build"
            ensure_cmake_query(macro_build)
            res = subprocess.run(
                ["cmake", "-S", str(macro_proj), "-B", str(macro_build)],
                capture_output=True,
                text=True,
            )
            if res.returncode != 0:
                print(f"FAILED: synthetic macro CMake configure failed: {res.stderr}")
                return False
            macro_reply = macro_build / ".cmake" / "api" / "v1" / "reply"
            macro_errors = find_cmake_codemodel_violations([macro_reply], required_targets=None)
            if not any("UMG" in err and "macro_tgt" in err for err in macro_errors):
                print(f"FAILED: CMake codemodel gate did not flag macro-injected link dependency: {macro_errors}")
                return False

            # Test 16b: Real function() mutating target_sources with forbidden UE source
            func_proj = tmpdir_path / "func_proj"
            func_proj.mkdir()
            (func_proj / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\n"
                "project(FuncMutation LANGUAGES CXX)\n"
                "function(inject_forbidden_source tgt)\n"
                "    target_sources(${tgt} PRIVATE Source/GV2/Private/UI/Forbidden.cpp)\n"
                "endfunction()\n"
                "add_library(func_tgt STATIC dummy.cpp)\n"
                "inject_forbidden_source(func_tgt)\n",
                encoding="utf-8",
            )
            (func_proj / "dummy.cpp").write_text("int g() { return 0; }\n", encoding="utf-8")
            forbidden_dir = func_proj / "Source" / "GV2" / "Private" / "UI"
            forbidden_dir.mkdir(parents=True, exist_ok=True)
            (forbidden_dir / "Forbidden.cpp").write_text("int forbidden() { return 1; }\n", encoding="utf-8")
            func_build = tmpdir_path / "func_build"
            ensure_cmake_query(func_build)
            res = subprocess.run(
                ["cmake", "-S", str(func_proj), "-B", str(func_build)],
                capture_output=True,
                text=True,
            )
            if res.returncode != 0:
                print(f"FAILED: synthetic function CMake configure failed: {res.stderr}")
                return False
            func_reply = func_build / ".cmake" / "api" / "v1" / "reply"
            func_errors = find_cmake_codemodel_violations([func_reply], required_targets=None)
            if not any("Forbidden.cpp" in err and "func_tgt" in err for err in func_errors):
                print(f"FAILED: CMake codemodel gate did not flag function-injected UE source: {func_errors}")
                return False

    # 17. Evaluated CMake File API codemodel negative checks (violations and incompleteness)
    with tempfile.TemporaryDirectory() as tmpdir:
        reply_dir = Path(tmpdir) / "reply"
        reply_dir.mkdir(parents=True)
        cm_file = reply_dir / "codemodel-v2-synthetic.json"
        t1_file = reply_dir / "target-bad-source.json"
        t2_file = reply_dir / "target-bad-link.json"
        t3_file = reply_dir / "target-clean.json"

        cm_data = {
            "kind": "codemodel",
            "version": {"major": 2, "minor": 0},
            "configurations": [{
                "name": "Debug",
                "targets": [
                    {"jsonFile": t1_file.name, "name": "bad_source"},
                    {"jsonFile": t2_file.name, "name": "bad_link"},
                    {"jsonFile": t3_file.name, "name": "clean_target"}
                ]
            }]
        }
        cm_file.write_text(json.dumps(cm_data), encoding="utf-8")

        t1_file.write_text(json.dumps({
            "name": "bad_source",
            "sources": [{"path": "Source/GV2/Private/SomeClass.cpp"}],
            "link": {"commandFragments": []}
        }), encoding="utf-8")

        t2_file.write_text(json.dumps({
            "name": "bad_link",
            "sources": [{"path": "Headless/main.cpp"}],
            "link": {"commandFragments": [{"fragment": "-lumg"}]}
        }), encoding="utf-8")

        t3_file.write_text(json.dumps({
            "name": "clean_target",
            "sources": [{"path": "Headless/main.cpp"}],
            "link": {"commandFragments": [{"fragment": "-lm"}]}
        }), encoding="utf-8")

        codemodel_errors = find_cmake_codemodel_violations([reply_dir], required_targets=None)
        if not any("bad_source" in err and "SomeClass.cpp" in err for err in codemodel_errors):
            print(f"FAILED: CMake codemodel gate did not flag forbidden source: {codemodel_errors}")
            return False
        if not any("bad_link" in err and "-lumg" in err for err in codemodel_errors):
            print(f"FAILED: CMake codemodel gate did not flag forbidden link fragment: {codemodel_errors}")
            return False

        # Check missing target JSON file is flagged as incomplete
        t1_file.unlink()
        incompleteness_errors = find_cmake_codemodel_violations([reply_dir], required_targets=None)
        if not any("target-bad-source.json" in err and "does not exist" in err for err in incompleteness_errors):
            print(f"FAILED: CMake codemodel gate silently ignored missing target JSON file: {incompleteness_errors}")
            return False

        # Check missing required portable target is flagged as incomplete
        missing_target_errors = find_cmake_codemodel_violations([reply_dir], required_targets={"gv2_content_core"})
        if not any("gv2_content_core" in err and "missing" in err for err in missing_target_errors):
            print(f"FAILED: CMake codemodel gate did not flag missing required portable target: {missing_target_errors}")
            return False

    # 18. Compiler-negative UBT probe (if UBT/UE is present)
    if ue_root is None:
        ue_root = Path(os.environ.get("UE_ROOT", "/opt/unreal-engine"))
    build_sh = ue_root / "Engine" / "Build" / "BatchFiles" / "Linux" / "Build.sh"
    if build_sh.is_file() and os.access(build_sh, os.X_OK):
        if not run_compiler_negative_ubt_probe(ue_root):
            return False
    else:
        print(f"[*] Skipping compiler-negative UBT probe in self-test: Unreal Engine Build.sh not found at {build_sh}")

    print("SUCCESS: GV2PresentationApply's dependency list stays within the ADR-0043 D2 allowlist")
    return True


def run_compiler_negative_ubt_probe(ue_root: Path | None = None, repo_root: Path = REPO_ROOT) -> bool:
    """Proves via actual UBT compilation failure that GV2PresentationApply cannot reach authority headers."""
    print("[*] Running compiler-negative UBT probe for GV2PresentationApply...")
    if ue_root is None:
        ue_root = Path(os.environ.get("UE_ROOT", "/opt/unreal-engine"))
    build_sh = ue_root / "Engine" / "Build" / "BatchFiles" / "Linux" / "Build.sh"
    if not build_sh.is_file() or not os.access(build_sh, os.X_OK):
        print(f"FAILED: Unreal Engine Build.sh not found or not executable at: {build_sh}", file=sys.stderr)
        return False

    uproject = repo_root / "GV2.uproject"
    if not uproject.is_file():
        print(f"FAILED: GV2.uproject not found at: {uproject}", file=sys.stderr)
        return False

    probe_file = repo_root / "Source" / "GV2PresentationApply" / "Private" / "GV2CompilerNegativeAuthorityProbe.cpp"
    probe_content = (
        '// Synthetic probe to verify compiler-negative authority isolation in UBT\n'
        '#include "GV2PresentationApply/PreparedPresentationTransaction.h"\n'
        '#include "GV2ContentCore/CanonicalHash.h" // Authority header must NOT be accessible in GV2PresentationApply\n'
        '\n'
        'void GV2CompilerNegativeAuthorityProbeAnchor()\n'
        '{\n'
        '}\n'
    )

    try:
        probe_file.write_text(probe_content, encoding="utf-8")
        cmd = [
            str(build_sh),
            "GV2Editor",
            "Linux",
            "Development",
            str(uproject),
            "-WaitMutex",
            "-NoHotReloadFromIDE",
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        output = res.stdout + "\n" + res.stderr
        if res.returncode == 0:
            print(f"FAILED: UBT unexpectedly SUCCEEDED with forbidden authority header included!\nOutput:\n{output}", file=sys.stderr)
            return False

        if "GV2ContentCore/CanonicalHash.h" not in output or "not found" not in output.lower():
            print(f"FAILED: UBT failed, but not for expected 'file not found' error:\nOutput:\n{output}", file=sys.stderr)
            return False

        print("SUCCESS: UBT correctly rejected authority header in GV2PresentationApply with compilation error.")
        return True
    finally:
        if probe_file.exists():
            probe_file.unlink()
        gen_script = repo_root / "Tools" / "Build" / "generate_build_identity.py"
        if gen_script.is_file():
            subprocess.run([sys.executable, str(gen_script)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Validate GV2PresentationApply module graph, CMake codemodel, and authority isolation."
    )
    parser.add_argument("--self-test", action="store_true", help="Run comprehensive gate self-test suite.")
    parser.add_argument("--reply-dir", type=Path, default=None, help="Exact CMake File API reply directory to inspect.")
    parser.add_argument("--ubt-probe", action="store_true", help="Run compiler-negative UBT probe.")
    parser.add_argument("--ue-root", type=Path, default=None, help="Root directory of Unreal Engine installation.")
    args = parser.parse_args(argv)

    if args.ubt_probe:
        return 0 if run_compiler_negative_ubt_probe(args.ue_root) else 1

    if args.self_test:
        return 0 if run_self_test(args.reply_dir, args.ue_root) else 1

    errors = validate_repository(args.reply_dir)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: GV2PresentationApply's dependency list stays within the ADR-0043 D2 allowlist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
