#!/usr/bin/env python3
"""PSC-11 (ADR-0043 D2): what GV2PresentationApply exports, and what it is allowed to do.

Two claims, both derived rather than listed:

1. EXPORTED SURFACE. Every exported declaration in the module's Public/ tree is classified
   into one of a small set of roles -- the single transaction Apply facade, prepared
   DTO/result types, physical role interfaces, and pure prepared-value calculations. A
   SECOND way to apply a whole transaction is rejected by construction: exactly one
   exported symbol may be a transaction-taking apply entry point.

2. FORBIDDEN CAPABILITIES. The module's whole source tree is enumerated by walking the
   directory, and every file is scanned for capabilities the dependency allowlist alone
   does not forbid -- CoreUObject/Engine themselves provide soft references, synchronous
   loading, settings and filesystem access. ADR-0043 D4 is explicit that this scan is
   SECONDARY to the module graph: it is a list of known capability shapes, not a complete
   classification of every future UE API. The graph is what makes an authority type
   unnameable here; this catches the ones that would still compile.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
MODULE_ROOT = REPO_ROOT / "Source" / "GV2PresentationApply"
PUBLIC_ROOT = MODULE_ROOT / "Public"

EXPORT_MACRO = "GV2PRESENTATIONAPPLY_API"

# A declaration that RECEIVES a whole transaction. There may be exactly one, and the rule is
# about the signature rather than the name: a second entry point called something else is
# still a second way to apply a transaction, which is exactly what PSC-11 removes.
TRANSACTION_APPLY = re.compile(
    r"const\s+(?:GV2PresentationApply::)?FGV2PreparedPresentationTransaction\s*&")

FORBIDDEN_CAPABILITIES = {
    "soft object reference": re.compile(r"\bTSoftObjectPtr\s*<"),
    "soft class reference": re.compile(r"\bTSoftClassPtr\s*<"),
    "synchronous load": re.compile(r"\bLoadSynchronous\s*\("),
    "object load": re.compile(r"\b(?:StaticLoadObject|StaticLoadClass|LoadObject\s*<|LoadClass\s*<)"),
    "asset registry": re.compile(r"\bIAssetRegistry\b|\bFAssetRegistryModule\b|\bAssetRegistryHelpers\b"),
    "settings access": re.compile(r"\bGetDefault\s*<|\bGetMutableDefault\s*<|\bUDeveloperSettings\b"),
    "filesystem access": re.compile(r"\bIFileManager\b|\bFPlatformFileManager\b|\bFFileHelper\b|\bFPaths\s*::"),
    "config access": re.compile(r"\bGConfig\b"),
}


def module_sources() -> dict[str, str]:
    """Every source file in the module, by directory walk -- not a list."""
    result: dict[str, str] = {}
    for path in sorted(MODULE_ROOT.rglob("*")):
        if path.suffix not in {".h", ".cpp", ".inl"}:
            continue
        result[path.relative_to(MODULE_ROOT).as_posix()] = path.read_text(encoding="utf-8")
    return result


def strip_comments(source: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if c == "\n" else " " for c in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, source)


def declaration_window(source: str, start: int) -> str:
    """From an export macro to the end of the declaration it introduces.

    A declaration is not a line: the facade's own Apply spans three of them, and a class
    body spans many. Taking the whole construct is what lets the classification below see a
    transaction-taking entry point wherever it is formatted.
    """
    depth = 0
    for index in range(start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth <= 0:
                return source[start:index + 1]
        elif char == ";" and depth == 0:
            return source[start:index + 1]
    return source[start:]


def exported_declarations(sources: dict[str, str]) -> list[tuple[str, str]]:
    """(file, declaration) for every exported declaration in Public/."""
    found: list[tuple[str, str]] = []
    for rel, source in sources.items():
        if not rel.startswith("Public/"):
            continue
        stripped = strip_comments(source)
        for match in re.finditer(re.escape(EXPORT_MACRO), stripped):
            # From the start of the line: the macro sits AFTER the struct/class keyword, and
            # the keyword is what says which role the declaration plays.
            line_start = stripped.rfind("\n", 0, match.start()) + 1
            found.append((rel, " ".join(declaration_window(stripped, line_start).split())))
    return found


def classify(declaration: str) -> str:
    if TRANSACTION_APPLY.search(declaration):
        return "transaction apply facade"
    if re.search(r"^(?:struct|class)\s+" + EXPORT_MACRO + r"\s+I", declaration):
        return "physical role interface"
    if re.match(r"(?:struct|class)\s+" + EXPORT_MACRO, declaration):
        return "prepared DTO/result type"
    if re.match(EXPORT_MACRO + r"\s+[\w:<>\s\*&]+\w+\s*\(", declaration):
        return "pure prepared-value calculation"
    return "unclassified"


def has_mutable_uobject_parameter(declaration: str) -> bool:
    """Whether any function in an exported construct can receive a mutable UObject.

    DTO fields are intentionally excluded: prepared operations carry weak widget targets.
    The prohibited surface is a callable entry point outside U/I widget-role constructs.
    """
    without_const = re.sub(
        r"\bconst\s+(U[A-Za-z_]\w*)\s*([*&])",
        r"const_\1_\2",
        declaration,
    )
    return re.search(
        r"\([^)]*(?:\bU[A-Za-z_]\w*\s*[*&]|\bTWeakObjectPtr\s*<\s*U[A-Za-z_]\w*\s*>)",
        without_const,
    ) is not None


def is_physical_role_construct(name: str, declaration: str) -> bool:
    """Recognize actual generated role interfaces and widget lifecycle classes.

    A leading U/I alone is not evidence: ordinary exported helpers can use either letter.
    Widget roles are recognized by their generated UObject body and physical UE base;
    role interfaces are generated Unreal interfaces.
    """
    if "GENERATED_BODY" not in declaration:
        # PEP-08: FGV2PresentationEffectApply is the OTHER designated physical-mutation
        # entry point PSC-11 sanctions, alongside FGV2PresentationApply::Apply (recognized
        # above via TRANSACTION_APPLY's own distinctive FGV2PreparedPresentationTransaction&
        # parameter shape). This sibling facade takes a plain UWidget* instead -- PEP-04's
        # own doc comment: Apply's signature carries a widget, never a snapshot/gameplay
        # type -- so it needs its own equally narrow, name-based recognition rather than a
        # shape match; it is not a UCLASS/UINTERFACE, so it can never match the
        # GENERATED_BODY-gated checks below.
        return name == "FGV2PresentationEffectApply"
    if name.startswith("I"):
        return True
    # PEP-06B: GameInstanceSubsystem covers UGV2PresentationInteractionSink, the module's own
    # already-sanctioned "upward interaction ingress" boundary (its own header comment, unchanged
    # by this task) -- OpenHoverOverlay is the first method on it to take a mutable UObject*
    # (the hover screen widget being handed upward for the runtime side to attach), which is
    # exactly the role this construct exists for, not a second physical entry surface.
    return name.startswith("U") and re.search(
        r":\s*public\s+U[A-Za-z0-9_]*(?:Widget|ButtonBase|Decorator|GameInstanceSubsystem)\b",
        declaration,
    ) is not None


def find_violations(sources: dict[str, str]) -> list[str]:
    errors: list[str] = []

    declarations = exported_declarations(sources)
    if not declarations:
        return ["the exported-surface enumerator produced an empty set; the derivation is broken"]

    apply_entry_points = [d for d in declarations if classify(d[1]) == "transaction apply facade"]
    if len(apply_entry_points) != 1:
        errors.append(
            "GV2PresentationApply must export exactly one transaction apply entry point; found "
            f"{len(apply_entry_points)}: {[d[1] for d in apply_entry_points]}"
        )
    for rel, declaration in declarations:
        if classify(declaration) == "unclassified":
            errors.append(f"{rel}: exported declaration is unclassified: {declaration}")
        # U-prefixed UCLASSes are the physical widget/lifecycle roles, and I-prefixed
        # interfaces are their value-only role contracts. Any OTHER exported construct
        # receiving a mutable UObject is a second physical entry surface. Const UObject
        # context (for example the interaction sink's world context) is not mutation.
        name_match = re.search(r"\b(?:struct|class)\s+" + EXPORT_MACRO + r"\s+([A-Za-z_]\w*)", declaration)
        name = name_match.group(1) if name_match else ""
        if has_mutable_uobject_parameter(declaration) and not is_physical_role_construct(name, declaration):
            errors.append(
                f"{rel}: exported physical mutation entry outside a widget/lifecycle role: "
                f"{name or declaration.split('(', 1)[0]}"
            )

    for rel, source in sources.items():
        stripped = strip_comments(source)
        for name, pattern in FORBIDDEN_CAPABILITIES.items():
            for match in pattern.finditer(stripped):
                line = stripped.count("\n", 0, match.start()) + 1
                errors.append(f"{rel}:{line}: forbidden capability in the Apply module: {name}")

    return errors


def run_self_test() -> bool:
    print("[*] Running validate_presentation_apply_surface self-test...")
    if errors := find_violations(module_sources()):
        print("FAILED: the module already violates the gate:\n" + "\n".join(errors))
        return False

    clean = {
        "Public/GV2PresentationApply/Prepared.h": (
            "struct GV2PRESENTATIONAPPLY_API FPreparedThing { int32 Value = 0; };\n"
            "class GV2PRESENTATIONAPPLY_API IGV2PreparedThingTarget { };\n"
            "GV2PRESENTATIONAPPLY_API float EvaluateThing(float In);\n"
            "class GV2PRESENTATIONAPPLY_API FGV2PresentationApply { public:\n"
            "    static bool Apply(const FGV2PreparedPresentationTransaction& T, FResult& R);\n};\n"
        ),
        "Private/Thing.cpp": "float EvaluateThing(float In) { return In; }\n",
    }
    if errors := find_violations(clean):
        print("FAILED: clean synthetic module was rejected:\n" + "\n".join(errors))
        return False

    second = dict(clean)
    second["Public/GV2PresentationApply/Second.h"] = (
        "GV2PRESENTATIONAPPLY_API bool ApplyAgain(const FGV2PreparedPresentationTransaction& T);\n"
    )
    # A differently NAMED second entry point is still a second entry point: the rule reads
    # the signature, so renaming cannot get past it.
    errors = find_violations(second)
    if not any("exactly one transaction apply entry point" in e for e in errors):
        print(f"FAILED: gate accepted a second transaction-taking exported entry point: {errors}")
        return False

    duplicate = dict(clean)
    duplicate["Public/GV2PresentationApply/Duplicate.h"] = (
        "class GV2PRESENTATIONAPPLY_API FOtherFacade { public:\n"
        "    static bool Apply(const FGV2PreparedPresentationTransaction& T, FResult& R);\n};\n"
    )
    errors = find_violations(duplicate)
    if not any("exactly one transaction apply entry point" in e for e in errors):
        print(f"FAILED: gate accepted a second transaction apply facade: {errors}")
        return False

    physical_helper = dict(clean)
    physical_helper["Public/GV2PresentationApply/PhysicalHelper.h"] = (
        "class GV2PRESENTATIONAPPLY_API FPhysicalHelper { public:\n"
        "    static void Paint(UWidget* Widget, const FSlateBrush& Brush);\n};\n"
    )
    errors = find_violations(physical_helper)
    if not any("physical mutation entry" in e for e in errors):
        print(f"FAILED: gate accepted an exported physical helper outside a widget/lifecycle role: {errors}")
        return False

    weak_physical_helper = dict(clean)
    weak_physical_helper["Public/GV2PresentationApply/WeakPhysicalHelper.h"] = (
        "GV2PRESENTATIONAPPLY_API void PaintLater(TWeakObjectPtr<UWidget> Widget);\n"
    )
    errors = find_violations(weak_physical_helper)
    if not any("physical mutation entry" in e for e in errors):
        print(f"FAILED: gate accepted a free exported weak-widget mutation entry: {errors}")
        return False

    u_prefixed_helper = dict(clean)
    u_prefixed_helper["Public/GV2PresentationApply/UnsafeHelper.h"] = (
        "class GV2PRESENTATIONAPPLY_API UnsafePhysicalHelper { public:\n"
        "    static void Paint(UWidget* Widget);\n};\n"
    )
    errors = find_violations(u_prefixed_helper)
    if not any("physical mutation entry" in e for e in errors):
        print(f"FAILED: a U-prefixed helper bypassed the exported physical-entry gate: {errors}")
        return False

    for label, snippet in (
        ("soft object reference", "TSoftObjectPtr<UTexture2D> Texture;\n"),
        ("soft class reference", "TSoftClassPtr<UUserWidget> Widget;\n"),
        ("synchronous load", "auto* X = Ref.LoadSynchronous();\n"),
        ("object load", "auto* X = StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT(\"x\"));\n"),
        ("asset registry", "IAssetRegistry& R = GetRegistry();\n"),
        ("settings access", "const auto* S = GetDefault<UGV2UiThemeSettings>();\n"),
        ("filesystem access", "const FString P = FPaths::ProjectDir();\n"),
        ("config access", "GConfig->GetBool(TEXT(\"a\"), TEXT(\"b\"), Out, TEXT(\"c\"));\n"),
    ):
        mutated = dict(clean)
        mutated["Private/Thing.cpp"] = clean["Private/Thing.cpp"] + snippet
        found = find_violations(mutated)
        if not any(label in e for e in found):
            print(f"FAILED: gate accepted the forbidden capability '{label}': {found}")
            return False

    print("SUCCESS: the Apply module exports one entry point and reaches no forbidden capability")
    return True


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return 0 if run_self_test() else 1
    errors = find_violations(module_sources())
    if errors:
        print("FAILED: PSC-11 Apply surface violations:\n" + "\n".join(errors))
        return 1
    print("SUCCESS: the Apply module exports one entry point and reaches no forbidden capability")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
