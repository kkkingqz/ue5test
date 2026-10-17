#!/usr/bin/env python3
"""PSC-14: every widget source that performs canonical viewport-dependent presentation
must implement the prepared viewport-refresh role. A stateful CommonUI button must also
restore its prepared presentation from the one native state-style callback.

The set is derived from production .cpp files, not maintained as a class list. A class is
in scope when one of its member functions uses the shared text-apply helper or one of the
prepared viewport scale evaluators. The independent side is the matching public class
declaration plus its concrete refresh implementation. The runtime automation test covers
the separate product path: Engine resize event -> committed screen -> physical font change.

This source gate is intentionally secondary. A future widget that invents a different
scaling API can evade these markers; the existing layout source audit and code review must
reject that parallel mechanism rather than treating this script as a complete UE API model.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# Both UI source roots, not just the apply module's. UGV2RecoveryScreenWidget lives in the
# GV2 module and performs viewport-dependent presentation, so scanning only
# GV2PresentationApply left the one widget outside the prepared pipeline also outside the
# gate meant to enumerate such widgets. Each entry pairs a private root with the public root
# its headers live in and the API macro those headers use.
SCAN_ROOTS = (
    (
        REPO_ROOT / "Source" / "GV2PresentationApply" / "Private" / "UI",
        REPO_ROOT / "Source" / "GV2PresentationApply" / "Public" / "UI",
    ),
    (
        REPO_ROOT / "Source" / "GV2" / "Private" / "UI",
        REPO_ROOT / "Source" / "GV2" / "Public" / "UI",
    ),
)
API_MACRO_PATTERN = r"(?:GV2PRESENTATIONAPPLY_API|GV2_API)"

CANONICAL_SCALE_MARKERS = (
    "FGV2WidgetTextApply::Apply(",
    "FGV2WidgetTextApply::ApplyRichText(",
    "EvaluatePreparedFontSize(",
    "EvaluatePreparedViewportScale(",
    # The Theme's own curve entry point. Its only remaining callers are inside the Theme
    # implementation itself, which owns no widget class and is therefore skipped by the
    # owner rule below -- but a future widget reaching for it is caught here rather than
    # becoming a second scaling path the way the recovery screen was.
    "EvaluateTextScale(",
)

# Widened from WidgetBase to also accept a plain Widget suffix: UGV2RecoveryScreenWidget is
# a widget that scales, and the old pattern could not name it. A file that uses a marker but
# declares NO widget owner (UGV2UiTheme.cpp, which defines the curve) is skipped rather than
# reported -- the limit this leaves is that a widget class named without either suffix would
# also be skipped, which is the same hole the WidgetBase-only pattern already had and is why
# this gate stays secondary to code review.
OWNER_PATTERN = re.compile(r"\b(UGV2[A-Za-z0-9_]*Widget(?:Base)?)::[A-Za-z0-9_]+\s*\(")


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def derive_scaled_widget_sources(sources: dict[str, str]) -> dict[str, str]:
    derived: dict[str, str] = {}
    for filename, source in sources.items():
        stripped = strip_comments(source)
        if not any(marker in stripped for marker in CANONICAL_SCALE_MARKERS):
            continue
        owners = set(OWNER_PATTERN.findall(stripped))
        if not owners:
            continue
        if len(owners) != 1:
            raise ValueError(
                f"{filename}: viewport-dependent source must have exactly one widget owner; "
                f"found {sorted(owners)}"
            )
        derived[next(iter(owners))] = filename
    return derived


def find_violations(private_sources: dict[str, str], public_headers: dict[str, str]) -> list[str]:
    errors: list[str] = []
    try:
        derived = derive_scaled_widget_sources(private_sources)
    except ValueError as error:
        return [str(error)]
    if not derived:
        return ["viewport-dependent widget enumerator produced an empty set"]

    for class_name, source_name in sorted(derived.items()):
        header_name = class_name.removeprefix("U") + ".h"
        header = strip_comments(public_headers.get(header_name, ""))
        if not header:
            errors.append(f"{source_name}: matching public header {header_name} was not found")
            continue
        class_match = re.search(
            rf"class\s+{API_MACRO_PATTERN}\s+{re.escape(class_name)}\b(?P<body>.*?\n\}};)",
            header,
            flags=re.DOTALL,
        )
        if class_match is None:
            errors.append(f"{header_name}: declaration for {class_name} was not found")
            continue
        class_body = class_match.group(0)
        if "public IGV2PreparedViewportRefreshTarget" not in class_body:
            errors.append(
                f"{header_name}: {class_name} performs viewport-dependent presentation but "
                "does not implement IGV2PreparedViewportRefreshTarget"
            )
        definition = re.compile(
            rf"\b{re.escape(class_name)}::RefreshPreparedViewportPresentation\s*\(")
        if definition.search(strip_comments(private_sources[source_name])) is None:
            errors.append(
                f"{source_name}: {class_name} declares viewport-dependent presentation but "
                "has no concrete RefreshPreparedViewportPresentation implementation"
            )

        if re.search(r":\s*public\s+UCommonButtonBase\b", class_body):
            if "NativeOnCurrentTextStyleChanged() override" not in class_body:
                errors.append(
                    f"{header_name}: {class_name} is a viewport-scaled CommonUI button but "
                    "does not override NativeOnCurrentTextStyleChanged"
                )
            source = strip_comments(private_sources[source_name])
            callback = re.search(
                rf"\b{re.escape(class_name)}::NativeOnCurrentTextStyleChanged\s*\(\s*\)\s*"
                rf"\{{(?P<body>.*?)\n\}}",
                source,
                flags=re.DOTALL,
            )
            if callback is None or "RestorePreparedLabelPresentation()" not in callback.group("body"):
                errors.append(
                    f"{source_name}: {class_name} does not restore prepared presentation "
                    "from the CommonUI state-style callback"
                )
    return errors


def repository_sources() -> tuple[dict[str, str], dict[str, str]]:
    # Every .cpp/.h under the UI roots, not just *WidgetBase.*: the filename glob was a
    # second naming restriction on top of the owner pattern, and it is what kept
    # GV2RecoveryScreenWidget.cpp out of the scan. Which files are IN SCOPE is decided by
    # the scale markers and the owner rule, not by what a file is called.
    private: dict[str, str] = {}
    public: dict[str, str] = {}
    for private_root, public_root in SCAN_ROOTS:
        for path in sorted(private_root.glob("*.cpp")):
            private[path.name] = path.read_text(encoding="utf-8")
        for path in sorted(public_root.glob("*.h")):
            public[path.name] = path.read_text(encoding="utf-8")
    return private, public


def run_self_test() -> bool:
    print("[*] Running validate_viewport_refresh_coverage self-test...")
    private, public = repository_sources()
    if errors := find_violations(private, public):
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    synthetic_private = {
        "GV2FutureWidgetBase.cpp": """
