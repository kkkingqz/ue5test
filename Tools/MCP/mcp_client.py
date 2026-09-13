#!/usr/bin/env python3
"""Unreal Engine MCP (Model Context Protocol) Client for GV2.

Provides JSON-RPC 2.0 communication over HTTP/SSE with Unreal Editor's
ModelContextProtocol plugin (default port 8000).
"""

from __future__ import annotations

import json
import os
import queue
import sys
import threading
import time
import urllib.error
import urllib.request
from typing import Any, Dict, List, Optional

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


DEFAULT_MCP_URL = os.environ.get("UNREAL_MCP_URL", "http://127.0.0.1:8000/mcp")


def _parse_sse_stream(line_iterator) -> Dict[str, Any]:
    """Parses an SSE stream into a JSON object.

    Handles:
    - single event with 'data: {...}'
    - short events ('data:{}', 'data: 1')
    - multiple chunks / multi-line data
    - ignores SSE comments (lines starting with ':')
    - raises RuntimeError on malformed JSON
    - raises ConnectionError if stream ends before complete data event is received
    """
    data_lines: List[str] = []

    for raw_line in line_iterator:
        if isinstance(raw_line, bytes):
            line = raw_line.decode("utf-8")
        else:
            line = raw_line
        line = line.rstrip("\r\n")

        if line == "":
            if data_lines:
                combined = "\n".join(data_lines)
                try:
                    return json.loads(combined)
                except json.JSONDecodeError as e:
                    raise RuntimeError(f"Malformed JSON in SSE event: {e}\nPayload: {combined}") from e
        elif line.startswith(":"):
            # Comment line in SSE, ignore
            continue
        elif line.startswith("data:"):
            content = line[5:]
            if content.startswith(" "):
                content = content[1:]
            data_lines.append(content)

    if data_lines:
        combined = "\n".join(data_lines)
        try:
            return json.loads(combined)
        except json.JSONDecodeError as e:
            raise RuntimeError(f"Malformed JSON in SSE event: {e}\nPayload: {combined}") from e

    raise ConnectionError("SSE stream ended before receiving complete data event")


