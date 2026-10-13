# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""ST Unico configuration files (.ucf): register writes that load FSM programs."""

import dataclasses

from . import fsm

FUNC_CFG_ACCESS = 0x01
PAGE_SEL = 0x02
PAGE_ADDRESS = 0x08
PAGE_VALUE = 0x09
FSM_ENABLE_A = 0x46
FSM_ENABLE_B = 0x47
EMB_FUNC_ODR_CFG_B = 0x5F
CTRL1_XL = 0x10

FSM_PROGRAMS = 0x17C
FSM_START_ADD = 0x17E

FSM_ODR_HZ = {0: 12.5, 1: 26, 2: 52, 3: 104}
XL_ODR_HZ = {0: 0, 1: 12.5, 2: 26, 3: 52, 4: 104, 5: 208, 6: 416, 7: 833}
XL_FS_G = {0: 2, 1: 16, 2: 4, 3: 8}


@dataclasses.dataclass
class Config:
    programs: list
    enabled: int
    fsm_odr_hz: float
    xl_odr_hz: float
    xl_fs_g: int


def _replay(text):
    main = {}
    emb = {}
    pages = {}
    emb_access = False
    page = 0
    addr = 0

    for line in text.splitlines():
        words = line.split()
        if len(words) != 3 or words[0] != "Ac":
            continue
        reg, val = int(words[1], 16), int(words[2], 16)
        if reg == FUNC_CFG_ACCESS:
            emb_access = bool(val & 0x80)
            continue
        if not emb_access:
            main[reg] = val
        elif reg == PAGE_SEL:
            page = val >> 4
        elif reg == PAGE_ADDRESS:
            addr = val
        elif reg == PAGE_VALUE:
            pages[(page << 8) | addr] = val
            addr = (addr + 1) & 0xFF
            if addr == 0:
                page += 1
        else:
            emb[reg] = val

    return main, emb, pages


def program_memory(text):
    """Program memory from the FSM start address, as written by a .ucf file."""
    _, _, pages = _replay(text)
    start = pages.get(FSM_START_ADD, 0) | (pages.get(FSM_START_ADD + 1, 0) << 8)
    return bytes(pages.get(a, 0) for a in range(start, 256 * 8))


def parse(text):
    """Replay the register writes of a .ucf file."""
    main, emb, pages = _replay(text)
    count = pages.get(FSM_PROGRAMS, 0)
    start = pages.get(FSM_START_ADD, 0) | (pages.get(FSM_START_ADD + 1, 0) << 8)
    memory = bytearray(256 * 8)
    for a, v in pages.items():
        if a < len(memory):
            memory[a] = v

    programs = []
    offset = start
    for i in range(count):
        program, size = fsm.parse_program(memory, offset)
        program.name = f"FSM{i + 1}"
        programs.append(program)
        offset += size

    ctrl1 = main.get(CTRL1_XL, 0)
    return Config(
        programs=programs,
        enabled=emb.get(FSM_ENABLE_A, 0) | (emb.get(FSM_ENABLE_B, 0) << 8),
        fsm_odr_hz=FSM_ODR_HZ[(emb.get(EMB_FUNC_ODR_CFG_B, 0) >> 3) & 3],
        xl_odr_hz=XL_ODR_HZ.get(ctrl1 >> 4, 0),
        xl_fs_g=XL_FS_G[(ctrl1 >> 2) & 3],
    )


def load(path):
    with open(path) as f:
        return parse(f.read())
