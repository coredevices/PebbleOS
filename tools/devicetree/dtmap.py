#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Turn a board devicetree into C, as told by the *.dtmap.yaml file of each driver.

The tree stays on the host: a node with status okay whose driver is enabled in Kconfig becomes a
call to the instance macro of its driver, in an instance file the driver includes. A spec only says
which classes the driver implements and where each macro argument comes from.

Classes are the software model and live in *.class.yaml files, unknown to the tree and the bindings.
Instance files only include class headers, so drivers do not need to know each other.

A labelled node is reached from other code as dt_<label>, of the type of its class, declared in
devicetree/devices.h.

Roles name the device code uses for a purpose, as ACCEL or CONSOLE_UART. Classes list the roles of
their devices, and roles.yml files of the board and the application give each role a device, which
devicetree/roles.c points the role at.
"""

import argparse
import itertools
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import fdt
from fdt import DtError

SPEC_DIRS = ("drivers", "kernel", "soc")
# Functions of a block that share its registers are sibling nodes at its address, as the I2C and
# SPI of a serial engine, and only one of them may be enabled
DTC_FLAGS = ("-Wno-unique_unit_address", "-Wunique_unit_address_if_enabled")
SPEC_KEYS = {
    "compatible",
    "config",
    "class",
    "symbol",
    "define",
    "args",
    "provides",
    "pinctrl",
    "ids",
    "children",
    "header",
    "drives",
}
CHILDREN_KEYS = {"node", "names"}
CLASS_KEYS = {"class", "type", "include", "provides", "device", "roles"}
ROLE_KEYS = {"symbol", "include"}
PINCTRL_KEYS = {"define", "pin", "flags", "values"}
IDS_KEYS = {"cell", "header", "prefix", "values"}
IDS_VALUES_KEYS = {"header", "prefix"}
ARG_KINDS = {
    "name",
    "reg",
    "prop",
    "strings",
    "irq",
    "clock",
    "assigned-clocks",
    "dma",
    "gpio",
    "pinctrl",
    "parent",
    "self",
    "flag",
    "string",
    "id",
    "size",
    "list",
    "mbox",
    "power-domain",
    "pwm",
    "supply",
    "referrer",
    "phandle",
    "reset",
    "flags",
}
ARG_OPTIONS = {
    "default",
    "as",
    "wrap",
    "field",
    "of",
    "child",
    "index",
    "item",
    "optional",
    "up",
    "remote",
    "all",
    "values",
    "cells",
}


def c_ident(name):
    return "".join(c if c.isalnum() else "_" for c in name)


def cells_number(cells):
    number = 0
    for cell in cells:
        number = (number << 32) | cell
    return number


def as_list(value):
    return value if isinstance(value, list) else [value]


def c_value(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def check_keys(error, data, known, what, required=()):
    if not isinstance(data, dict):
        raise error(f"{what}: {data!r} is not a mapping")
    if data.keys() - known:
        raise error(f"{what}: unknown {sorted(data.keys() - known)}")
    if set(required) - data.keys():
        raise error(f"{what}: missing {sorted(set(required) - data.keys())}")


def header_defines(header, prefix):
    """The numeric defines of a header with a prefix, as {suffix: value}."""
    number = r"\(?\s*(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*\)?"
    pattern = rf"^#define\s+{re.escape(prefix)}(\w+)\s+{number}\s*(?:/[/*].*)?$"
    found = re.findall(pattern, Path(header).read_text(), re.MULTILINE)
    return {
        suffix: int(value, 16 if value[:2] in ("0x", "0X") else 10)
        for suffix, value in found
    }


class Spec:
    def __init__(self, path, data, classes, include_dir=None):
        self.path = path
        check_keys(self._error, data, SPEC_KEYS, "spec", ("compatible", "config"))
        self.compatibles = as_list(data["compatible"])
        self.config = data["config"]
        self.classes = as_list(data.get("class", []))
        self.define = data.get("define")
        self.args = list(data.get("args", []))
        self.provides = data.get("provides", {})
        # Driver headers the provides templates need, as the modes of the parts of a panel driver
        self.includes = as_list(data.get("header", []))
        self.pinctrl = data.get("pinctrl")
        # The compatibles of the child a node must have for the driver to bind to it
        self.drives = as_list(data.get("drives", []))
        self.children = data.get("children")
        if self.children is not None:
            check_keys(
                self._error, self.children, CHILDREN_KEYS, "children", ("names",)
            )
            if not self.define:
                raise self._error("'children' needs 'define'")
        self.sources = []
        for name in self.classes:
            if name not in classes:
                raise self._error(f"unknown class '{name}'")
        self.class_info = {name: classes[name] for name in self.classes}
        symbol = data.get("symbol", {})
        if not isinstance(symbol, dict):
            if len(self.classes) != 1:
                raise self._error("'symbol' of a single name needs exactly one class")
            symbol = {self.classes[0]: symbol}
        if symbol.keys() - set(self.classes):
            raise self._error(
                f"'symbol' of unknown {sorted(symbol.keys() - set(self.classes))}"
            )
        if self.define and not self.classes:
            raise self._error("'define' needs a class")
        if (
            self.define
            and len(self.classes) != 1
            and symbol.keys() != set(self.classes)
        ):
            raise self._error("'define' of several classes needs a 'symbol' for each")
        for name, template in symbol.items():
            if "{sym}" in template and not self.define:
                raise self._error(
                    f"'symbol' of '{name}': '{{sym}}' names what 'define' defines"
                )
        fixed = [name for name, template in symbol.items() if "{sym}" not in template]
        if self.define and fixed and len(self.classes) != 1:
            raise self._error("'define' of several classes names them with '{sym}'")
        self.symbols = symbol
        # A SoC singleton defined from the tree keeps the name C code knows it by
        self.singleton = symbol[self.classes[0]] if self.define and fixed else None
        self.file = c_ident(self.compatibles[0])
        self._check_args(self.args)
        # Arguments a node may lack end a variadic macro, so only they may follow one
        trailing = [
            bool(arg.get("optional")) or "list" in arg or ("prop" in arg and "item" in arg)
            for arg in self.args
        ]
        if any(trailing[i] and not all(trailing[i:]) for i in range(len(trailing))):
            raise self._error("optional and list arguments must come last")
        if self.pinctrl is not None:
            check_keys(
                self._error, self.pinctrl, PINCTRL_KEYS, "pinctrl", ("define", "pin")
            )
            if len(self.classes) != 1:
                raise self._error("'pinctrl' needs exactly one class")
        self.ids = {
            kind: self._load_ids(kind, ids, include_dir)
            for kind, ids in data.get("ids", {}).items()
        }

    def _check_args(self, args):
        for arg in args:
            check_keys(self._error, arg, ARG_KINDS | ARG_OPTIONS, "argument")
            if len(arg.keys() & ARG_KINDS) != 1:
                raise self._error(f"argument {arg}: needs one of {sorted(ARG_KINDS)}")
            if "default" in arg and arg["default"] is None:
                raise self._error(f"argument {arg}: 'default' has no value")
            item = arg.get("item")
            if isinstance(item, dict):
                check_keys(
                    self._error, item, {"define", "args"}, "item", ("define", "args")
                )
                self._check_args(item["args"])

    def _load_ids(self, kind, ids, include_dir):
        """The cell and, for each abstract ID of the binding, the number the driver uses."""
        what = f"ids of '{kind}'"
        check_keys(self._error, ids, IDS_KEYS, what, IDS_KEYS)
        check_keys(self._error, ids["values"], IDS_VALUES_KEYS, what, IDS_VALUES_KEYS)
        if include_dir is None:
            raise self._error(f"{what}: no include directory")
        binding = Path(include_dir) / ids["header"]
        values = Path(include_dir) / ids["values"]["header"]
        for header in (binding, values):
            if not header.exists():
                raise self._error(f"{what}: no {header}")
        self.sources += [str(binding), str(values)]
        abstract = header_defines(binding, ids["prefix"])
        numbers = header_defines(values, ids["values"]["prefix"])
        if not abstract:
            raise self._error(
                f"{what}: nothing with prefix {ids['prefix']} in {binding}"
            )
        if len(set(abstract.values())) != len(abstract):
            raise self._error(f"{what}: an ID is defined twice in {binding}")
        missing = sorted(abstract.keys() - numbers.keys())
        if missing:
            raise self._error(f"{what}: {values} lacks {missing}")
        table = {value: numbers[name] for name, value in abstract.items()}
        return ids["cell"], table

    def _error(self, message):
        return DtError(f"{self.path}: {message}")


def find_files(srcdir, pattern, appdir=None):
    """Specs and classes of PebbleOS, and those of the application's own drivers."""
    dirs = [Path(srcdir) / directory for directory in SPEC_DIRS]
    if appdir is not None and Path(appdir).resolve() not in {d.resolve() for d in dirs}:
        dirs.append(Path(appdir))
    paths = []
    for directory in dirs:
        for path in sorted(directory.rglob(pattern)):
            if path not in paths:
                paths.append(path)
    return paths


