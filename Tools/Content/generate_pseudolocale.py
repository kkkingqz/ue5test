#!/usr/bin/env python3
"""Generates pseudolocalized PO catalogs from source catalogs (e.g. en.po, ru.po).
Expands text length by 25-30% with accented characters and bracket delimiters
to test UI layout resilience against string expansion without breaking format
specifiers or markup.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path
from typing import List, Tuple

# Character substitution map for pseudolocalization
CHAR_MAP = {
    'a': 'á', 'A': 'Á',
    'b': 'ḅ', 'B': 'Ḅ',
    'c': 'ç', 'C': 'Ç',
    'd': 'ḍ', 'D': 'Ḍ',
    'e': 'é', 'E': 'É',
    'f': 'ḟ', 'F': 'Ḟ',
    'g': 'ḡ', 'G': 'Ḡ',
    'h': 'ḥ', 'H': 'Ḥ',
    'i': 'í', 'I': 'Í',
    'j': 'ĵ', 'J': 'Ĵ',
    'k': 'ḳ', 'K': 'Ḳ',
    'l': 'ḷ', 'L': 'Ḷ',
    'm': 'ṃ', 'M': 'Ṃ',
    'n': 'ñ', 'N': 'Ñ',
    'o': 'ó', 'O': 'Ó',
    'p': 'ṕ', 'P': 'Ṕ',
    'q': 'q', 'Q': 'Q',
    'r': 'ṛ', 'R': 'Ṛ',
    's': 'š', 'S': 'Š',
    't': 'ṭ', 'T': 'Ṭ',
    'u': 'ú', 'U': 'Ú',
    'v': 'ṿ', 'V': 'Ṿ',
    'w': 'ŵ', 'W': 'Ŵ',
    'x': 'ẋ', 'X': 'Ẋ',
    'y': 'ý', 'Y': 'Ý',
    'z': 'ž', 'Z': 'Ž',
}

# Regex to find placeholders like {arg}, {0}, <span ...>, </span>, \n, \t, %d, %s
PROTECTED_PATTERN = re.compile(
    r'(\{[a-zA-Z0-9_]+\}|\{\d+\}|</?[a-zA-Z0-9_]+(?:\s+[^>]*)?>|\\[nrt"\'\\]|%[0-9.]*[a-zA-Z%])'
)


def validate_format_tokens(text: str) -> List[str]:
    """Validates that format tokens and markup in text are not malformed."""
    errors = []
    # Check unmatched braces '{' and '}'
    brace_depth = 0
    for i, ch in enumerate(text):
        if ch == '{':
            if brace_depth > 0:
                errors.append(f"nested or unclosed '{{' at index {i}")
            brace_depth += 1
        elif ch == '}':
            brace_depth -= 1
            if brace_depth < 0:
                errors.append(f"unexpected closing '}}' at index {i}")
                brace_depth = 0
    if brace_depth > 0:
        errors.append("unclosed '{' format token")

    # Check unclosed '<span' or markup tags
    if re.search(r'<[a-zA-Z/][^>]*$', text):
        errors.append("unclosed markup tag '<...'")

    return errors


def pseudolocalize_text(text: str, expansion_ratio: float = 0.3) -> str:
    """Transforms plain text into pseudolocalized text with ~30% expansion while preserving format tokens."""
    if not text:
        return text

    errors = validate_format_tokens(text)
    if errors:
        raise ValueError(f"Corrupted format tokens in source text '{text}': {'; '.join(errors)}")

    segments: List[Tuple[str, bool]] = []
    last_idx = 0

    for match in PROTECTED_PATTERN.finditer(text):
        if match.start() > last_idx:
            segments.append((text[last_idx:match.start()], False))
        segments.append((match.group(0), True))
        last_idx = match.end()

    if last_idx < len(text):
        segments.append((text[last_idx:], False))

    transformed = []
    for seg, is_protected in segments:
        if is_protected:
            transformed.append(seg)
        else:
            out_chars = []
            for c in seg:
                out_chars.append(CHAR_MAP.get(c, c))
            transformed.append("".join(out_chars))

    body = "".join(transformed)
    # Add expansion padding ~25-30%
    extra_len = int(len(body) * expansion_ratio)
    if extra_len > 0:
        padding = "~" * extra_len
        result = f"[{body} {padding}]"
    else:
        result = f"[{body}]"

    # Verify protected token preservation invariant
    src_tokens = PROTECTED_PATTERN.findall(text)
    dst_tokens = PROTECTED_PATTERN.findall(result)
    if src_tokens != dst_tokens:
        raise ValueError(
            f"Protected tokens mismatch after pseudolocalization: source={src_tokens}, result={dst_tokens}"
        )

    return result


def process_po_file(source_po: Path, target_po: Path, expansion_ratio: float = 0.3):
    content = source_po.read_text(encoding="utf-8")
    lines = content.splitlines()

    output_lines = []
    in_msgstr = False
    current_msgid = ""
    current_msgctxt = ""

    for line_idx, line in enumerate(lines, 1):
        if line.startswith("msgctxt "):
            current_msgctxt = line[8:].strip().strip('"')
            output_lines.append(line)
        elif line.startswith("msgid "):
            current_msgid = line[6:].strip().strip('"')
            output_lines.append(line)
        elif line.startswith("msgstr "):
            if current_msgid == "":
                # Header
                output_lines.append(line)
            else:
                try:
                    pseudo = pseudolocalize_text(current_msgid, expansion_ratio)
                except ValueError as e:
                    raise ValueError(f"{source_po}:{line_idx} (context={current_msgctxt}): {e}") from e
                output_lines.append(f'msgstr "{pseudo}"')
        elif line.startswith('"') and not in_msgstr:
            output_lines.append(line)
        else:
            output_lines.append(line)

    target_po.parent.mkdir(parents=True, exist_ok=True)
    target_po.write_text("\n".join(output_lines) + "\n", encoding="utf-8")


def run_self_test() -> bool:
    """Runs comprehensive self-tests on token preservation, expansion, idempotence, and error rejection."""
    print("[*] Running generate_pseudolocale self-test...")

    # 1. Format token preservation and ~30% expansion
    src = "Welcome {user}! You have <span color=\"gold\">{count} gold</span>.\\nNext level at %d XP."
    pseudo = pseudolocalize_text(src, 0.3)
    assert "{user}" in pseudo, "Placeholder {user} must be preserved"
    assert "{count}" in pseudo, "Placeholder {count} must be preserved"
    assert '<span color="gold">' in pseudo, "Markup <span...> must be preserved"
    assert "</span>" in pseudo, "Closing </span> must be preserved"
    assert "\\n" in pseudo, "Escape \\n must be preserved"
    assert "%d" in pseudo, "Format %d must be preserved"
    assert pseudo.startswith("["), "Must start with ["
    assert pseudo.endswith("]"), "Must end with ]"
    assert len(pseudo) >= int(len(src) * 1.25), f"Must be expanded by >= 25%: src len={len(src)}, dst len={len(pseudo)}"

    # 2. Idempotence test
    pseudo2 = pseudolocalize_text(src, 0.3)
    assert pseudo == pseudo2, "Repeated pseudolocalization must produce identical output"

    # 3. Negative test: Corrupted unclosed format token is rejected
    corrupted_cases = [
        "Welcome {user with unclosed brace",
        "Hello {broken {nested}} token",
        "Unexpected } closing brace",
        "Unclosed tag <span color='red' text",
    ]
    for bad_text in corrupted_cases:
        try:
            pseudolocalize_text(bad_text, 0.3)
            print(f"FAILED: Expected ValueError for corrupted text '{bad_text}'", file=sys.stderr)
            return False
        except ValueError:
            pass  # Expected rejection

    # 4. End-to-end PO file processing with temporary files
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_dir_path = Path(tmpdir)
        source_po_path = tmp_dir_path / "source.po"
        source_po_path.write_text(
            'msgid ""\n'
            'msgstr ""\n'
            '"Language: en\\n"\n'
            '\n'
            'msgctxt "test.item.sword"\n'
            'msgid "Iron Sword (+{power} ATK)"\n'
            'msgstr "Iron Sword (+{power} ATK)"\n'
            '\n'
            'msgctxt "test.welcome"\n'
            'msgid "Hello <b>{name}</b>!\\nWelcome to the shop."\n'
            'msgstr ""\n',
            encoding="utf-8"
        )
        target_po_path = tmp_dir_path / "qps-ploc.po"
        process_po_file(source_po_path, target_po_path, 0.3)

        assert target_po_path.is_file(), "Target PO file must be created"
        target_content = target_po_path.read_text(encoding="utf-8")
        assert "+{power} ATK" in target_content
        assert "<b>{name}</b>" in target_content
        assert "\\n" in target_content

        # 5. Negative test on PO file containing corrupted token
        bad_po_path = tmp_dir_path / "corrupted.po"
        bad_po_path.write_text(
            'msgid ""\n'
            'msgstr ""\n'
            '\n'
            'msgctxt "test.bad"\n'
            'msgid "Corrupted {token without closing"\n'
            'msgstr ""\n',
            encoding="utf-8"
        )
        try:
            process_po_file(bad_po_path, tmp_dir_path / "out_bad.po", 0.3)
            print("FAILED: Expected process_po_file to reject corrupted PO", file=sys.stderr)
            return False
        except ValueError:
            pass  # Expected rejection

    print("[+] All generate_pseudolocale self-tests passed successfully.")
    return True


def main():
    parser = argparse.ArgumentParser(description="Generate pseudolocale PO catalogs.")
    parser.add_argument("package_root", type=Path, nargs="?", default=None, help="Path to package root")
    parser.add_argument("--source-locale", type=str, default="en", help="Source locale")
    parser.add_argument("--target-locale", type=str, default="qps-ploc", help="Target pseudolocale code")
    parser.add_argument("--expansion-ratio", type=float, default=0.3, help="Expansion fraction (e.g. 0.3 for +30%%)")
    parser.add_argument("--output-dir", type=Path, default=None, help="Optional output directory")
    parser.add_argument("--output-po", type=Path, default=None, help="Optional explicit output PO file path")
    parser.add_argument("--self-test", action="store_true", help="Run self-tests and exit")

    args = parser.parse_args()

    if args.self_test:
        sys.exit(0 if run_self_test() else 1)

    if not args.package_root:
        parser.print_help(sys.stderr)
        sys.exit(2)

    loc_dir = args.package_root / "localization"
    source_po = loc_dir / f"{args.source_locale}.po"

    if not source_po.is_file():
        available = sorted(list(loc_dir.glob("*.po")))
        if available:
            source_po = available[0]
        else:
            print(f"No PO catalogs found in {loc_dir}", file=sys.stderr)
            sys.exit(1)

    if args.output_po:
        target_po = args.output_po
    elif args.output_dir:
        target_po = args.output_dir / f"{args.target_locale}.po"
    else:
        target_po = loc_dir / f"{args.target_locale}.po"

    try:
        process_po_file(source_po, target_po, args.expansion_ratio)
        print(f"Generated pseudolocale catalog: {target_po}")
    except ValueError as err:
        print(f"Error generating pseudolocale: {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
