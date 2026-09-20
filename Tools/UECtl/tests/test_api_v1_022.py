import json
import os
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ThreadingHTTPServer.daemon_threads = True

ROOT = Path(__file__).resolve().parents[1]
UECTL = ROOT / "uectl.py"


def run_server(handler):
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def project_env(tmp, server, **extra):
    root = Path(tmp) / "projects"
    cwd = root / "MyGame" / "Source"
    cwd.mkdir(parents=True)
    env = os.environ.copy()
    env.update({
        "UECTL_PROJECT_ROOT": str(root),
        "UECTL_WORKTREE_ROOT": str(Path(tmp) / "worktrees"),
        "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
        "UECTL_POLL_INTERVAL": "0.01",
        "UECTL_RETRY_ATTEMPTS": "2",
        "UECTL_RETRY_BACKOFF": "0.01",
        "UECTL_HTTP_TIMEOUT": "0.15",
        "UECTL_LOG_HTTP_TIMEOUT": "0.05",
    })
    env.update(extra)
    return env, cwd


def test_follow_logs_uses_metadata_as_authority_when_log_endpoint_fails():
    calls = []
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            calls.append(self.path)
            if self.path.startswith("/v1/jobs/j1/logs"):
                raw = b'{"error":"log temporarily unavailable"}'
                self.send_response(503)
                self.send_header("Content-Type", "application/json")
            else:
                raw = b'{"job_id":"j1","state":"succeeded","exit_code":0}'
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers(); self.wfile.write(raw)
    server = run_server(H)
    try:
        env = os.environ.copy(); env.update({
            "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
            "UECTL_POLL_INTERVAL":"0.01", "UECTL_RETRY_ATTEMPTS":"2", "UECTL_RETRY_BACKOFF":"0.01",
            "UECTL_LOG_HTTP_TIMEOUT":"0.05",
        })
        p = subprocess.run([sys.executable, str(UECTL), "logs", "-f", "j1"], env=env, text=True, capture_output=True, timeout=3)
        assert p.returncode == 0, p.stderr
        assert calls[0] == "/v1/jobs/j1"
        assert "warning" in p.stderr.lower()
    finally:
        server.shutdown()


def test_timeout_while_reading_http_error_body_does_not_traceback():
    hits = {"n": 0}
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            hits["n"] += 1
            part = b'{"error":"partial'
            self.send_response(503)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", "100")
            self.end_headers()
            self.wfile.write(part); self.wfile.flush()
            time.sleep(0.3)
    server = run_server(H)
    try:
        env = os.environ.copy(); env.update({
            "UECTL_ENDPOINT": f"http://127.0.0.1:{server.server_port}",
            "UECTL_HTTP_TIMEOUT":"0.05", "UECTL_RETRY_ATTEMPTS":"2", "UECTL_RETRY_BACKOFF":"0.01",
        })
        p = subprocess.run([sys.executable, str(UECTL), "status"], env=env, text=True, capture_output=True, timeout=3)
        assert p.returncode == 3
        assert hits["n"] == 2
        assert "Traceback" not in p.stderr
    finally:
        server.shutdown()


def test_dot_segments_are_invalid_job_ids():
    env = os.environ.copy(); env.update({"UECTL_ENDPOINT":"http://127.0.0.1:9", "UECTL_RETRY_ATTEMPTS":"1"})
    for value in (".", ".."):
        p = subprocess.run([sys.executable, str(UECTL), "logs", value], env=env, text=True, capture_output=True)
        assert p.returncode == 2
        assert "job-id" in p.stderr.lower()


