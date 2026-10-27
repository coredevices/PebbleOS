# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import glob
import os
import queue
import re
import shutil
import subprocess
import threading
import time

from .core import Runner, RunnerCaps, RunnerError

DEV_CONNECTION_PORT = 9000
RECONNECT_INTERVAL = 3
ADB_CONNECT_ATTEMPTS = 5
INSTALL_TIMEOUT = 600

VIA_CHOICES = ("adb", "cloud", "ip")

# The Android app's receiver that starts the Developer Connection for adb
ADB_DEV_CONNECTION_ACTION = "coredevices.coreapp.DEV_CONNECTION"
ADB_DEV_CONNECTION_RECEIVER = (
    "coredevices.coreapp/coredevices.coreapp.debug.DevConnectionReceiver"
)
ADB_BROADCAST_RESULT = re.compile(r'result=(-?\d+)(?:, data="(.*)")?')

# Developer Connection message that has the app install a firmware bundle,
# and the log with which older apps turn it down
INSTALL_FIRMWARE = 0x0D
APP_UNKNOWN_OPERATION = "Unknown operation."


class AdbDevConnectionError(RunnerError):
    pass


def _ignore_closed_reader(args):
    """libpebble2's reader thread dies with a traceback when the phone closes
    the connection, which the runner handles by reconnecting."""
    from websocket import WebSocketConnectionClosedException

    if (
        args.thread is not None
        and args.thread.name == "PebbleConnection"
        and isinstance(args.exc_value, WebSocketConnectionClosedException)
    ):
        return
    threading.__excepthook__(args)


