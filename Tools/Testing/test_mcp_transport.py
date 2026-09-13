#!/usr/bin/env python3
"""Tests for MCP client transport and SSE stream parsing.

Verifies:
1. Short SSE event (single line, minimal formatting)
2. Multi-chunk SSE event (payload split across multiple chunk writes)
3. Malformed SSE event (invalid JSON raises RuntimeError)
4. Disconnect (premature server closure raises ConnectionError/RuntimeError)
5. Timeout (unresponsive server raises TimeoutError/RuntimeError, no indefinite hang)
6. HTTP errors are rejected before parsing success-shaped bodies
7. SSE keep-alives cannot extend the absolute request deadline
8. JSON-RPC responses must echo the exact request id
"""

from __future__ import annotations

import http.server
import json
import os
import socket
import socketserver
import sys
import threading
import time
import unittest
from pathlib import Path

# Add repo root and Tools/MCP to sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
MCP_DIR = REPO_ROOT / "Tools" / "MCP"
for p in [str(REPO_ROOT), str(MCP_DIR)]:
    if p not in sys.path:
        sys.path.insert(0, p)

from mcp_client import UnrealMcpClient, _parse_sse_stream


class FakeMcpServer(socketserver.TCPServer):
    allow_reuse_address = True

    def __init__(self, handler_factory):
        super().__init__(("127.0.0.1", 0), handler_factory)
        self.server_port = self.server_address[1]