def test_job_metadata_identity_mismatch_is_protocol_error():
    class H(BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw=b'{"job_id":"j1"}'
            self.send_response(202); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            raw=b'{"job_id":"j2","project":"Other","workspace":"main","operation":"build","state":"succeeded","exit_code":0}'
            self.send_response(200); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env,cwd=project_env(tmp,server)
            p=subprocess.run([sys.executable,str(UECTL),"build","--no-stream","--json"],cwd=cwd,env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 1
        assert "metadata" in p.stderr.lower() or "job_id" in p.stderr.lower()
    finally:
        server.shutdown()


def test_501_is_not_retried():
    hits={"n":0}
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            hits["n"] += 1
            raw=b'{"error":"not implemented"}'
            self.send_response(501); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        env=os.environ.copy(); env.update({"UECTL_ENDPOINT":f"http://127.0.0.1:{server.server_port}","UECTL_RETRY_ATTEMPTS":"3","UECTL_RETRY_BACKOFF":"0.01"})
        p=subprocess.run([sys.executable,str(UECTL),"status"],env=env,text=True,capture_output=True,timeout=3)
        assert hits["n"] == 1
        assert p.returncode == 1
    finally:
        server.shutdown()


def test_client_and_api_version_headers_are_sent():
    seen={}
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            seen["ua"] = self.headers.get("User-Agent")
            seen["api"] = self.headers.get("X-UECTL-API-Version")
            seen["client"] = self.headers.get("X-UECTL-Client-Version")
            raw=b'{"service":"ue-build-service","ready":true,"api_version":"1"}'
            self.send_response(200); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        env=os.environ.copy(); env["UECTL_ENDPOINT"] = f"http://127.0.0.1:{server.server_port}"
        p=subprocess.run([sys.executable,str(UECTL),"status"],env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 0
        assert seen == {"ua":"uectl/0.2.2", "api":"1", "client":"0.2.2"}
    finally:
        server.shutdown()


def test_incremental_log_offset_contract_is_used():
    paths=[]
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw=b'{"job_id":"j1"}'
            self.send_response(202); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            paths.append(self.path)
            if self.path.startswith("/v1/jobs/j1/logs?offset=0"):
                raw=b'abc'; self.send_response(200); self.send_header("Content-Type","text/plain"); self.send_header("X-UECTL-Next-Offset","3")
            else:
                raw=b'{"job_id":"j1","project":"MyGame","workspace":"main","operation":"build","state":"succeeded","exit_code":0}'; self.send_response(200); self.send_header("Content-Type","application/json")
            self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env,cwd=project_env(tmp,server)
            p=subprocess.run([sys.executable,str(UECTL),"build"],cwd=cwd,env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 0, p.stderr
        assert "abc" in p.stdout
        assert any(path == "/v1/jobs/j1/logs?offset=0" for path in paths)
    finally:
        server.shutdown()


def test_create_response_requires_job_id_not_legacy_id():
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length", "0")))
            raw=b'{"id":"legacy-only"}'
            self.send_response(202); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env,cwd=project_env(tmp,server)
            p=subprocess.run([sys.executable,str(UECTL),"build","--no-stream"],cwd=cwd,env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 1
        assert "job_id" in p.stderr
    finally:
        server.shutdown()


def test_terminal_job_drains_all_offset_log_chunks():
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            self.rfile.read(int(self.headers.get("Content-Length","0")))
            raw=b'{"job_id":"jchunks"}'
            self.send_response(202); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            if self.path == "/v1/jobs/jchunks/logs?offset=0":
                raw=b"abc"; next_offset="3"; ctype="text/plain"
            elif self.path == "/v1/jobs/jchunks/logs?offset=3":
                raw=b"def"; next_offset="6"; ctype="text/plain"
            elif self.path == "/v1/jobs/jchunks/logs?offset=6":
                raw=b""; next_offset="6"; ctype="text/plain"
            else:
                raw=b'{"job_id":"jchunks","project":"MyGame","workspace":"main","operation":"build","state":"succeeded","exit_code":0}'; next_offset=None; ctype="application/json"
            self.send_response(200); self.send_header("Content-Type",ctype)
            if next_offset is not None: self.send_header("X-UECTL-Next-Offset",next_offset)
            self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        with tempfile.TemporaryDirectory() as tmp:
            env,cwd=project_env(tmp,server)
            p=subprocess.run([sys.executable,str(UECTL),"build"],cwd=cwd,env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 0, p.stderr
        assert "abcdef" in p.stdout
    finally:
        server.shutdown()


def test_non_follow_logs_drains_all_offset_chunks():
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            if self.path == "/v1/jobs/jchunks/logs?offset=0": raw,next_offset=b"abc","3"
            elif self.path == "/v1/jobs/jchunks/logs?offset=3": raw,next_offset=b"def","6"
            elif self.path == "/v1/jobs/jchunks/logs?offset=6": raw,next_offset=b"","6"
            else: raw,next_offset=b"bad",None
            self.send_response(200); self.send_header("Content-Type","text/plain")
            if next_offset is not None: self.send_header("X-UECTL-Next-Offset",next_offset)
            self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        env=os.environ.copy(); env.update({"UECTL_ENDPOINT":f"http://127.0.0.1:{server.server_port}","UECTL_LOG_HTTP_TIMEOUT":"0.2"})
        p=subprocess.run([sys.executable,str(UECTL),"logs","jchunks"],env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 0
        assert p.stdout == "abcdef"
    finally:
        server.shutdown()


def test_non_follow_logs_stops_at_initial_log_size_snapshot():
    paths=[]
    class H(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            paths.append(self.path)
            if self.path == "/v1/jobs/growing/logs?offset=0": raw,next_offset,size=b"abc","3","6"
            elif self.path == "/v1/jobs/growing/logs?offset=3": raw,next_offset,size=b"def","6","9"
            elif self.path == "/v1/jobs/growing/logs?offset=6": raw,next_offset,size=b"ghi","9","9"
            else: raw,next_offset,size=b"","0","0"
            self.send_response(200); self.send_header("Content-Type","text/plain")
            self.send_header("X-UECTL-Next-Offset",next_offset)
            self.send_header("X-UECTL-Log-Size",size)
            self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)
    server=run_server(H)
    try:
        env=os.environ.copy(); env.update({"UECTL_ENDPOINT":f"http://127.0.0.1:{server.server_port}","UECTL_LOG_HTTP_TIMEOUT":"0.2"})
        p=subprocess.run([sys.executable,str(UECTL),"logs","growing"],env=env,text=True,capture_output=True,timeout=3)
        assert p.returncode == 0
        assert p.stdout == "abcdef"
        assert "/v1/jobs/growing/logs?offset=6" not in paths
    finally:
        server.shutdown()
