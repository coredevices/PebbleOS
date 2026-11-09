# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Flattened devicetree (DTB) reader."""

import struct

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9


class DtError(Exception):
    pass


class Node:
    def __init__(self, name, parent):
        self.name = name
        self.parent = parent
        self.props = {}
        self.children = []
        self.labels = []

    @property
    def path(self):
        if self.parent is None:
            return "/"
        if self.parent.parent is None:
            return "/" + self.name
        return self.parent.path + "/" + self.name

    @property
    def basename(self):
        return self.name.split("@", 1)[0]

    @property
    def unit_address(self):
        parts = self.name.split("@", 1)
        return parts[1] if len(parts) == 2 else None

    def __repr__(self):
        return f"<Node {self.path}>"

    def has(self, prop):
        return prop in self.props

    def data(self, prop):
        if prop not in self.props:
            raise DtError(f"{self.path}: no '{prop}'")
        return self.props[prop]

    def u32s(self, prop):
        data = self.data(prop)
        if len(data) % 4:
            raise DtError(f"{self.path}: '{prop}' is not a list of 32-bit cells")
        return list(struct.unpack(f">{len(data) // 4}I", data))

    def u32(self, prop):
        cells = self.u32s(prop)
        if len(cells) != 1:
            raise DtError(f"{self.path}: '{prop}' must be a single cell")
        return cells[0]

    def u64(self, prop):
        hi, lo = self.u32s(prop)
        return (hi << 32) | lo

    def u8s(self, prop):
        return list(self.data(prop))

    def strings(self, prop):
        data = self.data(prop)
        if not data or data[-1] != 0:
            raise DtError(f"{self.path}: '{prop}' is not a string list")
        return [s.decode() for s in data[:-1].split(b"\0")]

    def string(self, prop):
        values = self.strings(prop)
        if len(values) != 1:
            raise DtError(f"{self.path}: '{prop}' must be a single string")
        return values[0]

    @property
    def status(self):
        return self.string("status") if "status" in self.props else "okay"

    @property
    def status_okay(self):
        return self.status in ("okay", "ok")

    @property
    def compatibles(self):
        return self.strings("compatible") if "compatible" in self.props else []

    def cells_of(self, prop, default):
        return self.u32(prop) if prop in self.props else default

    @property
    def address_cells(self):
        return self.parent.cells_of("#address-cells", 2) if self.parent else 2

    @property
    def size_cells(self):
        return self.parent.cells_of("#size-cells", 1) if self.parent else 1

    def walk(self):
        yield self
        for child in self.children:
            yield from child.walk()


class Tree:
    def __init__(self, root):
        self.root = root
        self.phandles = {}
        self.by_path = {}
        for node in root.walk():
            self.by_path[node.path] = node
            if "phandle" in node.props:
                self.phandles[node.u32("phandle")] = node
        symbols = self.by_path.get("/__symbols__")
        if symbols is not None:
            for label in symbols.props:
                target = self.by_path.get(symbols.string(label))
                if target is not None:
                    target.labels.append(label)
        # dtc orders labels by how they were added: sorted, the first one is stable
        for node in root.walk():
            node.labels.sort()

    def nodes(self):
        for node in self.root.walk():
            if node.path.startswith("/__"):
                continue
            yield node

    def phandle(self, value, where):
        try:
            return self.phandles[value]
        except KeyError:
            raise DtError(f"{where}: unknown phandle {value:#x}") from None

    def path(self, path):
        try:
            return self.by_path[path]
        except KeyError:
            raise DtError(f"unknown node path '{path}'") from None


def parse(data):
    (
        magic,
        _totalsize,
        off_struct,
        off_strings,
        _off_rsvmap,
        version,
        _last_comp,
        _boot_cpuid,
        size_strings,
        size_struct,
    ) = struct.unpack_from(">10I", data, 0)
    if magic != FDT_MAGIC:
        raise DtError("not a flattened devicetree")
    if version < 17:
        raise DtError(f"unsupported DTB version {version}")
    strings = data[off_strings : off_strings + size_strings]

    def string_at(offset):
        end = strings.index(b"\0", offset)
        return strings[offset:end].decode()

    pos = off_struct
    end = off_struct + size_struct
    root = None
    node = None
    while pos < end:
        (token,) = struct.unpack_from(">I", data, pos)
        pos += 4
        if token == FDT_BEGIN_NODE:
            name_end = data.index(b"\0", pos)
            name = data[pos:name_end].decode()
            pos = (name_end + 4) & ~3
            child = Node(name, node)
            if node is None:
                root = child
            else:
                node.children.append(child)
            node = child
        elif token == FDT_END_NODE:
            node = node.parent
        elif token == FDT_PROP:
            length, nameoff = struct.unpack_from(">II", data, pos)
            pos += 8
            node.props[string_at(nameoff)] = bytes(data[pos : pos + length])
            pos = (pos + length + 3) & ~3
        elif token == FDT_NOP:
            continue
        elif token == FDT_END:
            break
        else:
            raise DtError(f"bad FDT token {token:#x} at {pos - 4:#x}")
    if root is None:
        raise DtError("empty devicetree")
    return Tree(root)


def load(path):
    with open(path, "rb") as f:
        return parse(f.read())
