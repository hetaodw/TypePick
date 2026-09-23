# SPDX-License-Identifier: AGPL-3.0-only
"""Exercise the actual WinHTTP adapter; never contacts a remote service."""
import json
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

errors = []
received = []
scenario = ""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_POST(self):
        try:
            received.append(self.path)
            assert self.path == "/predict"
            assert "Authorization" not in self.headers
            request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            assert "model" not in request
            assert request["state"]["previous_text"] == "这家商店主要卖"
            assert request["questions"]["candidate"]["criteria"]["c1"] == "烟酒"
            answer = {"type": "choice", "choice": "c1", "confidence": 0.45,
                      "probabilities": {"c0": 0.35, "c1": 0.45, "abstain": 0.2}}
            code = 200
            if scenario == "abstained":
                answer.update(choice="abstain", confidence=0.7,
                              probabilities={"c0": 0.1, "c1": 0.2, "abstain": 0.7})
            if scenario == "bad_probabilities":
                answer["probabilities"]["c1"] = 10
            body = json.dumps({"answers": {"candidate": answer}}).encode()
            if scenario == "invalid_response":
                body = b"not json"
            if scenario == "response_too_large":
                body = b"x" * 70000
            if scenario == "http_503":
                code = 503
            if scenario == "http_302":
                code = 302
            if scenario == "timeout":
                time.sleep(0.8)
            self.send_response(code)
            if code == 302:
                self.send_header("Location", "/unexpected-redirect")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (ConnectionError, OSError):
            pass  # Deliberate timeout / oversized-response cases close early.
        except Exception as exc:
            errors.append(repr(exc))
            self.send_error(500)


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
env = os.environ.copy()
# A key that would fail validation if the local adapter ever consulted it.
env["TYPESAFE_API_KEY"] = "synthetic-invalid\nkey"
try:
    for scenario in ("recommended", "abstained", "invalid_response", "bad_probabilities",
                     "http_503", "http_302", "response_too_large", "timeout"):
        status = "invalid_response" if scenario == "bad_probabilities" else scenario
        subprocess.run([sys.argv[1], str(server.server_port), status,
                        "1" if scenario == "recommended" else "-1"],
                       check=True, env=env, timeout=5)
    assert len(received) == 8, received
    assert not errors, errors
finally:
    server.shutdown()
    server.server_close()
print("PASS: local WinHTTP transport, no credentials, no redirects, bounded failures")