def load_class(path, data):
    def error(message):
        return DtError(f"{path}: {message}")

    check_keys(error, data, CLASS_KEYS, "class", ("class", "type", "include"))
    roles = data.get("roles", {})
    if not isinstance(roles, dict):
        raise error("roles: not a mapping of each role to the Kconfig symbol needing it")
    for role, config in roles.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", str(role)):
            raise error(f"role '{role}': not a lowercase C identifier")
        if config is not None and not isinstance(config, str):
            raise error(f"role '{role}': {config!r} is not a Kconfig symbol")
    return data


def load_classes(srcdir, appdir=None):
    classes = {}
    for path in find_files(srcdir, "*.class.yaml", appdir):
        with open(path) as f:
            data = load_class(path, yaml.safe_load(f))
        if data["class"] in classes:
            raise DtError(f"{path}: another file has class '{data['class']}'")
        classes[data["class"]] = data
    return classes


def load_roles(classes):
    """The class of each role, and the Kconfig symbol needing it, as {role: (class, config)}."""
    roles = {}
    for cls in classes.values():
        for role, config in cls.get("roles", {}).items():
            if role in roles:
                other = roles[role][0]["class"]
                raise DtError(f"role '{role}' is in classes {other} and {cls['class']}")
            roles[role] = (cls, config)
    return roles


def load_specs(srcdir, appdir=None):
    classes = load_classes(srcdir, appdir)
    specs = {}
    include_dir = Path(srcdir) / "include"
    for path in find_files(srcdir, "*.dtmap.yaml", appdir):
        with open(path) as f:
            spec = Spec(path, yaml.safe_load(f), classes, include_dir)
        for compatible in spec.compatibles:
            specs.setdefault(compatible, []).append(spec)
    files = {}
    for compatible, found in specs.items():
        names = [name for spec in found for name in spec.classes]
        if len(names) != len(set(names)):
            raise DtError(f"several specs of '{compatible}' implement the same class")
        if len(found) > 1:
            # Drivers sharing a node, each with its own instance file
            for spec in found:
                if not spec.classes:
                    raise DtError(f"{spec.path}: sharing '{compatible}' needs a class")
                first = c_ident(spec.compatibles[0])
                spec.file = f"{first}_{c_ident(spec.classes[0])}"
    for spec in {id(s): s for found in specs.values() for s in found}.values():
        other = files.setdefault(spec.file, spec)
        if other is not spec:
            raise DtError(f"{spec.path}: same instance file as {other.path}")
    return specs


def as_spec_lists(specs):
    return {c: s if isinstance(s, list) else [s] for c, s in specs.items()}


def instance_file(spec):
    return f"devicetree/instances/{spec.file}.inc"


def instance_files(specs):
    found = [spec for group in as_spec_lists(specs).values() for spec in group]
    return sorted({instance_file(s) for s in found if s.define or s.pinctrl})


def device_pointer(cls, symbol):
    """The struct pbl_device of a class object, or None for classes whose objects are not devices."""
    template = cls.get("device", "&{sym}.dev")
    return template.replace("{sym}", symbol) if template else None


def related(a, b):
    """Whether two nodes are the same, or one is inside the other."""

    def inside(node, top):
        while node is not None:
            if node is top:
                return True
            node = node.parent
        return False

    return inside(a, b) or inside(b, a)


def translate(node, address):
    """A reg address as the CPU sees it, through the ranges of the buses above."""
    bus = node.parent
    while bus is not None and bus.parent is not None:
        if not bus.has("ranges"):
            # Not memory mapped, as an I2C bus: addresses are the bus's own
            return address
        entries = bus.u32s("ranges") if bus.props["ranges"] else []
        child_cells = bus.cells_of("#address-cells", 2)
        parent_cells = bus.address_cells
        size_cells = bus.cells_of("#size-cells", 1)
        step = child_cells + parent_cells + size_cells
        for start in range(0, len(entries), step):
            child = cells_number(entries[start : start + child_cells])
            parent = cells_number(
                entries[start + child_cells : start + child_cells + parent_cells]
            )
            size = cells_number(
                entries[start + child_cells + parent_cells : start + step]
            )
            if child <= address < child + size:
                address = parent + address - child
                break
        else:
            if entries:
                raise DtError(
                    f"{node.path}: {address:#x} is outside the ranges of {bus.path}"
                )
        bus = bus.parent
    return address


def enabled(node):
    while node is not None:
        if not node.status_okay:
            return False
        node = node.parent
    return True


class Device:
    def __init__(self, node, specs, owner=None, child_id=None):
        self.node = node
        self.specs = specs
        # A child, as regulators of a PMIC, belongs to the device of its owner
        self.owner = owner
        self.child_id = child_id
        # The first label names the device, the others are aliases of it
        self.label = node.labels[0] if node.labels else None
        self.aliases = node.labels[1:]
        self.name = self.label or c_ident(node.name)
        self.sym = f"dt_{self.label}" if self.label else f"dt_node{c_ident(node.path)}"

    def error(self, message):
        return DtError(f"{self.node.path}: {message}")


