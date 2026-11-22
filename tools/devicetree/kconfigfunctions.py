# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Kconfig preprocessor functions reading the board devicetree.

Kconfiglib imports this module by name when it is on sys.path. The tree is the
one compiled into DT_BUILD_DIR before Kconfig runs; a board without a tree has
no partitions.

    default $(dt_partition_offset,code) if $(dt_has_partition,code)
    depends on $(has_partition,debug-db)
    def_bool $(dt_nodelabel_enabled,lptimer2)

has_partition is for code using a partition: a board without a tree takes
every partition from its C layout.
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
def _enabled_labels(path):
    if path is None or not path.exists():
        return frozenset()
    import dtmap
    import fdt

    tree = fdt.load(path)
    return frozenset(
        label for node in tree.nodes() if dtmap.enabled(node) for label in node.labels
    )


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


def dt_nodelabel_enabled(kconf, _name, label):
    """Whether the node with this label is enabled, as the one of several instances a board
    uses for something."""
    return "y" if label in _enabled_labels(_dtb()) else "n"


functions = {
    "dt_has_partition": (dt_has_partition, 1, 1),
    "has_partition": (has_partition, 1, 1),
    "dt_partition_offset": (dt_partition_offset, 1, 1),
    "dt_partition_size": (dt_partition_size, 1, 1),
    "dt_nodelabel_enabled": (dt_nodelabel_enabled, 1, 1),
}
