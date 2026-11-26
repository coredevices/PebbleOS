# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "libs" / "pbl-cli"))

from pbl.runners import RunnerConfig, RunnerError
from pbl.runners.openocd import RTT_PORT, OpenOcdRunner, rtt_address


def _elf(path, symbols):
    """A minimal 32-bit ELF with a symbol table holding ``symbols``."""
    import struct

    strtab = b"\0"
    syms = [b"\0" * 16]
    for name, value in symbols.items():
        syms.append(struct.pack("<IIIBBH", len(strtab), value, 4, 0x11, 0, 0xFFF1))
        strtab += name.encode() + b"\0"
    symtab = b"".join(syms)
    shstrtab = b"\0.symtab\0.strtab\0.shstrtab\0"

    ehsize, shentsize = 52, 40
    sym_off = ehsize
    str_off = sym_off + len(symtab)
    shstr_off = str_off + len(strtab)
    sh_off = shstr_off + len(shstrtab)
    sections = [
        struct.pack("<10I", *[0] * 10),
        struct.pack("<10I", 1, 2, 0, 0, sym_off, len(symtab), 2, 1, 4, 16),
        struct.pack("<10I", 9, 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0),
        struct.pack("<10I", 17, 3, 0, 0, shstr_off, len(shstrtab), 0, 0, 1, 0),
    ]
    header = (
        b"\x7fELF\x01\x01\x01"
        + b"\0" * 9
        + struct.pack(
            "<HHIIIIIHHHHHH", 2, 40, 1, 0, 0, sh_off, 0, ehsize, 0, 0, shentsize, 4, 3
        )
    )
    path.write_bytes(header + symtab + strtab + shstrtab + b"".join(sections))
    return str(path)


def test_rtt_address_is_read_from_the_elf(tmp_path):
    elf = _elf(tmp_path / "fw.elf", {"main": 0x100, "_SEGGER_RTT": 0x20000040})
    assert rtt_address(elf) == 0x20000040


def test_rtt_address_needs_the_control_block(tmp_path):
    elf = _elf(tmp_path / "fw.elf", {"main": 0x100})
    with pytest.raises(RunnerError, match="no _SEGGER_RTT"):
        rtt_address(elf)


def _runner(tmp_path, config):
    build = SimpleNamespace(topdir=str(tmp_path), config=config)
    cfg = RunnerConfig(
        board_dir=str(tmp_path / "boards" / "disco"),
        elf_file=_elf(tmp_path / "fw.elf", {"_SEGGER_RTT": 0x24000000}),
        build=build,
    )
    return OpenOcdRunner(cfg)


def test_server_serves_rtt_for_rtt_builds(tmp_path):
    argv = _runner(tmp_path, {"CONFIG_RTT": True})._server(3333)
    assert argv[argv.index("-c", 4) :] == [
        "-c", "set RTT_ADDR 0x24000000",
        "-c", f"set RTT_PORT {RTT_PORT}",
        "-f", str(tmp_path / "tools" / "openocd" / "rtt.cfg"),
    ]  # fmt: skip


def test_server_leaves_rtt_out_otherwise(tmp_path):
    argv = _runner(tmp_path, {})._server(3333)
    assert "rtt.cfg" not in " ".join(argv)
