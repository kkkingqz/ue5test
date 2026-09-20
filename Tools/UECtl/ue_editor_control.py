#!/usr/bin/env python3
"""Narrow UnrealEditor process-control daemon for the ue-editor container."""
from __future__ import annotations

import argparse
import json
import os
import re
import signal
import shutil
import subprocess
import threading
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Optional

CONTROL_VERSION = "0.2.0"
PROJECT_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
MAX_BODY = 64 * 1024


class ControlError(RuntimeError):
    status = 400


class Conflict(ControlError):
    status = 409


class NotFound(ControlError):
    status = 404


class StartFailed(ControlError):
    status = 500


@dataclass(frozen=True)
class ProcessInfo:
    pid: int
    project: Optional[str]
    started_at: Optional[float]
    managed_group: bool
    recovered: bool = True


def valid_project_name(name: str) -> bool:
    return bool(PROJECT_RE.fullmatch(name))


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def resolve_project(project_root: Path, project: str) -> tuple[Path, Path]:
    if not valid_project_name(project):
        raise ControlError("invalid project name")
    root = project_root.resolve(strict=False)
    if not root.is_dir():
        raise NotFound("project root not found")
    candidate = root / project
    if not candidate.exists():
        raise NotFound("project directory not found")
    try:
        directory = candidate.resolve(strict=True)
    except FileNotFoundError as exc:
        raise NotFound("project directory not found") from exc
    if not _is_within(directory, root):
        raise ControlError("project resolves outside project root")
    if not directory.is_dir():
        raise NotFound("project directory not found")
    files: list[Path] = []
    for p in directory.glob("*.uproject"):
        try:
            resolved = p.resolve(strict=True)
        except FileNotFoundError:
            continue
        if resolved.is_file() and _is_within(resolved, directory):
            files.append(resolved)
    if len(files) != 1:
        raise ControlError(f"project must contain exactly one root .uproject file; found {len(files)}")
    return directory, files[0]


def _pid_state(pid: int) -> Optional[str]:
    try:
        text = Path(f"/proc/{pid}/stat").read_text()
    except (OSError, UnicodeError):
        return None
    close = text.rfind(")")
    if close < 0 or close + 2 >= len(text):
        return None
    return text[close + 2 : close + 3]


def _pid_alive(pid: int) -> bool:
    state = _pid_state(pid)
    return state is not None and state != "Z"


def rotate_log_once(path: Path, *, max_bytes: int, backups: int) -> bool:
    """Copy-truncate a large log so children holding O_APPEND keep writing safely."""
    if max_bytes <= 0 or backups <= 0:
        return False
    try:
        if not path.is_file() or path.stat().st_size <= max_bytes:
            return False
    except OSError:
        return False
    try:
        for index in range(backups, 1, -1):
            older = path.with_name(path.name + f".{index - 1}")
            newer = path.with_name(path.name + f".{index}")
            if older.exists():
                older.replace(newer)
        first = path.with_name(path.name + ".1")
        shutil.copyfile(path, first)
        with path.open("wb"):
            pass
        return True
    except OSError:
        return False


class LogRotator(threading.Thread):
    def __init__(
        self,
        path: Path,
        *,
        max_bytes: int,
        backups: int,
        interval: float,
    ):
        super().__init__(name="ue-editor-log-rotator", daemon=True)
        self.path = path
        self.max_bytes = max_bytes
        self.backups = backups
        self.interval = max(interval, 1.0)
        self._stop_event = threading.Event()

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        while not self._stop_event.wait(self.interval):
            rotate_log_once(self.path, max_bytes=self.max_bytes, backups=self.backups)


