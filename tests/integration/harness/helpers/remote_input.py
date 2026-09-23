# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The firmware's remote input endpoint (kernel/remote_input.c)."""

import enum

from libpebble2.protocol.base import PebblePacket
from libpebble2.protocol.base.types import Uint8, Uint16

ENDPOINT = 0xF00D


class Status(enum.IntEnum):
    OK = 0
    BUSY = 1
    INVALID = 2


class RemoteInputButton(PebblePacket):
    class Meta:
        endpoint = ENDPOINT
        endianness = ">"
        register = False

    command = Uint8(default=0x00)
    button = Uint8()
    presses = Uint8()
    hold_ms = Uint16()
    gap_ms = Uint16()


class RemoteInputSwipe(PebblePacket):
    class Meta:
        endpoint = ENDPOINT
        endianness = ">"
        register = False

    command = Uint8(default=0x01)
    direction = Uint8()
    duration_ms = Uint16()


class RemoteInputButtonSet(PebblePacket):
    class Meta:
        endpoint = ENDPOINT
        endianness = ">"
        register = False

    command = Uint8(default=0x02)
    buttons = Uint8()


class RemoteInputAck(PebblePacket):
    class Meta:
        endpoint = ENDPOINT

    command = Uint8()
    status = Uint8()
