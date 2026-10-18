import json, re, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from hunter_texts import FILES

out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent
(out / "definitions").mkdir(parents=True, exist_ok=True)
(out / "localization").mkdir(parents=True, exist_ok=True)

def po_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'

po = ['msgid ""', 'msgstr ""', '"Language: en\\n"', '"MIME-Version: 1.0\\n"',
      '"Content-Type: text/plain; charset=UTF-8\\n"', '"Plural-Forms: nplurals=2; plural=(n != 1);\\n"', '']
for name, rows in FILES.items():
    lines = ["{", "  schema_version: 1,", '  type: "text",', "  definitions: ["]
    po.append("# ---- " + name + " ----")
    po.append("")
    for tid, en, ru, note in rows:
        if note:
            lines.append("    // " + note)
        lines.append("    {")
        lines.append("      id: " + json.dumps(tid) + ",")
        lines.append("      data: { source_message: " + json.dumps(ru, ensure_ascii=False) + " },")
        lines.append("    },")
        if note:
            po.append("#. " + note)
        po.append("msgctxt " + po_str(tid))
        po.append("msgid " + po_str(ru))
        po.append("msgstr " + po_str(en))
        po.append("")
    lines += ["  ],", "}", ""]
    (out / "definitions" / (name + ".json5")).write_text("\n".join(lines), encoding="utf-8")
(out / "localization" / "en.po").write_text("\n".join(po), encoding="utf-8")