void UGV2FutureWidgetBase::ApplyText()
{
    FGV2WidgetTextApply::Apply(Label, Text);
}
"""
    }
    synthetic_public = {
        "GV2FutureWidgetBase.h": """
class GV2PRESENTATIONAPPLY_API UGV2FutureWidgetBase : public UCommonUserWidget
{
};
"""
    }
    errors = find_violations(synthetic_private, synthetic_public)
    if not any("does not implement IGV2PreparedViewportRefreshTarget" in error for error in errors):
        print(f"FAILED: gate accepted a scaled widget without the refresh role: {errors}")
        return False

    synthetic_public["GV2FutureWidgetBase.h"] = """
class GV2PRESENTATIONAPPLY_API UGV2FutureWidgetBase
    : public UCommonUserWidget
    , public IGV2PreparedViewportRefreshTarget
{
};
"""
    errors = find_violations(synthetic_private, synthetic_public)
    if not any("no concrete RefreshPreparedViewportPresentation" in error for error in errors):
        print(f"FAILED: gate accepted a refresh role with no implementation: {errors}")
        return False

    synthetic_private["GV2FutureWidgetBase.cpp"] = """
void UGV2FutureWidgetBase::ApplyText()
{
    FGV2WidgetTextApply::Apply(Label, Text);
}
void UGV2FutureWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    FGV2WidgetTextApply::RefreshFont(Label, Text, ViewportHeight);
}
"""
    synthetic_public["GV2FutureWidgetBase.h"] = """
class GV2PRESENTATIONAPPLY_API UGV2FutureWidgetBase
    : public UCommonButtonBase
    , public IGV2PreparedViewportRefreshTarget
{
    virtual void RefreshPreparedViewportPresentation(float ViewportHeight) override;
};
"""
    errors = find_violations(synthetic_private, synthetic_public)
    if not any("does not override NativeOnCurrentTextStyleChanged" in error for error in errors):
        print(f"FAILED: gate accepted a scaled CommonUI button without the state-style hook: {errors}")
        return False

    synthetic_private["GV2FutureWidgetBase.cpp"] += """
void UGV2FutureWidgetBase::NativeOnCurrentTextStyleChanged()
{
    Super::NativeOnCurrentTextStyleChanged();
}
"""
    synthetic_public["GV2FutureWidgetBase.h"] = synthetic_public["GV2FutureWidgetBase.h"].replace(
        "virtual void RefreshPreparedViewportPresentation(float ViewportHeight) override;",
        "virtual void RefreshPreparedViewportPresentation(float ViewportHeight) override;\n"
        "    virtual void NativeOnCurrentTextStyleChanged() override;",
    )
    errors = find_violations(synthetic_private, synthetic_public)
    if not any("does not restore prepared presentation" in error for error in errors):
        print(f"FAILED: gate accepted a state-style hook that discards prepared presentation: {errors}")
        return False

    print("SUCCESS: negative fixtures are rejected and repository coverage is complete")
    return True


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return 0 if run_self_test() else 1
    private, public = repository_sources()
    errors = find_violations(private, public)
    if errors:
        print("FAILED: PSC-14 viewport refresh coverage violations:\n" + "\n".join(errors))
        return 1
    derived = derive_scaled_widget_sources(private)
    print(
        "SUCCESS: every source-derived viewport-dependent widget implements refresh: "
        + ", ".join(sorted(derived))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
