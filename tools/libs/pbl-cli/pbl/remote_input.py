# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The firmware's remote input endpoint (fw/kernel/remote_input.c)."""

from libpebble2.protocol.base import PebblePacket
from libpebble2.protocol.base.types import Int16, Uint8, Uint16

ENDPOINT = 0xF00D

STATUS = {0: "ok", 1: "busy", 2: "invalid"}


class RemoteInputTouch(PebblePacket):
    class Meta:
        endpoint = ENDPOINT
        endianness = ">"
        register = False

    command = Uint8(default=0x03)
    x1 = Int16()
    y1 = Int16()
    x2 = Int16()
    y2 = Int16()
    duration_ms = Uint16()


class RemoteInputAck(PebblePacket):
    class Meta:
        endpoint = ENDPOINT

    command = Uint8()
    status = Uint8()
