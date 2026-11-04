# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Writes firmware and resources to the flash of a watch with a console."""

import queue
import struct
import zlib

from .link import FRAME_MTU, ConsoleError, ConsoleTimeout

CHANNEL = b"B"

CMD_ERASE = 1
CMD_WRITE = 2
CMD_CRC = 3
CMD_QUERY_REGION = 4
CMD_FINALIZE_REGION = 5

RESP_ERASE_DONE = 128
RESP_CRC = 130
RESP_REGION = 131
RESP_FINALIZED = 132
RESP_ERROR = 193

REGION_RECOVERY = 1
REGION_RESOURCES = 2

WRITE_HEADER = struct.Struct("<BI")
ERASE_TIMEOUT_S = 180.0
CRC_TIMEOUT_S = 60.0
RESPONSE_TIMEOUT_S = 10.0

# typedef struct FirmwareDescription {
#   uint32_t description_length;
#   uint32_t firmware_length;
#   uint32_t checksum;
# } FirmwareDescription;
FIRMWARE_DESCRIPTION = struct.Struct("<III")


class ImagingError(ConsoleError):
    pass


class Imager:
    def __init__(self, link, progress=None):
        self._link = link
        self._progress = progress
        self._responses = queue.Queue()
        self._link.set_channel(CHANNEL, self._responses.put)

    def close(self):
        self._link.set_channel(CHANNEL, None)

    def _response(self, opcode, timeout):
        try:
            payload = self._responses.get(timeout=timeout)
        except queue.Empty:
            raise ConsoleTimeout(f"no response to imaging opcode {opcode}") from None
        if payload[0] == RESP_ERROR:
            raise ImagingError(f"the firmware failed imaging opcode {opcode}")
        if payload[0] != opcode:
            raise ImagingError(f"expected imaging opcode {opcode}, got {payload[0]}")
        return payload[1:]

    def _check_no_error(self):
        # What the firmware sends for a frame comes before its acknowledgment.
        try:
            self._response(None, 0)
        except ConsoleTimeout:
            pass

    def region(self, region):
        self._link.send_frame(CHANNEL, bytes([CMD_QUERY_REGION, region]))
        return struct.unpack("<II", self._response(RESP_REGION, RESPONSE_TIMEOUT_S))

    def finalize(self, region):
        self._link.send_frame(CHANNEL, bytes([CMD_FINALIZE_REGION, region]))
        self._response(RESP_FINALIZED, RESPONSE_TIMEOUT_S)

    def erase(self, address, length):
        self._link.send_frame(CHANNEL, struct.pack("<BII", CMD_ERASE, address, length))
        self._response(RESP_ERASE_DONE, ERASE_TIMEOUT_S)

    def write(self, address, data):
        chunk = FRAME_MTU - WRITE_HEADER.size
        for i in range(0, len(data), chunk):
            self._link.send_frame(
                CHANNEL,
                WRITE_HEADER.pack(CMD_WRITE, address + i) + data[i : i + chunk],
            )
            self._check_no_error()
            if self._progress is not None:
                self._progress(min(i + chunk, len(data)), len(data))

    def crc(self, address, length):
        self._link.send_frame(
            CHANNEL,
            struct.pack("<BII", CMD_CRC, address, length),
            timeout=CRC_TIMEOUT_S,
        )
        return struct.unpack("<I", self._response(RESP_CRC, RESPONSE_TIMEOUT_S))[0]

    def _load(self, address, image):
        self.erase(address, len(image))
        self.write(address, image)
        crc = self.crc(address, len(image))
        expected = zlib.crc32(image)
        if crc != expected:
            raise ImagingError(f"CRC is {crc:#010x}, expected {expected:#010x}")

    def load_recovery(self, firmware):
        """Write a recovery firmware, given the contents of its .bin."""
        address, _length = self.region(REGION_RECOVERY)
        description = FIRMWARE_DESCRIPTION.pack(
            FIRMWARE_DESCRIPTION.size, len(firmware), zlib.crc32(firmware)
        )
        self._load(address, description + firmware)
        self.finalize(REGION_RECOVERY)

    def load_resources(self, pack):
        """Write resources, given the contents of a .pbpack."""
        address, length = self.region(REGION_RESOURCES)
        if len(pack) > length:
            raise ImagingError(f"{len(pack)} bytes do not fit in {length}")
        self._load(address, pack)
        self.finalize(REGION_RESOURCES)
