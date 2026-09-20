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


class State:
    requests = []


def run_server(routes):
    state = State()
    state.requests = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def _handle(self):
            length = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(length) if length else b""
            body = json.loads(raw) if raw else None
            state.requests.append((self.command, self.path, body, dict(self.headers)))
            key = (self.command, self.path.split("?", 1)[0])
            status, content_type, payload = routes[key](self.path, body) if callable(routes[key]) else routes[key]
            data = payload if isinstance(payload, bytes) else (
                json.dumps(payload).encode() if content_type == "application/json" else str(payload).encode()
            )
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        do_GET = _handle
        do_POST = _handle

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, state


def cli(args, endpoint, editor_endpoint=None, cwd=None):
    env = os.environ.copy()
    env.update({
        "UECTL_ENDPOINT": endpoint,
        "UECTL_EDITOR_ENDPOINT": editor_endpoint or endpoint,
        "UECTL_PROJECT_ROOT": str(Path(cwd).parents[0] if cwd else "/projects"),
        "UECTL_WORKTREE_ROOT": "/definitely-not-worktrees",
        "UECTL_POLL_INTERVAL": "0.01",
    })
    return subprocess.run([sys.executable, str(UECTL), *args], cwd=cwd, env=env, text=True, capture_output=True)


def make_project(tmp):
    project_root = Path(tmp) / "projects"
    cwd = project_root / "MyGame" / "Source"
    cwd.mkdir(parents=True)
    return project_root, cwd


