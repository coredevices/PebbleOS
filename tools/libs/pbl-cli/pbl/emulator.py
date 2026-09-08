# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Talking to a running QEMU.

``pbl qemu`` exposes two unix sockets in the build directory: the human
monitor, used for screendumps, and QMP, used to inject input events. The
firmware's Pebble protocol is served over a TCP serial port, reached with
:func:`connect_watch`.
"""

import json
import socket

from pbl.errors import CommandContextError

MONITOR_SOCKET = "qemu-mon.sock"
QMP_SOCKET = "qmp.sock"

# The TCP ports the emulator is launched with.
GDB_PORT = 1234
PEBBLE_TOOL_PORT = 12344
CONSOLE_PORT = 12345

# QEMU's absolute-axis range for input-send-event.
ABS_MAX = 32767


def _connect(path, what):
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(path)
    except OSError as e:
        raise CommandContextError(
            f"cannot reach the QEMU {what} socket at {path} -- is 'pbl qemu' running? ({e})"
        ) from e
    return sock


class Monitor:
    """The human monitor, for the commands QMP does not expose."""

    def __init__(self, path):
        self._sock = _connect(path, "monitor")

    def __enter__(self):
        self._read_until_prompt()
        return self

    def __exit__(self, *exc):
        self._sock.close()

    def _read_until_prompt(self):
        buf = b""
        while b"(qemu) " not in buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        return buf.decode(errors="replace")

    def command(self, cmd):
        self._sock.sendall((cmd + "\n").encode())
        return self._read_until_prompt()


class Qmp:
    """The QMP socket, for programmatic input injection."""

    def __init__(self, path):
        self._sock = _connect(path, "QMP")
        self._stream = self._sock.makefile("rw")
        self._stream.readline()  # greeting
        self.execute("qmp_capabilities")

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self._stream.close()
        self._sock.close()

    def execute(self, command, **arguments):
        request = {"execute": command}
        if arguments:
            request["arguments"] = arguments
        self._stream.write(json.dumps(request) + "\n")
        self._stream.flush()
        return json.loads(self._stream.readline())

    def touch_display(self):
        """Find the pebble-touch device and return its (width, height)."""
        stack = ["/machine"]
        while stack:
            path = stack.pop()
            for entry in self.execute("qom-list", path=path).get("return", []):
                if not entry.get("type", "").startswith("child<"):
                    continue
                child = path + "/" + entry["name"]
                name = entry["name"].lower()
                kind = entry.get("type", "").lower()
                if "touch" in name or "touch" in kind:
                    return (
                        self.execute("qom-get", path=child, property="display-width")[
                            "return"
                        ],
                        self.execute("qom-get", path=child, property="display-height")[
                            "return"
                        ],
                    )
                stack.append(child)
        raise CommandContextError("no pebble-touch device in the running machine")

    def send(self, events):
        self.execute("input-send-event", events=events)

    @staticmethod
    def move(width, height, x, y):
        """The absolute-axis events that move the finger to a pixel."""
        return [
            {"type": "abs", "data": {"axis": "x", "value": int(x / width * ABS_MAX)}},
            {"type": "abs", "data": {"axis": "y", "value": int(y / height * ABS_MAX)}},
        ]

    @staticmethod
    def button(down):
        return [{"type": "btn", "data": {"button": "left", "down": down}}]


def connect_watch(host="127.0.0.1", port=PEBBLE_TOOL_PORT):
    """A libpebble2 connection to the firmware, as the phone app would have."""
    try:
        from libpebble2.communication import PebbleConnection
        from libpebble2.communication.transports.qemu import QemuTransport
        from libpebble2.exceptions import ConnectionError as PebbleConnectionError
        from libpebble2.exceptions import TimeoutError as PebbleTimeoutError
    except ImportError as e:
        raise CommandContextError(f"libpebble2 is not installed ({e})") from e

    pebble = PebbleConnection(QemuTransport(host, port))
    try:
        pebble.connect()
    except PebbleConnectionError as e:
        raise CommandContextError(
            f"cannot reach the emulator's Pebble protocol port at {host}:{port} "
            f"-- is 'pbl qemu' running? ({e})"
        ) from e
    try:
        pebble.run_async()
    except PebbleTimeoutError:
        raise CommandContextError(
            f"the firmware is not answering on {host}:{port} -- the port serves "
            "one client at a time; is another feed or pebble tool connected?"
        ) from None
    identify_as_phone(pebble)
    return pebble


# What the version response tells the firmware: an Android phone app, with
# every protocol capability. The OS is in the low three bits.
PHONE_OS_ANDROID = 2
PHONE_CAPABILITIES = 0xFFFFFFFFFFFFFFFF


def identify_as_phone(pebble):
    """Answer the firmware's version request without waiting for one.

    The firmware only asks once, when the emulated session first opens, and
    the session outlives host connections; what depends on the answer (the
    phone's OS gates the music endpoint, for one) would otherwise never be
    re-established after the first connection goes away.
    """
    from libpebble2.protocol.system import AppVersionResponse, PhoneAppVersion

    pebble.send_packet(
        PhoneAppVersion(
            message=AppVersionResponse(
                protocol_version=0xFFFFFFFF,
                session_caps=0x80000000,
                platform_flags=PHONE_OS_ANDROID,
                response_version=2,
                major_version=3,
                minor_version=0,
                bugfix_version=0,
                protocol_caps=PHONE_CAPABILITIES,
            )
        )
    )
