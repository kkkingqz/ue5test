#!/usr/bin/env python3
"""PSC-10B structural gate for the retired pull-style/runtime-authority surface.

The actual set is every production C++ source/header under Source/GV2. Tests are
excluded because they may inspect bootstrap fixtures directly. The gate deliberately
matches symbols, not a list of known call sites: adding the old API anywhere fails.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source" / "GV2"

FORBIDDEN_SYMBOLS = {
    "GetConfiguredTheme": re.compile(r"\bGetConfiguredTheme\s*\("),
    "GetConfiguredRegistry": re.compile(r"\bGetConfiguredRegistry\s*\("),
    "Execute_ApplyCentralStyle": re.compile(r"\bExecute_ApplyCentralStyle\s*\("),
    "ApplyCentralStyle_Implementation": re.compile(r"\bApplyCentralStyle_Implementation\s*\("),
}

RECOVERY_CALL = re.compile(r"\bGetCoreMinimalTheme\s*\(")
RECOVERY_CALL_FILES = {
    "Private/Runtime/GV2RuntimeSubsystem.cpp",
    "Private/UI/GV2RecoveryScreenWidget.cpp",
    # PSC-10B: the snapshot RESOLVES the core-minimal Theme once at build time and pins it as
    # the session's text fallback. That is bootstrap resolution into the snapshot, the
    # opposite of the runtime pull this gate exists to forbid -- every reader downstream sees
    # it as an ordinary prepared value through FGV2PresentationPrepareContext.
    "Private/Application/GV2SessionContentSnapshot.cpp",
}
RECOVERY_DECLARATION_FILES = {
    "Private/UI/GV2UiTheme.cpp",
    "Public/UI/GV2UiTheme.h",
}

# Content-resolution capabilities that must not appear on a central-style path. The FILE SET
# they apply to is derived below from what each file actually does -- it is not a list of
# known filenames. A hand-written file list is the same construction that let
# GetConfiguredTheme() sit outside the PAH-08 authority set for three rounds (PAH-R7); a new
# preparer or a new value sink in a new file is covered here the moment it exists.
LATE_EFFECT_FORBIDDEN = (
    re.compile(r"\bLoadSynchronous\s*\("),
    re.compile(r"\bStaticLoadObject\s*\("),
    re.compile(r"\bGetSessionCatalog\s*\("),
    re.compile(r"\bResolveAndApply\s*\("),
)

# A file participates in central style if it prepares the subtree, dispatches the operation,
# defines a value sink, or is the late (hover-time) path that applies an already-prepared
# style. Each marker is a thing the file DOES, readable from the file itself.
CENTRAL_STYLE_PARTICIPANT_MARKERS = (
    re.compile(r"\bPrepareForSubtree\s*\("),
    re.compile(r"\bFPreparedCentralStyleOperation\b"),
    re.compile(r"::Apply[A-Za-z0-9_]*Style[A-Za-z0-9_]*\s*\("),
)

PAYLOAD_HEADER = (
    REPO_ROOT
    / "Source"
    / "GV2PresentationApply"
    / "Public"
    / "GV2PresentationApply"
    / "PreparedPresentationTransaction.h"
)


def production_sources() -> dict[str, str]:
    result: dict[str, str] = {}
    for path in sorted(SOURCE_ROOT.rglob("*")):
        if path.suffix not in {".h", ".cpp"} or "Tests" in path.parts:
            continue
        rel = path.relative_to(SOURCE_ROOT).as_posix()
        result[rel] = path.read_text(encoding="utf-8")
    return result


def line_of(source: str, offset: int) -> int:
    return source.count("\n", 0, offset) + 1


def strip_comments(source: str) -> str:
    """Blank comments while preserving offsets and line numbers."""
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if char == "\n" else " " for char in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, source)


def style_consumer_classes(sources: dict[str, str]) -> set[str]:
    pattern = re.compile(
        r"class\s+GV2_API\s+(U[A-Za-z0-9_]+)\s*:[^{]*"
        r"public\s+IGV2UiStyleConsumer[^{]*\{",
        re.DOTALL,
    )
    return {
        match.group(1)
        for rel, source in sources.items()
        if rel.startswith("Public/")
        for match in pattern.finditer(strip_comments(source))
    }


def top_level_blocks(source: str) -> list[tuple[int, str]]:
    """Every brace block that starts at column 0, with its offset.

    Function-scoped rather than file-scoped on purpose: a file may legitimately contain both
    a central-style value sink and unrelated content resolution (an image widget resolves
    image resources and also receives a tint role). The claim being checked is about the
    central-style code path, so the unit of the check is the function, not the file.
    """
    blocks: list[tuple[int, str]] = []
    index = 0
    while True:
        index = source.find("\n{\n", index)
        if index < 0:
            return blocks
        # Include the signature line(s): the marker that says "this function participates in
        # central style" is the function's own name, which sits above the brace.
        prev_close = source.rfind("\n}\n", 0, index)
        prev_blank = source.rfind("\n\n", 0, index)
        sig_start = max(prev_close + 3, prev_blank + 2, 0)
        end = source.find("\n}", index + 1)
        if end < 0:
            return blocks
        blocks.append((sig_start, source[sig_start:end + 2]))
        index = end + 1


def payload_roles() -> set[str]:
    """Every alternative of FPreparedCentralStylePayload, from the variant declaration."""
    source = PAYLOAD_HEADER.read_text(encoding="utf-8")
    match = re.search(
        r"using\s+FPreparedCentralStylePayload\s*=\s*TVariant<(?P<args>.*?)>\s*;",
        source,
        re.DOTALL,
    )
    if match is None:
        return set()
    args = re.sub(r"//[^\n]*", "", match.group("args"))
    return {arg.strip() for arg in args.split(",") if arg.strip()}


def apply_branch_roles(adapter: str) -> set[str]:
    """Payload roles the adapter's central-style visitor actually handles."""
    start = adapter.find("FPreparedCentralStyleOperation& Op")
    end = adapter.find("}, Operation);", start + 1)
    if start < 0 or end < 0:
        return set()
    return set(
        re.findall(
            r"const\s+GV2PresentationApply::(FPrepared[A-Za-z0-9_]+)\s*&",
            adapter[start:end],
        )
    )


