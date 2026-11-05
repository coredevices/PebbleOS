# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Talking to a running emulator: QEMU, or the firmware as a native program.

Both serve the firmware's Pebble protocol over a TCP port, reached with
:func:`connect_watch`. The qemu runner (``pbl run``) also exposes two unix
sockets in the build directory: the human monitor, used for screendumps, and
QMP, used to inject input events. A native program has neither: its screen
and its touch input go through the Pebble protocol instead.
"""

import json
import socket
import struct
import time
import zlib

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
            f"cannot reach the QEMU {what} socket at {path} -- is 'pbl run' running QEMU? ({e})"
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


def running(build):
    """The running emulator of the build."""
    if build.config.CONFIG_QEMU:
        return QemuEmulator(build)
    if build.config.CONFIG_SOC_POSIX and not build.config.CONFIG_POSIX_EMSCRIPTEN:
        return NativeEmulator(build)
    raise CommandContextError(f"board {build.board} is not an emulated board")


class QemuEmulator:
    def __init__(self, build):
        self.build = build

    def screenshot(self, output):
        with Monitor(self.build.join(MONITOR_SOCKET)) as monitor:
            response = monitor.command(f"screendump {output} -f png")
        return response

    def touch(self, x1, y1, x2, y2, duration, steps=12):
        """Put a finger down, move it to the end in ``steps`` and lift it."""
        with Qmp(self.build.join(QMP_SOCKET)) as qmp:
            width, height = qmp.touch_display()
            qmp.send(qmp.move(width, height, x1, y1) + qmp.button(True))
            if (x1, y1) == (x2, y2):
                time.sleep(duration)
            else:
                for step in range(1, steps + 1):
                    time.sleep(duration / steps)
                    qmp.send(
                        qmp.move(
                            width,
                            height,
                            x1 + (x2 - x1) * step // steps,
                            y1 + (y2 - y1) * step // steps,
                        )
                    )
            qmp.send(qmp.button(False))


class NativeEmulator:
    def __init__(self, build):
        self.build = build

    def screenshot(self, output):
        width, rows = protocol_screenshot(connect_watch())
        write_png(output, width, rows)
        return ""

    def touch(self, x1, y1, x2, y2, duration):
        remote_input_touch(connect_watch(), x1, y1, x2, y2, duration)
        # The finger moves on the watch's own timers.
        time.sleep(duration)


SCREENSHOT_TIMEOUT_S = 30


def protocol_screenshot(pebble, timeout=SCREENSHOT_TIMEOUT_S):
    """The screenshot endpoint's image: its width and RGB rows."""
    from libpebble2.exceptions import TimeoutError as PebbleTimeoutError
    from libpebble2.protocol.screenshots import (
        ScreenshotHeader,
        ScreenshotRequest,
        ScreenshotResponse,
    )
    from libpebble2.services.screenshot import Screenshot

    responses = pebble.get_endpoint_queue(ScreenshotResponse)
    try:
        pebble.send_packet(ScreenshotRequest())
        try:
            header = ScreenshotHeader.parse(responses.get(timeout=timeout).data)[0]
            if header.response_code != ScreenshotHeader.ResponseCode.OK:
                raise CommandContextError(
                    f"the firmware refused the screenshot: {header.response_code!s}"
                )
            data = header.data
            expected = Screenshot._get_expected_bytes(header)
            while len(data) < expected:
                data += responses.get(timeout=timeout).data
        except PebbleTimeoutError:
            raise CommandContextError(f"no screenshot after {timeout}s") from None
    finally:
        responses.close()
    return header.width, Screenshot._decode_image(header, data)


def write_png(path, width, rows):
    """Write RGB rows as an 8-bit PNG."""

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    raw = b"".join(b"\x00" + bytes(row) for row in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, len(rows), 8, 2, 0, 0, 0))
        )
        f.write(chunk(b"IDAT", zlib.compress(raw)))
        f.write(chunk(b"IEND", b""))


def remote_input_touch(pebble, x1, y1, x2, y2, duration, timeout=5):
    """Have the firmware's remote input endpoint drag a finger."""
    from libpebble2.exceptions import TimeoutError as PebbleTimeoutError

    from pbl.remote_input import STATUS, RemoteInputAck, RemoteInputTouch

    packet = RemoteInputTouch(
        x1=x1, y1=y1, x2=x2, y2=y2, duration_ms=max(1, round(duration * 1000))
    )
    try:
        ack = pebble.send_and_read(packet, RemoteInputAck, timeout=timeout)
    except PebbleTimeoutError:
        raise CommandContextError(
            "the firmware did not answer the remote input request"
        ) from None
    if ack.status != 0:
        raise CommandContextError(
            f"the firmware refused the touch: {STATUS.get(ack.status, ack.status)}"
        )


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
            f"-- is 'pbl run' running the emulator? ({e})"
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