def test_build_posts_context_streams_logs_and_returns_success():
    routes = {
        ("POST", "/v1/jobs/build"): (202, "application/json", {"job_id": "j1"}),
        ("GET", "/v1/jobs/j1/logs"): (200, "text/plain", "Compile Foo.cpp\nBUILD SUCCESS\n"),
        ("GET", "/v1/jobs/j1"): (200, "application/json", {"job_id": "j1", "state": "succeeded", "exit_code": 0, "engine": "5.8", "target": "MyGameEditor", "duration_ms": 12}),
    }
    server, state = run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env = os.environ.copy()
            env.update({
                "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
                "UECTL_POLL_INTERVAL": "0.01",
            })
            p = subprocess.run([sys.executable, str(UECTL), "build", "--target", "editor"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0, p.stderr
        assert "Compile Foo.cpp" in p.stdout
        post = next(x for x in state.requests if x[0] == "POST")
        assert post[1] == "/v1/jobs/build"
        assert post[2]["project"] == "MyGame"
        assert post[2]["workspace"] == "main"
        assert post[2]["target"] == "editor"
        assert post[2]["configuration"] == "Development"
        assert post[3].get("X-Uectl-Request-Id") or post[3].get("X-UECTL-Request-ID")
        assert "Authorization" not in post[3]
    finally:
        server.shutdown()


def test_json_mode_keeps_stdout_machine_readable_and_logs_on_stderr():
    routes = {
        ("POST", "/v1/jobs/build"): (202, "application/json", {"job_id": "j2"}),
        ("GET", "/v1/jobs/j2/logs"): (200, "text/plain", "compiler output\n"),
        ("GET", "/v1/jobs/j2"): (200, "application/json", {"job_id": "j2", "state": "succeeded", "exit_code": 0, "project": "MyGame", "workspace": "main"}),
    }
    server, _ = run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env = os.environ.copy()
            env.update({
                "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
                "UECTL_POLL_INTERVAL": "0.01",
            })
            p = subprocess.run([sys.executable, str(UECTL), "build", "--json"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0
        obj = json.loads(p.stdout)
        assert obj["job_id"] == "j2"
        assert "compiler output" in p.stderr
    finally:
        server.shutdown()


def test_editor_start_uses_separate_endpoint_and_project_not_path():
    routes = {
        ("POST", "/v1/editor/start"): (200, "application/json", {"state": "running", "pid": 123, "project": "MyGame"}),
    }
    server, state = run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env = os.environ.copy()
            env.update({
                "UECTL_ENDPOINT": "http://127.0.0.1:9",
                "UECTL_EDITOR_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
            })
            p = subprocess.run([sys.executable, str(UECTL), "editor", "start"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0, p.stderr
        req = state.requests[0]
        assert req[0:2] == ("POST", "/v1/editor/start")
        assert req[2] == {"project": "MyGame"}
        assert str(cwd) not in json.dumps(req[2])
    finally:
        server.shutdown()


def test_editor_stop_any_does_not_require_project_context_legacy_case():
    routes = {
        ("POST", "/v1/editor/stop"): (200, "application/json", {"state": "stopped"}),
    }
    server, state = run_server(routes)
    try:
        env = os.environ.copy()
        env.update({"UECTL_EDITOR_ENDPOINT": f"http://127.0.0.1:{server.server_port}"})
        p = subprocess.run([sys.executable, str(UECTL), "editor", "stop", "--any"], cwd="/tmp", env=env, text=True, capture_output=True)
        assert p.returncode == 0, p.stderr
        assert state.requests[0][2] == {"force": False, "any": True}
    finally:
        server.shutdown()



def test_http_error_json_array_does_not_traceback(tmp_path):
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            raw = b'["bad request"]'
            self.send_response(400)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy()
        env["UECTL_ENDPOINT"] = f"http://127.0.0.1:{server.server_port}"
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode == 2
        assert "Traceback" not in p.stderr
        assert '["bad request"]' in p.stderr
    finally:
        server.shutdown()


def test_status_missing_ready_is_failure():
    routes = {("GET", "/v1/status"): (200, "application/json", {"service": "ue-build-service"})}
    server, _ = run_server(routes)
    try:
        p = cli(["status"], f"http://127.0.0.1:{server.server_port}")
        assert p.returncode == 1
        assert "Ready:        False" in p.stdout
    finally:
        server.shutdown()


def test_build_log_503_is_best_effort_and_keeps_job():
    calls = {"logs": 0}
    def logs(path, body):
        calls["logs"] += 1
        if calls["logs"] == 1:
            return (503, "application/json", {"error": "temporary"})
        return (200, "text/plain", "BUILD SUCCESS\n")
    routes = {
        ("POST", "/v1/jobs/build"): (202, "application/json", {"job_id": "jr1"}),
        ("GET", "/v1/jobs/jr1/logs"): logs,
        ("GET", "/v1/jobs/jr1"): (200, "application/json", {"job_id": "jr1", "state": "succeeded", "exit_code": 0}),
    }
    server, _ = run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env = os.environ.copy()
            env.update({
                "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
                "UECTL_POLL_INTERVAL": "0.01",
                "UECTL_RETRY_ATTEMPTS": "3",
                "UECTL_RETRY_BACKOFF": "0.01",
            })
            p = subprocess.run([sys.executable, str(UECTL), "build", "--json"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0, p.stderr
        assert json.loads(p.stdout)["job_id"] == "jr1"
        assert calls["logs"] == 1
        assert "warning: build log unavailable" in p.stderr
    finally:
        server.shutdown()


def test_create_job_retries_post_with_same_request_id():
    request_ids = []
    calls = {"post": 0}
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            calls["post"] += 1
            request_ids.append(self.headers.get("X-UECTL-Request-ID"))
            raw_body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            if calls["post"] == 1:
                raw = b'{"error":"temporary"}'
                self.send_response(503)
            else:
                raw = b'{"job_id":"same-id-job"}'
                self.send_response(202)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path.endswith("/logs"):
                raw=b''; ctype="text/plain"
            else:
                raw=b'{"job_id":"same-id-job","state":"succeeded","exit_code":0}'; ctype="application/json"
            self.send_response(200); self.send_header("Content-Type",ctype); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env=os.environ.copy(); env.update({
                "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp)/"worktrees"),
                "UECTL_RETRY_ATTEMPTS":"3", "UECTL_RETRY_BACKOFF":"0.01", "UECTL_POLL_INTERVAL":"0.01"
            })
            p=subprocess.run([sys.executable,str(UECTL),"build","--no-stream"],cwd=cwd,env=env,text=True,capture_output=True)
        assert p.returncode == 0, p.stderr
        assert len(request_ids) == 2
        assert request_ids[0] and request_ids[0] == request_ids[1]
    finally:
        server.shutdown()


def test_editor_stop_is_project_scoped_by_default():
    routes = {("POST", "/v1/editor/stop"): (200, "application/json", {"state":"stopped","project":"MyGame"})}
    server, state = run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env=os.environ.copy(); env.update({
                "UECTL_EDITOR_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp)/"worktrees")
            })
            p=subprocess.run([sys.executable,str(UECTL),"editor","stop"],cwd=cwd,env=env,text=True,capture_output=True)
        assert p.returncode == 0, p.stderr
        assert state.requests[0][2] == {"project":"MyGame","force":False,"any":False}
    finally:
        server.shutdown()


def test_editor_stop_any_does_not_need_project_context():
    routes = {("POST", "/v1/editor/stop"): (200, "application/json", {"state":"stopped"})}
    server, state = run_server(routes)
    try:
        env=os.environ.copy(); env["UECTL_EDITOR_ENDPOINT"] = f"http://127.0.0.1:{server.server_port}"
        p=subprocess.run([sys.executable,str(UECTL),"editor","stop","--any"],cwd="/tmp",env=env,text=True,capture_output=True)
        assert p.returncode == 0, p.stderr
        assert state.requests[0][2] == {"force":False,"any":True}
    finally:
        server.shutdown()


def test_editor_restart_is_not_automatically_retried():
    calls = {"post": 0}
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            calls["post"] += 1
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"error":"response lost after restart"}'
            self.send_response(503)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers(); self.wfile.write(raw)
    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root, cwd = make_project(tmp)
            env=os.environ.copy(); env.update({
                "UECTL_EDITOR_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT": str(project_root),
                "UECTL_WORKTREE_ROOT": str(Path(tmp)/"worktrees"),
                "UECTL_RETRY_ATTEMPTS":"3", "UECTL_RETRY_BACKOFF":"0.01"
            })
            p=subprocess.run([sys.executable,str(UECTL),"editor","restart"],cwd=cwd,env=env,text=True,capture_output=True)
        assert p.returncode != 0
        assert calls["post"] == 1
    finally:
        server.shutdown()


def test_json_wait_failure_preserves_job_and_request_id():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw=b'{"job_id":"lost-job"}'
            self.send_response(202); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            raw=b'{"error":"still down"}'
            self.send_response(503); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=ThreadingHTTPServer(("127.0.0.1",0),H); threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root,cwd=make_project(tmp)
            env=os.environ.copy(); env.update({
                "UECTL_ENDPOINT":f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT":str(project_root),"UECTL_WORKTREE_ROOT":str(Path(tmp)/"worktrees"),
                "UECTL_RETRY_ATTEMPTS":"2","UECTL_RETRY_BACKOFF":"0.01"
            })
            p=subprocess.run([sys.executable,str(UECTL),"build","--json","--no-stream","--request-id","fixed-123"],cwd=cwd,env=env,text=True,capture_output=True)
        assert p.returncode != 0
        obj=json.loads(p.stdout)
        assert obj["job_id"] == "lost-job"
        assert obj["request_id"] == "fixed-123"
        assert obj["state"] == "client_error"
    finally:
        server.shutdown()


def test_persistent_log_endpoint_failure_does_not_hide_successful_job():
    routes = {
        ("POST", "/v1/jobs/build"): (202, "application/json", {"job_id":"logless"}),
        ("GET", "/v1/jobs/logless/logs"): (503, "application/json", {"error":"log store down"}),
        ("GET", "/v1/jobs/logless"): (200, "application/json", {"job_id":"logless","state":"succeeded","exit_code":0}),
    }
    server,_=run_server(routes)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            project_root,cwd=make_project(tmp)
            env=os.environ.copy(); env.update({
                "UECTL_ENDPOINT":f"http://127.0.0.1:{server.server_port}",
                "UECTL_PROJECT_ROOT":str(project_root),"UECTL_WORKTREE_ROOT":str(Path(tmp)/"worktrees"),
                "UECTL_RETRY_ATTEMPTS":"2","UECTL_RETRY_BACKOFF":"0.01"
            })
            p=subprocess.run([sys.executable,str(UECTL),"build","--json"],cwd=cwd,env=env,text=True,capture_output=True)
        assert p.returncode == 0, p.stderr
        assert json.loads(p.stdout)["state"] == "succeeded"
        assert "log" in p.stderr.lower()
    finally:
        server.shutdown()
