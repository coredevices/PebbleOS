# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import functools
import http.server
import os
import webbrowser

from .core import Runner, RunnerCaps, RunnerError


class _Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # A rebuild is picked up by reloading the page.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format, *args):
        pass


class WasmRunner(Runner):
    name = "wasm"

    def __init__(self, cfg, http_port=8000, browser=True):
        super().__init__(cfg)
        self.http_port = http_port
        self.browser = browser

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"run"})

    @classmethod
    def supports(cls, cfg):
        return bool(cfg.build.config.CONFIG_POSIX_EMSCRIPTEN)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--http-port",
            type=int,
            default=8000,
            help="Port the wasm runner serves the page on (default: 8000)",
        )
        parser.add_argument(
            "--no-browser",
            action="store_true",
            help="Do not open the page the wasm runner serves in a browser",
        )

    @classmethod
    def do_create(cls, cfg, args):
        return cls(
            cfg,
            http_port=getattr(args, "http_port", 8000),
            browser=not getattr(args, "no_browser", False),
        )

    def do_run(self, command):
        if self.cfg.extra_args:
            raise RunnerError("the wasm runner takes no arguments after '--'")

        build = self.cfg.build
        page = f"{build.project}.html"
        url = f"http://localhost:{self.http_port}/{page}"
        if self.cfg.dry_run:
            print(f"[dry-run] serve {build} at {url}")
            return 0
        if not os.path.isfile(build.join(page)):
            raise RunnerError(f"no {build.join(page)}; run 'pbl build' first")

        handler = functools.partial(_Handler, directory=str(build))
        try:
            server = http.server.ThreadingHTTPServer(
                ("localhost", self.http_port), handler
            )
        except OSError as e:
            raise RunnerError(f"cannot serve on port {self.http_port}: {e}") from e

        with server:
            print(f"Serving {url}, Ctrl-C to stop")
            if self.browser:
                webbrowser.open(url)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass
        return 0
