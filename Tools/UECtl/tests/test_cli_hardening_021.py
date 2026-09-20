import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ThreadingHTTPServer.daemon_threads = True
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
UECTL = ROOT / "uectl.py"


def make_project(tmp):
    project_root = Path(tmp) / "projects"
    cwd = project_root / "MyGame" / "Source"
    cwd.mkdir(parents=True)
    return project_root, cwd


def base_env(tmp, endpoint):
    project_root, cwd = make_project(tmp)
    env = os.environ.copy()
    env.update({
        "UECTL_ENDPOINT": endpoint,
        "UECTL_EDITOR_ENDPOINT": endpoint,
        "UECTL_PROJECT_ROOT": str(project_root),
        "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
        "UECTL_POLL_INTERVAL": "0.01",
        "UECTL_RETRY_ATTEMPTS": "2",
        "UECTL_RETRY_BACKOFF": "0.01",
    })
    return env, cwd


def test_internal_http_does_not_use_environment_proxy():
    hits = []

    class Proxy(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            hits.append(self.path)
            raw = b'{"service":"fake-proxy","ready":true}'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Proxy)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy()
        proxy = f"http://127.0.0.1:{server.server_port}"
        env.update({
            "UECTL_ENDPOINT": "http://ue-build-service.invalid:8765",
            "HTTP_PROXY": proxy,
            "http_proxy": proxy,
            "NO_PROXY": "",
            "no_proxy": "",
            "UECTL_HTTP_TIMEOUT": "0.1",
            "UECTL_RETRY_ATTEMPTS": "1",
        })
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode == 3
        assert hits == []
    finally:
        server.shutdown()


def test_incomplete_http_body_is_retryable_without_traceback():
    calls = {"n": 0}

    class H(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            calls["n"] += 1
            raw = b'{"ready":true}'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw) + 100))
            self.end_headers()
            self.wfile.write(raw)
            self.wfile.flush()
            self.close_connection = True

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy()
        env.update({
            "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
            "UECTL_HTTP_TIMEOUT": "0.2",
            "UECTL_RETRY_ATTEMPTS": "2",
            "UECTL_RETRY_BACKOFF": "0.01",
        })
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode == 3
        assert calls["n"] == 2
        assert "Traceback" not in p.stderr
    finally:
        server.shutdown()


@pytest.mark.parametrize("name,value", [
    ("UECTL_HTTP_TIMEOUT", "nan"),
    ("UECTL_HTTP_TIMEOUT", "inf"),
    ("UECTL_HTTP_TIMEOUT", "-1"),
    ("UECTL_POLL_INTERVAL", "-1"),
    ("UECTL_RETRY_BACKOFF", "inf"),
])
def test_invalid_numeric_config_is_clean_bad_request(name, value):
    env = os.environ.copy()
    env[name] = value
    p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
    assert p.returncode == 2
    assert "Traceback" not in p.stderr
    assert name in p.stderr


def test_malformed_endpoint_is_clean_bad_request():
    env = os.environ.copy()
    env["UECTL_ENDPOINT"] = "not-a-url"
    p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
    assert p.returncode == 2
    assert "Traceback" not in p.stderr
    assert "UECTL_ENDPOINT" in p.stderr or "endpoint" in p.stderr.lower()


def test_status_engines_null_does_not_traceback():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            raw = b'{"service":"ue-build-service","ready":true,"engines":null}'
            self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy(); env["UECTL_ENDPOINT"] = f"http://127.0.0.1:{server.server_port}"
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode == 0
        assert "Traceback" not in p.stderr
    finally:
        server.shutdown()


