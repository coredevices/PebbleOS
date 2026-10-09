# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pathlib

import pytest

from .. import fsm, ucf

ST = pathlib.Path(__file__).parent / "st"


def _program(name, index=0):
    return ucf.load(ST / name).programs[index]


def _run(program, values, axis):
    sim = fsm.Simulator(program)
    events = []
    for v in values:
        sample = [0.0, 0.0, 1.0]
        sample[axis] = v
        events += [e.sample for e in sim.step(tuple(sample))]
    return events


@pytest.mark.parametrize(
    "name",
    ["lsm6dso_wrist_tilt_xl.ucf", "lsm6dso_shake.ucf", "lsm6dso_wrist_navigation.ucf"],
)
def test_roundtrip(name):
    text = (ST / name).read_text()
    memory = ucf.program_memory(text)
    offset = 0
    for program in ucf.parse(text).programs:
        image = fsm.assemble(fsm.disassemble(program)).to_bytes()
        original = bytearray(memory[offset : offset + len(image)])
        original[4:6] = b"\x00\x00"
        assert image == bytes(original)
        offset += len(image)


def test_ucf_config():
    config = ucf.load(ST / "lsm6dso_wrist_navigation.ucf")
    assert len(config.programs) == 4
    assert config.enabled == 0xF
    assert config.fsm_odr_hz == 52


def test_wrist_tilt():
    program = _program("lsm6dso_wrist_tilt_xl.ucf")
    assert program.thresholds == [pytest.approx(0.1736, abs=1e-3)]
    assert _run(program, [0.0] * 20 + [0.5] * 10, axis=1) == [25]
    assert _run(program, [0.0] * 20 + [0.5] * 4 + [0.0] * 10, axis=1) == []
    assert _run(program, [0.5] * 30, axis=1) == []


def test_shake():
    program = _program("lsm6dso_shake.ucf")
    swings = [0] * 5 + [-2, -2, 2, 2, -2, -2] + [0] * 5
    assert _run(program, swings, axis=0) == [9]
    assert (
        _run(program, [0] * 5 + [-2] + [0] * 10 + [2] + [0] * 10 + [-2], axis=0) == []
    )
    assert _run(program, [0] * 5 + [-2.5] + [0] * 20, axis=0) == []


def test_flick_out():
    program = _program("lsm6dso_wrist_navigation.ucf", 2)
    flick = [0.3] * 5 + [0.0, -1.0, -2.5, -1.5]
    assert _run(program, flick + [-0.5] * 20 + [0.5] * 3, axis=1) == [29]
    assert _run(program, flick + [0.5] * 10, axis=1) == []


def test_flick_in():
    program = _program("lsm6dso_wrist_navigation.ucf", 3)
    values = [0.3] * 5 + [0.0] + [-0.8] * 10 + [-2.2, -1.0, 0.5] + [0.5] * 4
    assert _run(program, values, axis=1) == [19]


def test_unsimulated_rejected():
    program = fsm.assemble("timer TI3 2\ncode:\nNOP|TI3\nINCR\nCONTREL\n")
    with pytest.raises(fsm.FsmError):
        fsm.Simulator(program)


def test_reset_before_next():
    # AN5226: RESET is evaluated first, NEXT only when RESET does not hold
    program = fsm.assemble("thresh 1 1.0\nmask A +X\ncode:\nGNTH1|GNTH1\n")
    assert program.code == bytes([0x55])  # opcode 55h is SETP, not GNTH1|GNTH1
    program = fsm.assemble(
        "thresh 1 1.0\nthresh 2 0.5\nmask A +X\ncode:\nGNTH1|GNTH2\nCONTREL\n"
    )
    assert _run(program, [0, 2, 0.7, 0], axis=0) == [2]


def test_sctc1_shared_window():
    window = """
        thresh 1 1.0
        mask A +X
        timer TI3 4
        code:
        {mode}
        NOP|GNTH1
        TI3|LRTH1
        TI3|GNTH1
        CONTREL
        """
    # Second swing 3 samples after the first, third 3 samples after that
    values = [0, 2, 0, 0, -2, 0, 0, 2, 0]
    assert _run(fsm.assemble(window.format(mode="SCTC0")), values, axis=0) == [7]
    assert _run(fsm.assemble(window.format(mode="SCTC1")), values, axis=0) == []


def test_even_size():
    program = fsm.assemble("timer TI3 2\ncode:\nNOP|TI3\n")
    assert len(program.to_bytes()) % 2 == 0


def test_temporary_mask():
    program = fsm.assemble(
        """
        thresh 1 1.5
        thresh 2 -1.5
        mask A +X -X
        timer TI3 5
        code:
        NOP|GNTH1
        TI3|LNTH2
        CONTREL
        """
    )
    # +X over then under: fires
    assert _run(program, [0, 2, 0, -2, 0], axis=0) == [3]
    # -X over (x < -1.5) then -X under (x > 1.5): fires too
    assert _run(program, [0, -2, 0, 2, 0], axis=0) == [3]
    # +X over then +X over again: never under
    assert _run(program, [0, 2, 0, 2, 0, 0, 0, 0, 0], axis=0) == []


def test_timers_in_ms():
    text = "odr 26\ntimer TI3 190ms\ncode:\nNOP|TI3\nCONTREL\n"
    assert fsm.assemble(text).timers == {3: 5}
    assert fsm.assemble(text, odr=52).timers == {3: 10}
    with pytest.raises(fsm.FsmError):
        fsm.assemble("timer TI3 190ms\ncode:\nNOP|TI3\n")


def test_overrides():
    text = "thresh 1 0.5\nmask A +X\ntimer TI3 4\ncode:\nTI3|GNTH1\nCONTREL\n"
    program = fsm.assemble(text, odr=26, overrides={"thresh1": "0.8", "TI3": "100ms"})
    assert program.thresholds == [0.8]
    assert program.timers == {3: 3}
    with pytest.raises(fsm.FsmError):
        fsm.assemble(text, overrides={"thresh2": "1.0"})
