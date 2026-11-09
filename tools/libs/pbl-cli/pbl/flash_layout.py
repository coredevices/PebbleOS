# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The board's flash layout, read from the partition header it selects or generates."""

import os
import re

from pbl.errors import CommandContextError

_LAYOUTS = ("dual_slot", "single_slot")
_PARTITION_RE = re.compile(
    r"#define\s+PART_(\w+)_OFFSET\s+(0x[0-9A-Fa-f]+)\s*"
    r"#define\s+PART_\1_SIZE\s+(?:PBL_(KIB|MIB)\((\d+)\)|(0x[0-9A-Fa-f]+))"
)
_UNITS = {"KIB": 1024, "MIB": 1024 * 1024}


def _header(build):
    if build.config.get("CONFIG_BOARD_PARTITIONS_DT") is True:
        return build.join("generated", "include", "devicetree", "partitions.h")
    for layout in _LAYOUTS:
        if build.config.get(f"CONFIG_BOARD_PARTITIONS_{layout.upper()}") is True:
            return os.path.join(
                build.topdir, "include", "pbl", "board", f"partitions_{layout}.h"
            )
    raise CommandContextError(f"no flash layout for board {build.board}")


def regions(build):
    """``{name: (offset, size)}`` of the board's flash partitions."""
    with open(_header(build)) as f:
        text = f.read()

    return {
        name: (int(offset, 16), int(count) * _UNITS[unit] if unit else int(size, 16))
        for name, offset, unit, count, size in _PARTITION_RE.findall(text)
    }
