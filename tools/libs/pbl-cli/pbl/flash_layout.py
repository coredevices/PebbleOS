# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The board's flash layout, read from the header its flash part selects.

``fw/flash_region/flash_region_<part>.h`` lists the regions back to back,
from FLASH_REGION_BASE_ADDRESS when the part is memory-mapped, from 0
otherwise.
"""

import os
import re

from pbl.errors import CommandContextError

_HEADER_RE = re.compile(r"flash_region_(\w+)\.h")
_BASE_RE = re.compile(r"#define FLASH_REGION_BASE_ADDRESS\s+(0x[0-9A-Fa-f]+)")
_REGION_RE = re.compile(r"MACRO\((\w+),\s*(0x[0-9A-Fa-f]+)")


def _header(build):
    layout_dir = os.path.join(build.topdir, "fw", "flash_region")
    for name in sorted(os.listdir(layout_dir)):
        match = _HEADER_RE.fullmatch(name)
        if match and build.config.get(f"CONFIG_FLASH_{match.group(1).upper()}") is True:
            return os.path.join(layout_dir, name)
    raise CommandContextError(f"no flash layout for board {build.board}")


def regions(build):
    """``{name: (address, size)}`` of the board's flash regions."""
    with open(_header(build)) as f:
        text = f.read()

    base = _BASE_RE.search(text)
    address = int(base.group(1), 16) if base else 0
    layout = {}
    for name, size in _REGION_RE.findall(text):
        layout[name] = (address, int(size, 16))
        address += int(size, 16)
    return layout
