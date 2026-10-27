# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import queue
import re
import shutil
import struct
import subprocess
import time

from .core import Runner, RunnerCaps, RunnerError

DEV_CONNECTION_PORT = 9000
RECOVERY_TIMEOUT = 120
REENABLE_TIMEOUT = 300
RECONNECT_INTERVAL = 3

# The Android app's receiver that starts the Developer Connection for adb
ADB_DEV_CONNECTION_ACTION = "coredevices.coreapp.DEV_CONNECTION"
ADB_DEV_CONNECTION_RECEIVER = (
    "coredevices.coreapp/coredevices.coreapp.debug.DevConnectionReceiver"
)
ADB_BROADCAST_RESULT = re.compile(r'result=(-?\d+)(?:, data="(.*)")?')

# The firmware's reset command value, which libpebble2's Reset enum does not match
RESET_INTO_RECOVERY = 0xFF

# Version reply: frame header, command, then the running firmware's metadata,
# whose flags byte libpebble2 reads as a single is_recovery bool
VERSION_ENDPOINT = 0x0010
VERSION_RESPONSE = 0x01
VERSION_RUNNING_FLAGS_OFFSET = 4 + 1 + 4 + 32 + 8
FW_METADATA_IS_RECOVERY = 0x01


class AdbDevConnectionError(RunnerError):
    pass


class PhoneRunner(Runner):
    name = "phone"

    def __init__(self, cfg, address=None, adb_serial=None):
        super().__init__(cfg)
        self.address = address
        self.adb_serial = adb_serial
        self.adb_port = None

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash"}, flash_resources=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--address",
            help="Phone address (Developer Connection) for the phone runner; "
            "without it, an Android phone is reached over adb",
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
        if self.address:
            return f"ws://{self.address}:{DEV_CONNECTION_PORT}/"
        self._adb_start_dev_connection()
        return f"ws://127.0.0.1:{self.adb_port}/"

    def _connect(self):
        try:
            from libpebble2.communication import PebbleConnection
            from libpebble2.communication.transports.websocket import (
                WebsocketTransport,
            )
        except ImportError as e:
            raise RunnerError(f"libpebble2 is not installed ({e})") from e

        pebble = PebbleConnection(WebsocketTransport(self._url()))
        pebble.connect()
        pebble.run_async()
        return pebble

    @staticmethod
    def _reset(pebble, command):
        from libpebble2.protocol.system import Reset

        pebble.send_packet(Reset(command=command))

    @staticmethod
    def _in_recovery(pebble):
        from libpebble2.exceptions import TimeoutError as PebbleTimeoutError
        from libpebble2.protocol.system import WatchVersion, WatchVersionRequest

        flags = queue.Queue()

        def on_message(message):
            endpoint, command = struct.unpack_from(">HB", message, 2)
            if endpoint == VERSION_ENDPOINT and command == VERSION_RESPONSE:
                flags.put(message[VERSION_RUNNING_FLAGS_OFFSET])

        handle = pebble.register_raw_inbound_handler(on_message)
        try:
            pebble.send_packet(WatchVersion(data=WatchVersionRequest()))
            return bool(flags.get(timeout=15) & FW_METADATA_IS_RECOVERY)
        except queue.Empty:
            raise PebbleTimeoutError("no version reply from the watch") from None
        finally:
            pebble.unregister_endpoint(handle)

    def _wait_for_recovery(self, pebble):
        from libpebble2.exceptions import ConnectionError as PebbleConnectionError
        from libpebble2.exceptions import PebbleError

        # The phone may keep the connection open while the watch reboots, or
        # drop it. Older apps also turn the Developer Connection off when the
        # watch disconnects, so it has to be enabled again: over adb until the
        # watch is back, otherwise by hand.
        timeout = RECOVERY_TIMEOUT
        start = time.monotonic()
        prompted = False
        while time.monotonic() - start < timeout:
            time.sleep(RECONNECT_INTERVAL)
            try:
                if not pebble.transport.connected:
                    pebble = self._connect()
                if self._in_recovery(pebble):
                    return pebble
            except AdbDevConnectionError:
                continue
            except PebbleConnectionError:
                if not self.address:
                    continue
                if not prompted:
                    print(
                        "The phone closed the Developer Connection: enable it "
                        "again in the Pebble app to continue"
                    )
                    prompted = True
                    timeout = time.monotonic() - start + REENABLE_TIMEOUT
            except PebbleError:
                continue
        raise RunnerError(f"the watch did not come back in PRF in {timeout:.0f}s")

    @staticmethod
    def _put(pebble, object_type, path):
        from libpebble2.services.putbytes import PutBytes

        with open(path, "rb") as f:
            data = f.read()

        def progress(_interval, sent, total):
            print(f"\r{path}: {sent * 100 // total}%", end="", flush=True)

        pb = PutBytes(pebble, object_type, data, bank=0)
        pb.register_handler("progress", progress)
        pb.send()
        print()

    def _flash(self):
        from libpebble2.protocol.system import SystemMessage
        from libpebble2.services.putbytes import PutBytesType

        if self.cfg.firmware_slot not in (None, 0):
            raise RunnerError(
                "PRF installs into slot 0; configure with -DCONFIG_FIRMWARE_SLOT=0"
            )
        if not self.cfg.pbpack_file:
            raise RunnerError("only the normal firmware can be installed")

        # PRF invalidates the firmware slots and installs into slot 0, and only
        # commits the firmware together with the resources
        pebble = self._connect()
        if not self._in_recovery(pebble):
            print("Rebooting the watch into PRF")
            self._reset(pebble, RESET_INTO_RECOVERY)
            pebble = self._wait_for_recovery(pebble)

        pebble.send_and_read(
            SystemMessage(message_type=SystemMessage.Type.FirmwareUpdateStart),
            SystemMessage,
        )
        self._put(pebble, PutBytesType.Firmware, self.cfg.bin_file)
        self._put(pebble, PutBytesType.SystemResources, self.cfg.pbpack_file)
        pebble.send_packet(
            SystemMessage(message_type=SystemMessage.Type.FirmwareUpdateComplete)
        )

    def do_run(self, command):
        phone = f"at {self.address}" if self.address else "over adb"
        if self.cfg.dry_run:
            print(f"[dry-run] {command} through the phone {phone}")
            return
        if not self.address and not shutil.which("adb"):
            raise RunnerError(
                "adb not found: install it, or give the phone's address with --address"
            )
        from libpebble2.exceptions import PebbleError

        try:
            self._flash()
        except PebbleError as e:
            raise RunnerError(f"{command} through the phone failed: {e}") from e
        finally:
            if self.adb_port is not None:
                self._adb("forward", "--remove", f"tcp:{self.adb_port}")