class PhoneRunner(Runner):
    name = "phone"

    def __init__(self, cfg, via=None, address=None, adb_serial=None):
        super().__init__(cfg)
        self.via = via or ("ip" if address else "adb")
        self.address = address
        self.adb_serial = adb_serial
        self.adb_port = None

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash"}, flash_resources=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--via",
            choices=VIA_CHOICES,
            help="How the phone runner reaches the phone's Developer "
            "Connection: adb (Android, the default), cloud (signed in with "
            "'pebble login') or ip (implied by --address)",
        )
        parser.add_argument(
            "--address",
            help="Phone address (Developer Connection) for the phone runner",
        )
        parser.add_argument(
            "--adb-serial",
            help="Serial of the Android phone for the phone runner, when adb "
            "sees several",
        )

    @classmethod
    def do_create(cls, cfg, args):
        return cls(
            cfg,
            via=getattr(args, "via", None),
            address=getattr(args, "address", None),
            adb_serial=getattr(args, "adb_serial", None),
        )

    def _adb(self, *args):
        cmd = ["adb"]
        if self.adb_serial:
            cmd += ["-s", self.adb_serial]
        cmd += args
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            raise RunnerError(
                f"{self.quote(*cmd)} failed: {result.stderr.strip() or result.stdout.strip()}"
            )
        return result.stdout

    def _adb_start_dev_connection(self):
        output = self._adb(
            "shell",
            "am",
            "broadcast",
            "-a",
            ADB_DEV_CONNECTION_ACTION,
            "-n",
            ADB_DEV_CONNECTION_RECEIVER,
        )
        m = ADB_BROADCAST_RESULT.search(output)
        if not m or m.group(2) is None:
            raise RunnerError(
                "the Pebble app on the phone did not answer: is it installed "
                "and up to date?"
            )
        if m.group(1) != "0":
            raise AdbDevConnectionError(
                f"the Pebble app could not start the Developer Connection: {m.group(2)}"
            )
        if self.adb_port is None:
            self.adb_port = int(self._adb("forward", "tcp:0", f"tcp:{m.group(2)}"))

    def _url(self):
        if self.via == "ip":
            return f"ws://{self.address}:{DEV_CONNECTION_PORT}/"
        self._adb_start_dev_connection()
        return f"ws://127.0.0.1:{self.adb_port}/"

    @staticmethod
    def _cloud_transport():
        from . import cloudpebble

        try:
            return cloudpebble.ProxyTransport(cloudpebble.token())
        except cloudpebble.NotLoggedIn:
            raise RunnerError(
                "sign in with 'pebble login' (pebble-tool) to use --via cloud"
            ) from None

    def _connect(self):
        try:
            from libpebble2.communication import PebbleConnection
            from libpebble2.communication.transports.websocket import (
                WebsocketTransport,
            )
            from libpebble2.exceptions import ConnectionError as PebbleConnectionError
        except ImportError as e:
            raise RunnerError(f"libpebble2 is not installed ({e})") from e

        if self.via == "cloud":
            pebble = PebbleConnection(self._cloud_transport())
            pebble.connect()
        else:
            url = self._url()
            # The app restarts its server for the adb broadcast, which the
            # port forward may reach before it listens again
            for attempt in range(ADB_CONNECT_ATTEMPTS if self.via == "adb" else 1):
                pebble = PebbleConnection(WebsocketTransport(url))
                try:
                    pebble.connect()
                    break
                except PebbleConnectionError:
                    if attempt == ADB_CONNECT_ATTEMPTS - 1 or self.via != "adb":
                        raise
                    time.sleep(1)
        # Not run_async(): it waits for the watch to answer, while the bundle
        # install only needs the phone
        threading.Thread(
            target=pebble.run_sync, name="PebbleConnection", daemon=True
        ).start()
        return pebble

    def _bundle(self):
        build = self.cfg.build
        cmd = ["cmake", "--build", build.path, "--target", "bundle"]
        if subprocess.call(cmd, stdout=subprocess.DEVNULL) != 0:
            raise RunnerError(f"{self.quote(*cmd)} failed")
        bundles = glob.glob(build.join("*.pbz"))
        if not bundles:
            raise RunnerError(f"no firmware bundle in {build.path}")
        return max(bundles, key=os.path.getmtime)

    @staticmethod
    def _watch_install(pebble, events):
        from libpebble2.communication.transports.websocket import MessageTargetPhone
        from libpebble2.communication.transports.websocket.protocol import (
            WebSocketInstallStatus,
            WebSocketPhoneAppLog,
        )

        return [
            pebble.register_transport_endpoint(
                MessageTargetPhone,
                WebSocketPhoneAppLog,
                lambda message: events.put(("log", message.payload)),
            ),
            pebble.register_transport_endpoint(
                MessageTargetPhone,
                WebSocketInstallStatus,
                lambda message: events.put(("status", message.status)),
            ),
        ]

    def _reconnect(self, deadline):
        from libpebble2.exceptions import PebbleError

        while time.monotonic() < deadline:
            time.sleep(RECONNECT_INTERVAL)
            try:
                return self._connect()
            except (AdbDevConnectionError, PebbleError):
                continue
        raise RunnerError("could not reconnect to the phone")

    def _install_bundle(self, pebble, path):
        """Has the app install the bundle, deciding itself whether it goes
        through PRF."""
        from libpebble2.communication.transports.websocket.protocol import (
            WebSocketInstallStatus,
        )

        with open(path, "rb") as f:
            data = f.read()
        print(f"Sending {path} to the phone")
        events = queue.Queue()
        handles = self._watch_install(pebble, events)
        pebble.transport.ws.send_binary(bytes([INSTALL_FIRMWARE]) + data)

        # The app goes on with the install, and reports it, when the watch
        # reboots and the connection drops
        deadline = time.monotonic() + INSTALL_TIMEOUT
        try:
            while time.monotonic() < deadline:
                try:
                    kind, value = events.get(timeout=1)
                except queue.Empty:
                    if not pebble.transport.connected:
                        print("The phone closed the connection, reconnecting")
                        pebble = self._reconnect(deadline)
                        handles = self._watch_install(pebble, events)
                    continue
                if kind == "status":
                    if value != WebSocketInstallStatus.StatusCode.Success:
                        raise RunnerError(
                            "the Pebble app failed to install the firmware"
                        )
                    return
                if value == APP_UNKNOWN_OPERATION:
                    raise RunnerError(
                        "the Pebble app cannot install firmware: update it"
                    )
                print(value)
            raise RunnerError(
                f"the Pebble app did not install the firmware in {INSTALL_TIMEOUT}s"
            )
        finally:
            for handle in handles:
                pebble.unregister_endpoint(handle)

    def _flash(self):
        if not self.cfg.pbpack_file:
            raise RunnerError("only the normal firmware can be installed")

        # The app installs the bundle over Bluetooth, so the firmware crosses
        # the Developer Connection in one message rather than chunk by chunk
        self._install_bundle(self._connect(), self._bundle())

    def do_run(self, command):
        if (self.via == "ip") != bool(self.address):
            raise RunnerError("--address goes with --via ip, and only with it")
        if self.adb_serial and self.via != "adb":
            raise RunnerError("--adb-serial goes with --via adb only")
        phone = {
            "adb": "over adb",
            "cloud": "over the cloud",
            "ip": f"at {self.address}",
        }[self.via]
        if self.cfg.dry_run:
            print(f"[dry-run] {command} through the phone {phone}")
            return
        if self.via == "adb" and not shutil.which("adb"):
            raise RunnerError(
                "adb not found: install it, or reach the phone with another --via"
            )
        from libpebble2.exceptions import PebbleError

        threading.excepthook = _ignore_closed_reader
        try:
            self._flash()
        except PebbleError as e:
            raise RunnerError(
                f"{command} through the phone failed: {e or type(e).__name__}"
            ) from e
        finally:
            if self.adb_port is not None:
                self._adb("forward", "--remove", f"tcp:{self.adb_port}")
