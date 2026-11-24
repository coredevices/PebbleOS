# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Kconfig preprocessor functions reading the board devicetree.

Kconfiglib imports this module by name when it is on sys.path. The tree is the
one compiled into DT_BUILD_DIR before Kconfig runs; a board without a tree has
no partitions and no labelled nodes.

    default $(dt_partition_offset,code) if $(dt_has_partition,code)
    depends on $(has_partition,debug-db)
    default $(dt_nodelabel_reg_addr,retained_ram) if $(dt_has_nodelabel,retained_ram)
    default $(dt_memory_free_size,sram0) if $(dt_has_nodelabel,sram0)

has_partition is for code using a partition: a board without a tree takes
every partition from its C layout. Register addresses are the CPU's, through
the ranges of the buses above. The free part of a memory node is the largest
span of it no reserved-memory region covers.
"""

import functools
import os
from pathlib import Path


@functools.cache
def _partitions(path):
    if path is None or not path.exists():
        return {}
    # Only with a tree: the docs read the Kconfig without one, or dtmap's needs
    import dtmap
    import fdt

    return dtmap.partitions(fdt.load(path))


@functools.cache
def _tree(path):
    if path is None or not path.exists():
        return None
    import fdt

    return fdt.load(path)


def _dtb():
    build_dir = os.environ.get("DT_BUILD_DIR")
    return Path(build_dir) / "board.dtb" if build_dir else None


def _lookup(label):
    return _partitions(_dtb()).get(label)


def dt_has_partition(kconf, _name, label):
    return "y" if _lookup(label) else "n"


def has_partition(kconf, _name, label):
    dtb = _dtb()
    if dtb is None or not dtb.exists():
        return "y"
    return dt_has_partition(kconf, _name, label)


def dt_partition_offset(kconf, _name, label):
    part = _lookup(label)
    return hex(part[0]) if part else "0x0"


def dt_partition_size(kconf, _name, label):
    part = _lookup(label)
    return hex(part[1]) if part else "0x0"


def _node(label):
    tree = _tree(_dtb())
    if tree is None:
        return None
    return next((node for node in tree.nodes() if label in node.labels), None)


def _regs(node):
    import dtmap

    cells = node.u32s("reg") if node.has("reg") else []
    acells, scells = node.address_cells, node.size_cells
    step = acells + scells
    return [
        (
            dtmap.translate(node, dtmap.cells_number(cells[i : i + acells])),
            dtmap.cells_number(cells[i + acells : i + step]),
        )
        for i in range(0, len(cells) - step + 1, step)
    ]


def _reg(label, index):
    node = _node(label)
    regs = _regs(node) if node is not None else []
    index = int(index, 0)
    return regs[index] if index < len(regs) else None


def dt_has_nodelabel(kconf, _name, label):
    return "y" if _node(label) is not None else "n"


def dt_nodelabel_reg_addr(kconf, _name, label, index="0"):
    reg = _reg(label, index)
    return hex(reg[0]) if reg else "0x0"


def dt_nodelabel_reg_size(kconf, _name, label, index="0"):
    reg = _reg(label, index)
    return hex(reg[1]) if reg else "0x0"


def _memory_free(label):
    node = _node(label)
    if node is None or not node.has("reg"):
        return None
    start, size = _regs(node)[0]
    end = start + size
    reserved = _tree(_dtb()).by_path.get("/reserved-memory")
    taken = [
        (max(a, start), min(a + s, end))
        for child in (reserved.children if reserved is not None else [])
        if child.status_okay
        for a, s in _regs(child)
        if a < end and a + s > start
    ]
    free, cursor = (start, 0), start
    for a, b in sorted(taken) + [(end, end)]:
        if a - cursor > free[1]:
            free = (cursor, a - cursor)
        cursor = max(cursor, b)
    return free


def dt_memory_free_addr(kconf, _name, label):
    free = _memory_free(label)
    return hex(free[0]) if free else "0x0"


def dt_memory_free_size(kconf, _name, label):
    free = _memory_free(label)
    return hex(free[1]) if free else "0x0"


functions = {
    "dt_has_partition": (dt_has_partition, 1, 1),
    "has_partition": (has_partition, 1, 1),
    "dt_partition_offset": (dt_partition_offset, 1, 1),
    "dt_partition_size": (dt_partition_size, 1, 1),
    "dt_has_nodelabel": (dt_has_nodelabel, 1, 1),
    "dt_nodelabel_reg_addr": (dt_nodelabel_reg_addr, 1, 2),
    "dt_nodelabel_reg_size": (dt_nodelabel_reg_size, 1, 2),
    "dt_memory_free_addr": (dt_memory_free_addr, 1, 1),
    "dt_memory_free_size": (dt_memory_free_size, 1, 1),
}
