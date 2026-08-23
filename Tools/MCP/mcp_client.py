#!/usr/bin/env python3
"""Unreal Engine MCP (Model Context Protocol) Client for GV2.

Provides JSON-RPC 2.0 communication over HTTP/SSE with Unreal Editor's
ModelContextProtocol plugin (default port 8000).
"""

from __future__ import annotations

import json
import os
import sys
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


class UnrealMcpClient:
    """Client for interacting with Unreal Editor's MCP Server."""

    def __init__(self, url: str = DEFAULT_MCP_URL, timeout: float = 120.0):
        self.url = url
        self.timeout = timeout
        self.session_id: Optional[str] = None
        self.req_id = 0
        if HAS_REQUESTS:
            self.session = requests.Session()
        else:
            self.session = None
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

        if HAS_REQUESTS and self.session is not None:
            try:
                r = self.session.post(self.url, json=payload, headers=headers, stream=True, timeout=self.timeout)
                if not self.session_id and "Mcp-Session-Id" in r.headers:
                    self.session_id = r.headers["Mcp-Session-Id"]

                content_type = r.headers.get("content-type", "")
                if "text/event-stream" in content_type:
                    for line in r.iter_lines():
                        if line:
                            decoded = line.decode("utf-8").strip()
                            if decoded.startswith("data: "):
                                return json.loads(decoded[6:])
                    return {}
                else:
                    if r.text:
                        return r.json()
                    return {}
            except requests.exceptions.ConnectionError as e:
                raise ConnectionError(
                    f"Failed to connect to Unreal Editor MCP server at {self.url}. "
                    f"Ensure Unreal Editor is running with ModelContextProtocol enabled on port 8000."
                ) from e
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
                    body = resp.read().decode("utf-8")

                    if "text/event-stream" in content_type:
                        for line in body.splitlines():
                            line = line.strip()
                            if line.startswith("data: "):
                                return json.loads(line[6:])
                        return {}
                    elif body:
                        return json.loads(body)
                    return {}
            except urllib.error.HTTPError as e:
                error_body = e.read().decode("utf-8") if e.fp else ""
                raise RuntimeError(f"MCP HTTP {e.code} Error for {method}: {error_body}") from e
            except urllib.error.URLError as e:
                raise ConnectionError(
                    f"Failed to connect to Unreal Editor MCP server at {self.url}. "
                    f"Ensure Unreal Editor is running with ModelContextProtocol enabled. Reason: {e.reason}"
                ) from e

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

    def list_tests(self, name_filter: str = "", tag_filter: str = "", limit: int = 500) -> List[str]:
        """Lists available automation tests matching the filter."""
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
        if isinstance(res, dict) and "returnValue" in res:
            val = res["returnValue"]
            if isinstance(val, str):
                try:
                    return json.loads(val)
                except Exception:
                    return {"raw": val}
            elif isinstance(val, dict):
                return val
        return res if isinstance(res, dict) else {"raw": res}

    def get_test_results(self) -> Dict[str, Any]:
        """Retrieves detailed results for the most recent test run."""
        res = self.call_tool(
            "AutomationTestToolset.AutomationTestToolset",
            "GetTestResults",
            {}
        )
        if isinstance(res, dict) and "returnValue" in res:
            val = res["returnValue"]
            if isinstance(val, str):
                try:
                    return json.loads(val)
                except Exception:
                    return {"raw": val}
            elif isinstance(val, dict):
                return val
        return res if isinstance(res, dict) else {"raw": res}


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
