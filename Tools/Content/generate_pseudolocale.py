#!/usr/bin/env python3
"""Generates pseudolocalized PO catalogs from source catalogs (e.g. en.po).
Expands text length by 25-30% with accented characters and bracket delimiters
to test UI layout resilience against string expansion without breaking format
specifiers or markup.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

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

# Regex to find placeholders like {arg}, {0}, <span ...>, </span>, \n, \t
PROTECTED_PATTERN = re.compile(
    r'(\{[a-zA-Z0-9_]+\}|\{\d+\}|<span[^>]*>|</span>|\\[nrt"\'\\]|%[0-9]*[a-zA-Z])'
)


def pseudolocalize_text(text: str, expansion_ratio: float = 0.3) -> str:
    """Transforms plain text into pseudolocalized text with ~30% expansion while preserving format tokens."""
    if not text:
        return text

    segments = []
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
        return f"[{body} {padding}]"
    return f"[{body}]"


def process_po_file(source_po: Path, target_po: Path, expansion_ratio: float = 0.3):
    content = source_po.read_text(encoding="utf-8")
    lines = content.splitlines()

    output_lines = []
    in_msgstr = False
    current_msgid = ""
    current_msgstr_lines = []

    for line in lines:
        if line.startswith("msgid "):
            current_msgid = line[6:].strip().strip('"')
            output_lines.append(line)
        elif line.startswith("msgstr "):
            if current_msgid == "":
                # Header
                output_lines.append(line)
            else:
                pseudo = pseudolocalize_text(current_msgid, expansion_ratio)
                output_lines.append(f'msgstr "{pseudo}"')
        elif line.startswith('"') and not in_msgstr:
            output_lines.append(line)
        else:
            output_lines.append(line)

    target_po.parent.mkdir(parents=True, exist_ok=True)
    target_po.write_text("\n".join(output_lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Generate pseudolocale PO catalogs.")
    parser.add_argument("package_root", type=Path, help="Path to package root")
    parser.add_argument("--source-locale", type=str, default="en", help="Source locale")
    parser.add_argument("--target-locale", type=str, default="qps-ploc", help="Target pseudolocale code")
    parser.add_argument("--expansion-ratio", type=float, default=0.3, help="Expansion fraction (e.g. 0.3 for +30%%)")

    args = parser.parse_args()
    loc_dir = args.package_root / "localization"
    source_po = loc_dir / f"{args.source_locale}.po"

    if not source_po.is_file():
        available = list(loc_dir.glob("*.po"))
        if available:
            source_po = sorted(available)[0]
        else:
            print(f"No PO catalogs found in {loc_dir}", file=sys.stderr)
            sys.exit(1)

    target_po = loc_dir / f"{args.target_locale}.po"
    process_po_file(source_po, target_po, args.expansion_ratio)
    print(f"Generated pseudolocale catalog: {target_po}")


if __name__ == "__main__":
    main()