def cast_targets(source: str, start_marker: str, end_marker: str) -> set[str]:
    start = source.find(start_marker)
    end = source.find(end_marker, start + len(start_marker))
    if start < 0 or end < 0:
        return set()
    return set(re.findall(r"Cast<(U[A-Za-z0-9_]+)>\(Widget\)", source[start:end]))


def find_violations(sources: dict[str, str], roles: set[str] | None = None) -> list[str]:
    errors: list[str] = []
    for rel, source in sources.items():
        stripped = strip_comments(source)
        for name, pattern in FORBIDDEN_SYMBOLS.items():
            for match in pattern.finditer(stripped):
                errors.append(f"{rel}:{line_of(source, match.start())}: retired runtime symbol {name}")

        if rel not in RECOVERY_DECLARATION_FILES:
            for match in RECOVERY_CALL.finditer(stripped):
                if rel not in RECOVERY_CALL_FILES:
                    errors.append(
                        f"{rel}:{line_of(source, match.start())}: GetCoreMinimalTheme is only allowed in cold-start recovery"
                    )

        for block_start, block in top_level_blocks(stripped):
            if not any(marker.search(block) for marker in CENTRAL_STYLE_PARTICIPANT_MARKERS):
                continue
            for pattern in LATE_EFFECT_FORBIDDEN:
                for match in pattern.finditer(block):
                    errors.append(
                        f"{rel}:{line_of(source, block_start + match.start())}: a central-style path "
                        "performs content resolution/loading; resolve it once into the session snapshot instead"
                    )
    consumers = style_consumer_classes(sources)
    preparer = sources.get("Private/UI/GV2CentralStylePreparer.cpp", "")
    adapter = sources.get("Private/UI/GV2LegacyPresentationApplyAdapter.cpp", "")
    prepare_targets = cast_targets(preparer, "void EmitForWidget", "bool OwnsSubtreeStyling")
    apply_targets = cast_targets(adapter, "FPreparedCentralStyleOperation& Op", "}, Operation);")

    # The subtree walk's target set IS the interface implementation set: a widget the walk
    # can meet must have a role, and a branch for a class the walk cannot meet would be a
    # preparer branch production never executes.
    if prepare_targets != consumers:
        errors.append(
            "central-style Prepare target set differs from interface implementations; "
            f"missing={sorted(consumers - prepare_targets)}, extra={sorted(prepare_targets - consumers)}"
        )

    # Apply is enumerated by the payload variant, not by the interface: a role whose target
    # is created outside any prepared subtree (the hover popover) still needs exactly one
    # apply branch, and no role may be left unhandled.
    roles = payload_roles() if roles is None else roles
    handled = apply_branch_roles(adapter)
    if roles != handled:
        errors.append(
            "central-style Apply branches differ from FPreparedCentralStylePayload alternatives; "
            f"unhandled={sorted(roles - handled)}, unknown={sorted(handled - roles)}"
        )
    if not consumers or not roles:
        errors.append("central-style enumerators produced an empty set; the derivation is broken")
    if not consumers <= apply_targets:
        errors.append(
            "every style consumer must also be an Apply cast target; "
            f"missing={sorted(consumers - apply_targets)}"
        )

    helper_call = re.compile(r"(?<!::)\bApply[A-Za-z0-9_]+StyleValues?\s*\(")
    for rel, source in sources.items():
        if not rel.endswith(".cpp"):
            continue
        if rel == "Private/UI/GV2LegacyPresentationApplyAdapter.cpp":
            continue
        stripped = strip_comments(source)
        for match in helper_call.finditer(stripped):
            function_start = stripped.rfind("NativePreConstruct", 0, match.start())
            prior_close = stripped.rfind("}", 0, match.start())
            if function_start < 0 or function_start < prior_close:
                errors.append(
                    f"{rel}:{line_of(source, match.start())}: style value sink called outside transaction adapter"
                )
                continue
            function_prefix = stripped[function_start:match.start()]
            if "IsDesignTime()" not in function_prefix:
                errors.append(
                    f"{rel}:{line_of(source, match.start())}: NativePreConstruct style sink is not guarded by IsDesignTime"
                )

    return errors


