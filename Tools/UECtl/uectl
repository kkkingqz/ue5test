#!/usr/bin/env python3
"""uectl - thin Unreal Engine build/editor control client.

No authentication is implemented. The client is intended for an isolated Docker
network where ue-build-service and ue-editor-control are not exposed to untrusted
networks.
"""
from __future__ import annotations

import argparse
import http.client as httpclient
import json
import math
import os
import re
import sys
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Optional, TypeVar
from urllib import error as urlerror
from urllib import request as urlrequest
from urllib.parse import urlsplit

UECTL_VERSION = "0.2.2"
UECTL_API_VERSION = "1"
RETRYABLE_HTTP_STATUSES = {408, 425, 429, 500, 502, 503, 504}
TERMINAL_STATES = {"succeeded", "failed", "cancelled"}
KNOWN_JOB_STATES = {"created", "validated", "queued", "running", *TERMINAL_STATES}
REQUEST_ID_RE = re.compile(r"^[A-Za-z0-9._:-]{1,128}$")
JOB_ID_RE = re.compile(r"^[A-Za-z0-9._:-]{1,128}$")

EXIT_OK = 0
EXIT_GENERAL = 1
EXIT_BAD_REQUEST = 2
EXIT_UNAVAILABLE = 3
EXIT_CANCELLED = 4
EXIT_BUILD_FAILED = 6

T = TypeVar("T")


@dataclass(frozen=True)
class ProjectContext:
    project: str
    workspace: str


class UectlError(RuntimeError):
    def __init__(
        self,
        message: str,
        exit_code: int = EXIT_GENERAL,
        *,
        retryable: bool = False,
    ):
        super().__init__(message)
        self.exit_code = exit_code
        self.retryable = retryable




