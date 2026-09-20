import json
import os
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ThreadingHTTPServer.daemon_threads = True
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UECTL = ROOT / "uectl.py"


def test_outside_project_returns_2():
    env = os.environ.copy()
    env.update({"UECTL_PROJECT_ROOT": "/projects", "UECTL_WORKTREE_ROOT": "/worktrees"})
    p = subprocess.run([sys.executable, str(UECTL), "build"], cwd="/tmp", env=env, text=True, capture_output=True)
    assert p.returncode == 2


def test_unavailable_service_returns_3(tmp_path):
    root = tmp_path / "projects"
    cwd = root / "MyGame"
    cwd.mkdir(parents=True)
    env = os.environ.copy()
    env.update({
        "UECTL_PROJECT_ROOT": str(root),
        "UECTL_WORKTREE_ROOT": str(tmp_path / "worktrees"),
        "UECTL_ENDPOINT": "http://127.0.0.1:9",
        "UECTL_HTTP_TIMEOUT": "0.1",
    })
    p = subprocess.run([sys.executable, str(UECTL), "build"], cwd=cwd, env=env, text=True, capture_output=True)
    assert p.returncode == 3


def test_failed_job_maps_small_ue_exit_to_6(tmp_path):
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            raw = b'{"job_id":"f1"}'
            self.send_response(202); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path.endswith('/logs'):
                raw = b'compile error\n'; self.send_response(200); self.send_header("Content-Type", "text/plain"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
            else:
                raw = b'{"job_id":"f1","state":"failed","exit_code":1}'
                self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server = ThreadingHTTPServer(("127.0.0.1",0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        root = tmp_path / "projects"; cwd = root / "MyGame"; cwd.mkdir(parents=True)
        env = os.environ.copy(); env.update({
            "UECTL_PROJECT_ROOT": str(root), "UECTL_WORKTREE_ROOT": str(tmp_path/'worktrees'),
            "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}", "UECTL_POLL_INTERVAL":"0.01"
        })
        p = subprocess.run([sys.executable, str(UECTL), "build"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 6
        assert "compile error" in p.stdout
    finally:
        server.shutdown()
