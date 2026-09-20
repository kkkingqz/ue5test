import importlib.util
import sys
import json
import os
import signal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ue_editor_control", ROOT / "ue_editor_control.py")
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
try:
    spec.loader.exec_module(module)
except FileNotFoundError:
    module = None


def test_editor_control_module_exists():
    assert module is not None


def test_project_name_validation_blocks_path_escape():
    assert module.valid_project_name("MyGame")
    assert module.valid_project_name("game-2_test")
    assert not module.valid_project_name("../etc")
    assert not module.valid_project_name("foo/bar")


def test_resolve_project_requires_one_root_uproject(tmp_path):
    root = tmp_path / "projects"
    project = root / "MyGame"
    project.mkdir(parents=True)
    (project / "MyGame.uproject").write_text("{}")
    resolved = module.resolve_project(root, "MyGame")
    assert resolved == (project.resolve(), (project / "MyGame.uproject").resolve())


def test_controller_builds_fixed_argv_without_shell(tmp_path, monkeypatch):
    project_root = tmp_path / "projects"
    project = project_root / "MyGame"
    project.mkdir(parents=True)
    uproject = project / "MyGame.uproject"
    uproject.write_text("{}")
    engine_root = tmp_path / "ue"
    binary = engine_root / "Engine/Binaries/Linux/UnrealEditor"
    binary.parent.mkdir(parents=True)
    binary.write_text("binary")
    log_file = tmp_path / "editor.log"

    calls = []

    class Proc:
        pid = 4321
        def poll(self): return None

    def fake_popen(argv, **kwargs):
        calls.append((argv, kwargs))
        return Proc()

    monkeypatch.setattr(module.subprocess, "Popen", fake_popen)
    ctl = module.EditorController(project_root, engine_root, log_file, stop_timeout=0.01)
    result = ctl.start("MyGame")
    assert result["state"] == "running"
    argv, kwargs = calls[0]
    assert argv == [str(binary.resolve()), str(uproject.resolve())]
    assert kwargs["start_new_session"] is True
    assert "shell" not in kwargs or kwargs["shell"] is False



def _make_editor_tree(tmp_path):
    project_root = tmp_path / "projects"
    project = project_root / "MyGame"
    project.mkdir(parents=True)
    uproject = project / "MyGame.uproject"
    uproject.write_text("{}")
    engine_root = tmp_path / "ue"
    binary = engine_root / "Engine/Binaries/Linux/UnrealEditor"
    binary.parent.mkdir(parents=True)
    binary.write_text("binary")
    return project_root, project, uproject, engine_root, binary


def test_missing_project_is_not_found(tmp_path):
    root = tmp_path / "projects"
    root.mkdir()
    try:
        module.resolve_project(root, "Missing")
    except module.NotFound:
        pass
    else:
        raise AssertionError("expected NotFound")


def test_start_rejects_process_that_exits_during_startup(tmp_path, monkeypatch):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    class Proc:
        pid = 4321
        returncode = 42
        def poll(self): return 42
    monkeypatch.setattr(module.subprocess, "Popen", lambda *a, **k: Proc())
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    try:
        ctl.start("MyGame")
    except module.StartFailed as exc:
        assert "42" in str(exc)
    else:
        raise AssertionError("expected StartFailed")


def test_controller_recovers_running_editor_from_process_scan(tmp_path, monkeypatch):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    monkeypatch.setattr(ctl, "_scan_running_editors", lambda: [module.ProcessInfo(pid=9876, project="MyGame", started_at=None, managed_group=False)])
    result = ctl.status()
    assert result["state"] == "running"
    assert result["pid"] == 9876
    assert result["project"] == "MyGame"
    assert result["recovered"] is True


def test_project_scoped_stop_rejects_other_project(tmp_path, monkeypatch):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    other = project_root / "OtherGame"; other.mkdir(); (other/"OtherGame.uproject").write_text("{}")
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    monkeypatch.setattr(ctl, "_scan_running_editors", lambda: [module.ProcessInfo(pid=9876, project="OtherGame", started_at=None, managed_group=False)])
    try:
        ctl.stop(project="MyGame", any_project=False)
    except module.Conflict as exc:
        assert "OtherGame" in str(exc)
    else:
        raise AssertionError("expected Conflict")


def test_invalid_content_length_returns_400(tmp_path):
    from http.client import HTTPConnection
    import threading
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    server = module.ThreadingHTTPServer(("127.0.0.1",0), module.make_handler(ctl))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        conn=HTTPConnection("127.0.0.1", server.server_port, timeout=2)
        conn.putrequest("POST","/v1/editor/start")
        conn.putheader("Content-Type","application/json")
        conn.putheader("Content-Length","abc")
        conn.endheaders()
        resp=conn.getresponse(); data=resp.read()
        assert resp.status == 400
        assert b"Content-Length" in data
    finally:
        server.shutdown()


def test_unknown_project_editor_blocks_second_start(tmp_path, monkeypatch):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    monkeypatch.setattr(ctl, "_scan_running_editors", lambda: [module.ProcessInfo(pid=9999, project=None, started_at=None, managed_group=False)])
    try:
        ctl.start("MyGame")
    except module.Conflict as exc:
        assert "project" in str(exc).lower()
    else:
        raise AssertionError("expected Conflict")


def test_process_scan_detects_editor_even_when_project_unknown(tmp_path):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    proc_root = tmp_path / "proc"
    proc = proc_root / "1234"
    proc.mkdir(parents=True)
    (proc / "exe").symlink_to(binary)
    (proc / "cmdline").write_bytes(str(binary).encode() + b"\0-foo\0")
    (proc / "stat").write_text("1234 (UnrealEditor) S 1 1 1 0 0\n")
    ctl = module.EditorController(
        project_root, engine_root, tmp_path/"editor.log", startup_grace=0, proc_root=proc_root
    )
    infos = ctl._scan_running_editors()
    assert len(infos) == 1
    assert infos[0].pid == 1234
    assert infos[0].project is None


def test_handler_supports_request_read_timeout(tmp_path):
    project_root, project, uproject, engine_root, binary = _make_editor_tree(tmp_path)
    ctl = module.EditorController(project_root, engine_root, tmp_path/"editor.log", startup_grace=0)
    handler = module.make_handler(ctl, request_timeout=0.05)
    assert handler is not None


def test_rotate_log_once_copytruncates_and_keeps_backup(tmp_path):
    log = tmp_path / "editor.log"
    log.write_bytes(b"0123456789")
    rotated = module.rotate_log_once(log, max_bytes=5, backups=2)
    assert rotated is True
    assert log.read_bytes() == b""
    assert (tmp_path / "editor.log.1").read_bytes() == b"0123456789"
