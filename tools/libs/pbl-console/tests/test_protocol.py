# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

from pblconsole.link import SEND_TIMEOUT_S, ConsoleLink
from pblconsole.protocol import OPEN, FrameTransport


def test_close_does_not_wait_for_a_watch_that_is_not_listening():
    link = ConsoleLink("unused")
    link._write = lambda data: None
    transport = FrameTransport(link)
    transport._open = True

    start = time.monotonic()
    transport.disconnect()

    assert time.monotonic() - start < SEND_TIMEOUT_S / 2
    assert not transport.connected


def test_lost_answer_to_the_session_open_is_asked_for_again():
    link = ConsoleLink("unused")
    transport = FrameTransport(link)
    sends = []

    def send_frame(channel, payload, seq=None, timeout=None):
        sends.append(payload)
        if len(sends) == 2:
            transport._feed(bytes([OPEN]))

    link.send_frame = send_frame
    transport.connect()

    assert sends == [bytes([OPEN]), bytes([OPEN])]
    assert transport.connected