class Output:
    def __init__(self):
        self.includes = set()
        self.body = []


class Generator:
    def __init__(self, tree, specs, config):
        self.tree = tree
        self.specs = as_spec_lists(specs)
        self.config = config
        self.outputs = {name: Output() for name in instance_files(specs)}
        self.out = None
        self.declarations = []
        self.aliases = {}
        self.symbols = {}
        # While an instance is generated: its node, and what it needs first
        self.instance = None
        self.deps = []
        self.parents = []
        self.needs = {}
        self.unbound = []
        # Nodes a driver reads as data, as the OTP cells of a flash
        self.consumed = set()
        self.devices = {}
        self.pin_states = {}
        # The class objects of each labelled device, as {label: {class: symbol}}
        self.labelled = {}

    def _device(self, node):
        if node.path not in self.devices:
            found = [self.specs[c] for c in node.compatibles if c in self.specs]
            # The first compatible with an enabled driver, as Kconfig enables drivers
            specs = next(
                (s for s in found if any(spec.config in self.config for spec in s)),
                found[0] if found else [],
            )
            self.devices[node.path] = (
                Device(node, specs) if specs else self._child_device(node)
            ) or Device(node, [])
        return self.devices[node.path]

    def _child_device(self, node):
        """A child that a spec with 'children' instantiates, as a regulator of a PMIC."""
        name = node.name.split("@")[0]
        candidates = [(node.parent, None)]
        if node.parent is not None and node.parent.parent is not None:
            candidates.append((node.parent.parent, node.parent.name))
        for owner, container in candidates:
            if owner is None:
                continue
            for spec in self._device(owner).specs:
                children = spec.children or {}
                if children.get("node") == container and name in children.get(
                    "names", {}
                ):
                    return Device(
                        node,
                        [spec],
                        owner=self._device(owner),
                        child_id=children["names"][name],
                    )
        return None

    def _bound(self, dev):
        """The specs of the node whose driver is enabled, and that drive what is on it."""
        return [
            spec
            for spec in dev.specs
            if spec.config in self.config and self._drives(dev, spec)
        ]

    @staticmethod
    def _drives(dev, spec):
        return not spec.drives or any(
            enabled(child) and set(child.compatibles) & set(spec.drives)
            for child in dev.node.children
        )

    def run(self):
        if self.tree is None:
            return
        for node in self.tree.nodes():
            if (
                node.parent is None
                or not node.compatibles
                or "simple-bus" in node.compatibles
                or PARTITIONS in node.compatibles
            ):
                continue
            if not enabled(node):
                continue
            dev = self._device(node)
            bound = self._bound(dev)
            if not bound:
                self.unbound.append(node.path)
            for spec in bound:
                if spec.children:
                    self._instantiate_children(dev, spec)
                elif spec.define:
                    self._instantiate(dev, spec)
                else:
                    self._declare_device(dev, spec)
        self._check_cycles()

    def _check_cycles(self):
        done, path = set(), []

        def visit(device):
            if device in path:
                cycle = " -> ".join([*path[path.index(device) :], device])
                raise DtError(f"devices need each other: {cycle}")
            if device in done:
                return
            path.append(device)
            for other in self.needs.get(device, ()):
                visit(other)
            path.pop()
            done.add(device)

        for device in self.needs:
            visit(device)

    def _class_symbols(self, dev, spec, sym=None):
        """The symbol of each class object a spec gives a node, by class."""
        sym = sym or dev.sym
        if spec.symbols:
            return {name: t.replace("{sym}", sym) for name, t in spec.symbols.items()}
        if spec.define:
            return {spec.classes[0]: sym}
        return {}

    def _define(self, symbol, node):
        if self.symbols.setdefault(symbol, node.path) != node.path:
            raise DtError(
                f"{node.path}: '{symbol}' is also {self.symbols[symbol]}: label one"
            )

    def _alias(self, alias, symbol, node):
        self._define(alias, node)
        self.aliases[alias] = symbol

    def _declare_device(self, dev, spec):
        symbols = self._class_symbols(dev, spec)
        for name, symbol in symbols.items():
            self._declare(spec.class_info[name], symbol)
        for label in [dev.label, *dev.aliases] if dev.label else []:
            self.labelled.setdefault(label, {}).update(symbols)
        for label in dev.aliases:
            others = self._class_symbols(dev, spec, f"dt_{label}")
            for name, alias in others.items():
                if alias != symbols[name]:
                    self._alias(alias, symbols[name], dev.node)

    def _declare(self, cls, symbol):
        declaration = (cls["include"], cls["type"], symbol)
        if declaration not in self.declarations:
            self.declarations.append(declaration)

    def _instantiate_children(self, dev, spec):
        """One instance per child, matched by name, as the regulators of a PMIC."""
        container = dev.node
        if "node" in spec.children:
            name = spec.children["node"]
            container = next((c for c in dev.node.children if c.name == name), None)
            if container is None:
                return
        names = spec.children["names"]
        for child in container.children:
            if not enabled(child):
                continue
            name = child.name.split("@")[0]
            if name not in names:
                raise DtError(f"{child.path}: not one of {sorted(names)}")
            self._instantiate(self._device(child), spec)

    def _instantiate(self, dev, spec):
        self.out = self.outputs[instance_file(spec)]
        first = dev.sym
        if len(spec.classes) == 1 and spec.symbols:
            first = self._class_symbols(dev, spec)[spec.classes[0]]
        self._define(first, dev.node)
        for symbol in self._class_symbols(dev, spec).values():
            self._define(symbol, dev.node)
        self.instance, self.deps, self.parents = dev.node, [], []
        args = [first, *self._args(dev, spec.args)]
        self.instance = None
        self.out.body.append(f"{spec.define}({', '.join(args)});")
        for name, symbol in self._class_symbols(dev, spec).items():
            own = device_pointer(spec.class_info[name], symbol)
            if own is None:
                continue
            self.needs[own] = [*self.deps, *self.parents]
            if self.deps:
                deps = ", ".join([symbol, own, *self.deps])
                self.out.body.append(f"PBL_DEVICE_DEPENDS({deps});")
        if dev.label or spec.singleton:
            self._declare_device(dev, spec)

    def _args(self, dev, args):
        """The arguments a node has. An empty list or an absent optional argument ends them, as
        those of a variadic macro, so none may follow it."""
        values, ended = [], None
        for arg in args:
            value = self._arg(dev, arg)
            if value is None or ("list" in arg and value == ""):
                ended = ended or arg
            elif ended is not None:
                raise dev.error(f"has what {arg} needs, but not what {ended} needs")
            else:
                values.append(value)
        return values

    def _provider(self, consumer, node, kind):
        """A node others refer to: disabled or driverless ones break the consumer at build time."""
        dev = self._device(node)
        if not enabled(node):
            raise consumer.error(f"'{kind}' from {node.path}, which is disabled")
        if not self._bound(dev):
            raise consumer.error(
                f"'{kind}' from {node.path}, which has no enabled driver"
            )
        return dev

    def _template(self, consumer, dev, kind):
        """How to refer to a provider: template, symbol, class, spec, the class of the device
        and its symbol, which an unlabelled provider has too."""
        for spec in self._bound(dev):
            if kind in spec.provides:
                self.out.includes.update(spec.includes)
                symbol, device = None, None
                if spec.classes:
                    device = self._class_symbols(dev, spec).get(spec.classes[0])
                    if dev.label or not spec.define or spec.singleton:
                        symbol = device
                owner = spec.class_info[spec.classes[0]] if spec.classes else None
                return spec.provides[kind], symbol, None, spec, owner, device
            for name, cls in spec.class_info.items():
                if kind in cls.get("provides", {}):
                    device = self._class_symbols(dev, spec).get(name)
                    symbol = None
                    if dev.label or not spec.define or spec.singleton:
                        symbol = device
                    return cls["provides"][kind], symbol, cls, spec, cls, device
        raise consumer.error(f"{dev.node.path} provides no '{kind}'")

    def _transform(self, consumer, dev, spec, kind, cells):
        """Abstract IDs of the binding, turned into the numbers of the driver."""
        if kind not in spec.ids:
            return cells
        cell, table = spec.ids[kind]
        if cell >= len(cells):
            raise consumer.error(f"'{kind}' from {dev.node.path} lacks cell {cell}")
        if cells[cell] not in table:
            raise consumer.error(f"'{kind}' from {dev.node.path}: no ID {cells[cell]}")
        return [*cells[:cell], table[cells[cell]], *cells[cell + 1 :]]

    def _need(self, node, cls, symbol, parent):
        """Note the device an instance refers to. Its parent, and the devices of its own node, of
        the nodes above and of those inside, come up with it, so they are not dependencies."""
        if self.instance is None or symbol is None or cls is None:
            return
        device = device_pointer(cls, symbol)
        if device is None:
            return
        if parent:
            self.parents.append(device)
        elif not related(self.instance, node) and device not in self.deps:
            self.deps.append(device)
            # An unlabelled provider is declared only for this
            self._declare(cls, symbol)
            self.out.includes.update((cls["include"], "devicetree/devices.h"))

    def _provided(self, consumer, node, kind, cells, parent=False):
        dev = self._provider(consumer, node, kind)
        template, symbol, cls, spec, owner, device = self._template(consumer, dev, kind)
        cells = self._transform(consumer, dev, spec, kind, cells)
        self._need(node, owner, device, parent)
        if cls is not None:
            self.out.includes.add(cls["include"])

        def field(match):
            key = match.group(1)
            if key == "sym":
                if symbol is None:
                    raise consumer.error(
                        f"'{kind}' from {node.path}, which needs a label"
                    )
                self.out.includes.add("devicetree/devices.h")
                return symbol
            if int(key) >= len(cells):
                raise consumer.error(f"'{kind}' from {node.path} lacks cell {key}")
            return str(cells[int(key)])

        return re.sub(r"\{(sym|\d+)\}", field, template)

    def _phandles(self, dev, prop, cells_prop, empty=False):
        """The entries of a phandle list. With empty, a 0 phandle is an entry left empty."""
        values = dev.node.u32s(prop) if dev.node.has(prop) else []
        entries = []
        pos = 0
        while pos < len(values):
            if empty and values[pos] == 0:
                entries.append(None)
                pos += 1
                continue
            target = self.tree.phandle(values[pos], f"{dev.node.path}: '{prop}'")
            count = target.u32(cells_prop)
            cells = values[pos + 1 : pos + 1 + count]
            if len(cells) != count:
                raise dev.error(f"'{prop}': {target.path} takes {count} cells")
            entries.append((target, cells))
            pos += 1 + count
        return entries

    def _index(self, dev, key, names_prop):
        """An entry of a property given by index, or by name from its names property."""
        if not isinstance(key, str):
            return key
        names = dev.node.strings(names_prop) if dev.node.has(names_prop) else []
        if key not in names:
            raise dev.error(f"no '{key}' in '{names_prop}'")
        return names.index(key)

    def _interrupt(self, dev, key, kind):
        index = self._index(dev, key, "interrupt-names")
        if dev.node.has("interrupts-extended"):
            entries = self._phandles(dev, "interrupts-extended", "#interrupt-cells")
            if index >= len(entries):
                raise dev.error(f"no interrupt {index}")
            target, cells = entries[index]
            return self._provided(dev, target, kind, cells)
        node = dev.node
        while node is not None and not node.has("interrupt-parent"):
            node = node.parent
        if node is None:
            raise dev.error("no interrupt parent")
        parent = self.tree.phandle(node.u32("interrupt-parent"), dev.node.path)
        count = parent.u32("#interrupt-cells")
        cells = dev.node.u32s("interrupts")
        entry = cells[index * count : (index + 1) * count]
        if len(entry) != count:
            raise dev.error(f"no interrupt {index}")
        return self._provided(dev, parent, kind, entry)

    def _translate(self, dev, address):
        """A reg address as the CPU sees it, through the ranges of the buses above."""
        return translate(dev.node, address)

    def _reg_entry(self, dev, key):
        index = self._index(dev, key, "reg-names")
        address_cells = dev.node.address_cells
        entry_cells = address_cells + dev.node.size_cells
        cells = dev.node.u32s("reg")
        start = index * entry_cells
        if start + entry_cells > len(cells):
            raise dev.error(f"no reg entry {index}")
        address = cells_number(cells[start : start + address_cells])
        return address, cells_number(cells[start + address_cells : start + entry_cells])

    def _reg(self, dev, key):
        return f"{self._translate(dev, self._reg_entry(dev, key)[0]):#x}"

    def _list(self, dev, node, path, item):
        """An item per enabled child of the node reached through children of the given compatibles:
        a template of the child's reg cells and name, or a macro with arguments read from the child.
        """
        for compatible in path:
            node = next(
                (
                    c
                    for c in node.children
                    if compatible in c.compatibles and enabled(c)
                ),
                None,
            )
            if node is None:
                return None
            self.consumed.add(node.path)
        items = []
        for child in node.children:
            if not enabled(child):
                continue
            if isinstance(item, dict):
                # A macro per child, its arguments read from the child
                child_dev = Device(child, [], owner=dev)
                values = self._args(child_dev, item["args"])
                items.append(f"{item['define']}({', '.join(values)})")
                continue
            cells = child.u32s("reg") if child.has("reg") else []
            try:
                items.append(item.format(*cells, name=child.basename))
            except (KeyError, IndexError) as error:
                raise dev.error(f"{child.path}: no {error} for {item!r}") from None
        return ", ".join(items)

    def _assigned_clocks(self, dev, item):
        """The parents and rates a clock provider assigns, as Linux does with assigned-clocks:
        an item macro of the clock, its parent, {} for none, and its rate in Hz, 0 for none."""
        clocks = self._phandles(dev, "assigned-clocks", "#clock-cells")
        parents = self._phandles(
            dev, "assigned-clock-parents", "#clock-cells", empty=True
        )
        rates = (
            dev.node.u32s("assigned-clock-rates")
            if dev.node.has("assigned-clock-rates")
            else []
        )
        if len(parents) > len(clocks) or len(rates) > len(clocks):
            raise dev.error("more assigned parents or rates than 'assigned-clocks'")
        parents += [None] * (len(clocks) - len(parents))
        rates += [0] * (len(clocks) - len(rates))
        items = []
        for (target, cells), entry, rate in zip(clocks, parents, rates):
            if entry is None and rate == 0:
                raise dev.error(
                    f"assigned clock of {target.path} has no parent nor rate"
                )
            clock = self._provided(dev, target, "clock", cells)
            parent = (
                "{}"
                if entry is None
                else self._provided(dev, entry[0], "clock", entry[1])
            )
            items.append(f"{item}({clock}, {parent}, {rate})")
        return ", ".join(items)

    def _dma(self, dev, src, name):
        names = src.node.strings("dma-names")
        if name not in names:
            raise src.error(f"no '{name}' in 'dma-names'")
        entries = self._phandles(src, "dmas", "#dma-cells")
        if names.index(name) >= len(entries):
            raise src.error(f"no dma '{name}'")
        target, cells = entries[names.index(name)]
        init = self._provided(src, target, "dma", cells)
        sym = f"{dev.sym}_dma_{c_ident(name)}"
        self.out.body.append(f"static const struct pbl_dma_chan {sym} = {init};")
        return f"&{sym}"

    def _pin(self, pinctrl, pin, group):
        spec = pinctrl
        flags = spec.get("flags", {})
        values = spec.get("values", {})
        unknown = set(group.props) - {"pins", "function", "phandle", *flags, *values}
        if unknown:
            raise DtError(f"{group.path}: unsupported {sorted(unknown)}")
        parts = [spec["pin"].format(pin=pin, function=group.string("function").upper())]
        parts += [name for prop, name in flags.items() if group.has(prop)]
        parts += [
            name.format(value=group.u32(prop))
            for prop, name in values.items()
            if group.has(prop)
        ]
        return " | ".join(parts)

    def _pinctrl(self, dev, name):
        names = dev.node.strings("pinctrl-names")
        if name not in names:
            raise dev.error(f"no '{name}' in 'pinctrl-names'")
        prop = f"pinctrl-{names.index(name)}"
        states = [
            self.tree.phandle(value, f"{dev.node.path}: '{prop}'")
            for value in dev.node.u32s(prop)
        ]
        if len(states) != 1:
            raise dev.error(f"'{prop}' must point at one state")
        ctrl = self._provider(dev, states[0].parent, "pinctrl")
        spec = next((s for s in self._bound(ctrl) if s.pinctrl is not None), None)
        if spec is None:
            raise dev.error(f"{ctrl.node.path} is no pin controller")
        sym = self._pin_state(dev, spec, states[0])
        self.out.includes.add("devicetree/devices.h")
        return f"&{sym}"

    def _pin_state(self, consumer, spec, state):
        """The state's symbol. It goes in the pin controller's file, once, if something uses it."""
        if state.path in self.pin_states:
            return self.pin_states[state.path]
        if not state.labels:
            raise consumer.error(f"pin state {state.path} needs a label")
        sym = f"dt_{state.labels[0]}"
        self._define(sym, state)
        for label in state.labels[1:]:
            self._alias(f"dt_{label}", sym, state)
        pins = [
            self._pin(spec.pinctrl, pin, group)
            for group in state.children
            for pin in group.strings("pins")
        ]
        out = self.outputs[instance_file(spec)]
        out.includes.add("devicetree/devices.h")
        out.body.append(f"{spec.pinctrl['define']}({', '.join([sym, *pins])});")
        self._declare(spec.class_info[spec.classes[0]], sym)
        self.pin_states[state.path] = sym
        return sym

    def _remote(self, dev, port):
        """The device at the other end of a port's endpoint, in the graph of Linux."""
        node = dev.node
        ports = next((c for c in node.children if c.name == "ports"), node)
        names = ("port",) if port == 0 else ()
        found = next(
            (c for c in ports.children if c.name in (*names, f"port@{port:x}")), None
        )
        if found is None:
            return None
        endpoint = next(
            (c for c in found.children if c.basename == "endpoint" and enabled(c)), None
        )
        if endpoint is None or not endpoint.has("remote-endpoint"):
            return None
        remote = self.tree.phandle(
            endpoint.u32("remote-endpoint"), f"{endpoint.path}: 'remote-endpoint'"
        )
        target = remote.parent.parent
        if target.name == "ports":
            target = target.parent
        if not enabled(target):
            raise dev.error(
                f"{target.path}, at the other end of port {port}, is disabled"
            )
        return target

    def _referrer(self, dev, prop):
        """The enabled node whose property points at this one, as a GPIO controller's gpio-ranges."""
        phandle = dev.node.u32("phandle") if dev.node.has("phandle") else None
        found = [
            n
            for n in self.tree.nodes()
            if phandle is not None
            and n.has(prop)
            and enabled(n)
            and n.u32s(prop)[0] == phandle
        ]
        if len(found) > 1:
            raise dev.error(f"several nodes refer to it with '{prop}'")
        return found[0] if found else None

    def _value(self, dev, arg):
        """The argument as C, or None when the node does not have it."""
        kind = next(iter(arg.keys() & ARG_KINDS))
        key = arg[kind]
        if arg.get("up"):
            # The function of a block read from the block, as the PWM of a timer, and a child
            # from the device it is part of, as the regulator of a PMIC
            dev = Device(dev.owner.node if dev.owner else dev.node.parent, [])
        node = dev.node
        if "remote" in arg:
            node = self._remote(dev, arg["remote"])
            if node is None:
                return None
        if "child" in arg:
            # A property of the device on this bus, as the clock of the memory a controller drives,
            # or of a child node by name
            child = arg["child"]
            node = next(
                (
                    c
                    for c in node.children
                    if enabled(c)
                    and (
                        c.name == child
                        if isinstance(child, str)
                        else c.unit_address == f"{child:x}"
                    )
                ),
                None,
            )
            if node is None:
                return None
        if "of" in arg:
            if not node.has(arg["of"]):
                return None
            # The first phandle, as all cells of nvmem-cells are in the same device
            node = self.tree.phandle(
                node.u32s(arg["of"])[0], f"{dev.node.path}: '{arg['of']}'"
            )
        if kind == "name":
            return f'"{dev.name}"'
        # The node read, as the memory a device is given with memory-region
        src = dev if node is dev.node else Device(node, [])
        if kind == "reg":
            return self._reg(src, key)
        if kind == "size":
            return f"{self._reg_entry(src, key)[1]:#x}"
        if kind == "list":
            if "item" not in arg:
                raise dev.error(f"{arg}: 'list' needs 'item'")
            return self._list(dev, node, as_list(key), arg["item"])
        if kind == "parent":
            owner = dev.owner.node if dev.owner else node.parent
            return self._provided(dev, owner, key, [], parent=True)
        if kind == "self":
            # A node inside a device, as an nvmem cell of a flash, refers to that device
            while node.parent is not None and not self._device(node).specs:
                node = node.parent
            return self._provided(dev, node, key, [])
        if kind == "id":
            if dev.child_id is None:
                raise dev.error("'id' is for children")
            return str(dev.child_id)
        if kind == "flag":
            return "true" if node.has(key) else "false"
        if kind == "string":
            if not node.has(key):
                return None
            value = node.string(key)
            if "values" not in arg:
                return f'"{value}"'
            # An enumerated string property, as C
            if value not in arg["values"]:
                raise dev.error(
                    f"'{key}' of '{value}', not one of {sorted(arg['values'])}"
                )
            return c_value(arg["values"][value])
        if kind == "irq":
            if not (node.has("interrupts") or node.has("interrupts-extended")):
                return None
            return self._interrupt(src, key, arg.get("as", "irq"))
        if kind == "clock":
            if not node.has("clocks"):
                return None
            # A named input a node may lack, as an optional crystal
            names = node.strings("clock-names") if node.has("clock-names") else []
            if isinstance(key, str) and key not in names and self._may_lack(arg):
                return None
            index = self._index(src, key, "clock-names")
            entries = self._phandles(src, "clocks", "#clock-cells")
            if index >= len(entries):
                raise src.error(f"no clock {index}")
            target, cells = entries[index]
            return self._provided(src, target, "clock", cells)
        if kind == "assigned-clocks":
            return (
                self._assigned_clocks(src, key) if node.has("assigned-clocks") else None
            )
        if kind == "prop":
            if not node.has(key):
                return None
            if "index" in arg:
                cells = node.u32s(key)
                if arg["index"] >= len(cells):
                    raise dev.error(f"no cell {arg['index']} in '{key}'")
                return str(cells[arg["index"]])
            if "item" in arg:
                # A table of rows, as the OCV points of a battery
                cells = node.u32s(key)
                width = arg.get("cells", 1)
                if len(cells) % width:
                    raise dev.error(f"'{key}' is not rows of {width} cells")
                rows = [cells[i : i + width] for i in range(0, len(cells), width)]
                return ", ".join(arg["item"].format(*row) for row in rows)
            return str(node.u32(key))
        if kind == "strings":
            if not node.has(key):
                return None
            item = arg.get("item", "{}")
            return ", ".join(item.format(value) for value in node.strings(key))
        if kind == "dma":
            return self._dma(dev, src, key) if node.has("dmas") else None
        if kind == "flags":
            # Boolean properties as a C mask, as the mode of a SPI device
            present = [token for prop, token in key.items() if node.has(prop)]
            return " | ".join(present) if present else "0U"
        if kind == "reset":
            if not node.has("resets"):
                return None
            index = self._index(src, key, "reset-names")
            entries = self._phandles(src, "resets", "#reset-cells")
            if index >= len(entries):
                raise src.error(f"no reset {index}")
            target, cells = entries[index]
            return self._provided(src, target, "reset", cells)
        if kind == "power-domain":
            if not node.has("power-domains"):
                return None
            index = self._index(src, key, "power-domain-names")
            entries = self._phandles(src, "power-domains", "#power-domain-cells")
            if index >= len(entries):
                raise src.error(f"no power domain {index}")
            target, cells = entries[index]
            return self._provided(src, target, "power-domain", cells)
        if kind == "mbox":
            if not node.has("mboxes"):
                return None
            index = self._index(src, key, "mbox-names")
            entries = self._phandles(src, "mboxes", "#mbox-cells")
            if index >= len(entries):
                raise src.error(f"no mbox {index}")
            target, cells = entries[index]
            return self._provided(src, target, "mbox", cells)
        if kind == "pwm":
            if not node.has("pwms"):
                return None
            index = self._index(src, key, "pwm-names")
            entries = self._phandles(src, "pwms", "#pwm-cells")
            if index >= len(entries):
                raise src.error(f"no pwm {index}")
            target, cells = entries[index]
            return self._provided(src, target, "pwm", cells)
        if kind == "supply":
            prop = f"{key}-supply"
            if not node.has(prop):
                return None
            target = self.tree.phandle(node.u32(prop), f"{node.path}: '{prop}'")
            return self._provided(dev, target, "regulator", [])
        if kind == "phandle":
            if not node.has(key):
                return None
            target = self.tree.phandle(node.u32(key), f"{node.path}: '{key}'")
            return self._provided(dev, target, arg.get("as", key), [])
        if kind == "referrer":
            target = self._referrer(dev, key)
            if target is None:
                return None
            return self._provided(dev, target, arg.get("as", key), [])
        if kind == "gpio":
            prop = f"{key}-gpios" if key else "gpios"
            if not node.has(prop):
                return None
            entries = self._phandles(src, prop, "#gpio-cells")
            if arg.get("all"):
                return ", ".join(
                    self._provided(src, target, "gpio", cells)
                    for target, cells in entries
                )
            index = arg.get("index", 0)
            if index >= len(entries):
                raise src.error(f"no entry {index} in '{prop}'")
            target, cells = entries[index]
            return self._provided(src, target, "gpio", cells)
        if kind == "pinctrl":
            return self._pinctrl(src, key) if node.has("pinctrl-names") else None
        raise AssertionError(kind)

    @staticmethod
    def _may_lack(arg):
        return "default" in arg or bool(arg.get("optional"))

    def _arg(self, dev, arg):
        value = self._value(dev, arg)
        if value is None and arg.get("optional"):
            return None
        if value is None:
            if "default" not in arg:
                raise dev.error(f"missing what {arg} needs")
            value = ", ".join(c_value(item) for item in as_list(arg["default"]))
        if "wrap" in arg:
            value = f"{arg['wrap']}({value})"
        if "field" in arg:
            value = f".{arg['field']} = {value}"
        return value

    def instances(self):
        """The text of each instance file, by name."""
        result = {}
        for name, out in self.outputs.items():
            lines = ["/* Generated from the board devicetree. */", ""]
            lines += [f"#include <{header}>" for header in sorted(out.includes)]
            lines += ["", *out.body]
            result[name] = "\n".join(lines) + "\n"
        return result

    def header(self):
        includes = sorted({include for include, _, _ in self.declarations})
        lines = ["/* Generated from the board devicetree. */", "", "#pragma once", ""]
        lines += [f"#include <{include}>" for include in includes]
        lines.append("")
        lines += [f"extern const {type_} {sym};" for _, type_, sym in self.declarations]
        if self.aliases:
            lines.append("")
            lines += [f"#define {alias} {sym}" for alias, sym in self.aliases.items()]
        unbound = [path for path in self.unbound if path not in self.consumed]
        if unbound:
            lines += ["", "/* Enabled, no driver in this configuration:"]
            lines += [f" * {path}" for path in unbound]
            lines.append(" */")
        return "\n".join(lines) + "\n"


