#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""CANopen object dictionary generator.

Reads the YAML description of a device and writes the C object dictionary
the CANopen subsystem runs on, a header with its index and sub-index
macros, and an EDS file for the tools on the other end of the bus. The
communication objects (0x1000-0x1fff) come from the device, heartbeat,
SYNC, emergency and PDO sections; the application objects from the
objects section. See docs/subsystems/canopen.md for the format.
"""

import argparse
import re
import sys
from dataclasses import dataclass, field

import yaml

# name: (CANopen data type, C type, size in bytes)
TYPES = {
    "bool": (0x01, "bool", 1),
    "i8": (0x02, "int8_t", 1),
    "i16": (0x03, "int16_t", 2),
    "i32": (0x04, "int32_t", 4),
    "u8": (0x05, "uint8_t", 1),
    "u16": (0x06, "uint16_t", 2),
    "u32": (0x07, "uint32_t", 4),
    "f32": (0x08, "float", 4),
    "string": (0x09, "char", None),
    "octets": (0x0A, "uint8_t", None),
    "f64": (0x11, "double", 8),
    "i64": (0x15, "int64_t", 8),
    "u64": (0x1B, "uint64_t", 8),
}

C_TYPES = {
    0x01: "BOOLEAN",
    0x02: "INTEGER8",
    0x03: "INTEGER16",
    0x04: "INTEGER32",
    0x05: "UNSIGNED8",
    0x06: "UNSIGNED16",
    0x07: "UNSIGNED32",
    0x08: "REAL32",
    0x09: "VISIBLE_STRING",
    0x0A: "OCTET_STRING",
    0x11: "REAL64",
    0x15: "INTEGER64",
    0x1B: "UNSIGNED64",
}

ACCESS = ("ro", "wo", "rw", "const")

ERROR_BEHAVIOR = {"pre-operational": 0, "none": 1, "stopped": 2}

COB_INVALID = 1 << 31
COB_NO_RTR = 1 << 30

MAX_PDOS = 512
MAX_MAP = 8

IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class Error(Exception):
    pass


@dataclass
class Entry:
    index: int
    sub: int
    name: str
    type: str
    access: str
    default: object = 0
    size: int = 0
    tpdo: bool = False
    rpdo: bool = False
    nodeid: bool = False

    @property
    def readable(self):
        return self.access != "wo"

    @property
    def writable(self):
        return self.access in ("wo", "rw")

    @property
    def member(self):
        return f"x{self.index:04x}_{self.sub:02x}"


@dataclass
class Object:
    index: int
    name: str
    kind: str
    entries: list = field(default_factory=list)


def prv_int(value, what):
    if isinstance(value, bool) or not isinstance(value, int):
        raise Error(f"{what}: expected an integer, got {value!r}")
    return value


def prv_check_keys(node, allowed, what):
    if not isinstance(node, dict):
        raise Error(f"{what}: expected a mapping")
    unknown = set(node) - set(allowed)
    if unknown:
        raise Error(f"{what}: unknown keys {', '.join(sorted(unknown))}")


def prv_entry(index, sub, name, node, what, inherit=None):
    inherit = inherit or {}
    type_ = node.get("type", inherit.get("type"))
    if type_ not in TYPES:
        raise Error(f"{what}: unknown type {type_!r}")
    access = node.get("access", inherit.get("access", "rw"))
    if access not in ACCESS:
        raise Error(f"{what}: unknown access {access!r}")

    default = node.get("default", inherit.get("default"))
    size = TYPES[type_][2]
    if size is None:
        default = "" if default is None else default
        if type_ == "string" and not isinstance(default, str):
            raise Error(f"{what}: string default must be a string")
        if type_ == "octets":
            if isinstance(default, str):
                default = bytes.fromhex(default)
            default = bytes(default)
        size = node.get("size", inherit.get("size", max(len(default), 1)))
        size = prv_int(size, f"{what}: size")
        if len(default) > size:
            raise Error(f"{what}: default longer than size {size}")
    else:
        default = 0 if default is None else default
        if type_ in ("f32", "f64"):
            if isinstance(default, bool) or not isinstance(default, (int, float)):
                raise Error(f"{what}: expected a number")
        elif type_ == "bool":
            default = int(bool(default))
        else:
            prv_int(default, f"{what}: default")
            bits = size * 8
            lo, hi = (
                (-(1 << (bits - 1)), (1 << (bits - 1)) - 1)
                if type_.startswith("i")
                else (0, (1 << bits) - 1)
            )
            if not lo <= default <= hi:
                raise Error(f"{what}: default {default} out of range")

    pdo = node.get("pdo", inherit.get("pdo", False))
    if pdo not in (True, False, "tx", "rx"):
        raise Error(f"{what}: pdo must be true, false, tx or rx")
    tpdo = pdo == "tx" or (pdo is True and access != "wo")
    rpdo = pdo == "rx" or (pdo is True and access in ("wo", "rw"))
    if tpdo and access == "wo":
        raise Error(f"{what}: a write-only entry cannot go in a TPDO")
    if rpdo and access not in ("wo", "rw"):
        raise Error(f"{what}: a read-only entry cannot go in an RPDO")
    if (tpdo or rpdo) and TYPES[type_][2] is None:
        raise Error(f"{what}: strings cannot be mapped to PDOs")

    return Entry(index, sub, name, type_, access, default, size, tpdo, rpdo)


def prv_var(index, name, type_, access, default=0, **kw):
    node = {"type": type_, "access": access, "default": default, **kw}
    nodeid = node.pop("nodeid", False)
    e = prv_entry(index, 0, name, node, f"0x{index:04x}")
    e.nodeid = nodeid
    return e


class Dictionary:
    def __init__(self, doc, name):
        prv_check_keys(
            doc,
            (
                "device",
                "errors",
                "error_behavior",
                "emcy",
                "heartbeat",
                "sync",
                "rpdos",
                "tpdos",
                "objects",
            ),
            "top level",
        )
        self.name = name
        if not IDENT.match(self.name):
            raise Error(f"name {self.name!r} is not a C identifier")
        self.objects = {}
        self.names = {}
        self.device = doc.get("device") or {}
        prv_check_keys(
            self.device,
            (
                "type",
                "name",
                "hardware_version",
                "software_version",
                "vendor_id",
                "product_code",
                "revision",
                "serial",
            ),
            "device",
        )

        self.prv_objects(doc.get("objects") or [])
        self.prv_comm(doc)

    def prv_add(self, obj):
        if obj.index in self.objects:
            raise Error(f"object 0x{obj.index:04x} defined twice")
        self.objects[obj.index] = obj

    def prv_comm_var(self, index, param, *args, **kw):
        self.prv_add(Object(index, param, "var", [prv_var(index, param, *args, **kw)]))

    def prv_comm_array(self, index, param, sub0_access, entries):
        sub0 = prv_var(
            index, "Highest sub-index supported", "u8", sub0_access, len(entries)
        )
        obj = Object(index, param, "array", [sub0])
        for sub, e in enumerate(entries, 1):
            e.index = index
            e.sub = sub
            obj.entries.append(e)
        self.prv_add(obj)

    def prv_comm_record(self, index, param, entries):
        sub0 = prv_var(
            index, "Highest sub-index supported", "u8", "const", entries[-1].sub
        )
        obj = Object(index, param, "record", [sub0])
        for e in entries:
            e.index = index
            obj.entries.append(e)
        self.prv_add(obj)

    def prv_sub(self, sub, name, type_, access, default=0, **kw):
        e = prv_var(0, name, type_, access, default, **kw)
        e.sub = sub
        return e

    def prv_comm(self, doc):
        dev = self.device
        self.prv_comm_var(0x1000, "Device type", "u32", "ro", dev.get("type", 0))
        self.prv_comm_var(0x1001, "Error register", "u8", "ro", 0, pdo="tx")

        errors = prv_int(doc.get("errors", 8), "errors")
        if not 0 <= errors <= 254:
            raise Error("errors: 0 to 254")
        self.num_errors = errors
        if errors:
            self.prv_comm_array(
                0x1003,
                "Pre-defined error field",
                "rw",
                [
                    self.prv_sub(0, "Standard error field", "u32", "ro")
                    for _ in range(errors)
                ],
            )
            self.objects[0x1003].entries[0].default = 0

        sync = doc.get("sync") or {}
        prv_check_keys(sync, ("cob_id", "period", "window"), "sync")
        self.prv_comm_var(
            0x1005, "COB-ID SYNC message", "u32", "rw", sync.get("cob_id", 0x80)
        )
        self.prv_comm_var(
            0x1006, "Communication cycle period", "u32", "rw", sync.get("period", 0)
        )
        self.prv_comm_var(
            0x1007, "Synchronous window length", "u32", "rw", sync.get("window", 0)
        )

        for index, key, param in (
            (0x1008, "name", "Manufacturer device name"),
            (0x1009, "hardware_version", "Manufacturer hardware version"),
            (0x100A, "software_version", "Manufacturer software version"),
        ):
            if key in dev:
                self.prv_comm_var(index, param, "string", "const", str(dev[key]))

        emcy = doc.get("emcy") or {}
        prv_check_keys(emcy, ("cob_id", "inhibit"), "emcy")
        if "cob_id" in emcy:
            self.prv_comm_var(0x1014, "COB-ID EMCY", "u32", "rw", emcy["cob_id"])
        else:
            self.prv_comm_var(0x1014, "COB-ID EMCY", "u32", "rw", 0x80, nodeid=True)
        self.prv_comm_var(
            0x1015, "Inhibit time EMCY", "u16", "rw", emcy.get("inhibit", 0)
        )

        hb = doc.get("heartbeat") or {}
        prv_check_keys(hb, ("producer", "consumers"), "heartbeat")
        consumers = hb.get("consumers", 0)
        if isinstance(consumers, int) and not isinstance(consumers, bool):
            consumers = [{} for _ in range(consumers)]
        if not isinstance(consumers, list) or len(consumers) > 127:
            raise Error("heartbeat: consumers must be a count or a list, up to 127")
        entries = []
        for i, c in enumerate(consumers):
            prv_check_keys(c, ("node", "time"), f"heartbeat consumer {i + 1}")
            node = prv_int(c.get("node", 0), "heartbeat consumer node")
            time = prv_int(c.get("time", 0), "heartbeat consumer time")
            if not 0 <= node <= 127 or not 0 <= time <= 0xFFFF:
                raise Error(f"heartbeat consumer {i + 1}: node 0-127, time 0-65535 ms")
            entries.append(
                self.prv_sub(
                    0, "Consumer heartbeat time", "u32", "rw", (node << 16) | time
                )
            )
        self.num_hb_consumers = len(entries)
        if entries:
            self.prv_comm_array(0x1016, "Consumer heartbeat time", "ro", entries)
        self.prv_comm_var(
            0x1017, "Producer heartbeat time", "u16", "rw", hb.get("producer", 0)
        )

        self.prv_comm_record(
            0x1018,
            "Identity object",
            [
                self.prv_sub(1, "Vendor-ID", "u32", "ro", dev.get("vendor_id", 0)),
                self.prv_sub(
                    2, "Product code", "u32", "ro", dev.get("product_code", 0)
                ),
                self.prv_sub(3, "Revision number", "u32", "ro", dev.get("revision", 0)),
                self.prv_sub(4, "Serial number", "u32", "ro", dev.get("serial", 0)),
            ],
        )

        self.prv_comm_record(
            0x1200,
            "SDO server parameter",
            [
                self.prv_sub(
                    1, "COB-ID client to server", "u32", "ro", 0x600, nodeid=True
                ),
                self.prv_sub(
                    2, "COB-ID server to client", "u32", "ro", 0x580, nodeid=True
                ),
            ],
        )

        behavior = doc.get("error_behavior", "pre-operational")
        if behavior not in ERROR_BEHAVIOR:
            raise Error(f"error_behavior: one of {', '.join(ERROR_BEHAVIOR)}")
        entry = self.prv_sub(
            0, "Communication error", "u8", "rw", ERROR_BEHAVIOR[behavior]
        )
        self.prv_comm_array(0x1029, "Error behavior object", "const", [entry])

        self.rpdos = self.prv_pdos(doc.get("rpdos") or [], False)
        self.tpdos = self.prv_pdos(doc.get("tpdos") or [], True)

    def prv_ref(self, ref, tx, what):
        if isinstance(ref, dict):
            prv_check_keys(ref, ("index", "sub"), what)
            key = (prv_int(ref.get("index"), what), prv_int(ref.get("sub", 0), what))
        else:
            if not isinstance(ref, str) or ref not in self.names:
                raise Error(f"{what}: unknown object {ref!r}")
            key = self.names[ref]
        obj = self.objects.get(key[0])
        e = next((e for e in obj.entries if e.sub == key[1]), None) if obj else None
        if e is None:
            raise Error(f"{what}: no entry 0x{key[0]:04x}:{key[1]}")
        if not (e.tpdo if tx else e.rpdo):
            raise Error(
                f"{what}: {ref!r} cannot be mapped to {'a TPDO' if tx else 'an RPDO'}"
            )
        return e

    def prv_pdos(self, pdos, tx):
        kind = "tpdo" if tx else "rpdo"
        if not isinstance(pdos, list) or len(pdos) > MAX_PDOS:
            raise Error(f"{kind}s: expected a list of up to {MAX_PDOS}")
        comm_base, map_base = (0x1800, 0x1A00) if tx else (0x1400, 0x1600)
        keys = ("cob_id", "enabled", "transmission", "event_timer", "mapping")
        if tx:
            keys += ("inhibit", "sync_start")

        for i, pdo in enumerate(pdos):
            what = f"{kind} {i + 1}"
            prv_check_keys(pdo, keys, what)
            mapping = [
                self.prv_ref(r, tx, f"{what} mapping") for r in pdo.get("mapping", [])
            ]
            if len(mapping) > MAX_MAP:
                raise Error(f"{what}: up to {MAX_MAP} mapped entries")
            if sum(e.size for e in mapping) > 8:
                raise Error(f"{what}: mapping longer than 8 bytes")

            transmission = prv_int(pdo.get("transmission", 254 if tx else 255), what)
            if not (0 <= transmission <= 240 or transmission in (254, 255)):
                raise Error(f"{what}: transmission type 0-240, 254 or 255")

            nodeid = False
            if "cob_id" in pdo:
                cob = prv_int(pdo["cob_id"], f"{what}: cob_id")
            elif i < 4:
                cob = (0x180 if tx else 0x200) + 0x100 * i
                nodeid = True
            else:
                raise Error(f"{what}: cob_id is required past the fourth PDO")
            if not pdo.get("enabled", True):
                cob |= COB_INVALID
            if tx:
                cob |= COB_NO_RTR

            comm = [
                self.prv_sub(
                    1, f"COB-ID used by {kind.upper()}", "u32", "rw", cob, nodeid=nodeid
                ),
                self.prv_sub(2, "Transmission type", "u8", "rw", transmission),
            ]
            if tx:
                comm.append(
                    self.prv_sub(3, "Inhibit time", "u16", "rw", pdo.get("inhibit", 0))
                )
            comm.append(
                self.prv_sub(5, "Event timer", "u16", "rw", pdo.get("event_timer", 0))
            )
            if tx:
                comm.append(
                    self.prv_sub(
                        6, "SYNC start value", "u8", "rw", pdo.get("sync_start", 0)
                    )
                )
            self.prv_comm_record(
                comm_base + i, f"{kind.upper()} communication parameter", comm
            )

            entries = []
            for n in range(MAX_MAP):
                val = 0
                if n < len(mapping):
                    e = mapping[n]
                    val = (e.index << 16) | (e.sub << 8) | (e.size * 8)
                entries.append(
                    self.prv_sub(0, f"Application object {n + 1}", "u32", "rw", val)
                )
            self.prv_comm_array(
                map_base + i, f"{kind.upper()} mapping parameter", "rw", entries
            )
            self.objects[map_base + i].entries[0].default = len(mapping)

        return len(pdos)

    def prv_name(self, name, key, what):
        if not isinstance(name, str) or not IDENT.match(name):
            raise Error(f"{what}: name {name!r} is not a C identifier")
        if name in self.names:
            raise Error(f"{what}: name {name!r} used twice")
        self.names[name] = key

    def prv_objects(self, objects):
        if not isinstance(objects, list):
            raise Error("objects: expected a list")
        for node in objects:
            index = prv_int(
                node.get("index") if isinstance(node, dict) else None, "object index"
            )
            what = f"object 0x{index:04x}"
            if not 0x2000 <= index <= 0xFFFF:
                raise Error(f"{what}: application objects go at 0x2000-0xffff")
            name = node.get("name")
            kind = node.get("type")
            common = ("index", "name", "description")

            if kind == "array":
                prv_check_keys(
                    node,
                    common
                    + ("type", "element", "length", "access", "pdo", "default", "size"),
                    what,
                )
                length = prv_int(node.get("length"), f"{what}: length")
                if not 1 <= length <= 254:
                    raise Error(f"{what}: length 1-254")
                defaults = node.get("default", 0)
                if not isinstance(defaults, list):
                    defaults = [defaults] * length
                if len(defaults) != length:
                    raise Error(f"{what}: {length} defaults expected")
                self.prv_name(name, (index, 0), what)
                obj = Object(index, name, "array")
                obj.entries.append(
                    prv_var(index, "Highest sub-index supported", "u8", "const", length)
                )
                for sub in range(1, length + 1):
                    inherit = {
                        k: node[k] for k in ("access", "pdo", "size") if k in node
                    }
                    inherit["type"] = node.get("element")
                    obj.entries.append(
                        prv_entry(
                            index,
                            sub,
                            f"{name}[{sub}]",
                            {"default": defaults[sub - 1]},
                            f"{what}:{sub}",
                            inherit,
                        )
                    )
                    self.names[f"{name}[{sub}]"] = (index, sub)
            elif kind == "record":
                prv_check_keys(node, common + ("type", "members"), what)
                members = node.get("members")
                if not isinstance(members, list) or not 1 <= len(members) <= 254:
                    raise Error(f"{what}: members must be a list of 1-254 entries")
                self.prv_name(name, (index, 0), what)
                obj = Object(index, name, "record")
                sub = 0
                entries = []
                for m in members:
                    sub = prv_int(m.get("sub", sub + 1), f"{what}: sub")
                    mwhat = f"{what}:{sub}"
                    prv_check_keys(
                        m,
                        (
                            "sub",
                            "name",
                            "type",
                            "access",
                            "pdo",
                            "default",
                            "size",
                            "description",
                        ),
                        mwhat,
                    )
                    if not 1 <= sub <= 254 or any(e.sub == sub for e in entries):
                        raise Error(f"{mwhat}: sub-index 1-254, once")
                    mname = m.get("name")
                    if not isinstance(mname, str) or not IDENT.match(mname):
                        raise Error(f"{mwhat}: name {mname!r} is not a C identifier")
                    self.names[f"{name}.{mname}"] = (index, sub)
                    entries.append(prv_entry(index, sub, mname, m, mwhat))
                entries.sort(key=lambda e: e.sub)
                obj.entries.append(
                    prv_var(
                        index,
                        "Highest sub-index supported",
                        "u8",
                        "const",
                        entries[-1].sub,
                    )
                )
                obj.entries += entries
            else:
                prv_check_keys(
                    node, common + ("type", "access", "pdo", "default", "size"), what
                )
                self.prv_name(name, (index, 0), what)
                obj = Object(
                    index, name, "var", [prv_entry(index, 0, name, node, what)]
                )
            self.prv_add(obj)

    def entries(self):
        for index in sorted(self.objects):
            yield from self.objects[index].entries


def prv_c_value(e):
    if e.type in ("string", "octets"):
        data = e.default.encode() if e.type == "string" else e.default
        return "{" + ", ".join(f"0x{b:02x}" for b in data) + "}"
    if e.type == "f32":
        return f"{float(e.default)!r}f"
    if e.type == "f64":
        return f"{float(e.default)!r}"
    if e.type.startswith("u"):
        return f"0x{e.default:x}U"
    if e.type in ("i64",) and e.default == -(1 << 63):
        return "INT64_MIN"
    return str(e.default)


def prv_c_member(e):
    ctype = TYPES[e.type][1]
    if TYPES[e.type][2] is None:
        return f"{ctype} {e.member}[{e.size}];"
    return f"{ctype} {e.member};"


def prv_attr(e):
    attr = []
    if e.readable:
        attr.append("PBL_CANOPEN_ATTR_READ")
    if e.writable:
        attr.append("PBL_CANOPEN_ATTR_WRITE")
    if e.tpdo:
        attr.append("PBL_CANOPEN_ATTR_TPDO")
    if e.rpdo:
        attr.append("PBL_CANOPEN_ATTR_RPDO")
    if e.nodeid:
        attr.append("PBL_CANOPEN_ATTR_NODEID")
    return " | ".join(attr) or "0"


def write_c(od, src):
    n = od.name
    comm = [e for e in od.entries() if e.index < 0x2000]
    app = [e for e in od.entries() if e.index >= 0x2000]
    buf_size = max([8] + [e.size for e in od.entries()])
    out = [
        "/* Generated from " + src + " by tools/cmake/canopen_od.py, do not edit. */",
        "",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        "#include <pbl/canopen/canopen.h>",
        "",
        f'#include "{n}.h"',
        "",
    ]
    for area, entries in (("comm", comm), ("app", app)):
        out.append(f"struct {n}__{area} {{")
        for e in entries:
            out.append(f"  {prv_c_member(e)}")
        if not entries:
            out.append("  uint8_t unused;")
        out.append("};")
        out.append("")
        out.append(f"static const struct {n}__{area} s_{area}_defaults = {{")
        for e in entries:
            out.append(f"  .{e.member} = {prv_c_value(e)},")
        out.append("};")
        out.append("")
        out.append(f"static struct {n}__{area} s_{area};")
        out.append("")

    out.append(f"static uint8_t s_buf[{buf_size}];")
    for what, count, typ in (
        ("rpdos", od.rpdos, "rpdo"),
        ("tpdos", od.tpdos, "tpdo"),
        ("hb_consumers", od.num_hb_consumers, "hb_consumer"),
    ):
        if count:
            out.append(f"static struct pbl_canopen__{typ} s_{what}[{count}];")
    out.append("")

    out.append("static const struct pbl_canopen_od_entry s_entries[] = {")
    for e in od.entries():
        area = "comm" if e.index < 0x2000 else "app"
        ptr = (
            f"s_{area}.{e.member}"
            if TYPES[e.type][2] is None
            else f"&s_{area}.{e.member}"
        )
        out.append(
            f"  {{0x{e.index:04x}, 0x{e.sub:02x}, PBL_CANOPEN_TYPE_{C_TYPES[TYPES[e.type][0]]}, "
            f"{prv_attr(e)}, {e.size}, {ptr}}},"
        )
    out.append("};")
    out.append("")

    def arr(what, count):
        return f"s_{what}" if count else "nullptr"

    out += [
        f"const struct pbl_canopen_od {n} = {{",
        "  .entries = s_entries,",
        f"  .num_entries = {sum(1 for _ in od.entries())},",
        "  .comm = &s_comm,",
        "  .comm_defaults = &s_comm_defaults,",
        "  .comm_size = sizeof(s_comm),",
        "  .app = &s_app,",
        "  .app_defaults = &s_app_defaults,",
        "  .app_size = sizeof(s_app),",
        "  .buf = s_buf,",
        "  .buf_size = sizeof(s_buf),",
        f"  .rpdos = {arr('rpdos', od.rpdos)},",
        f"  .tpdos = {arr('tpdos', od.tpdos)},",
        f"  .hb_consumers = {arr('hb_consumers', od.num_hb_consumers)},",
        f"  .num_rpdos = {od.rpdos},",
        f"  .num_tpdos = {od.tpdos},",
        f"  .num_hb_consumers = {od.num_hb_consumers},",
        f"  .num_errors = {od.num_errors},",
        "};",
    ]
    return "\n".join(out) + "\n"


def write_h(od, src):
    n = od.name
    prefix = n.upper()
    out = [
        "/* Generated from " + src + " by tools/cmake/canopen_od.py, do not edit. */",
        "",
        "#pragma once",
        "",
        "#include <pbl/canopen/canopen.h>",
        "",
    ]
    for index in sorted(od.objects):
        obj = od.objects[index]
        if index < 0x2000:
            continue
        out.append(f"#define {prefix}_{obj.name.upper()} 0x{index:04x}U")
        if obj.kind == "record":
            for e in obj.entries[1:]:
                out.append(
                    f"#define {prefix}_{obj.name.upper()}_{e.name.upper()} {e.sub}U"
                )
    out += ["", f"extern const struct pbl_canopen_od {n};"]
    return "\n".join(out) + "\n"


def prv_eds_value(e):
    if e.type == "string":
        return e.default
    if e.type == "octets":
        return " ".join(f"{b:02X}" for b in e.default)
    if e.type in ("f32", "f64"):
        return repr(float(e.default))
    if e.type.startswith("u"):
        return f"$NODEID+0x{e.default:X}" if e.nodeid else f"0x{e.default:X}"
    return str(e.default)


def write_eds(od, src):
    dev = od.device
    lines = [
        "[FileInfo]",
        f"FileName={od.name}.eds",
        "FileVersion=1",
        "FileRevision=0",
        "EDSVersion=4.0",
        f"Description=Generated from {src}",
        "CreatedBy=canopen_od.py",
        "",
        "[DeviceInfo]",
        "VendorName=",
        f"VendorNumber=0x{dev.get('vendor_id', 0):X}",
        f"ProductName={dev.get('name', '')}",
        f"ProductNumber=0x{dev.get('product_code', 0):X}",
        f"RevisionNumber=0x{dev.get('revision', 0):X}",
    ]
    for rate in (10, 20, 50, 125, 250, 500, 800, 1000):
        lines.append(f"BaudRate_{rate}=1")
    lines += [
        "SimpleBootUpMaster=0",
        "SimpleBootUpSlave=1",
        "Granularity=8",
        "DynamicChannelsSupported=0",
        "GroupMessaging=0",
        f"NrOfRXPDO={od.rpdos}",
        f"NrOfTXPDO={od.tpdos}",
        "LSS_Supported=0",
        "",
        "[DummyUsage]",
    ]
    for i in range(1, 8):
        lines.append(f"Dummy{i:04X}={1 if od.rpdos else 0}")
    lines.append("")

    mandatory = [i for i in sorted(od.objects) if i in (0x1000, 0x1001, 0x1018)]
    optional = [i for i in sorted(od.objects) if i < 0x2000 and i not in mandatory]
    manufacturer = [i for i in sorted(od.objects) if 0x2000 <= i < 0x6000]
    optional += [i for i in sorted(od.objects) if i >= 0x6000]
    for section, indices in (
        ("MandatoryObjects", mandatory),
        ("OptionalObjects", optional),
        ("ManufacturerObjects", manufacturer),
    ):
        lines.append(f"[{section}]")
        lines.append(f"SupportedObjects={len(indices)}")
        for n, i in enumerate(indices, 1):
            lines.append(f"{n}=0x{i:04X}")
        lines.append("")

    def entry(section, e, name):
        return [
            f"[{section}]",
            f"ParameterName={name}",
            "ObjectType=0x7",
            f"DataType=0x{TYPES[e.type][0]:04X}",
            f"AccessType={e.access}",
            f"DefaultValue={prv_eds_value(e)}",
            f"PDOMapping={1 if e.tpdo or e.rpdo else 0}",
            "",
        ]

    for index in sorted(od.objects):
        obj = od.objects[index]
        if obj.kind == "var":
            lines += entry(f"{index:04X}", obj.entries[0], obj.name)
            continue
        lines += [
            f"[{index:04X}]",
            f"ParameterName={obj.name}",
            f"ObjectType={'0x8' if obj.kind == 'array' else '0x9'}",
            f"SubNumber={len(obj.entries)}",
            "",
        ]
        for e in obj.entries:
            lines += entry(f"{index:04X}sub{e.sub:X}", e, e.name)

    return "\r\n".join(lines) + "\r\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input", help="object dictionary YAML file")
    parser.add_argument("--name", default="od", help="C symbol of the dictionary")
    parser.add_argument("--source", help="file name to cite in the generated files")
    parser.add_argument("--c", dest="c", help="C file to write")
    parser.add_argument("--h", dest="h", help="header to write")
    parser.add_argument("--eds", help="EDS file to write")
    args = parser.parse_args()

    with open(args.input) as f:
        doc = yaml.safe_load(f) or {}

    src = args.source or args.input
    try:
        od = Dictionary(doc, args.name)
    except Error as e:
        sys.exit(f"{args.input}: {e}")

    for path, fn, nl in (
        (args.c, write_c, "\n"),
        (args.h, write_h, "\n"),
        (args.eds, write_eds, ""),
    ):
        if path:
            with open(path, "w", newline=nl) as f:
                f.write(fn(od, src))


if __name__ == "__main__":
    main()
