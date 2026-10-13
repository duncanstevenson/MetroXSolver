#!/usr/bin/env python3
import http.server
import socketserver
import subprocess
import json
import os
import sys

PORT = 8000
UI_DIR = os.path.join(os.path.dirname(__file__), "ui")
SOLVER_EXE = os.path.join(os.path.dirname(__file__), "metrox_solver.exe")

class MetroXHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=UI_DIR, **kwargs)

    def do_POST(self):
        if self.path == "/api/solve":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode("utf-8")
            
            try:
                data = json.loads(body)
                scenario_text = data.get("scenario", "")

                temp_file = os.path.join(os.path.dirname(__file__), "_temp_run.txt")
                with open(temp_file, "w", encoding="utf-8") as f:
                    f.write(scenario_text)

                # Call metrox_solver.exe with --json flag
                cmd = [SOLVER_EXE, temp_file, "--json"]
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    check=False
                )

                if os.path.exists(temp_file):
                    try:
                        os.remove(temp_file)
                    except OSError:
                        pass

                if result.returncode != 0:
                    self.send_response(500)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    err_payload = json.dumps({"error": result.stderr or result.stdout or "Solver failed"})
                    self.wfile.write(err_payload.encode("utf-8"))
                    return

                # Send JSON output back to UI
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(result.stdout.encode("utf-8"))

            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

def main():
    if not os.path.exists(SOLVER_EXE):
        print(f"Warning: {SOLVER_EXE} not found. Please compile the C++ solver first.")

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), MetroXHandler) as httpd:
        print("=========================================================")
        print(f" Metro X Studio running at: http://localhost:{PORT}")
        print(" Press Ctrl+C to stop the server.")
        print("=========================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

if __name__ == "__main__":
    main()