PARTITIONS = "fixed-partitions"
PARTITIONS_HEADER = "devicetree/partitions.h"
REGS_HEADER = "devicetree/regs.h"
REGS_KEYS = {"config", "labels"}
KIB = 1024
MIB = 1024 * KIB


def size_expr(size):
    if size % MIB == 0:
        return f"PBL_MIB({size // MIB})"
    if size % KIB == 0:
        return f"PBL_KIB({size // KIB})"
    return f"{size:#x}"


def partitions_header(tree):
    """The PART_* table of the board's flash, from its fixed-partitions node."""
    lines = ["/* Generated from the board devicetree. */", "", "#pragma once"]
    tables = []
    if tree is not None:
        tables = [n for n in tree.nodes() if PARTITIONS in n.compatibles and enabled(n)]
    if len(tables) > 1:
        raise DtError(f"{tables[1].path}: only one flash may have fixed partitions")
    if tables:
        lines += ["", "#include <pbl/util/units.h>"]
        names = set()
        regions = []
        for part in tables[0].children:
            if not part.status_okay:
                continue
            label = part.string("label") if part.has("label") else part.basename
            name = "PART_" + c_ident(label).upper()
            if name in names:
                raise DtError(f"{part.path}: partition {label} is defined twice")
            names.add(name)
            cells = part.u32s("reg")
            if len(cells) != part.address_cells + part.size_cells:
                raise DtError(f"{part.path}: 'reg' must be one offset and size")
            offset = cells_number(cells[: part.address_cells])
            size = cells_number(cells[part.address_cells :])
            regions.append((offset, offset + size, part.path))
            lines += [
                "",
                f"#define {name}_OFFSET {offset:#09x}",
                f"#define {name}_SIZE   {size_expr(size)}",
                f"#define {name}        PBL_FLASH_PARTITION({name})",
            ]
        regions.sort()
        for (_, end, path), (start, _, other) in itertools.pairwise(regions):
            if start < end:
                raise DtError(f"{other}: overlaps {path}")
    return "\n".join(lines) + "\n"


