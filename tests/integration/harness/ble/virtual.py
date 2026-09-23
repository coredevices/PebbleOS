# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Bluetooth without radios: two of Bumble's software controllers on one
simulated link, each served over TCP as H4. One is the emulated watch's
controller, the other the harness's."""

import os
import socket
import subprocess
import sys
import time

from harness.errors import HarnessError

START_TIMEOUT_S = 10.0
HARNESS_ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)


def _free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class VirtualLink:
    def __init__(self, log_path):
        self.watch_port = _free_port()
        self.host_port = _free_port()
        self._log_path = log_path
        self._log = None
        self._process = None

    @property
    def watch_chardev(self):
        """The watch's controller, as a QEMU -serial spec."""
        return f"tcp:127.0.0.1:{self.watch_port}"

    @property
    def host_controller(self):
        """The harness's controller, as a Bumble transport."""
        return f"tcp-client:127.0.0.1:{self.host_port}"

    def start(self):
        os.makedirs(os.path.dirname(self._log_path) or ".", exist_ok=True)
        self._log = open(self._log_path, "w")  # noqa: SIM115
        env = dict(os.environ)
        env["PYTHONPATH"] = os.pathsep.join(
            p for p in (HARNESS_ROOT, env.get("PYTHONPATH")) if p
        )
        self._process = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "harness.ble.virtual",
                f"tcp-server:127.0.0.1:{self.watch_port}",
                f"tcp-server:127.0.0.1:{self.host_port}",
            ],
            stdout=self._log,
            stderr=subprocess.STDOUT,
            env=env,
        )
        deadline = time.monotonic() + START_TIMEOUT_S
        for port in (self.watch_port, self.host_port):
            while not self._listening(port):
                if self._process.poll() is not None:
                    raise HarnessError(
                        f"the virtual Bluetooth link exited; see {self._log_path}"
                    )
                if time.monotonic() > deadline:
                    raise HarnessError("the virtual Bluetooth link did not start")
                time.sleep(0.1)

    @staticmethod
    def _listening(port):
        # lsof-free check: a server socket is bound when we cannot bind it.
        with socket.socket() as s:
            try:
                s.bind(("127.0.0.1", port))
            except OSError:
                return True
        return False

    def stop(self):
        if self._process is not None:
            self._process.terminate()
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait()
            self._process = None
        if self._log is not None:
            self._log.close()
            self._log = None


def _serve(transports):
    """Run linked software controllers, one per Bumble transport."""
    import asyncio

    import bumble.logging
    from bumble.controller import Controller
    from bumble.link import LocalLink
    from bumble.transport import open_transport

    async def main():
        link = LocalLink()
        opened = []
        for index, name in enumerate(transports):
            transport = await open_transport(name)
            opened.append(transport)
            Controller(
                f"C{index}",
                host_source=transport.source,
                host_sink=transport.sink,
                link=link,
            )
        await asyncio.get_running_loop().create_future()

    bumble.logging.setup_basic_logging()
    asyncio.run(main())


if __name__ == "__main__":
    _serve(sys.argv[1:])
