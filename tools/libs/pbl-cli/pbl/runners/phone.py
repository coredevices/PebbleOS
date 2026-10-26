# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import queue
import struct
import time

from .core import Runner, RunnerCaps, RunnerError

DEV_CONNECTION_PORT = 9000
RECOVERY_TIMEOUT = 120
RECONNECT_INTERVAL = 3

# The firmware's reset command values, which libpebble2's Reset enum does not match
RESET_NORMAL = 0x00
RESET_INTO_RECOVERY = 0xFF

# Version reply: frame header, command, then the running firmware's metadata,
# whose flags byte libpebble2 reads as a single is_recovery bool
VERSION_ENDPOINT = 0x0010
VERSION_RESPONSE = 0x01
VERSION_RUNNING_FLAGS_OFFSET = 4 + 1 + 4 + 32 + 8
FW_METADATA_IS_RECOVERY = 0x01


class PhoneRunner(Runner):
    name = "phone"

    def __init__(self, cfg, address=None):
        super().__init__(cfg)
        self.address = address

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash", "reset"}, flash_resources=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--address",
            help="Phone address (Developer Connection) for the phone runner",
        )

    @classmethod
    def do_create(cls, cfg, args):
        return cls(cfg, address=getattr(args, "address", None))

    def _connect(self):
        try:
            from libpebble2.communication import PebbleConnection
            from libpebble2.communication.transports.websocket import (
                WebsocketTransport,
            )
        except ImportError as e:
            raise RunnerError(f"libpebble2 is not installed ({e})") from e

        pebble = PebbleConnection(
            WebsocketTransport(f"ws://{self.address}:{DEV_CONNECTION_PORT}/")
        )
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
        from libpebble2.exceptions import PebbleError

        # The phone may keep the connection open while the watch reboots, or
        # drop it
        deadline = time.monotonic() + RECOVERY_TIMEOUT
        while time.monotonic() < deadline:
            time.sleep(RECONNECT_INTERVAL)
            try:
                if not pebble.transport.connected:
                    pebble = self._connect()
                if self._in_recovery(pebble):
                    return pebble
            except PebbleError:
                continue
        raise RunnerError(f"the watch did not come back in PRF in {RECOVERY_TIMEOUT}s")

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
        if not self.address:
            raise RunnerError("Phone address not specified, use --address")
        if self.cfg.dry_run:
            print(f"[dry-run] {command} through the phone at {self.address}")
            return
        from libpebble2.exceptions import PebbleError

        try:
            if command == "flash":
                self._flash()
            elif command == "reset":
                self._reset(self._connect(), RESET_NORMAL)
        except PebbleError as e:
            raise RunnerError(f"{command} through the phone failed: {e}") from e