def load_regs(srcdir, appdir=None):
    """The labels whose registers code reaches before there are devices, from *.regs.yaml."""
    regs = []
    for path in find_files(srcdir, "*.regs.yaml", appdir):
        with open(path) as f:
            data = yaml.safe_load(f)

        def error(message, path=path):
            return DtError(f"{path}: {message}")

        check_keys(error, data, REGS_KEYS, "regs", ("config", "labels"))
        regs.append((data["config"], as_list(data["labels"]), error))
    return regs


def regs_header(tree, regs, config):
    """PBL_DT_REG_* and PBL_DT_SIZE_* of the nodes named in the *.regs.yaml of enabled code."""
    lines = ["/* Generated from the board devicetree. */", "", "#pragma once"]
    nodes = {}
    if tree is not None:
        nodes = {label: node for node in tree.nodes() for label in node.labels}
    defines = {}
    for symbol, labels, error in regs:
        if symbol not in config:
            continue
        for label in labels:
            node = nodes.get(label)
            if node is None:
                raise error(f"no node labelled '{label}'")
            cells = node.u32s("reg") if node.has("reg") else []
            entry = node.address_cells + node.size_cells
            if len(cells) < entry:
                raise error(f"{node.path} has no reg")
            address = translate(node, cells_number(cells[: node.address_cells]))
            size = cells_number(cells[node.address_cells : entry])
            name = c_ident(label).upper()
            defines[f"PBL_DT_REG_{name}"] = f"{address:#x}UL"
            defines[f"PBL_DT_SIZE_{name}"] = f"{size:#x}U"
    if defines:
        lines.append("")
        lines += [f"#define {name} {value}" for name, value in sorted(defines.items())]
    return "\n".join(lines) + "\n"


