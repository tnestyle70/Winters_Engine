from __future__ import annotations

import argparse
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def make_handler(source_parent: Path, folder_name: str):
    class TarStreamHandler(BaseHTTPRequestHandler):
        server_version = "WintersTarStream/1.0"

        def do_GET(self) -> None:
            if self.path in ("/", "/index.html"):
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.end_headers()
                text = (
                    "Winters EldenRing tar stream server\n\n"
                    f"GET /{folder_name}.tar to download a tar stream.\n"
                    "Example:\n"
                    f"  curl.exe -L http://{self.server.server_address[0]}:{self.server.server_address[1]}/{folder_name}.tar -o D:\\\\{folder_name}.tar\n"
                )
                self.wfile.write(text.encode("utf-8"))
                return

            if self.path != f"/{folder_name}.tar":
                self.send_error(404, "Not found")
                return

            source = source_parent / folder_name
            if not source.exists():
                self.send_error(404, f"Source not found: {source}")
                return

            self.send_response(200)
            self.send_header("Content-Type", "application/x-tar")
            self.send_header("Content-Disposition", f'attachment; filename="{folder_name}.tar"')
            self.end_headers()

            proc = subprocess.Popen(
                ["tar", "-cf", "-", "-C", str(source_parent), folder_name],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            assert proc.stdout is not None
            try:
                while True:
                    chunk = proc.stdout.read(1024 * 1024)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
            except (BrokenPipeError, ConnectionResetError):
                proc.kill()
            finally:
                proc.stdout.close()
                proc.wait()

        def log_message(self, fmt: str, *args) -> None:
            print(f"{self.client_address[0]} - {fmt % args}", flush=True)

    return TarStreamHandler


def main() -> int:
    parser = argparse.ArgumentParser(description="Stream the EldenRing resource folder as a tar over HTTP.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument(
        "--source-parent",
        default=r"C:\Users\tnest\Desktop\Winters\Client\Bin\Resource",
    )
    parser.add_argument("--folder-name", default="EldenRing")
    args = parser.parse_args()

    source_parent = Path(args.source_parent)
    handler = make_handler(source_parent, args.folder_name)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"Serving {source_parent / args.folder_name} as http://{args.host}:{args.port}/{args.folder_name}.tar", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