def test_invalid_duration_metadata_does_not_turn_success_into_client_failure():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"job_id":"jdur"}'
            self.send_response(202); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path.startswith("/v1/jobs/fastdone/logs"):
                raw = b""; ctype = "text/plain"
            else:
                raw = b'{"job_id":"jdur","state":"succeeded","exit_code":0,"duration_ms":"oops"}'; ctype = "application/json"
            self.send_response(200); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            p = subprocess.run([sys.executable, str(UECTL), "build", "--no-stream"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0
        assert "SUCCEEDED" in p.stdout
        assert "Traceback" not in p.stderr
    finally:
        server.shutdown()


def test_invalid_job_id_is_rejected_before_http_request():
    env = os.environ.copy()
    env.update({"UECTL_ENDPOINT": "http://127.0.0.1:9", "UECTL_RETRY_ATTEMPTS": "1"})
    p = subprocess.run([sys.executable, str(UECTL), "logs", "../status"], env=env, text=True, capture_output=True)
    assert p.returncode == 2
    assert "job-id" in p.stderr.lower()


def test_unknown_job_state_fails_instead_of_waiting_forever():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"job_id":"junkstate"}'
            self.send_response(202); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path.endswith("/logs"):
                raw = b""; ctype = "text/plain"
            else:
                raw = b'{"job_id":"junkstate","state":"alien"}'; ctype = "application/json"
            self.send_response(200); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            p = subprocess.run([sys.executable, str(UECTL), "build", "--no-stream"], cwd=cwd, env=env, text=True, capture_output=True, timeout=3.0)
        assert p.returncode == 1
        assert "unknown job state" in p.stderr.lower()
    finally:
        server.shutdown()


def test_editor_start_failed_state_is_nonzero():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"state":"failed","message":"boom"}'
            self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            p = subprocess.run([sys.executable, str(UECTL), "editor", "start"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 1
    finally:
        server.shutdown()


def test_editor_uses_separate_longer_timeout():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            time.sleep(0.12)
            raw = b'{"state":"running","project":"MyGame","pid":42}'
            self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            env["UECTL_HTTP_TIMEOUT"] = "0.05"
            env["UECTL_EDITOR_HTTP_TIMEOUT"] = "0.5"
            p = subprocess.run([sys.executable, str(UECTL), "editor", "start"], cwd=cwd, env=env, text=True, capture_output=True)
        assert p.returncode == 0, p.stderr
    finally:
        server.shutdown()


def test_live_log_failure_does_not_retry_or_delay_terminal_job():
    calls = {"logs": 0, "meta": 0}
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"job_id":"fastdone"}'
            self.send_response(202); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path.startswith("/v1/jobs/fastdone/logs"):
                calls["logs"] += 1
                raw = b'{"error":"logs temporarily unavailable"}'
                self.send_response(503); self.send_header("Content-Type", "application/json")
            else:
                calls["meta"] += 1
                raw = b'{"job_id":"fastdone","state":"succeeded","exit_code":0}'
                self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            env["UECTL_RETRY_ATTEMPTS"] = "5"
            env["UECTL_RETRY_BACKOFF"] = "0.1"
            t0 = time.monotonic()
            p = subprocess.run([sys.executable, str(UECTL), "build"], cwd=cwd, env=env, text=True, capture_output=True)
            elapsed = time.monotonic() - t0
        assert p.returncode == 0, p.stderr
        assert calls["meta"] == 1
        assert calls["logs"] <= 2
        assert elapsed < 2.0
    finally:
        server.shutdown()


def test_exhausted_503_maps_to_unavailable_exit_3():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            raw = b'{"error":"down"}'
            self.send_response(503); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy(); env.update({
            "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
            "UECTL_RETRY_ATTEMPTS": "2", "UECTL_RETRY_BACKOFF": "0.01",
        })
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode == 3
    finally:
        server.shutdown()


def test_ctrl_c_after_job_id_emits_json_recovery_context():
    get_started = threading.Event()

    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw = b'{"job_id":"interrupt-job"}'
            self.send_response(202); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            get_started.set()
            time.sleep(5)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            proc = subprocess.Popen([sys.executable, str(UECTL), "build", "--json", "--no-stream", "--request-id", "interrupt-rid"], cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            assert get_started.wait(2)
            proc.send_signal(signal.SIGINT)
            out, err = proc.communicate(timeout=2)
        assert proc.returncode == 130
        obj = json.loads(out)
        assert obj["state"] == "client_interrupted"
        assert obj["job_id"] == "interrupt-job"
        assert obj["request_id"] == "interrupt-rid"
        assert obj["job_continues"] is True
    finally:
        server.shutdown()


def test_ctrl_c_during_create_emits_request_id_recovery_context():
    post_started = threading.Event()

    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            post_started.set()
            time.sleep(5)

    server = ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env, cwd = base_env(tmp, f"http://127.0.0.1:{server.server_port}")
            proc = subprocess.Popen([sys.executable, str(UECTL), "build", "--json", "--no-stream", "--request-id", "create-interrupt-rid"], cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            assert post_started.wait(2)
            proc.send_signal(signal.SIGINT)
            out, err = proc.communicate(timeout=2)
        assert proc.returncode == 130
        obj = json.loads(out)
        assert obj["state"] == "client_interrupted"
        assert obj["request_id"] == "create-interrupt-rid"
        assert "job_id" not in obj
        assert obj["job_may_exist"] is True
    finally:
        server.shutdown()


def test_version_subcommand_exists_and_succeeds_without_server():
    env = os.environ.copy(); env.update({
        "UECTL_ENDPOINT": "http://127.0.0.1:9",
        "UECTL_HTTP_TIMEOUT": "0.05",
        "UECTL_RETRY_ATTEMPTS": "1",
    })
    p = subprocess.run([sys.executable, str(UECTL), "version"], env=env, text=True, capture_output=True)
    assert p.returncode == 0
    assert "uectl 0.2.2" in p.stdout


def test_control_plane_does_not_follow_cross_host_redirects():
    external_hits = []

    class External(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            external_hits.append(self.path)
            raw = b'{"ready":true}'
            self.send_response(200); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(raw))); self.end_headers(); self.wfile.write(raw)

    external = ThreadingHTTPServer(("127.0.0.1", 0), External)
    threading.Thread(target=external.serve_forever, daemon=True).start()

    class Redirector(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            self.send_response(302)
            self.send_header("Location", f"http://127.0.0.1:{external.server_port}/outside")
            self.send_header("Content-Length", "0")
            self.end_headers()

    redirector = ThreadingHTTPServer(("127.0.0.1", 0), Redirector)
    threading.Thread(target=redirector.serve_forever, daemon=True).start()
    try:
        env = os.environ.copy(); env.update({
            "UECTL_ENDPOINT": f"http://127.0.0.1:{redirector.server_port}",
            "UECTL_RETRY_ATTEMPTS": "1",
        })
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True)
        assert p.returncode != 0
        assert external_hits == []
    finally:
        redirector.shutdown(); external.shutdown()
