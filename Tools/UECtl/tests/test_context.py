from pathlib import Path
import importlib.util
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("uectl", ROOT / "uectl.py")
uectl = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = uectl
spec.loader.exec_module(uectl)


def test_detects_main_project_context():
    ctx = uectl.detect_context(Path("/projects/MyGame/Source/Foo"), Path("/projects"), Path("/worktrees"))
    assert ctx.project == "MyGame"
    assert ctx.workspace == "main"


def test_detects_worktree_context():
    ctx = uectl.detect_context(Path("/worktrees/MyGame/codex-a17f/Plugins/MyTools"), Path("/projects"), Path("/worktrees"))
    assert ctx.project == "MyGame"
    assert ctx.workspace == "codex-a17f"