class UnrealMcpClient:
    """Client for interacting with Unreal Editor's MCP Server."""

    def __init__(self, url: str = DEFAULT_MCP_URL, timeout: float = 120.0, auto_initialize: bool = True):
        self.url = url
        self.timeout = timeout
        self.session_id: Optional[str] = None
        self.req_id = 0
        self.last_response_id: Optional[int] = None
        if HAS_REQUESTS:
            self.session = requests.Session()
        else:
            self.session = None
        if auto_initialize:
            self.initialize()

    def _send_raw(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        self.req_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": self.req_id,
            "method": method
        }
        if params is not None:
            payload["params"] = params

        headers = {
            "Content-Type": "application/json",
            "Accept": "text/event-stream, application/json"
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id

        request_id = self.req_id
        outcome: queue.Queue[tuple[bool, Any]] = queue.Queue(maxsize=1)

        def perform_request() -> None:
            try:
                outcome.put((True, self._send_raw_blocking(method, payload, headers)))
            except Exception as exc:
                outcome.put((False, exc))

        threading.Thread(target=perform_request, daemon=True).start()
        try:
            succeeded, value = outcome.get(timeout=self.timeout)
        except queue.Empty as exc:
            raise TimeoutError(
                f"MCP request exceeded absolute deadline of {self.timeout}s for {method}"
            ) from exc

        if not succeeded:
            raise value

        if not isinstance(value, dict):
            raise RuntimeError(
                f"Malformed JSON-RPC response for {method}: expected object, got {type(value).__name__}"
            )
        if value.get("jsonrpc") != "2.0":
            raise RuntimeError(f"Malformed JSON-RPC response for {method}: missing jsonrpc='2.0'")
        if value.get("id") != request_id:
            raise RuntimeError(
                f"MCP response id mismatch for {method}: expected {request_id}, got {value.get('id')!r}"
            )

        self.last_response_id = request_id
        return value

    def _send_raw_blocking(
        self,
        method: str,
        payload: Dict[str, Any],
        headers: Dict[str, str],
    ) -> Dict[str, Any]:
        """Performs one blocking HTTP exchange; _send_raw owns the absolute deadline."""

        if HAS_REQUESTS and self.session is not None:
            try:
                r = self.session.post(self.url, json=payload, headers=headers, stream=True, timeout=self.timeout)
                r.raise_for_status()
                if not self.session_id and "Mcp-Session-Id" in r.headers:
                    self.session_id = r.headers["Mcp-Session-Id"]

                content_type = r.headers.get("content-type", "")
                if "text/event-stream" in content_type:
                    return _parse_sse_stream(r.iter_lines())
                else:
                    if not r.text or not r.text.strip():
                        raise ConnectionError("Empty response received from MCP server")
                    try:
                        return r.json()
                    except json.JSONDecodeError as e:
                        raise RuntimeError(f"Malformed JSON response from MCP server: {e}\nBody: {r.text}") from e
            except requests.exceptions.Timeout as e:
                raise TimeoutError(f"MCP request timed out after {self.timeout}s for {method}") from e
            except requests.exceptions.ConnectionError as e:
                raise ConnectionError(
                    f"Failed to connect to Unreal Editor MCP server at {self.url}. "
                    f"Ensure Unreal Editor is running with ModelContextProtocol enabled on port 8000."
                ) from e
            except (ConnectionError, TimeoutError, RuntimeError):
                raise
            except Exception as e:
                raise RuntimeError(f"MCP request failed for {method}: {e}") from e
        else:
            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(self.url, data=data, headers=headers, method="POST")
            try:
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    if not self.session_id:
                        session_header = resp.headers.get("Mcp-Session-Id")
                        if session_header:
                            self.session_id = session_header

                    content_type = resp.headers.get("Content-Type", "")
                    if "text/event-stream" in content_type:
                        def _urllib_lines():
                            while True:
                                raw = resp.readline()
                                if not raw:
                                    break
                                yield raw
                        return _parse_sse_stream(_urllib_lines())
                    else:
                        body = resp.read().decode("utf-8")
                        if not body or not body.strip():
                            raise ConnectionError("Empty response received from MCP server")
                        try:
                            return json.loads(body)
                        except json.JSONDecodeError as e:
                            raise RuntimeError(f"Malformed JSON response from MCP server: {e}\nBody: {body}") from e
            except urllib.error.HTTPError as e:
                error_body = e.read().decode("utf-8") if e.fp else ""
                raise RuntimeError(f"MCP HTTP {e.code} Error for {method}: {error_body}") from e
            except urllib.error.URLError as e:
                reason_str = str(e.reason).lower() if e.reason else ""
                if "timed out" in reason_str:
                    raise TimeoutError(f"MCP request timed out after {self.timeout}s for {method}: {e.reason}") from e
                raise ConnectionError(
                    f"Failed to connect to Unreal Editor MCP server at {self.url}. "
                    f"Ensure Unreal Editor is running with ModelContextProtocol enabled. Reason: {e.reason}"
                ) from e
            except (ConnectionError, TimeoutError, RuntimeError):
                raise
            except Exception as e:
                raise RuntimeError(f"MCP request failed for {method}: {e}") from e

    def initialize(self) -> Dict[str, Any]:
        """Initializes the MCP session with the Unreal Editor."""
        res = self._send_raw("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "gv2-mcp-client", "version": "1.0"}
        })
        try:
            self._send_raw("notifications/initialized")
        except Exception:
            pass
        return res

    def list_toolsets(self) -> List[str]:
        """Returns the list of available toolset names."""
        res = self._send_raw("tools/call", {"name": "list_toolsets", "arguments": {}})
        content = res.get("result", {}).get("content", [])
        if not content:
            return []
        text = content[0].get("text", "")
        toolsets = []
        for line in text.splitlines():
            if line.startswith("- "):
                toolset_name = line[2:].split(":")[0].strip()
                toolsets.append(toolset_name)
        return toolsets

    def describe_toolset(self, toolset_name: str) -> Dict[str, Any]:
        """Returns details and tools schema for a given toolset."""
        res = self._send_raw("tools/call", {
            "name": "describe_toolset",
            "arguments": {"toolset_name": toolset_name}
        })
        content = res.get("result", {}).get("content", [])
        if content and "text" in content[0]:
            try:
                return json.loads(content[0]["text"])
            except Exception:
                return {"raw": content[0]["text"]}
        return res

    def call_tool(self, toolset_name: str, tool_name: str, arguments: Dict[str, Any]) -> Any:
        """Invokes a specific tool on a toolset and parses the result."""
        res = self._send_raw("tools/call", {
            "name": "call_tool",
            "arguments": {
                "toolset_name": toolset_name,
                "tool_name": tool_name,
                "arguments": arguments
            }
        })
        if "error" in res:
            raise RuntimeError(f"MCP Tool Error [{toolset_name}.{tool_name}]: {res['error']}")

        result = res.get("result", {})
        content = result.get("content", [])
        for item in content:
            if item.get("type") == "text":
                text_val = item.get("text", "")
                try:
                    return json.loads(text_val)
                except Exception:
                    return text_val
        return result

    # --- Automation Test Toolset Helpers ---

    def discover_tests(self, force_rediscover: bool = False) -> Any:
        """Discovers automation tests in the running Unreal Editor."""
        return self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "DiscoverTests",
            {"bForceRediscover": force_rediscover}
        )

    def list_tests(self, name_filter: str = "", tag_filter: str = "", limit: int = 0) -> List[str]:
        """Lists available automation tests matching the filter (limit=0 means unlimited)."""
        res = self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "ListTests",
            {"nameFilter": name_filter, "tagFilter": tag_filter, "limit": limit}
        )
        if isinstance(res, dict) and "returnValue" in res:
            try:
                parsed = json.loads(res["returnValue"])
                return parsed.get("tests", [])
            except Exception:
                pass
        elif isinstance(res, dict) and "tests" in res:
            return res.get("tests", [])
        return []

    def run_tests_by_filter(self, filter_expr: str) -> Dict[str, Any]:
        """Runs automation tests matching a filter (e.g. 'StartsWith:GV2')."""
        res = self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "RunTestsByFilter",
            {"filterExpression": filter_expr}
        )
        parsed: Dict[str, Any]
        if isinstance(res, dict) and "returnValue" in res:
            val = res["returnValue"]
            if isinstance(val, str):
                try:
                    parsed = json.loads(val)
                except Exception:
                    parsed = {"raw": val}
            elif isinstance(val, dict):
                parsed = val
            else:
                parsed = {"raw": res}
        elif isinstance(res, dict):
            parsed = res
        else:
            parsed = {"raw": res}

        if isinstance(parsed, dict) and "tests" in parsed and "schema_version" not in parsed:
            parsed["schema_version"] = 1
        return parsed

    def get_test_status(self, task_id: str) -> Dict[str, Any]:
        """Retrieves lightweight status snapshot for the specified automation test task_id."""
        if not task_id or not isinstance(task_id, str) or not task_id.strip():
            raise ValueError("Calling GetTestStatus without an exact task_id is forbidden.")
        tid = task_id.strip()
        res = self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "GetTestStatus",
            {"taskId": tid, "task_id": tid}
        )
        parsed: Dict[str, Any]
        if isinstance(res, dict) and "returnValue" in res:
            val = res["returnValue"]
            if isinstance(val, str):
                try:
                    parsed = json.loads(val)
                except Exception:
                    parsed = {"raw": val}
            elif isinstance(val, dict):
                parsed = val
            else:
                parsed = {"raw": res}
        elif isinstance(res, dict):
            parsed = res
        else:
            parsed = {"raw": res}

        if isinstance(parsed, dict):
            ret_id = parsed.get("taskId") or parsed.get("task_id")
            if not isinstance(ret_id, str) or not ret_id.strip():
                raise ValueError(f"GetTestStatus response is missing exact task_id '{tid}'")
            if ret_id.strip() != tid:
                raise ValueError(
                    f"Mismatched task_id in GetTestStatus: expected '{tid}', got '{ret_id}'"
                )
        return parsed

    def get_test_results(self, task_id: str) -> Dict[str, Any]:
        """Retrieves detailed results for the specified automation test task_id.

        Generic 'latest result' retrieval without task_id is strictly forbidden.
        """
        if not task_id or not isinstance(task_id, str) or not task_id.strip():
            raise ValueError("Calling GetTestResults without an exact task_id is forbidden.")
        tid = task_id.strip()
        res = self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "GetTestResults",
            {"taskId": tid, "task_id": tid}
        )
        parsed: Dict[str, Any]
        if isinstance(res, dict) and "returnValue" in res:
            val = res["returnValue"]
            if isinstance(val, str):
                try:
                    parsed = json.loads(val)
                except Exception:
                    parsed = {"raw": val}
            elif isinstance(val, dict):
                parsed = val
            else:
                parsed = {"raw": res}
        elif isinstance(res, dict):
            parsed = res
        else:
            parsed = {"raw": res}

        if isinstance(parsed, dict):
            ret_id = parsed.get("taskId") or parsed.get("task_id")
            if not isinstance(ret_id, str) or not ret_id.strip():
                raise ValueError(f"GetTestResults response is missing exact task_id '{tid}'")
            if ret_id.strip() != tid:
                raise ValueError(
                    f"Mismatched task_id in GetTestResults: expected '{tid}', got '{ret_id}'"
                )
            if "tests" in parsed:
                parsed["task_id"] = tid
                if "schema_version" not in parsed:
                    parsed["schema_version"] = 1
        return parsed


if __name__ == "__main__":
    try:
        client = UnrealMcpClient()
        print(f"Connected to Unreal MCP Server. Session ID: {client.session_id}")
        toolsets = client.list_toolsets()
        print(f"Available toolsets ({len(toolsets)}):")
        for ts in toolsets:
            print(f"  - {ts}")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
