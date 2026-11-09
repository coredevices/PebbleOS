# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Pebble Protocol over GATT, the phone's side of a session
(pebble/comm/ble/kernel_le_client/ppogatt).

Reversed, the watch hosts the service and the phone starts the session with
a Reset Request; forward, the phone hosts it and the watch starts it. The
side that starts it sends a Reset Request, the other answers with a Reset
Complete, and the first confirms with its own.
Every packet carries a 1-byte header, ``sn << 3 | type``; data packets are
sent in a window and acknowledged by sequence number.

This class is transport-agnostic and not thread-safe: it is driven from one
event loop, which feeds it received packets and timer ticks.
"""

import asyncio
import enum
import logging
import time
from dataclasses import dataclass

logger = logging.getLogger(__name__)

SN_MOD = 32
VERSION = 1
# Windows the phone offers; the watch caps them to its own.
RX_WINDOW = 25
TX_WINDOW = 25
# ATT header (3) plus the PPoGATT header (1).
OVERHEAD = 4
ACK_TIMEOUT_S = 3.0
MAX_RETRANSMITS = 3
RESET_TIMEOUT_S = 5.0


class PacketType(enum.IntEnum):
    DATA = 0
    ACK = 1
    RESET_REQUEST = 2
    RESET_COMPLETE = 3


def header(packet_type, sn=0):
    return bytes([(sn % SN_MOD) << 3 | packet_type])


@dataclass(frozen=True)
class LateAcks:
    """Acks as they reach the watch over a link at ``interval_s`` on which
    it skips up to ``latency`` connection events: ``events`` late while the
    watch is sending, and, once it has sent nothing for ``quiet_events``,
    held until it sends again or wakes after its last skipped event."""

    interval_s: float
    latency: int
    events: int = 1
    quiet_events: int = 1


class PPoGATT:
    """``write(bytes)`` sends one GATT write; ``on_data(bytes)`` gets the
    Pebble Protocol stream; ``on_open()`` and ``on_reset()`` report the
    session state. ``late_acks``, a :class:`LateAcks`, delays the Acks."""

    def __init__(
        self,
        write,
        on_data,
        on_open=None,
        on_reset=None,
        serial=b"PBLHARNESS00",
        late_acks=None,
    ):
        self._write = write
        self.late_acks = late_acks
        self._last_rx = 0.0
        self._on_data = on_data
        self._on_open = on_open
        self._on_reset = on_reset
        self._serial = serial[:12].ljust(12, b"\0")
        self.mtu = 23
        self.is_open = False
        self._reset_state()

    def _reset_state(self):
        self.is_open = False
        self._tx_sn = 0
        self._rx_sn = 0
        self._tx_window = TX_WINDOW
        self._in_flight = []  # (sn, packet, sent_at)
        self._pending = bytearray()
        self._retransmits = 0
        self._reset_sent_at = None
        self._answered_reset = False
        self._ignore_data = 0
        self._held_acks = []  # (due, packet)

    @property
    def max_payload(self):
        return max(1, self.mtu - OVERHEAD)

    # --- session ------------------------------------------------------------

    def reset(self):
        """Start (or restart) the session."""
        self._reset_state()
        self._reset_sent_at = time.monotonic()
        self._write(header(PacketType.RESET_REQUEST) + bytes([VERSION]) + self._serial)

    def _answer_reset(self):
        self._reset_state()
        self._answered_reset = True
        self._write(header(PacketType.RESET_COMPLETE) + bytes([RX_WINDOW, TX_WINDOW]))

    def _complete_reset(self, payload):
        if len(payload) >= 2:
            # The watch's windows: what it can receive, then send.
            self._tx_window = max(1, min(TX_WINDOW, payload[0]))
        if not self._answered_reset:
            self._write(
                header(PacketType.RESET_COMPLETE) + bytes([RX_WINDOW, TX_WINDOW])
            )
        self._answered_reset = False
        self._reset_sent_at = None
        self.is_open = True
        if self._on_open:
            self._on_open()
        self._pump()

    # --- receiving ----------------------------------------------------------

    def receive(self, packet):
        if not packet:
            return
        self._last_rx = time.monotonic()
        self._release_acks()
        packet_type, sn, payload = packet[0] & 0x7, packet[0] >> 3, packet[1:]
        logger.debug(
            "rx %s sn=%d len=%d", PacketType(packet_type).name, sn, len(payload)
        )
        if packet_type == PacketType.RESET_COMPLETE:
            self._complete_reset(payload)
        elif packet_type == PacketType.RESET_REQUEST:
            if self.is_open and self._on_reset:
                self._on_reset()
            self._answer_reset()
        elif not self.is_open:
            return
        elif packet_type == PacketType.DATA:
            self._receive_data(sn, payload)
        elif packet_type == PacketType.ACK:
            self._receive_ack(sn)

    @property
    def expected_sn(self):
        """The sequence number of the next data packet expected."""
        return self._rx_sn

    def ignore_data(self, count):
        """Drop the next ``count`` data packets received, as if lost."""
        self._ignore_data = count

    def _receive_data(self, sn, payload):
        if self._ignore_data:
            self._ignore_data -= 1
            return
        if sn == self._rx_sn:
            self._rx_sn = (self._rx_sn + 1) % SN_MOD
            self._ack(sn)
            self._on_data(payload)
        else:
            # Out of order: acknowledge the last one in order again.
            self._ack(self._rx_sn - 1)

    def _ack(self, sn):
        packet = header(PacketType.ACK, sn)
        if self.late_acks is None:
            self._write(packet)
            return
        delay = self.late_acks.events * self.late_acks.interval_s
        self._held_acks.append((time.monotonic() + delay, packet))
        asyncio.get_running_loop().call_later(delay, self._ack_due)

    def _ack_due(self):
        late = self.late_acks
        now = time.monotonic()
        if now - self._last_rx <= late.quiet_events * late.interval_s:
            self._release_acks()
            return
        wake = self._last_rx + late.latency * late.interval_s
        asyncio.get_running_loop().call_later(max(0.0, wake - now), self._release_acks)

    def _release_acks(self):
        now = time.monotonic()
        while self._held_acks and self._held_acks[0][0] <= now:
            self._write(self._held_acks.pop(0)[1])

    def _receive_ack(self, sn):
        for i, (flight_sn, _, _) in enumerate(self._in_flight):
            if flight_sn == sn:
                del self._in_flight[: i + 1]
                self._retransmits = 0
                break
        self._pump()

    # --- sending ------------------------------------------------------------

    def send(self, data):
        self._pending += data
        self._pump()

    def _pump(self):
        if not self.is_open:
            return
        while self._pending and len(self._in_flight) < self._tx_window:
            chunk = bytes(self._pending[: self.max_payload])
            del self._pending[: len(chunk)]
            packet = header(PacketType.DATA, self._tx_sn) + chunk
            self._in_flight.append((self._tx_sn, packet, time.monotonic()))
            self._tx_sn = (self._tx_sn + 1) % SN_MOD
            self._write(packet)

    def tick(self):
        """Retransmit what timed out; restart a stuck reset or session."""
        now = time.monotonic()
        if self._reset_sent_at is not None:
            if now - self._reset_sent_at > RESET_TIMEOUT_S:
                self.reset()
            return
        if not self._in_flight or now - self._in_flight[0][2] < ACK_TIMEOUT_S:
            return
        self._retransmits += 1
        if self._retransmits > MAX_RETRANSMITS:
            if self._on_reset:
                self._on_reset()
            self.reset()
            return
        self._in_flight = [(sn, packet, now) for sn, packet, _ in self._in_flight]
        for _, packet, _ in self._in_flight:
            self._write(packet)