class TestMcpTransport(unittest.TestCase):
    def test_parse_sse_stream_unit(self) -> None:
        """Unit tests for _parse_sse_stream parser."""
        # 1. Short event
        lines = [b"data: {\"short\": 1}\r\n", b"\r\n"]
        self.assertEqual(_parse_sse_stream(lines), {"short": 1})

        # 2. No space after data:
        lines = [b"data:{\"no_space\": true}\n", b"\n"]
        self.assertEqual(_parse_sse_stream(lines), {"no_space": True})

        # 3. Comment lines ignored
        lines = [b": keep-alive\n", b"data: {\"ok\": true}\n", b"\n"]
        self.assertEqual(_parse_sse_stream(lines), {"ok": True})

        # 4. Multi-line data combined
        lines = [b"data: {\"part1\": 1,\n", b"data: \"part2\": 2}\n", b"\n"]
        self.assertEqual(_parse_sse_stream(lines), {"part1": 1, "part2": 2})

        # 5. Malformed JSON raises RuntimeError
        with self.assertRaises(RuntimeError):
            _parse_sse_stream([b"data: {invalid json}\n", b"\n"])

        # 6. Stream ends without data event raises ConnectionError
        with self.assertRaises(ConnectionError):
            _parse_sse_stream([b": keep-alive\n"])

    def test_server_short_event(self) -> None:
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Mcp-Session-Id", "sess-short")
                self.end_headers()
                self.wfile.write(b'data: {"jsonrpc":"2.0","id":1,"result":"short_ok"}\n\n')
                self.wfile.flush()

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        thread = threading.Thread(target=server.handle_request, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/mcp"
            client = UnrealMcpClient(url=url, timeout=5.0, auto_initialize=False)
            res = client._send_raw("test_method")
            self.assertEqual(res, {"jsonrpc": "2.0", "id": 1, "result": "short_ok"})
            self.assertEqual(client.session_id, "sess-short")
        finally:
            server.server_close()

    def test_server_multi_chunk_event(self) -> None:
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                # Send split chunks
                self.wfile.write(b'data: {"jsonrpc": "2.0", ')
                self.wfile.flush()
                time.sleep(0.02)
                self.wfile.write(b'"id": 1, "result": ')
                self.wfile.flush()
                time.sleep(0.02)
                self.wfile.write(b'{"chunked": true}}\n\n')
                self.wfile.flush()

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        thread = threading.Thread(target=server.handle_request, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/mcp"
            client = UnrealMcpClient(url=url, timeout=5.0, auto_initialize=False)
            res = client._send_raw("test_chunked")
            self.assertEqual(res.get("result"), {"chunked": True})
        finally:
            server.server_close()

    def test_server_malformed_event(self) -> None:
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                self.wfile.write(b"data: {broken: json}\n\n")
                self.wfile.flush()

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        thread = threading.Thread(target=server.handle_request, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/mcp"
            client = UnrealMcpClient(url=url, timeout=5.0, auto_initialize=False)
            with self.assertRaises(RuntimeError) as ctx:
                client._send_raw("test_malformed")
            self.assertIn("Malformed JSON in SSE", str(ctx.exception))
        finally:
            server.server_close()

    def test_server_disconnect(self) -> None:
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                # Close connection abruptly without response
                self.close_connection = True

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        thread = threading.Thread(target=server.handle_request, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/mcp"
            client = UnrealMcpClient(url=url, timeout=5.0, auto_initialize=False)
            with self.assertRaises((ConnectionError, RuntimeError)):
                client._send_raw("test_disconnect")
        finally:
            server.server_close()

    def test_server_timeout(self) -> None:
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                # Sleep longer than client timeout
                time.sleep(0.5)
                try:
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.end_headers()
                    self.wfile.write(b'data: {"id": 1}\n\n')
                except Exception:
                    pass

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        thread = threading.Thread(target=server.handle_request, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/mcp"
            client = UnrealMcpClient(url=url, timeout=0.1, auto_initialize=False)
            start = time.time()
            with self.assertRaises((TimeoutError, RuntimeError)):
                client._send_raw("test_timeout")
            elapsed = time.time() - start
            # Verify client didn't hang indefinitely (timeout was 0.1s, should return < 0.4s)
            self.assertLess(elapsed, 0.4)
        finally:
            server.server_close()

    def test_http_500_success_shaped_body_is_rejected(self) -> None:
        """A proxy/server HTTP failure must never be accepted as an MCP success."""
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"jsonrpc":"2.0","id":1,"result":{"ok":true}}')

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        threading.Thread(target=server.handle_request, daemon=True).start()
        try:
            client = UnrealMcpClient(
                url=f"http://127.0.0.1:{server.server_port}/mcp",
                timeout=1.0,
                auto_initialize=False,
            )
            with self.assertRaises(RuntimeError):
                client._send_raw("test_http_error")
        finally:
            server.server_close()

    def test_sse_keepalives_do_not_extend_absolute_deadline(self) -> None:
        """Periodic bytes must not turn the configured total timeout into an idle timeout."""
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                for _ in range(20):
                    try:
                        self.wfile.write(b": keep-alive\n\n")
                        self.wfile.flush()
                    except OSError:
                        break
                    time.sleep(0.03)

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        threading.Thread(target=server.handle_request, daemon=True).start()
        try:
            client = UnrealMcpClient(
                url=f"http://127.0.0.1:{server.server_port}/mcp",
                timeout=0.12,
                auto_initialize=False,
            )
            start = time.monotonic()
            with self.assertRaises(TimeoutError):
                client._send_raw("test_keepalive_deadline")
            self.assertLess(time.monotonic() - start, 0.35)
        finally:
            server.server_close()

    def test_json_rpc_response_id_must_match_request(self) -> None:
        """A stale/crossed JSON-RPC response cannot satisfy the current request."""
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                body_len = int(self.headers.get("Content-Length", 0))
                if body_len > 0:
                    self.rfile.read(body_len)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"jsonrpc":"2.0","id":999,"result":{"ok":true}}')

            def log_message(self, format, *args):
                pass

        server = FakeMcpServer(Handler)
        threading.Thread(target=server.handle_request, daemon=True).start()
        try:
            client = UnrealMcpClient(
                url=f"http://127.0.0.1:{server.server_port}/mcp",
                timeout=1.0,
                auto_initialize=False,
            )
            with self.assertRaises(RuntimeError) as ctx:
                client._send_raw("test_response_id")
            self.assertIn("response id", str(ctx.exception))
        finally:
            server.server_close()


def main() -> int:
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TestMcpTransport)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