ROLES_SOURCE = "devicetree/roles.c"
ROLES_FILE = "roles.yml"


def role_files(srcdir, board, revision, appdir=None):
    """The roles files of the board, its revision and the application, in the order they apply."""
    paths = [Path(srcdir) / "boards" / board / ROLES_FILE]
    if revision:
        paths.append(Path(srcdir) / "boards" / board / f"roles.{revision}.yml")
    if appdir is not None:
        paths += [Path(appdir) / ROLES_FILE, Path(appdir) / "boards" / f"{board}.{ROLES_FILE}"]
    return paths


def merge_roles(paths):
    """The device of each role: a later file replaces a role, or removes it with null."""
    roles = {}
    for path in paths:
        if not Path(path).exists():
            continue
        with open(path) as f:
            data = yaml.safe_load(f) or {}
        if not isinstance(data, dict):
            raise DtError(f"{path}: not a mapping of roles to devices")
        for role, device in data.items():
            if device is None:
                roles.pop(role, None)
                continue
            if isinstance(device, dict):
                check_keys(
                    lambda message, path=path: DtError(f"{path}: {message}"),
                    device,
                    ROLE_KEYS,
                    f"role '{role}'",
                    ("symbol",),
                )
            elif not isinstance(device, str):
                raise DtError(f"{path}: role '{role}': {device!r} is not a label or a symbol")
            roles[role] = device
    return roles