class NoRedirectHandler(urlrequest.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class HttpClient:
    def __init__(self, base_url: str, timeout: float = 10.0):
        base_url = base_url.rstrip("/")
        try:
            parsed = urlsplit(base_url)
            port = parsed.port
        except ValueError as exc:
            raise UectlError(f"invalid endpoint URL: {base_url!r}", EXIT_BAD_REQUEST) from exc
        if parsed.scheme not in {"http", "https"} or not parsed.hostname:
            raise UectlError(f"invalid endpoint URL: {base_url!r}", EXIT_BAD_REQUEST)
        if parsed.query or parsed.fragment:
            raise UectlError("endpoint URL must not contain query or fragment", EXIT_BAD_REQUEST)
        if parsed.username is not None or parsed.password is not None:
            raise UectlError("endpoint URL must not contain credentials", EXIT_BAD_REQUEST)
        if port is not None and not (1 <= port <= 65535):
            raise UectlError(f"invalid endpoint URL port: {port}", EXIT_BAD_REQUEST)
        self.base_url = base_url
        self.timeout = timeout
        # Control-plane requests must stay on the isolated Docker network.
        # Never inherit HTTP(S)_PROXY from the Agent Canvas environment.
        self.opener = urlrequest.build_opener(urlrequest.ProxyHandler({}), NoRedirectHandler())

    def _request(
        self,
        method: str,
        path: str,
        body: Optional[dict[str, Any]] = None,
        headers: Optional[dict[str, str]] = None,
    ) -> tuple[bytes, str, dict[str, str]]:
        payload = None
        merged_headers = {
            "Accept": "application/json, text/plain;q=0.9",
            "User-Agent": f"uectl/{UECTL_VERSION}",
            "X-UECTL-API-Version": UECTL_API_VERSION,
            "X-UECTL-Client-Version": UECTL_VERSION,
        }
        if body is not None:
            payload = json.dumps(body, separators=(",", ":")).encode("utf-8")
            merged_headers["Content-Type"] = "application/json"
        if headers:
            merged_headers.update(headers)

        req = urlrequest.Request(
            f"{self.base_url}{path}",
            data=payload,
            headers=merged_headers,
            method=method,
        )
        try:
            with self.opener.open(req, timeout=self.timeout) as resp:
                return (
                    resp.read(),
                    resp.headers.get_content_type(),
                    {k.lower(): v for k, v in resp.headers.items()},
                )
        except urlerror.HTTPError as exc:
            try:
                raw = exc.read(64 * 1024)
            except (httpclient.HTTPException, TimeoutError, OSError, ConnectionError):
                raw = b""
            detail = raw.decode("utf-8", errors="replace").strip()
            if detail:
                try:
                    parsed = json.loads(detail)
                    if isinstance(parsed, dict):
                        candidate = parsed.get("error") or parsed.get("message")
                        if candidate is not None:
                            detail = str(candidate)
                except json.JSONDecodeError:
                    pass
            message = f"HTTP {exc.code} from {self.base_url}{path}"
            if detail:
                message += f": {detail}"
            retryable = exc.code in RETRYABLE_HTTP_STATUSES
            if retryable:
                code = EXIT_UNAVAILABLE
            elif 400 <= exc.code < 500:
                code = EXIT_BAD_REQUEST
            else:
                code = EXIT_GENERAL
            raise UectlError(message, code, retryable=retryable) from exc
        except (httpclient.HTTPException, urlerror.URLError, TimeoutError, ConnectionError, OSError) as exc:
            raise UectlError(
                f"service unavailable at {self.base_url}: {exc}",
                EXIT_UNAVAILABLE,
                retryable=True,
            ) from exc

    def json(
        self,
        method: str,
        path: str,
        body: Optional[dict[str, Any]] = None,
        headers: Optional[dict[str, str]] = None,
    ) -> dict[str, Any]:
        raw, _, _ = self._request(method, path, body=body, headers=headers)
        if not raw:
            return {}
        try:
            obj = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise UectlError(f"invalid JSON response from {self.base_url}{path}") from exc
        if not isinstance(obj, dict):
            raise UectlError(f"expected JSON object from {self.base_url}{path}")
        return obj

    def text(self, method: str, path: str) -> str:
        raw, _, _ = self._request(method, path)
        return raw.decode("utf-8", errors="replace")


def detect_context(cwd: Path, project_root: Path, worktree_root: Path) -> ProjectContext:
    """Resolve logical project/workspace from a container path."""
    cwd = cwd.resolve(strict=False)
    project_root = project_root.resolve(strict=False)
    worktree_root = worktree_root.resolve(strict=False)

    try:
        rel = cwd.relative_to(project_root)
    except ValueError:
        rel = None
    if rel is not None:
        if not rel.parts:
            raise ValueError(f"current directory {cwd} is the project root, not a project")
        return ProjectContext(project=rel.parts[0], workspace="main")

    try:
        rel = cwd.relative_to(worktree_root)
    except ValueError:
        rel = None
    if rel is not None:
        if len(rel.parts) < 2:
            raise ValueError(f"current directory {cwd} is not inside a worktree")
        return ProjectContext(project=rel.parts[0], workspace=rel.parts[1])

    raise ValueError(f"current directory {cwd} is outside {project_root} and {worktree_root}")


def env_float(
    name: str,
    default: float,
    *,
    minimum: float = 0.0,
    allow_equal: bool = False,
) -> float:
    try:
        value = float(os.environ.get(name, str(default)))
    except ValueError as exc:
        raise UectlError(f"{name} must be a number", EXIT_BAD_REQUEST) from exc
    if not math.isfinite(value):
        raise UectlError(f"{name} must be finite", EXIT_BAD_REQUEST)
    valid = value >= minimum if allow_equal else value > minimum
    if not valid:
        op = ">=" if allow_equal else ">"
        raise UectlError(f"{name} must be {op} {minimum:g}", EXIT_BAD_REQUEST)
    return value


def env_int(name: str, default: int) -> int:
    try:
        value = int(os.environ.get(name, str(default)))
    except ValueError as exc:
        raise UectlError(f"{name} must be an integer", EXIT_BAD_REQUEST) from exc
    if value < 1:
        raise UectlError(f"{name} must be >= 1", EXIT_BAD_REQUEST)
    return value


def config() -> dict[str, Any]:
    return {
        "endpoint": os.environ.get("UECTL_ENDPOINT", "http://ue-build-service:8765"),
        "editor_endpoint": os.environ.get("UECTL_EDITOR_ENDPOINT", "http://ue-editor:8766"),
        "project_root": Path(os.environ.get("UECTL_PROJECT_ROOT", "/projects")),
        "worktree_root": Path(os.environ.get("UECTL_WORKTREE_ROOT", "/worktrees")),
        "timeout": env_float("UECTL_HTTP_TIMEOUT", 10.0),
        "editor_timeout": env_float("UECTL_EDITOR_HTTP_TIMEOUT", 30.0),
        "log_timeout": env_float("UECTL_LOG_HTTP_TIMEOUT", 1.0),
        "poll_interval": env_float("UECTL_POLL_INTERVAL", 0.5),
        "retry_attempts": env_int("UECTL_RETRY_ATTEMPTS", 5),
        "retry_backoff": env_float("UECTL_RETRY_BACKOFF", 0.25, minimum=0.0, allow_equal=True),
    }


def resolve_context(args: argparse.Namespace, cfg: dict[str, Any]) -> ProjectContext:
    project = getattr(args, "project", None)
    workspace = getattr(args, "workspace", None)
    if project:
        return ProjectContext(project=project, workspace=workspace or "main")
    try:
        ctx = detect_context(Path.cwd(), cfg["project_root"], cfg["worktree_root"])
    except ValueError as exc:
        raise UectlError(str(exc), EXIT_BAD_REQUEST) from exc
    if workspace:
        return ProjectContext(project=ctx.project, workspace=workspace)
    return ctx


def emit_json(obj: dict[str, Any]) -> None:
    print(json.dumps(obj, ensure_ascii=False, separators=(",", ":")))


def job_exit_code(metadata: dict[str, Any]) -> int:
    state = str(metadata.get("state", "failed"))
    if state == "succeeded":
        return EXIT_OK
    if state == "cancelled":
        return EXIT_CANCELLED
    remote = metadata.get("exit_code")
    if isinstance(remote, int) and 10 <= remote <= 255:
        return remote
    return EXIT_BUILD_FAILED


def validate_request_id(value: Optional[str]) -> str:
    request_id = value or str(uuid.uuid4())
    if not REQUEST_ID_RE.fullmatch(request_id):
        raise UectlError(
            "request-id must be 1..128 characters from [A-Za-z0-9._:-]",
            EXIT_BAD_REQUEST,
        )
    return request_id




def validate_job_id(value: str) -> str:
    if value in {".", ".."} or not JOB_ID_RE.fullmatch(value):
        raise UectlError(
            "job-id must be 1..128 characters from [A-Za-z0-9._:-] and may not be '.' or '..'",
            EXIT_BAD_REQUEST,
        )
    return value


def job_state(metadata: dict[str, Any]) -> str:
    state = metadata.get("state")
    if not isinstance(state, str) or state not in KNOWN_JOB_STATES:
        raise UectlError(f"unknown job state from build service: {state!r}")
    return state

def retry_call(
    fn: Callable[[], T],
    *,
    attempts: int,
    backoff: float,
) -> T:
    for attempt in range(1, attempts + 1):
        try:
            return fn()
        except UectlError as exc:
            if not exc.retryable or attempt >= attempts:
                raise
            if backoff > 0:
                time.sleep(backoff * min(2 ** (attempt - 1), 8))
    raise AssertionError("unreachable")


def create_job(
    client: HttpClient,
    operation: str,
    payload: dict[str, Any],
    *,
    request_id: str,
    retry_attempts: int,
    retry_backoff: float,
) -> str:
    def do_create() -> dict[str, Any]:
        return client.json(
            "POST",
            f"/v1/jobs/{operation}",
            body=payload,
            headers={"X-UECTL-Request-ID": request_id},
        )

    obj = retry_call(do_create, attempts=retry_attempts, backoff=retry_backoff)
    job_id = obj.get("job_id")
    if not isinstance(job_id, str) or not job_id:
        raise UectlError("build service did not return required job_id")
    return validate_job_id(job_id)


def fetch_log_delta(
    client: HttpClient,
    job_id: str,
    seen: int,
    *,
    retry_attempts: int = 1,
    retry_backoff: float = 0.0,
) -> tuple[str, int, Optional[int]]:
    job_id = validate_job_id(job_id)

    def fetch_offset() -> tuple[bytes, str, dict[str, str]]:
        return client._request("GET", f"/v1/jobs/{job_id}/logs?offset={seen}")

    raw, content_type, headers = retry_call(
        fetch_offset, attempts=retry_attempts, backoff=retry_backoff
    )
    next_header = headers.get("x-uectl-next-offset")
    if next_header is not None:
        try:
            next_offset = int(next_header)
        except ValueError as exc:
            raise UectlError("invalid X-UECTL-Next-Offset from build service") from exc
        if next_offset < seen or next_offset != seen + len(raw):
            raise UectlError(
                f"invalid log offset from build service: requested {seen}, received next {next_offset} for {len(raw)} bytes"
            )
        size_header = headers.get("x-uectl-log-size")
        log_size: Optional[int] = None
        if size_header is not None:
            try:
                log_size = int(size_header)
            except ValueError as exc:
                raise UectlError("invalid X-UECTL-Log-Size from build service") from exc
            if log_size < next_offset:
                raise UectlError(
                    f"invalid log size from build service: size {log_size} is below next offset {next_offset}"
                )
        return raw.decode("utf-8", errors="replace"), next_offset, log_size

    # Transitional compatibility with pre-v1 servers. If the server ignored
    # ?offset= but still returned text/plain, treat this response as the full
    # legacy log. Otherwise re-request /logs for old path-based routers.
    if content_type == "text/plain":
        if len(raw) < seen:
            return raw.decode("utf-8", errors="replace"), len(raw), None
        return raw[seen:].decode("utf-8", errors="replace"), len(raw), None

    legacy_raw, _, _ = retry_call(
        lambda: client._request("GET", f"/v1/jobs/{job_id}/logs"),
        attempts=retry_attempts,
        backoff=retry_backoff,
    )
    if len(legacy_raw) < seen:
        return legacy_raw.decode("utf-8", errors="replace"), len(legacy_raw), None
    return legacy_raw[seen:].decode("utf-8", errors="replace"), len(legacy_raw), None


def validate_job_metadata(
    metadata: dict[str, Any],
    *,
    job_id: str,
    project: Optional[str] = None,
    workspace: Optional[str] = None,
    operation: Optional[str] = None,
) -> None:
    expected = {
        "job_id": job_id,
        "project": project,
        "workspace": workspace,
        "operation": operation,
    }
    for key, wanted in expected.items():
        if wanted is None or key not in metadata:
            continue
        actual = metadata.get(key)
        if actual != wanted:
            raise UectlError(
                f"job metadata mismatch for {key}: expected {wanted!r}, got {actual!r}"
            )


def wait_job(
    client: HttpClient,
    job_id: str,
    *,
    project: Optional[str],
    workspace: Optional[str],
    operation: Optional[str],
    log_client: Optional[HttpClient],
    stream: bool,
    json_mode: bool,
    poll_interval: float,
    retry_attempts: int,
    retry_backoff: float,
) -> dict[str, Any]:
    seen = 0
    log_out = sys.stderr if json_mode else sys.stdout
    last_log_error: Optional[str] = None

    def try_stream_log() -> None:
        nonlocal seen, last_log_error
        if log_client is None:
            return
        try:
            # Live log delivery is best-effort. Do not let it delay authoritative
            # job metadata polling with its own retry/backoff loop.
            delta, seen, _ = fetch_log_delta(log_client, job_id, seen)
        except UectlError as exc:
            message = str(exc)
            if message != last_log_error:
                print(f"uectl: warning: build log unavailable: {message}", file=sys.stderr, flush=True)
                last_log_error = message
            return
        last_log_error = None
        if delta:
            print(delta, end="", file=log_out, flush=True)

    while True:
        meta = retry_call(
            lambda: client.json("GET", f"/v1/jobs/{job_id}"),
            attempts=retry_attempts,
            backoff=retry_backoff,
        )
        validate_job_metadata(
            meta, job_id=job_id, project=project, workspace=workspace, operation=operation
        )
        state = job_state(meta)
        if stream:
            try_stream_log()
        if state in TERMINAL_STATES:
            if stream and last_log_error is None:
                # Terminal metadata is published only after the server closes and
                # flushes the log. Drain successful offset streams to EOF, but do
                # not retry a live-log failure merely because the job is terminal.
                while True:
                    before = seen
                    try_stream_log()
                    if seen == before:
                        break
            return meta
        time.sleep(poll_interval)


def human_job_header(
    operation: str,
    ctx: ProjectContext,
    job_id: str,
    request_id: str,
    payload: dict[str, Any],
) -> None:
    print(f"Operation:     {operation}")
    print(f"Project:       {ctx.project}")
    print(f"Workspace:     {ctx.workspace}")
    if payload.get("target"):
        print(f"Target:        {payload['target']}")
    if payload.get("configuration"):
        print(f"Configuration: {payload['configuration']}")
    print(f"Job:           {job_id}")
    print(f"Request ID:    {request_id}")
    print()


def _emit_job_client_error(
    *,
    json_mode: bool,
    error: UectlError,
    request_id: str,
    job_id: Optional[str] = None,
) -> None:
    if not json_mode:
        return
    obj: dict[str, Any] = {
        "state": "client_error",
        "request_id": request_id,
        "error": str(error),
    }
    if job_id:
        obj["job_id"] = job_id
    emit_json(obj)


def _emit_job_interrupted(
    *,
    json_mode: bool,
    request_id: str,
    job_id: Optional[str] = None,
) -> None:
    if json_mode:
        obj: dict[str, Any] = {
            "state": "client_interrupted",
            "request_id": request_id,
        }
        if job_id is None:
            obj["job_may_exist"] = True
        else:
            obj["job_id"] = job_id
            obj["job_continues"] = True
        emit_json(obj)
        return
    if job_id is None:
        print(
            f"uectl: interrupted before create response; remote job may exist; "
            f"request-id: {request_id}; retry with --request-id {request_id}",
            file=sys.stderr,
        )
    else:
        print(
            f"uectl: interrupted; remote job continues; job-id: {job_id}, "
            f"request-id: {request_id}",
            file=sys.stderr,
        )


def run_job(args: argparse.Namespace, operation: str, extra: dict[str, Any]) -> int:
    cfg = config()
    ctx = resolve_context(args, cfg)
    payload: dict[str, Any] = {"project": ctx.project, "workspace": ctx.workspace}
    payload.update({k: v for k, v in extra.items() if v is not None})
    client = HttpClient(cfg["endpoint"], cfg["timeout"])
    log_client = HttpClient(cfg["endpoint"], cfg["log_timeout"])
    json_mode = bool(getattr(args, "json", False))
    no_stream = bool(getattr(args, "no_stream", False))
    request_id = validate_request_id(getattr(args, "request_id", None))

    try:
        job_id = create_job(
            client,
            operation,
            payload,
            request_id=request_id,
            retry_attempts=cfg["retry_attempts"],
            retry_backoff=cfg["retry_backoff"],
        )
    except KeyboardInterrupt:
        _emit_job_interrupted(json_mode=json_mode, request_id=request_id)
        return 130
    except UectlError as exc:
        _emit_job_client_error(
            json_mode=json_mode,
            error=exc,
            request_id=request_id,
        )
        raise UectlError(
            f"{exc} (request-id: {request_id}; retry with --request-id {request_id})",
            exc.exit_code,
            retryable=exc.retryable,
        ) from exc

    if not json_mode:
        human_job_header(operation, ctx, job_id, request_id, payload)

    try:
        meta = wait_job(
            client,
            job_id,
            project=ctx.project,
            workspace=ctx.workspace,
            operation=operation,
            log_client=log_client,
            stream=not no_stream,
            json_mode=json_mode,
            poll_interval=cfg["poll_interval"],
            retry_attempts=cfg["retry_attempts"],
            retry_backoff=cfg["retry_backoff"],
        )
    except KeyboardInterrupt:
        _emit_job_interrupted(json_mode=json_mode, request_id=request_id, job_id=job_id)
        return 130
    except UectlError as exc:
        _emit_job_client_error(
            json_mode=json_mode,
            error=exc,
            request_id=request_id,
            job_id=job_id,
        )
        raise UectlError(
            f"{exc} (job-id: {job_id}, request-id: {request_id})",
            exc.exit_code,
            retryable=exc.retryable,
        ) from exc

    meta.setdefault("job_id", job_id)
    meta.setdefault("request_id", request_id)
    meta.setdefault("project", ctx.project)
    meta.setdefault("workspace", ctx.workspace)

    if json_mode:
        emit_json(meta)
    else:
        state = str(meta.get("state", "unknown")).upper()
        print()
        print(state)
        if "exit_code" in meta:
            print(f"Exit code: {meta['exit_code']}")
        if "duration_ms" in meta:
            duration = meta.get("duration_ms")
            if isinstance(duration, (int, float)) and not isinstance(duration, bool) and math.isfinite(float(duration)) and duration >= 0:
                print(f"Duration: {float(duration) / 1000:.3f}s")
            else:
                print("uectl: warning: ignoring invalid duration_ms in server response", file=sys.stderr)
    return job_exit_code(meta)


def cmd_status(args: argparse.Namespace) -> int:
    cfg = config()
    client = HttpClient(cfg["endpoint"], cfg["timeout"])
    obj = retry_call(
        lambda: client.json("GET", "/v1/status"),
        attempts=cfg["retry_attempts"],
        backoff=cfg["retry_backoff"],
    )
    ready = obj.get("ready") is True
    if args.json:
        emit_json(obj)
    else:
        print(f"Service:      {obj.get('service', 'ue-build-service')}")
        print(f"Ready:        {ready}")
        if "active_jobs" in obj:
            print(f"Active jobs:  {obj['active_jobs']}")
        if "queued_jobs" in obj:
            print(f"Queued jobs:  {obj['queued_jobs']}")
        engines = obj.get("engines")
        if isinstance(engines, (list, tuple)):
            print(f"Engines:      {', '.join(map(str, engines))}")
    return EXIT_OK if ready else EXIT_GENERAL


def cmd_logs(args: argparse.Namespace) -> int:
    cfg = config()
    job_id = validate_job_id(args.job_id)
    client = HttpClient(cfg["endpoint"], cfg["timeout"])
    log_client = HttpClient(cfg["endpoint"], cfg["log_timeout"])

    if not args.follow:
        seen = 0
        snapshot_end: Optional[int] = None
        while True:
            delta, next_seen, log_size = retry_call(
                lambda: fetch_log_delta(log_client, job_id, seen),
                attempts=cfg["retry_attempts"],
                backoff=cfg["retry_backoff"],
            )
            if snapshot_end is None and log_size is not None:
                snapshot_end = log_size
            if delta:
                print(delta, end="")
            if snapshot_end is not None and next_seen >= snapshot_end:
                break
            if next_seen == seen:
                break
            seen = next_seen
        return EXIT_OK

    seen = 0
    last_log_error: Optional[str] = None
    while True:
        meta = retry_call(
            lambda: client.json("GET", f"/v1/jobs/{job_id}"),
            attempts=cfg["retry_attempts"],
            backoff=cfg["retry_backoff"],
        )
        validate_job_metadata(meta, job_id=job_id)
        state = job_state(meta)

        try:
            delta, seen, _ = fetch_log_delta(log_client, job_id, seen)
            last_log_error = None
            if delta:
                print(delta, end="", flush=True)
        except UectlError as exc:
            message = str(exc)
            if message != last_log_error:
                print(f"uectl: warning: build log unavailable: {message}", file=sys.stderr, flush=True)
                last_log_error = message

        if state in TERMINAL_STATES:
            if last_log_error is not None:
                return job_exit_code(meta)
            while True:
                before = seen
                try:
                    delta, seen, _ = fetch_log_delta(log_client, job_id, seen)
                    if delta:
                        print(delta, end="", flush=True)
                except UectlError as exc:
                    message = str(exc)
                    if message != last_log_error:
                        print(f"uectl: warning: build log unavailable: {message}", file=sys.stderr, flush=True)
                    break
                if seen == before:
                    break
            return job_exit_code(meta)
        time.sleep(cfg["poll_interval"])


def cmd_cancel(args: argparse.Namespace) -> int:
    cfg = config()
    job_id = validate_job_id(args.job_id)
    client = HttpClient(cfg["endpoint"], cfg["timeout"])
    obj = retry_call(
        lambda: client.json("POST", f"/v1/jobs/{job_id}/cancel", body={}),
        attempts=cfg["retry_attempts"],
        backoff=cfg["retry_backoff"],
    )
    if args.json:
        emit_json(obj)
    else:
        print(f"Job {job_id}: {obj.get('state', 'cancel requested')}")
    return EXIT_OK


def editor_project(args: argparse.Namespace, cfg: dict[str, Any]) -> str:
    if getattr(args, "project", None):
        return args.project
    return resolve_context(args, cfg).project


def cmd_editor(args: argparse.Namespace) -> int:
    cfg = config()
    client = HttpClient(cfg["editor_endpoint"], cfg["editor_timeout"])
    action = args.editor_command

    if action == "status":
        obj = retry_call(
            lambda: client.json("GET", "/v1/editor/status"),
            attempts=cfg["retry_attempts"],
            backoff=cfg["retry_backoff"],
        )
    elif action == "start":
        project = editor_project(args, cfg)
        obj = retry_call(
            lambda: client.json("POST", "/v1/editor/start", body={"project": project}),
            attempts=cfg["retry_attempts"],
            backoff=cfg["retry_backoff"],
        )
    elif action == "stop":
        any_project = bool(args.any)
        body: dict[str, Any] = {"force": bool(args.force), "any": any_project}
        if not any_project:
            body["project"] = editor_project(args, cfg)
        obj = retry_call(
            lambda: client.json("POST", "/v1/editor/stop", body=body),
            attempts=cfg["retry_attempts"],
            backoff=cfg["retry_backoff"],
        )
    elif action == "restart":
        project = editor_project(args, cfg)
        body = {"project": project, "force": bool(args.force)}
        # restart is not idempotent: if the daemon completed the restart but the
        # HTTP response was lost, retrying would restart the Editor a second time.
        obj = client.json("POST", "/v1/editor/restart", body=body)
    else:
        raise UectlError(f"unknown editor action: {action}", EXIT_BAD_REQUEST)

    if args.json:
        emit_json(obj)
    else:
        print(f"Editor:  {obj.get('state', 'unknown')}")
        if obj.get("project"):
            print(f"Project: {obj['project']}")
        if obj.get("pid"):
            print(f"PID:     {obj['pid']}")
        if obj.get("message"):
            print(obj["message"])
    state = obj.get("state")
    if action in {"start", "restart"}:
        return EXIT_OK if state == "running" else EXIT_GENERAL
    if action == "stop":
        return EXIT_OK if state == "stopped" else EXIT_GENERAL
    if action == "status":
        return EXIT_OK if state in {"running", "stopped"} else EXIT_GENERAL
    return EXIT_GENERAL


def cmd_version(args: argparse.Namespace) -> int:
    cfg = config()
    result: dict[str, Any] = {"client": UECTL_VERSION, "api": UECTL_API_VERSION}
    try:
        client = HttpClient(cfg["endpoint"], min(cfg["timeout"], 1.0))
        status = client.json("GET", "/v1/status")
    except UectlError:
        status = None
    if isinstance(status, dict):
        server_version = status.get("server_version") or status.get("version")
        if isinstance(server_version, (str, int, float)):
            result["server"] = str(server_version)
        api_version = status.get("api_version") or status.get("api")
        if isinstance(api_version, (str, int, float)):
            result["server_api"] = str(api_version)
        result["server_reachable"] = True
    else:
        result["server_reachable"] = False

    if args.json:
        emit_json(result)
    else:
        print(f"uectl {UECTL_VERSION}")
        if result.get("server_reachable"):
            if "server" in result:
                print(f"UE Build Service {result['server']}")
            else:
                print("UE Build Service reachable")
            if "server_api" in result:
                print(f"Server API {result['server_api']}")
    return EXIT_OK


def add_context_flags(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--project", help="logical project name; otherwise infer from cwd")
    parser.add_argument("--workspace", help="logical workspace id; otherwise infer from cwd")


def add_job_output_flags(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--json", action="store_true", help="emit final metadata as JSON; streamed logs go to stderr")
    parser.add_argument("--no-stream", action="store_true", help="do not stream build logs while waiting")
    parser.add_argument(
        "--request-id",
        help="reuse a previous idempotency key after an ambiguous create failure",
    )


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="uectl",
        description="Thin client for Unreal Engine build service and UE Editor control",
    )
    p.add_argument("--version", action="version", version=f"uectl {UECTL_VERSION}")
    sub = p.add_subparsers(dest="command", required=True)

    version = sub.add_parser("version", help="show uectl and build-service versions")
    version.add_argument("--json", action="store_true")
    version.set_defaults(func=cmd_version)

    status = sub.add_parser("status", help="show build service status")
    status.add_argument("--json", action="store_true")
    status.set_defaults(func=cmd_status)

    build = sub.add_parser("build", help="build an Unreal target")
    build.add_argument("--target", choices=("editor", "game", "client", "server"), default="editor")
    build.add_argument("--configuration", default="Development")
    add_context_flags(build)
    add_job_output_flags(build)
    build.set_defaults(func=lambda a: run_job(a, "build", {"target": a.target, "configuration": a.configuration}))

    plugin = sub.add_parser("plugin-build", help="BuildPlugin for a project plugin")
    plugin.add_argument("plugin")
    plugin.add_argument("--platform", default="Linux")
    add_context_flags(plugin)
    add_job_output_flags(plugin)
    plugin.set_defaults(func=lambda a: run_job(a, "plugin-build", {"plugin": a.plugin, "platform": a.platform}))

    test = sub.add_parser("test", help="run Unreal Automation Tests")
    test.add_argument("--filter", dest="test_filter")
    add_context_flags(test)
    add_job_output_flags(test)
    test.set_defaults(func=lambda a: run_job(a, "test", {"filter": a.test_filter}))

    cook = sub.add_parser("cook", help="cook project content")
    cook.add_argument("--platform", default="Linux")
    add_context_flags(cook)
    add_job_output_flags(cook)
    cook.set_defaults(func=lambda a: run_job(a, "cook", {"platform": a.platform}))

    package = sub.add_parser("package", help="package project")
    package.add_argument("--platform", default="Linux")
    package.add_argument("--configuration", default="Development")
    add_context_flags(package)
    add_job_output_flags(package)
    package.set_defaults(func=lambda a: run_job(a, "package", {"platform": a.platform, "configuration": a.configuration}))

    clean = sub.add_parser("clean", help="clean generated project build data through the service")
    add_context_flags(clean)
    add_job_output_flags(clean)
    clean.set_defaults(func=lambda a: run_job(a, "clean", {}))

    logs = sub.add_parser("logs", help="show saved job log")
    logs.add_argument("job_id")
    logs.add_argument("-f", "--follow", action="store_true")
    logs.set_defaults(func=cmd_logs)

    cancel = sub.add_parser("cancel", help="cancel a queued/running job")
    cancel.add_argument("job_id")
    cancel.add_argument("--json", action="store_true")
    cancel.set_defaults(func=cmd_cancel)

    editor = sub.add_parser("editor", help="control UnrealEditor process in the UE Editor container")
    editor_sub = editor.add_subparsers(dest="editor_command", required=True)

    editor_status = editor_sub.add_parser("status", help="show UnrealEditor process state")
    editor_status.add_argument("--json", action="store_true")
    editor_status.set_defaults(func=cmd_editor)

    editor_start = editor_sub.add_parser("start", help="start UnrealEditor for a project main workspace")
    editor_start.add_argument("--project", help="logical project name; otherwise infer from cwd")
    editor_start.add_argument("--json", action="store_true")
    editor_start.set_defaults(func=cmd_editor)

    editor_stop = editor_sub.add_parser("stop", help="gracefully stop UnrealEditor")
    editor_stop.add_argument("--project", help="expected running project; otherwise infer from cwd")
    editor_stop.add_argument("--any", action="store_true", help="administrative override: stop whichever single project is open")
    editor_stop.add_argument("--force", action="store_true", help="allow forced termination after graceful stop")
    editor_stop.add_argument("--json", action="store_true")
    editor_stop.set_defaults(func=cmd_editor)

    editor_restart = editor_sub.add_parser("restart", help="restart UnrealEditor for a project")
    editor_restart.add_argument("--project", help="logical project name; otherwise infer from cwd")
    editor_restart.add_argument("--force", action="store_true")
    editor_restart.add_argument("--json", action="store_true")
    editor_restart.set_defaults(func=cmd_editor)

    return p


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except UectlError as exc:
        print(f"uectl: {exc}", file=sys.stderr)
        return exc.exit_code
    except KeyboardInterrupt:
        print("uectl: interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
