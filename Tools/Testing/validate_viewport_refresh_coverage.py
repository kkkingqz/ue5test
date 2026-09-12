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
PRIVATE_UI = REPO_ROOT / "Source" / "GV2PresentationApply" / "Private" / "UI"
PUBLIC_UI = REPO_ROOT / "Source" / "GV2PresentationApply" / "Public" / "UI"

CANONICAL_SCALE_MARKERS = (
    "FGV2WidgetTextApply::Apply(",
    "FGV2WidgetTextApply::ApplyRichText(",
    "EvaluatePreparedFontSize(",
    "EvaluatePreparedViewportScale(",
)
OWNER_PATTERN = re.compile(r"\b(UGV2[A-Za-z0-9_]+WidgetBase)::[A-Za-z0-9_]+\s*\(")


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
        if len(owners) != 1:
            raise ValueError(
                f"{filename}: viewport-dependent source must have exactly one WidgetBase owner; "
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
            rf"class\s+GV2PRESENTATIONAPPLY_API\s+{re.escape(class_name)}\b(?P<body>.*?\n\}};)",
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
    private = {
        path.name: path.read_text(encoding="utf-8")
        for path in sorted(PRIVATE_UI.glob("*WidgetBase.cpp"))
    }
    public = {
        path.name: path.read_text(encoding="utf-8")
        for path in sorted(PUBLIC_UI.glob("*WidgetBase.h"))
    }
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