class EditorController:
    def __init__(
        self,
        project_root: Path,
        engine_root: Path,
        log_file: Path,
        *,
        stop_timeout: float = 15.0,
        startup_grace: float = 0.5,
        fixed_args: Optional[list[str]] = None,
        proc_root: Path = Path("/proc"),
    ):
        self.project_root = project_root
        self.engine_root = engine_root
        self.log_file = log_file
        self.stop_timeout = max(stop_timeout, 0.0)
        self.startup_grace = max(startup_grace, 0.0)
        self.fixed_args = list(fixed_args or [])
        self.proc_root = proc_root
        self._lock = threading.RLock()
        self._proc: Optional[subprocess.Popen[Any]] = None
        self._current: Optional[ProcessInfo] = None

    @property
    def binary(self) -> Path:
        return (self.engine_root / "Engine/Binaries/Linux/UnrealEditor").resolve(strict=False)

    def _project_from_cmdline(self, args: list[str]) -> Optional[str]:
        try:
            root = self.project_root.resolve(strict=True)
        except FileNotFoundError:
            return None
        for arg in args[1:]:
            value = arg[len("-Project=") :] if arg.startswith("-Project=") else arg
            if not value.endswith(".uproject"):
                continue
            p = Path(value).resolve(strict=False)
            if not _is_within(p, root):
                continue
            try:
                rel = p.relative_to(root)
            except ValueError:
                continue
            if len(rel.parts) != 2 or not valid_project_name(rel.parts[0]):
                continue
            if p.parent != (root / rel.parts[0]).resolve(strict=False):
                continue
            return rel.parts[0]
        return None

    def _scan_running_editors(self) -> list[ProcessInfo]:
        binary = self.binary
        if not binary.is_file():
            return []
        binary_s = str(binary)
        found: list[ProcessInfo] = []
        try:
            entries = list(self.proc_root.iterdir())
        except OSError:
            return []
        for entry in entries:
            if not entry.name.isdigit():
                continue
            pid = int(entry.name)
            try:
                stat_text = (entry / "stat").read_text()
            except (OSError, UnicodeError):
                continue
            close = stat_text.rfind(")")
            if close < 0 or close + 2 >= len(stat_text) or stat_text[close + 2 : close + 3] == "Z":
                continue
            try:
                exe = os.readlink(entry / "exe")
                exe_path = str(Path(exe).resolve(strict=False))
            except OSError:
                continue
            if exe_path != binary_s:
                continue
            try:
                raw = (entry / "cmdline").read_bytes()
            except OSError:
                continue
            args = [x.decode("utf-8", errors="replace") for x in raw.split(b"\0") if x]
            project = self._project_from_cmdline(args)
            try:
                managed_group = os.getpgid(pid) == pid if self.proc_root == Path("/proc") else False
            except (OSError, ProcessLookupError):
                managed_group = False
            found.append(
                ProcessInfo(
                    pid=pid,
                    project=project,
                    started_at=None,
                    managed_group=managed_group,
                    recovered=True,
                )
            )
        return found

    def _clear_managed(self) -> None:
        self._proc = None
        self._current = None

    def _running_infos(self) -> list[ProcessInfo]:
        if self._proc is not None and self._current is not None:
            if self._proc.poll() is None:
                return [self._current]
            self._clear_managed()
        return self._scan_running_editors()

    def _single_running(self) -> Optional[ProcessInfo]:
        infos = self._running_infos()
        if len(infos) > 1:
            projects = ", ".join(f"{x.project}:{x.pid}" for x in infos)
            raise Conflict(f"multiple UnrealEditor processes detected: {projects}")
        return infos[0] if infos else None

    def status(self) -> dict[str, Any]:
        with self._lock:
            infos = self._running_infos()
            if not infos:
                return {"state": "stopped"}
            if len(infos) > 1:
                return {
                    "state": "conflict",
                    "pids": [x.pid for x in infos],
                    "projects": [x.project for x in infos],
                    "message": "multiple UnrealEditor processes detected; refusing automatic control",
                }
            info = infos[0]
            result: dict[str, Any] = {
                "state": "running",
                "pid": info.pid,
                "project": info.project,
                "recovered": info.recovered,
            }
            if info.started_at is not None:
                result["uptime_s"] = max(0.0, time.time() - info.started_at)
            return result

    def start(self, project: str) -> dict[str, Any]:
        with self._lock:
            running = self._single_running()
            if running is not None:
                if running.project == project:
                    result = self.status()
                    result["message"] = "UnrealEditor is already running"
                    return result
                if running.project is None:
                    raise Conflict("UnrealEditor is already running but its project could not be identified")
                raise Conflict(f"UnrealEditor is already running for project {running.project}")

            _, uproject = resolve_project(self.project_root, project)
            binary = self.binary
            if not binary.is_file():
                raise NotFound(f"UnrealEditor binary not found: {binary}")

            try:
                self.log_file.parent.mkdir(parents=True, exist_ok=True)
                log = self.log_file.open("ab", buffering=0)
            except OSError as exc:
                raise StartFailed(f"cannot open Editor log {self.log_file}: {exc}") from exc

            argv = [str(binary), str(uproject), *self.fixed_args]
            try:
                proc = subprocess.Popen(
                    argv,
                    stdin=subprocess.DEVNULL,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
            except OSError as exc:
                log.close()
                raise StartFailed(f"failed to start UnrealEditor: {exc}") from exc
            finally:
                # Popen duplicates the descriptor for the child; the daemon does not
                # need to keep its own copy open after spawning.
                try:
                    log.close()
                except Exception:
                    pass

            started_at = time.time()
            info = ProcessInfo(
                pid=proc.pid,
                project=project,
                started_at=started_at,
                managed_group=True,
                recovered=False,
            )
            self._proc = proc
            self._current = info

            deadline = time.monotonic() + self.startup_grace
            while True:
                rc = proc.poll()
                if rc is not None:
                    self._clear_managed()
                    raise StartFailed(f"UnrealEditor exited during startup with code {rc}")
                if time.monotonic() >= deadline:
                    break
                time.sleep(min(0.05, max(0.0, deadline - time.monotonic())))
            return self.status()

    def _signal(self, info: ProcessInfo, sig: signal.Signals) -> None:
        if info.managed_group:
            os.killpg(info.pid, sig)
        else:
            os.kill(info.pid, sig)

    def _still_running(self, info: ProcessInfo) -> bool:
        if self._proc is not None and self._current is not None and self._current.pid == info.pid:
            return self._proc.poll() is None
        return _pid_alive(info.pid)

    def stop(
        self,
        *,
        project: Optional[str] = None,
        any_project: bool = False,
        force: bool = False,
    ) -> dict[str, Any]:
        with self._lock:
            info = self._single_running()
            if info is None:
                return {"state": "stopped", "message": "UnrealEditor is not running"}
            if not any_project:
                if not isinstance(project, str):
                    raise ControlError("project is required unless any=true")
                if info.project != project:
                    raise Conflict(
                        f"UnrealEditor is running for project {info.project}, not requested project {project}"
                    )

            try:
                self._signal(info, signal.SIGTERM)
            except ProcessLookupError:
                self._clear_managed()
                return {"state": "stopped", "project": info.project}

            deadline = time.monotonic() + self.stop_timeout
            while time.monotonic() < deadline:
                if not self._still_running(info):
                    self._clear_managed()
                    return {"state": "stopped", "project": info.project}
                time.sleep(0.1)

            if not force:
                raise Conflict("UnrealEditor did not exit before timeout; retry with force=true")

            try:
                self._signal(info, signal.SIGKILL)
            except ProcessLookupError:
                pass
            if self._proc is not None and self._current is not None and self._current.pid == info.pid:
                try:
                    self._proc.wait(timeout=5)
                except Exception:
                    pass
            else:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and _pid_alive(info.pid):
                    time.sleep(0.05)
            self._clear_managed()
            return {"state": "stopped", "project": info.project, "forced": True}

    def restart(self, project: str, force: bool = False) -> dict[str, Any]:
        with self._lock:
            running = self._single_running()
            if running is not None:
                if running.project != project:
                    raise Conflict(f"UnrealEditor is already running for project {running.project}")
                self.stop(project=project, force=force)
            return self.start(project)


def make_handler(controller: EditorController, *, request_timeout: float = 5.0):
    class Handler(BaseHTTPRequestHandler):
        server_version = f"ue-editor-control/{CONTROL_VERSION}"

        def setup(self) -> None:
            super().setup()
            self.connection.settimeout(max(request_timeout, 0.1))

        def log_message(self, fmt: str, *args: Any) -> None:
            print(f"ue-editor-control: {self.address_string()} - {fmt % args}", flush=True)

        def _read_json(self) -> dict[str, Any]:
            raw_length = self.headers.get("Content-Length", "0")
            try:
                length = int(raw_length)
            except (TypeError, ValueError) as exc:
                raise ControlError("invalid Content-Length") from exc
            if length < 0:
                raise ControlError("invalid Content-Length")
            if length == 0:
                return {}
            if length > MAX_BODY:
                raise ControlError("request body too large")
            try:
                raw = self.rfile.read(length)
            except (TimeoutError, OSError) as exc:
                raise ControlError("request body read timed out") from exc
            if len(raw) != length:
                raise ControlError("incomplete request body")
            try:
                obj = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise ControlError("invalid JSON body") from exc
            if not isinstance(obj, dict):
                raise ControlError("JSON body must be an object")
            return obj

        def _send(self, status: int, obj: dict[str, Any]) -> None:
            data = json.dumps(obj, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(data)

        def _dispatch(self) -> None:
            try:
                if self.command == "GET" and self.path == "/v1/editor/status":
                    self._send(200, controller.status())
                    return
                if self.command == "POST" and self.path in {
                    "/v1/editor/start", "/v1/editor/stop", "/v1/editor/restart"
                }:
                    body = self._read_json()
                    if self.path == "/v1/editor/start":
                        project = body.get("project")
                        if not isinstance(project, str):
                            raise ControlError("project is required")
                        self._send(200, controller.start(project))
                        return
                    if self.path == "/v1/editor/stop":
                        force = body.get("force", False)
                        any_project = body.get("any", False)
                        project = body.get("project")
                        if not isinstance(force, bool):
                            raise ControlError("force must be boolean")
                        if not isinstance(any_project, bool):
                            raise ControlError("any must be boolean")
                        if not any_project and not isinstance(project, str):
                            raise ControlError("project is required unless any=true")
                        self._send(
                            200,
                            controller.stop(
                                project=project if isinstance(project, str) else None,
                                any_project=any_project,
                                force=force,
                            ),
                        )
                        return
                    project = body.get("project")
                    force = body.get("force", False)
                    if not isinstance(project, str):
                        raise ControlError("project is required")
                    if not isinstance(force, bool):
                        raise ControlError("force must be boolean")
                    self._send(200, controller.restart(project, force=force))
                    return
                self._send(404, {"error": "not found"})
            except ControlError as exc:
                self._send(exc.status, {"error": str(exc)})
            except Exception as exc:
                self._send(500, {"error": f"internal error: {exc}"})

        def do_GET(self) -> None:
            self._dispatch()

        def do_POST(self) -> None:
            self._dispatch()

    return Handler


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Narrow UnrealEditor process control daemon")
    p.add_argument("--host", default=os.environ.get("UE_EDITOR_CONTROL_HOST", "0.0.0.0"))
    p.add_argument("--port", type=int, default=int(os.environ.get("UE_EDITOR_CONTROL_PORT", "8766")))
    p.add_argument("--project-root", type=Path, default=Path(os.environ.get("UE_EDITOR_PROJECT_ROOT", "/projects")))
    p.add_argument("--engine-root", type=Path, default=Path(os.environ.get("UE_EDITOR_ENGINE_ROOT", "/opt/ue/5.8")))
    p.add_argument(
        "--log-file",
        type=Path,
        default=Path(os.environ.get("UE_EDITOR_LOG", "/home/ubuntu/.local/state/ue-editor/UnrealEditor.log")),
    )
    p.add_argument("--stop-timeout", type=float, default=float(os.environ.get("UE_EDITOR_STOP_TIMEOUT", "15")))
    p.add_argument("--startup-grace", type=float, default=float(os.environ.get("UE_EDITOR_STARTUP_GRACE", "0.5")))
    p.add_argument("--request-timeout", type=float, default=float(os.environ.get("UE_EDITOR_REQUEST_TIMEOUT", "5")))
    p.add_argument("--log-max-mb", type=float, default=float(os.environ.get("UE_EDITOR_LOG_MAX_MB", "64")))
    p.add_argument("--log-backups", type=int, default=int(os.environ.get("UE_EDITOR_LOG_BACKUPS", "3")))
    p.add_argument("--log-rotate-interval", type=float, default=float(os.environ.get("UE_EDITOR_LOG_ROTATE_INTERVAL", "30")))
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    controller = EditorController(
        args.project_root,
        args.engine_root,
        args.log_file,
        stop_timeout=args.stop_timeout,
        startup_grace=args.startup_grace,
    )
    max_log_bytes = max(0, int(args.log_max_mb * 1024 * 1024))
    rotator = LogRotator(
        args.log_file,
        max_bytes=max_log_bytes,
        backups=max(args.log_backups, 0),
        interval=args.log_rotate_interval,
    )
    if max_log_bytes > 0 and args.log_backups > 0:
        rotate_log_once(args.log_file, max_bytes=max_log_bytes, backups=args.log_backups)
        rotator.start()

    server = ThreadingHTTPServer(
        (args.host, args.port),
        make_handler(controller, request_timeout=args.request_timeout),
    )
    server.daemon_threads = True
    print(f"ue-editor-control {CONTROL_VERSION} listening on {args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        rotator.stop()
        if rotator.is_alive():
            rotator.join(timeout=2)
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