def run_self_test() -> bool:
    synthetic_roles = {"FPreparedSyntheticRoleStyle"}
    clean = {
        "Private/UI/Clean.cpp": "void ApplyPreparedStyle() {}\n",
        "Private/UI/GV2UiTheme.cpp": "void UGV2UiTheme::GetCoreMinimalTheme() {}\n",
        "Public/UI/GV2UiTheme.h": "static void GetCoreMinimalTheme();\n",
        "Public/UI/GV2StyledWidget.h": (
            "class GV2_API UGV2StyledWidget : public UWidget, "
            "public IGV2UiStyleConsumer {};\n"
        ),
        "Private/UI/GV2RecoveryScreenWidget.cpp": "auto* T = GetCoreMinimalTheme();\n",
        "Private/UI/GV2CentralStylePreparer.cpp": (
            "void EmitForWidget() { Cast<UGV2StyledWidget>(Widget); } "
            "bool OwnsSubtreeStyling() {}\n"
        ),
        "Private/UI/GV2LegacyPresentationApplyAdapter.cpp": (
            "FPreparedCentralStyleOperation& Op; Cast<UGV2StyledWidget>(Widget); "
            "const GV2PresentationApply::FPreparedSyntheticRoleStyle& S; }, Operation);\n"
        ),
    }
    if errors := find_violations(clean, synthetic_roles):
        print("FAILED: clean synthetic input was rejected:\n" + "\n".join(errors))
        return False

    mutations = {
        "configured theme": ("Private/UI/Synthetic.cpp", "GetConfiguredTheme();\n"),
        "configured registry": ("Private/UI/Synthetic.cpp", "GetConfiguredRegistry();\n"),
        "style entry point": ("Private/UI/Synthetic.cpp", "Execute_ApplyCentralStyle(W);\n"),
        "non-recovery minimal theme": ("Private/UI/Synthetic.cpp", "GetCoreMinimalTheme();\n"),
        "late hover load": (
            "Private/UI/GV2RichTextWidgetBase.cpp",
            "void UGV2RichTextWidgetBase::ApplyRichTextStyleValues()\n{\n"
            "    Class.LoadSynchronous();\n}\n",
        ),
        "late hover resolve-and-apply": (
            "Private/UI/GV2RichTextPopoverWidgetBase.cpp",
            "void UGV2RichTextPopoverWidgetBase::ApplyPopoverStyleValues()\n{\n"
            "    FGV2ImagePresentation::ResolveAndApply();\n}\n",
        ),
        "style helper outside adapter": (
            "Private/UI/Synthetic.cpp",
            "void Commit() { ApplyButtonStyleValues(); }\n",
        ),
        "runtime NativePreConstruct style": (
            "Private/UI/Synthetic.cpp",
            "void NativePreConstruct() { ApplyButtonStyleValues(); }\n",
        ),
    }
    for label, (path, source) in mutations.items():
        mutated = dict(clean)
        mutated[path] = source
        if not find_violations(mutated, synthetic_roles):
            print(f"FAILED: gate accepted synthetic violation: {label}")
            return False

    for label, path, source in (
        (
            "new style consumer without Prepare/Apply branches",
            "Public/UI/GV2UnwiredWidget.h",
            "class GV2_API UGV2UnwiredWidget : public UWidget, public IGV2UiStyleConsumer {};\n",
        ),
        (
            "style consumer missing from Prepare",
            "Private/UI/GV2CentralStylePreparer.cpp",
            "void EmitForWidget() {} bool OwnsSubtreeStyling() {}\n",
        ),
        (
            "style consumer missing from Apply",
            "Private/UI/GV2LegacyPresentationApplyAdapter.cpp",
            "FPreparedCentralStyleOperation& Op; }, Operation);\n",
        ),
    ):
        mutated = dict(clean)
        mutated[path] = source
        if not find_violations(mutated, synthetic_roles):
            print(f"FAILED: gate accepted synthetic violation: {label}")
            return False

    # PSC-10B: the payload variant -- not the interface -- enumerates Apply. A role without a
    # branch, and a branch without a role, must both fail.
    unhandled = dict(clean)
    unhandled["Private/UI/GV2LegacyPresentationApplyAdapter.cpp"] = (
        "FPreparedCentralStyleOperation& Op; Cast<UGV2StyledWidget>(Widget); }, Operation);\n"
    )
    if not find_violations(unhandled, synthetic_roles):
        print("FAILED: gate accepted a payload role with no Apply branch")
        return False
    if not find_violations(clean, synthetic_roles | {"FPreparedNeverHandledStyle"}):
        print("FAILED: gate accepted a newly declared payload role with no Apply branch")
        return False

    # A content-resolution capability inside a function that participates in central style
    # must be rejected -- and the same capability in a NON-participating function of the
    # same file must not be, or the rule would be a file-level ban in disguise.
    participating = dict(clean)
    participating["Private/UI/GV2Participant.cpp"] = (
        "void UGV2Foo::ApplyFooStyleValues()\n{\n    Class.LoadSynchronous();\n}\n"
    )
    if not find_violations(participating, synthetic_roles):
        print("FAILED: gate accepted a synchronous load inside a central-style value sink")
        return False
    unrelated = dict(clean)
    unrelated["Private/UI/GV2Participant.cpp"] = (
        "void UGV2Foo::ApplyFooStyleValues()\n{\n    Sink();\n}\n"
        "void UGV2Foo::LoadPicture()\n{\n    Class.LoadSynchronous();\n}\n"
    )
    if find_violations(unrelated, synthetic_roles):
        print("FAILED: gate flagged a load in a function that does not participate in central style")
        return False

    print("SUCCESS: central-style/runtime-authority boundary rejects every synthetic legacy path")
    return True


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return 0 if run_self_test() else 1
    errors = find_violations(production_sources())
    if errors:
        print("FAILED: PSC-10B runtime boundary violations:\n" + "\n".join(errors))
        return 1
    print("SUCCESS: central style and late RichText effects have no runtime authority pull path")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
