#!/usr/bin/env python3
"""Coordinator service for internal signed actions."""
import os
import hmac
import hashlib
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
import json
from urllib.parse import parse_qs, urlparse

BUILD_SIGN_KEY = os.environ.get('BUILD_SIGN_KEY', '')
FLAG = os.environ.get('FLAG', 'flag{test-only}')
BIND_HOST = '127.0.0.1'
BIND_PORT = 9002

if 'FLAG' in os.environ:
    del os.environ['FLAG']


class CoordinatorHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def _verify_hmac(self, timestamp_str, action, signature):
        try:
            ts = int(timestamp_str)
            if abs(time.time() - ts) > 60:
                return False
        except ValueError:
            return False

        message = f"{timestamp_str}|{action}"
        expected = hmac.new(
            BUILD_SIGN_KEY.encode(),
            message.encode(),
            hashlib.sha256
        ).hexdigest()

        return hmac.compare_digest(expected, signature)

    def _send_json(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _send_error(self, message, status=403):
        self._send_json({'error': message}, status)

    def do_GET(self):
        if self.path.startswith('/internal/artifact/fetch'):
            self._handle_artifact_fetch()
        elif self.path == '/health':
            self._send_json({'status': 'ok'})
        else:
            self._send_error('not found', 404)

    def do_POST(self):
        if self.path.startswith('/internal/artifact/sign'):
            self._handle_sign()
        else:
            self._send_error('not found', 404)

    def _handle_artifact_fetch(self):
        parsed = urlparse(self.path)
        params = {k: v[0] for k, v in parse_qs(parsed.query).items()}

        timestamp = params.get('timestamp', '')
        action = params.get('action', '')
        signature = params.get('signature', '')

        if not all([timestamp, action, signature]):
            self._send_error('missing auth parameters')
            return

        if action != 'artifact:read':
            self._send_error('invalid action')
            return

        if not self._verify_hmac(timestamp, action, signature):
            self._send_error('invalid signature')
            return

        self._send_json({
            'success': True,
            'flag': FLAG
        })

    def _handle_sign(self):
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode() if content_length > 0 else '{}'
        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            self._send_error('invalid json', 400)
            return

        timestamp = data.get('timestamp', '')
        action = data.get('action', '')
        signature = data.get('signature', '')

        if not self._verify_hmac(timestamp, action, signature):
            self._send_error('invalid signature')
            return

        self._send_json({
            'success': True,
            'signed': True,
            'artifact': data.get('artifact', '')
        })


if __name__ == '__main__':
    server = HTTPServer((BIND_HOST, BIND_PORT), CoordinatorHandler)
    print(f'[Coordinator] Listening on {BIND_HOST}:{BIND_PORT}')
    server.serve_forever()
