# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Firmware installation over the Pebble protocol, as the phone app does it:
announce the update, send the firmware and its resources with PutBytes,
install both, then tell the watch the update is complete so it reboots into
it."""

import json
import logging
import struct
import time
import zipfile

from harness.errors import HarnessError
from harness.helpers.phone import send_raw

logger = logging.getLogger(__name__)

SYSTEM_MESSAGE_ENDPOINT = 0x12
FIRMWARE_UPDATE_START = 0x01
FIRMWARE_UPDATE_COMPLETE = 0x02
FIRMWARE_UPDATE_START_RESPONSE = 0x0A
FIRMWARE_UPDATE_STARTED = 0x01

OBJECT_FIRMWARE = 0x01
OBJECT_RECOVERY = 0x02
OBJECT_SYSTEM_RESOURCES = 0x03
CHUNK_SIZE = 2000
PROGRESS_INTERVAL_S = 10.0


class FirmwareBundle:
    """A firmware .pbz: its firmware and system resources."""

    def __init__(self, path):
        self.path = path
        with zipfile.ZipFile(path) as bundle:
            manifest = json.loads(bundle.read("manifest.json"))
            firmware = manifest["firmware"]
            self.firmware = bundle.read(firmware["name"])
            self.firmware_type = firmware["type"]
            self.version_tag = firmware.get("versionTag")
            resources = manifest.get("resources")
            self.resources = bundle.read(resources["name"]) if resources else None

    @property
    def size(self):
        return len(self.firmware) + len(self.resources or b"")


def _start(pebble, size, timeout):
    from libpebble2.protocol.system import SystemMessage

    queue = pebble.get_endpoint_queue(SystemMessage)
    try:
        send_raw(
            pebble,
            SYSTEM_MESSAGE_ENDPOINT,
            struct.pack("<BBII", 0, FIRMWARE_UPDATE_START, 0, size),
        )
        deadline = time.monotonic() + timeout
        while True:
            message = queue.get(timeout=max(0.0, deadline - time.monotonic()))
            if message.message_type == FIRMWARE_UPDATE_START_RESPONSE:
                break
    finally:
        queue.close()
    if message.extra_data.response != FIRMWARE_UPDATE_STARTED:
        raise HarnessError(
            f"the watch refused the firmware update ({message.extra_data.response:#x})"
        )


def _put_bytes(pebble, object_type, data, what):
    from libpebble2.protocol import transfers
    from libpebble2.util import stm32_crc

    def request(payload):
        response = pebble.send_and_read(
            transfers.PutBytes(**payload), transfers.PutBytesResponse
        )
        if response.result != transfers.PutBytesResponse.Result.ACK:
            raise HarnessError(f"the watch refused {what} ({payload['command']:#x})")
        return response.cookie

    cookie = request(
        {
            "command": 0x01,
            "data": transfers.PutBytesInit(
                object_size=len(data), object_type=object_type, bank=0, filename=""
            ),
        }
    )
    started = last = time.monotonic()
    for offset in range(0, len(data), CHUNK_SIZE):
        chunk = data[offset : offset + CHUNK_SIZE]
        request(
            {
                "command": 0x02,
                "data": transfers.PutBytesPut(cookie=cookie, payload=chunk),
            }
        )
        if time.monotonic() - last >= PROGRESS_INTERVAL_S:
            last = time.monotonic()
            logger.info("%s: %d of %d bytes", what, offset + len(chunk), len(data))
    request(
        {
            "command": 0x03,
            "data": transfers.PutBytesCommit(
                cookie=cookie, object_crc=stm32_crc.crc32(data)
            ),
        }
    )
    elapsed = time.monotonic() - started
    logger.info(
        "%s: %d bytes in %.0f s (%.1f KiB/s)",
        what,
        len(data),
        elapsed,
        len(data) / 1024 / max(elapsed, 1e-3),
    )
    return cookie


def _install(pebble, cookie, what):
    from libpebble2.protocol import transfers

    response = pebble.send_and_read(
        transfers.PutBytes(command=0x05, data=transfers.PutBytesInstall(cookie=cookie)),
        transfers.PutBytesResponse,
    )
    if response.result != transfers.PutBytesResponse.Result.ACK:
        raise HarnessError(f"the watch did not install {what}")


def install_firmware(pebble, bundle, start_timeout=30):
    """Send ``bundle`` (a :class:`FirmwareBundle`) to the watch; it reboots
    into it a few seconds after this returns."""
    object_type = {"normal": OBJECT_FIRMWARE, "recovery": OBJECT_RECOVERY}[
        bundle.firmware_type
    ]
    _start(pebble, bundle.size, start_timeout)
    cookies = [
        (_put_bytes(pebble, object_type, bundle.firmware, "firmware"), "firmware")
    ]
    if bundle.resources is not None:
        cookies.append(
            (
                _put_bytes(
                    pebble, OBJECT_SYSTEM_RESOURCES, bundle.resources, "resources"
                ),
                "resources",
            )
        )
    # Installed only once both are there, so a failed transfer changes nothing.
    for cookie, what in cookies:
        _install(pebble, cookie, what)
    send_raw(
        pebble,
        SYSTEM_MESSAGE_ENDPOINT,
        struct.pack("<BB", 0, FIRMWARE_UPDATE_COMPLETE),
    )