def roles_source(gen, roles, chosen, config):
    """devicetree/roles.c: each role pointing at its device.

    A role needed by an enabled Kconfig symbol must have a device with a driver. Other roles are
    left out while their symbol is disabled, or their device has no driver.
    """
    labels = set()
    if gen.tree is not None:
        labels = {label for node in gen.tree.nodes() for label in node.labels}
    includes = {"devicetree/devices.h"}
    lines = []
    for role, device in sorted(chosen.items()):
        if role not in roles:
            raise DtError(f"unknown role '{role}', not one of {sorted(roles)}")
        cls, needed_by = roles[role]
        if needed_by is not None and needed_by not in config:
            continue
        if isinstance(device, dict):
            target = device["symbol"]
            includes.update(as_list(device.get("include", [])))
        else:
            if device not in labels:
                raise DtError(f"role '{role}': no node is labelled '{device}'")
            symbol = gen.labelled.get(device, {}).get(cls["class"])
            if symbol is None:
                if needed_by is None and device not in gen.labelled:
                    continue
                if device in gen.labelled:
                    raise DtError(f"role '{role}': '{device}' is no {cls['class']}")
                raise DtError(
                    f"role '{role}': '{device}' has no driver, and CONFIG_{needed_by} needs it"
                )
            target = f"&{symbol}"
        includes.add(cls["include"])
        lines.append(f"const {cls['type']} *const {role.upper()} = {target};")
    for role, (_, needed_by) in sorted(roles.items()):
        if needed_by in config and role not in chosen:
            raise DtError(f"role '{role}' has no device, and CONFIG_{needed_by} needs it")
    out = ["/* Generated from the device roles of the board. */", ""]
    out += [f"#include <{include}>" for include in sorted(includes)]
    if lines:
        out += ["", *lines]
    return "\n".join(out) + "\n"


