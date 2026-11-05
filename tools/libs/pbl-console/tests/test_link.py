# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""How reads split the stream must not change what comes out of it."""

import random

import pytest
from pblconsole.link import FRAME_MTU, ConsoleLink, encode_frame

STREAM = (
    b"I - 10:00:03.291 a.c:1> one\r\n"
    + encode_frame(b"P", 0, bytes(range(256)) + bytes(FRAME_MTU - 256))
    + b"pebble> "
    + b"\r\x1b[KI - 10:00:03.292 :0> NL:8d44 4\r\n"
    + encode_frame(b"A", 0, b"\x05")
    + b"pebble> hel"
    + b"\r\x1b[KW - 10:00:03.293 b.c:2> two\r\npebble> hel"
    + encode_frame(b"P", 0, b"\x01abc")
    + b"p\r\nCommands:\r\npebble> "
)


def run(chunks):
    logs, frames = [], []
    link = ConsoleLink("unused", on_log=lambda line, match: logs.append(match[4]))
    link.set_channel(b"P", frames.append)
    link._command = "help"
    for chunk in chunks:
        link.feed(chunk)
    return logs, frames, link._acked, link._echoed, link._output, link._at_prompt()


REFERENCE = run([STREAM])


def test_reference():
    logs, frames, acked, echoed, output, at_prompt = REFERENCE
    assert logs == ["one", "NL:8d44 4", "two"]
    assert len(frames) == 2
    assert len(frames[0]) == FRAME_MTU
    assert acked == 5
    assert echoed
    assert output == ["Commands:"]
    assert at_prompt


def test_byte_by_byte():
    assert run([STREAM[i : i + 1] for i in range(len(STREAM))]) == REFERENCE


@pytest.mark.parametrize("cut", range(1, len(STREAM)))
def test_every_split(cut):
    assert run([STREAM[:cut], STREAM[cut:]]) == REFERENCE


def test_random_chunks():
    rng = random.Random(1)
    for _ in range(500):
        chunks, i = [], 0
        while i < len(STREAM):
            size = rng.randint(1, 40)
            chunks.append(STREAM[i : i + size])
            i += size
        assert run(chunks) == REFERENCE


def test_damaged_frame_is_dropped_and_stream_recovers():
    frame = bytearray(encode_frame(b"P", 0, b"\x01abc"))
    frame[5] = ord("!")
    logs, frames, *_ = run([bytes(frame) + b"I - 10:00:03.291 a.c:1> after\r\n"])
    assert not frames
    assert logs == ["after"]


def test_firmware_frames_with_a_seq_are_acknowledged_and_handled_once():
    frames, written = [], []
    link = ConsoleLink("unused")
    link._write = written.append
    link.set_channel(b"P", frames.append)

    link.feed(encode_frame(b"P", 7, b"\x01abc"))
    link.feed(encode_frame(b"P", 7, b"\x01abc"))
    link.feed(encode_frame(b"P", 8, b"\x01def"))

    assert frames == [b"\x01abc", b"\x01def"]
    assert written == [
        encode_frame(b"A", 0, bytes([7])),
        encode_frame(b"A", 0, bytes([7])),
        encode_frame(b"A", 0, bytes([8])),
    ]


def test_firmware_frames_without_a_seq_are_not_acknowledged():
    frames, written = [], []
    link = ConsoleLink("unused")
    link._write = written.append
    link.set_channel(b"P", frames.append)

    link.feed(encode_frame(b"P", 0, b"\x02"))
    link.feed(encode_frame(b"P", 0, b"\x02"))

    assert frames == [b"\x02", b"\x02"]
    assert not written


def test_new_session_can_reuse_the_last_seq():
    frames = []
    link = ConsoleLink("unused")
    link._write = lambda data: None
    link.set_channel(b"P", frames.append)
    link.feed(encode_frame(b"P", 1, b"\x01abc"))

    link.set_channel(b"P", frames.append)
    link.feed(encode_frame(b"P", 1, b"\x01abc"))

    assert len(frames) == 2


def test_lost_tail_of_a_frame_does_not_merge_it_with_the_next():
    frames = []
    link = ConsoleLink("unused")
    link._write = lambda data: None
    link.set_channel(b"P", frames.append)

    first = encode_frame(b"P", 1, b"\x01" + b"a" * 100)
    second = encode_frame(b"P", 2, b"\x01" + b"b" * 100)
    link.feed(first[:-64] + second)

    assert not frames
