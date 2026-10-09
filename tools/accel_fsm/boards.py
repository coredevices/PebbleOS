# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Accelerometer axis mapping per board, as in fw/board/boards/board_*.c."""

import dataclasses

from . import fsm


@dataclasses.dataclass(frozen=True)
class AxisConfig:
    # Sensor axis feeding each watch axis (x, y, z), and its sign
    axis_map: tuple
    axis_dir: tuple

    def to_sensor(self, sample):
        """Convert a watch-frame sample to the sensor frame."""
        out = [0, 0, 0]
        for watch_axis, sensor_axis in enumerate(self.axis_map):
            out[sensor_axis] = self.axis_dir[watch_axis] * sample[watch_axis]
        return tuple(out)

    def to_watch(self, sample):
        return tuple(
            self.axis_dir[watch_axis] * sample[sensor_axis]
            for watch_axis, sensor_axis in enumerate(self.axis_map)
        )


BOARDS = {
    "obelix": AxisConfig(axis_map=(0, 1, 2), axis_dir=(-1, 1, 1)),
    "asterix": AxisConfig(axis_map=(1, 0, 2), axis_dir=(1, 1, 1)),
    "getafix": AxisConfig(axis_map=(0, 1, 2), axis_dir=(-1, 1, 1)),
}

IDENTITY = AxisConfig(axis_map=(0, 1, 2), axis_dir=(1, 1, 1))


class BoardError(Exception):
    pass


def get(name):
    try:
        return BOARDS[name]
    except KeyError:
        raise BoardError(
            f"unknown board {name!r}, known: {', '.join(sorted(BOARDS))}"
        ) from None


def remap_mask(mask, config):
    """Convert a watch-frame mask to the sensor frame."""
    out = mask & 0x03
    for watch_axis, sensor_axis in enumerate(config.axis_map):
        pos = bool(mask & (0x80 >> (2 * watch_axis)))
        neg = bool(mask & (0x40 >> (2 * watch_axis)))
        if config.axis_dir[watch_axis] < 0:
            pos, neg = neg, pos
        out |= (0x80 >> (2 * sensor_axis)) if pos else 0
        out |= (0x40 >> (2 * sensor_axis)) if neg else 0
    return out


def remap_program(program, config):
    """Convert a watch-frame program to the sensor frame of a board."""
    if program.frame == "sensor":
        return program
    code = bytearray(program.code)
    set_mask = {fsm.MNEMONICS[n] for n in ("SMA", "SMB", "SMC")}
    for addr, op, _ in fsm.iter_instructions(program):
        if op in set_mask:
            i = addr - program.code_offset + 1
            code[i] = remap_mask(code[i], config)
    return dataclasses.replace(
        program,
        masks=[remap_mask(m, config) for m in program.masks],
        code=bytes(code),
        frame="sensor",
    )
