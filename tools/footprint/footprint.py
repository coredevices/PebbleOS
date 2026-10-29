#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""ROM and RAM footprint reports for the firmware ELF.

Every symbol is attributed to the source file that declares it, as recorded
in the DWARF debug info, and the sizes are rolled up along the source tree.
"""

import argparse
import json
import os
import sys

from elftools.elf.constants import SH_FLAGS
from elftools.elf.elffile import ELFFile

DW_OP_ADDR = 0x03

NO_SOURCE = "(no source)"
NO_SYMBOL = "(no symbol)"
GENERATED = "(generated)"
TOOLCHAIN = "(toolchain)"
EXTERNAL = "(external)"

# Sources outside the checkout are reported from these directories down.
TOOLCHAIN_ROOTS = ("picolibc", "newlib", "libgcc")

HTML_TEMPLATE = os.path.join(os.path.dirname(__file__), "treemap.html")


class Node:
    def __init__(self, name):
        self.name = name
        self.size = 0
        self.children = {}
        self.section = None
        self.address = None

    def add(self, path, size, section=None, address=None):
        node = self
        node.size += size
        for part in path:
            node = node.children.setdefault(part, Node(part))
            node.size += size
        if node.address is None:
            node.section = section
            node.address = address

    def sorted_children(self):
        return sorted(self.children.values(), key=lambda n: (-n.size, n.name))

    def to_dict(self):
        d = {"name": self.name, "size": self.size}
        if self.children:
            d["children"] = [c.to_dict() for c in self.sorted_children()]
        else:
            if self.section is not None:
                d["section"] = self.section
            if self.address is not None:
                d["address"] = self.address
        return d


def section_lma(elf, section):
    for seg in elf.iter_segments():
        if seg["p_type"] != "PT_LOAD" or not seg.section_in_segment(section):
            continue
        return section["sh_addr"] - seg["p_vaddr"] + seg["p_paddr"]
    return section["sh_addr"]


def report_sections(elf, kind):
    """Indices of the allocated sections that occupy ROM or RAM.

    ROM holds everything with contents in the image. RAM holds what is
    writable, zero-initialized, or copied out of the image at startup.
    """
    selected = {}
    for index, section in enumerate(elf.iter_sections()):
        flags = section["sh_flags"]
        if not flags & SH_FLAGS.SHF_ALLOC or section["sh_size"] == 0:
            continue
        nobits = section["sh_type"] == "SHT_NOBITS"
        if kind == "rom":
            wanted = not nobits
        else:
            wanted = (
                nobits
                or flags & SH_FLAGS.SHF_WRITE
                or section_lma(elf, section) != section["sh_addr"]
            )
        if wanted:
            selected[index] = section
    return selected


class SourceIndex:
    """Maps the addresses of functions and variables to their source files."""

    def __init__(self, elf, base):
        self.base = base
        self.addr_size = elf.elfclass // 8
        self.little_endian = elf.little_endian
        self.files = {}
        self.functions = {}
        self.variables = {}
        if elf.has_dwarf_info():
            self._index(elf.get_dwarf_info())

    def _cu_files(self, dwarf, cu):
        files = self.files.get(cu.cu_offset)
        if files is not None:
            return files
        files = []
        lineprog = dwarf.line_program_for_CU(cu)
        if lineprog is not None:
            top = cu.get_top_DIE()
            comp_dir = top.attributes.get("DW_AT_comp_dir")
            comp_dir = comp_dir.value.decode(errors="replace") if comp_dir else "."
            comp_dir = self._resolve(self.base, comp_dir)
            header = lineprog.header
            version = header["version"]
            dirs = [d.decode(errors="replace") for d in header["include_directory"]]
            for entry in header["file_entry"]:
                index = entry.dir_index
                if version >= 5:
                    directory = dirs[index] if index < len(dirs) else ""
                else:
                    directory = dirs[index - 1] if 0 < index <= len(dirs) else ""
                name = entry.name.decode(errors="replace")
                directory = self._resolve(comp_dir, directory)
                files.append(self._resolve(directory, name))
            # Before DWARF 5, file numbers start at 1.
            if version < 5:
                files.insert(0, None)
        self.files[cu.cu_offset] = files
        return files

    def _resolve(self, cwd, path):
        # The build maps the checkout to "." in the debug info.
        if path == "." or path.startswith("./"):
            return os.path.normpath(os.path.join(self.base, path))
        return os.path.normpath(os.path.join(cwd, path))

    def _decl_file(self, dwarf, die):
        for _ in range(4):
            attr = die.attributes.get("DW_AT_decl_file")
            if attr is not None:
                files = self._cu_files(dwarf, die.cu)
                if 0 <= attr.value < len(files):
                    return files[attr.value]
                return None
            for origin in ("DW_AT_specification", "DW_AT_abstract_origin"):
                if origin in die.attributes:
                    die = die.get_DIE_from_attribute(origin)
                    break
            else:
                return None
        return None

    def _static_address(self, die):
        attr = die.attributes.get("DW_AT_location")
        if attr is None or attr.form not in (
            "DW_FORM_exprloc",
            "DW_FORM_block1",
            "DW_FORM_block",
        ):
            return None
        expr = attr.value
        if len(expr) != 1 + self.addr_size or expr[0] != DW_OP_ADDR:
            return None
        order = "little" if self.little_endian else "big"
        return int.from_bytes(bytes(expr[1:]), order)

    def _index(self, dwarf):
        for cu in dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag == "DW_TAG_subprogram":
                    low_pc = die.attributes.get("DW_AT_low_pc")
                    if low_pc is None or low_pc.value in self.functions:
                        continue
                    self.functions[low_pc.value] = self._decl_file(dwarf, die)
                elif die.tag == "DW_TAG_variable":
                    address = self._static_address(die)
                    if address is None or address in self.variables:
                        continue
                    self.variables[address] = self._decl_file(dwarf, die)

    def lookup(self, sym_type, address):
        if sym_type == "STT_FUNC":
            return self.functions.get(address)
        return self.variables.get(address) or self.functions.get(address)


class PathMapper:
    """Turns absolute source paths into tree paths relative to the checkout."""

    def __init__(self, base, build_dir):
        self.base = os.path.realpath(base)
        self.build_dir = os.path.realpath(build_dir)

    @staticmethod
    def _under(path, root):
        return path == root or path.startswith(root + os.sep)

    def __call__(self, path, file_name):
        if path is None:
            # Without debug info, local symbols still carry their file name.
            return [NO_SOURCE] + ([file_name] if file_name else [])
        path = os.path.realpath(path)
        if self._under(path, self.build_dir):
            rel = os.path.relpath(path, self.build_dir)
            return [GENERATED] + rel.split(os.sep)
        if self._under(path, self.base):
            return os.path.relpath(path, self.base).split(os.sep)
        parts = path.lstrip(os.sep).split(os.sep)
        for i in reversed(range(len(parts) - 1)):
            if parts[i] in TOOLCHAIN_ROOTS:
                return [TOOLCHAIN] + parts[i:]
        return [EXTERNAL] + parts


def symbols(elf, sections):
    """The function and object symbols in ``sections``, one per address."""
    symtab = elf.get_section_by_name(".symtab")
    if symtab is None:
        raise SystemExit("error: the ELF has no symbol table")
    thumb = elf["e_machine"] == "EM_ARM"
    found = {}
    file_symbol = None
    for number, sym in enumerate(symtab.iter_symbols()):
        sym_type = sym["st_info"]["type"]
        bind = sym["st_info"]["bind"]
        if sym_type == "STT_FILE":
            # Several objects can share a file name: keep them apart.
            file_symbol = (sym.name, number)
            continue
        if sym_type not in ("STT_FUNC", "STT_OBJECT") or sym["st_size"] == 0:
            continue
        index = sym["st_shndx"]
        if not isinstance(index, int) or index not in sections:
            continue
        address = sym["st_value"]
        if thumb and sym_type == "STT_FUNC":
            address &= ~1
        key = (index, address)
        # Of aliases, keep the global name.
        prev = found.get(key)
        if prev is None or (prev[3] == "STB_LOCAL" and bind != "STB_LOCAL"):
            local_file = file_symbol if bind == "STB_LOCAL" else None
            found[key] = (sym.name, sym_type, sym["st_size"], bind, local_file)
    for (index, address), (name, sym_type, size, _, local_file) in sorted(
        found.items()
    ):
        yield index, address, name, sym_type, size, local_file


class Footprint:
    """An ELF file, indexed once for any number of reports."""

    def __init__(self, f, base):
        self.elf = ELFFile(f)
        self.index = SourceIndex(self.elf, os.path.realpath(base))
        build_dir = os.path.dirname(os.path.abspath(f.name))
        self.mapper = PathMapper(base, build_dir)

    def tree(self, kind):
        sections = report_sections(self.elf, kind)
        rows = []
        attributed = dict.fromkeys(sections, 0)
        ends = {}
        # Where the symbols of each file resolved to, for the locals with no
        # debug info of their own, such as compound literals.
        file_paths = {}
        for shndx, address, name, sym_type, size, local_file in symbols(
            self.elf, sections
        ):
            section = sections[shndx]
            end = min(address + size, section["sh_addr"] + section["sh_size"])
            # Overlapping symbols only count the bytes not already claimed.
            start = max(address, ends.get(shndx, 0))
            size = max(0, end - start)
            ends[shndx] = max(ends.get(shndx, 0), end)
            if size == 0:
                continue
            attributed[shndx] += size
            path = self.index.lookup(sym_type, address)
            if path and local_file and os.path.basename(path) == local_file[0]:
                file_paths[local_file] = path
            rows.append((path, local_file, name, size, section.name, address))

        root = Node("Root")
        for path, local_file, name, size, section, address in rows:
            if path is None and local_file:
                path = file_paths.get(local_file)
            file_name = local_file[0] if local_file else None
            root.add(self.mapper(path, file_name) + [name], size, section, address)

        for shndx, section in sections.items():
            rest = section["sh_size"] - attributed[shndx]
            if rest > 0:
                root.add([NO_SYMBOL, section.name], rest, section.name)

        return root


def print_tree(root, depth, out=sys.stdout):
    total = root.size or 1
    rows = []

    def walk(node, prefix, child_prefix, level):
        rows.append((prefix + node.name, node))
        if depth is not None and level >= depth:
            return
        children = node.sorted_children()
        for i, child in enumerate(children):
            last = i == len(children) - 1
            walk(
                child,
                child_prefix + ("└── " if last else "├── "),
                child_prefix + ("    " if last else "│   "),
                level + 1,
            )

    walk(root, "", "", 0)

    width = max(len(label) for label, _ in rows)
    header = f"{'Path':<{width}} {'Size':>10} {'%':>8}  Section"
    print(header, file=out)
    print("=" * len(header), file=out)
    for label, node in rows:
        section = node.section if not node.children and node.section else ""
        pct = 100.0 * node.size / total
        print(f"{label:<{width}} {node.size:>10} {pct:>7.2f}%  {section}", file=out)
    print("=" * len(header), file=out)
    print(f"{'Total':<{width}} {root.size:>10}", file=out)


def write_json(path, tree):
    with open(path, "w") as f:
        json.dump(tree.to_dict(), f, indent=1)
        f.write("\n")


def write_html(path, trees, title):
    with open(HTML_TEMPLATE) as f:
        template = f.read()
    data = {"title": title, "reports": {k: t.to_dict() for k, t in trees.items()}}
    # Keeps a symbol name from closing the script element.
    payload = json.dumps(data, separators=(",", ":")).replace("</", "<\\/")
    with open(path, "w") as f:
        f.write(template.replace("/*FOOTPRINT_DATA*/null", payload))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, help="Firmware ELF file")
    parser.add_argument(
        "--base",
        default=os.path.join(os.path.dirname(__file__), "..", ".."),
        help="Source tree the paths are reported relative to",
    )
    sub = parser.add_subparsers(dest="report", required=True)
    for kind in ("rom", "ram"):
        p = sub.add_parser(kind, help=f"Print the {kind.upper()} usage by source file")
        p.add_argument(
            "-d", "--depth", type=int, help="Only print this many tree levels"
        )
        p.add_argument("--json", metavar="FILE", help="Also write the tree as JSON")
    p = sub.add_parser(
        "all", help="Write both reports as JSON, and an interactive treemap"
    )
    p.add_argument("--output-dir", required=True, metavar="DIR")
    p.add_argument("--title", default="pebbleos", help="Title of the treemap")
    args = parser.parse_args()

    with open(args.elf, "rb") as f:
        footprint = Footprint(f, args.base)
        if args.report == "all":
            trees = {kind: footprint.tree(kind) for kind in ("rom", "ram")}
            os.makedirs(args.output_dir, exist_ok=True)
            for kind, tree in trees.items():
                write_json(os.path.join(args.output_dir, f"{kind}.json"), tree)
            html = os.path.join(args.output_dir, "footprint.html")
            write_html(html, trees, args.title)
            print(f"Footprint reports written to {args.output_dir}, see {html}")
            return

        tree = footprint.tree(args.report)
        if args.json:
            write_json(args.json, tree)
        try:
            print_tree(tree, args.depth)
        except BrokenPipeError:
            # Piped into head or a pager that quit early.
            sys.stdout = None


if __name__ == "__main__":
    main()
