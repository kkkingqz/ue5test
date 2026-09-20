import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UECTL = ROOT / "uectl.py"


def test_help_works():
    p = subprocess.run([sys.executable, str(UECTL), "--help"], text=True, capture_output=True)
    assert p.returncode == 0, p.stderr
    assert "editor" in p.stdout
    assert "build" in p.stdout