def write_if_changed(path, content):
    path = Path(path)
    if path.exists() and path.read_bytes() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


def find_dts(srcdir, board, revision):
    directory = Path(srcdir) / "boards" / board
    names = ([f"{board}_{revision}.dts"] if revision else []) + [f"{board}.dts"]
    return next(
        (directory / name for name in names if (directory / name).exists()), None
    )


def depfile_sources(text):
    """The prerequisites of a make rule, where a backslash escapes a space of a path."""
    sources = text.replace("\\\n", " ").split(":", 1)[1]
    return [s.replace("\\ ", " ") for s in re.findall(r"(?:\\ |\S)+", sources)]


def overlay_files(board, appdir):
    """The devicetree overlays of the application, in the order they apply."""
    if appdir is None:
        return []
    return [Path(appdir) / "app.overlay", Path(appdir) / "boards" / f"{board}.overlay"]


def compile_dts(dts, cc, include_dirs, outdir, overlays=()):
    """Compile to outdir/board.dtb, returning it and the sources it came from.

    Overlays follow the tree, so that they can change its nodes by label.
    """
    dtc = shutil.which("dtc")
    if dtc is None:
        raise DtError(f"{dts} needs dtc: install it")
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    source = Path(dts)
    if overlays:
        source = outdir / "board.overlaid.dts"
        text = "".join(f'#include "{Path(path).resolve()}"\n' for path in [dts, *overlays])
        write_if_changed(source, text.encode())
    pre, depfile, dtb = (
        outdir / "board.dts.pre",
        outdir / "board.dts.d",
        outdir / "board.dtb",
    )
    cpp = [
        cc,
        "-E",
        "-nostdinc",
        "-undef",
        "-D__DTS__",
        "-x",
        "assembler-with-cpp",
        "-P",
    ]
    cpp += ["-MD", "-MF", str(depfile), "-MT", "board.dtb"]
    cpp += [flag for path in include_dirs for flag in ("-I", str(path))]
    result = subprocess.run(
        [*cpp, "-o", str(pre), str(source)], capture_output=True, text=True, check=False
    )
    if result.returncode != 0:
        raise DtError(f"preprocessing {dts} failed:\n{result.stderr}")
    tmp = outdir / "board.dtb.new"
    result = subprocess.run(
        [
            dtc,
            "-@",
            *DTC_FLAGS,
            "-I",
            "dts",
            "-O",
            "dtb",
            "-o",
            str(tmp),
            str(pre),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or result.stderr:
        raise DtError(f"dtc failed on {dts}:\n{result.stderr}")
    write_if_changed(dtb, tmp.read_bytes())
    tmp.unlink()
    sources = depfile_sources(depfile.read_text())
    return dtb, [src for src in sources if not overlays or Path(src) != source]


def kconfig_symbol(compatible):
    return "DT_HAS_" + re.sub(r"[^A-Z0-9]", "_", compatible.upper()) + "_ENABLED"


def kconfig_text(tree, compatibles):
    present = set()
    if tree is not None:
        for node in tree.nodes():
            if enabled(node):
                present.update(node.compatibles)
    lines = ["# Generated from the board devicetree."]
    for compatible in sorted(compatibles):
        lines += [
            "",
            f"config {kconfig_symbol(compatible)}",
            f"\tdef_bool {'y' if compatible in present else 'n'}",
        ]
    return "\n".join(lines) + "\n"


def configure(srcdir, builddir, board, revision, cc, appdir=None):
    """Prepare what Kconfig needs.

    Writes Kconfig.dt. Returns the dtb or None, the files generated from it, and what it all depends on.
    """
    outdir = Path(builddir) / "devicetree"
    specs = load_specs(srcdir, appdir)
    deps = [str(path) for path in find_files(srcdir, "*.dtmap.yaml", appdir)]
    deps += [str(path) for path in find_files(srcdir, "*.class.yaml", appdir)]
    deps += [str(path) for path in find_files(srcdir, "*.regs.yaml", appdir)]
    deps += sorted(
        {src for group in specs.values() for spec in group for src in spec.sources}
    )
    tree, dtb = None, None
    dts = find_dts(srcdir, board, revision)
    overlays = [path for path in overlay_files(board, appdir) if path.exists()]
    if dts is not None:
        root = Path(srcdir)
        dtb, sources = compile_dts(
            dts, cc, [root / "boards", root / "dts", root / "include"], outdir, overlays
        )
        deps += sources
        tree = fdt.load(dtb)
    elif overlays:
        raise DtError(f"{overlays[0]}: board {board} has no devicetree to overlay")
    kconfig = outdir / "Kconfig.dt"
    write_if_changed(kconfig, kconfig_text(tree, [*specs, PARTITIONS]).encode())
    paths = role_files(srcdir, board, revision, appdir)
    deps += [str(path) for path in paths if path.exists()]
    roles = merge_roles(paths)
    write_if_changed(outdir / ROLES_FILE, yaml.safe_dump(roles, sort_keys=True).encode())
    outputs = [PARTITIONS_HEADER, REGS_HEADER, ROLES_SOURCE, *instance_files(specs)]
    return str(dtb) if dtb else None, outputs, deps


def load_config(path):
    config = set()
    with open(path) as f:
        for line in f:
            match = re.match(r"CONFIG_(\w+)=y$", line.strip())
            if match:
                config.add(match.group(1))
    return config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--srcdir", required=True)
    parser.add_argument(
        "--appdir", help="the application, for the specs of its drivers"
    )
    parser.add_argument("--dtb", help="the compiled tree, if the board has one")
    parser.add_argument("--config", required=True, help="the .config file")
    parser.add_argument("--roles", help="the device of each role, merged at configuration")
    parser.add_argument(
        "--outdir", required=True, help="include directory to generate into"
    )
    args = parser.parse_args()

    try:
        tree = fdt.load(args.dtb) if args.dtb else None
        specs = load_specs(args.srcdir, args.appdir)
        config = load_config(args.config)
        generator = Generator(tree, specs, config)
        generator.run()
        partitions = partitions_header(tree)
        regs = regs_header(tree, load_regs(args.srcdir, args.appdir), config)
        chosen = {}
        if args.roles:
            with open(args.roles) as f:
                chosen = yaml.safe_load(f) or {}
        roles = roles_source(
            generator, load_roles(load_classes(args.srcdir, args.appdir)), chosen, config
        )
    except DtError as error:
        sys.exit(f"devicetree: {error}")

    outputs = {
        "devicetree/devices.h": generator.header(),
        PARTITIONS_HEADER: partitions,
        REGS_HEADER: regs,
        ROLES_SOURCE: roles,
        **generator.instances(),
    }
    for name, text in outputs.items():
        write_if_changed(Path(args.outdir) / name, text.encode())
    for path in (Path(args.outdir) / "devicetree" / "instances").glob("*.inc"):
        if f"devicetree/instances/{path.name}" not in outputs:
            path.unlink()


if __name__ == "__main__":
    main()
