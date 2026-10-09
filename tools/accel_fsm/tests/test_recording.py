# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from .. import boards, recording


def test_roundtrip():
    samples = [(i, -i, 1000) for i in range(130)]
    rec = recording.parse(recording.build("walk", "obelix", 19230, samples))
    assert rec.label == "walk"
    assert rec.board == "obelix"
    assert rec.complete
    assert rec.samples == samples
    assert len(rec.chunks) == 3
    assert rec.gaps() == []


def test_unfinished():
    samples = [(1, 2, 3)] * 104
    data = bytearray(recording.build("bed", "obelix", 19230, samples))
    data[8:12] = b"\xff\xff\xff\xff"
    data += b"\xff" * 512
    rec = recording.parse(bytes(data))
    assert not rec.complete
    assert rec.samples == samples


def test_gap():
    samples = [(0, 0, 1000)] * 104
    data = bytearray(recording.build("run", "obelix", 19230, samples))
    # Second chunk starts 20 samples late
    offset = recording.HEADER.size + recording.CHUNK.size + 52 * recording.SAMPLE.size
    data[offset + 4 : offset + 8] = (1000 + 20 * 19230 // 1000).to_bytes(4, "little")
    rec = recording.parse(bytes(data))
    assert rec.gaps() == [(52, 20)]
    assert [(first, len(s)) for first, s in rec.segments()] == [(0, 52), (52, 52)]


def test_axes():
    obelix = boards.get("obelix")
    assert obelix.to_sensor((100, 200, 300)) == (-100, 200, 300)
    asterix = boards.get("asterix")
    assert asterix.to_sensor((100, 200, 300)) == (200, 100, 300)
    assert asterix.to_watch(asterix.to_sensor((1, 2, 3))) == (1, 2, 3)


def test_remap():
    from .. import fsm

    # asterix: watch Y is sensor X; obelix: watch X is sensor -X
    assert boards.remap_mask(fsm.parse_mask("+Y"), boards.get("asterix")) == 0x80
    assert boards.remap_mask(fsm.parse_mask("+X +V"), boards.get("obelix")) == 0x42
    program = fsm.assemble("frame watch\nmask A +Y\ncode:\nSMB +Z -X\nSTOP\n")
    sensor = boards.remap_program(program, boards.get("obelix"))
    assert sensor.frame == "sensor"
    assert sensor.masks == [0x20]
    assert sensor.code[1] == fsm.parse_mask("+Z +X")
