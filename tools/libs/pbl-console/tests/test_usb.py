# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import struct

import usb.core
from pblconsole.usb import StreamReassembler, UsbTransport, parse_selector


def _message(endpoint, payload):
    return struct.pack("!HH", len(payload), endpoint) + payload


def test_messages_split_across_transfers_come_out_whole():
    first = _message(2001, bytes(5))
    second = _message(16, bytes(range(150)))
    stream = first + second
    reassembler = StreamReassembler()

    out = []
    for i in range(0, len(stream), 64):
        reassembler.feed(stream[i : i + 64])
        while (packet := reassembler.next()) is not None:
            out.append(packet)

    assert out == [first, second]


def test_partial_header_waits_for_more():
    reassembler = StreamReassembler()
    reassembler.feed(b"\x00")

    assert reassembler.next() is None


def test_selector():
    assert parse_selector(None) == (None, None)
    assert parse_selector("any") == (None, None)
    assert parse_selector("1209:0001") == (0x1209, 0x0001)
    assert parse_selector("1209:") == (0x1209, None)


class _SlowEndpoint:
    """Takes at most one packet per write, and times out once, as a busy device does."""

    wMaxPacketSize = 64

    def __init__(self):
        self.data = b""
        self.writes = 0

    def write(self, data, timeout=None):
        self.writes += 1
        if self.writes == 2:
            raise usb.core.USBTimeoutError("timeout")
        chunk = bytes(data[:64])
        self.data += chunk
        return len(chunk)


def test_short_writes_are_resumed():
    transport = UsbTransport()
    transport._ep_out = _SlowEndpoint()
    transport._open = True
    message = _message(0xBEEF, bytes(range(200)))

    transport.send_packet(message)

    assert transport._ep_out.data == message
