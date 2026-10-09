# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""LSM6DSO finite state machine: program format, assembler and simulator.

Follows ST AN5226 (LSM6DSO: Finite State Machine). Features that need inputs
other than the accelerometer (gyroscope, angles, magnetometer), the shared long
counter and self-modifying programs (SETP) are not simulated.

Where AN5226 leaves room for interpretation:
- The temporary mask narrows to the axes that met a condition when it holds,
  and is left alone when it does not.
- The time counter only runs in states that have a timer condition.
"""

import dataclasses
import math
import re
import struct

CONFIG_B_DES = 0x80
CONFIG_B_HYST = 0x40
CONFIG_B_ANGLE = 0x20
CONFIG_B_PAS = 0x10
CONFIG_B_STOPDONE = 0x04
CONFIG_B_LC = 0x02
CONFIG_B_JMP = 0x01

# Condition nibbles
NOP, TI1, TI2, TI3, TI4 = 0x0, 0x1, 0x2, 0x3, 0x4
GNTH1, GNTH2, LNTH1, LNTH2 = 0x5, 0x6, 0x7, 0x8
GLTH1, LLTH1, GRTH1, LRTH1 = 0x9, 0xA, 0xB, 0xC
PZC, NZC = 0xD, 0xE
TIMERS = (TI1, TI2, TI3, TI4)

CONDITIONS = {
    NOP: "NOP",
    TI1: "TI1",
    TI2: "TI2",
    TI3: "TI3",
    TI4: "TI4",
    GNTH1: "GNTH1",
    GNTH2: "GNTH2",
    LNTH1: "LNTH1",
    LNTH2: "LNTH2",
    GLTH1: "GLTH1",
    LLTH1: "LLTH1",
    GRTH1: "GRTH1",
    LRTH1: "LRTH1",
    PZC: "PZC",
    NZC: "NZC",
}
CONDITION_CODES = {name: code for code, name in CONDITIONS.items()}

# Commands: opcode -> (mnemonic, parameter bytes)
COMMANDS = {
    0x00: ("STOP", 0),
    0x11: ("CONT", 0),
    0x22: ("CONTREL", 0),
    0x33: ("SRP", 0),
    0x44: ("CRP", 0),
    0x55: ("SETP", 2),
    0x66: ("SELMA", 0),
    0x77: ("SELMB", 0),
    0x88: ("SELMC", 0),
    0x99: ("OUTC", 0),
    0xAA: ("STHR1", 2),
    0xBB: ("STHR2", 2),
    0xCC: ("SELTHR1", 0),
    0xDD: ("SELTHR3", 0),
    0xEE: ("SISW", 0),
    0xFF: ("REL", 0),
    0x12: ("SSIGN0", 0),
    0x13: ("SSIGN1", 0),
    0x14: ("SRTAM0", 0),
    0x21: ("SRTAM1", 0),
    0x23: ("SINMUX", 1),
    0x24: ("STIMER3", 1),
    0x31: ("STIMER4", 1),
    0x32: ("SWAPMSK", 0),
    0x34: ("INCR", 0),
    0x41: ("JMP", 3),
    0x42: ("CANGLE", 0),
    0x43: ("SMA", 1),
    0xDF: ("SMB", 1),
    0xFE: ("SMC", 1),
    0x5B: ("SCTC0", 0),
    0x7C: ("SCTC1", 0),
    0xC7: ("UMSKIT", 0),
    0xEF: ("MSKITEQ", 0),
    0xF5: ("MSKIT", 0),
}
MNEMONICS = {name: op for op, (name, _) in COMMANDS.items()}
JMP = MNEMONICS["JMP"]

# Commands and conditions that need the PAS byte
NEEDS_PAS = {"SCTC0", "SCTC1", "CANGLE", "MSKIT", "MSKITEQ", "UMSKIT"}
UNSIMULATED = {"SETP", "INCR", "CANGLE"}

# Mask bits: +X -X +Y -Y +Z -Z +V -V
MASK_BITS = ["+X", "-X", "+Y", "-Y", "+Z", "-Z", "+V", "-V"]


class FsmError(Exception):
    pass


def half_to_float(raw):
    return struct.unpack("<e", struct.pack("<H", raw))[0]


def float_to_half(value):
    return struct.unpack("<H", struct.pack("<e", value))[0]


def mask_str(mask):
    return " ".join(b for i, b in enumerate(MASK_BITS) if mask & (0x80 >> i)) or "0"


def parse_mask(text):
    mask = 0
    for tok in text.replace(",", " ").split():
        if tok == "0":
            continue
        try:
            mask |= 0x80 >> MASK_BITS.index(tok.upper())
        except ValueError:
            raise FsmError(f"bad mask axis {tok!r}") from None
    return mask


@dataclasses.dataclass
class Program:
    thresholds: list
    masks: list
    timers: dict
    code: bytes
    hysteresis: float = None
    decimation: int = None
    pas: bool = False
    angle: bool = False
    long_counter: bool = False
    name: str = ""
    # Axes the masks refer to: "sensor", or "watch" (remapped per board on load)
    frame: str = "sensor"

    def _long_timers(self):
        return sorted(t for t in self.timers if t in (1, 2))

    def _short_timers(self):
        return sorted(t for t in self.timers if t in (3, 4))

    def _data(self):
        body = bytearray()
        for t in self.thresholds:
            body += struct.pack("<H", float_to_half(t))
        if self.hysteresis is not None:
            body += struct.pack("<H", float_to_half(self.hysteresis))
        for m in self.masks:
            body += bytes([m, 0])
        if self.angle:
            body += bytes(10)
        if self.timers:
            body += bytes(2 if self._long_timers() else 1)
        for t in self._long_timers():
            body += struct.pack("<H", self.timers[t])
        for t in self._short_timers():
            body += bytes([self.timers[t]])
        if self.decimation is not None:
            body += bytes([self.decimation, 0])
        if self.pas:
            body += bytes([0])
        return bytes(body)

    @property
    def code_offset(self):
        return 6 + len(self._data())

    def config(self):
        if len(self.thresholds) > 3 or len(self.masks) > 3:
            raise FsmError("at most 3 thresholds and 3 masks")
        if self._long_timers() not in ([], [1], [1, 2]):
            raise FsmError("long timers must be allocated in order (TI1, TI2)")
        if self._short_timers() not in ([], [3], [3, 4]):
            raise FsmError("short timers must be allocated in order (TI3, TI4)")
        config_a = (
            (len(self.thresholds) << 6)
            | (len(self.masks) << 4)
            | (len(self._long_timers()) << 2)
            | len(self._short_timers())
        )
        config_b = (
            (CONFIG_B_DES if self.decimation is not None else 0)
            | (CONFIG_B_HYST if self.hysteresis is not None else 0)
            | (CONFIG_B_ANGLE if self.angle else 0)
            | (CONFIG_B_PAS if self.pas else 0)
            | (CONFIG_B_LC if self.long_counter else 0)
        )
        return config_a, config_b

    def to_bytes(self):
        """The program image to load, with RP and PP cleared."""
        config_a, config_b = self.config()
        code = self.code
        # SIZE must be even: pad with STOP
        if (6 + len(self._data()) + len(code)) % 2:
            code += b"\x00"
        size = 6 + len(self._data()) + len(code)
        if size > 255:
            raise FsmError("program too large")
        return bytes([config_a, config_b, size, 0, 0, 0]) + self._data() + code


def parse_program(data, offset=0):
    """Parse one program starting at data[offset]; returns (program, size)."""
    config_a, config_b, size = data[offset : offset + 3]
    n_thresh = (config_a >> 6) & 3
    n_mask = (config_a >> 4) & 3
    n_long = (config_a >> 2) & 3
    n_short = config_a & 3
    if n_long > 2 or n_short > 2:
        raise FsmError(f"bad CONFIG_A 0x{config_a:02x}")

    p = offset + 6

    def half():
        nonlocal p
        value = half_to_float(struct.unpack_from("<H", data, p)[0])
        p += 2
        return value

    thresholds = [half() for _ in range(n_thresh)]
    hysteresis = half() if config_b & CONFIG_B_HYST else None
    masks = []
    for _ in range(n_mask):
        masks.append(data[p])
        p += 2
    angle = bool(config_b & CONFIG_B_ANGLE)
    if angle:
        p += 10
    if n_long or n_short:
        p += 2 if n_long else 1
    timers = {}
    for i in range(n_long):
        timers[1 + i] = struct.unpack_from("<H", data, p)[0]
        p += 2
    for i in range(n_short):
        timers[3 + i] = data[p]
        p += 1
    decimation = None
    if config_b & CONFIG_B_DES:
        decimation = data[p]
        p += 2
    pas = bool(config_b & CONFIG_B_PAS)
    if pas:
        p += 1

    code = bytes(data[p : offset + size])
    program = Program(
        thresholds=thresholds,
        masks=masks,
        timers=timers,
        code=code,
        hysteresis=hysteresis,
        decimation=decimation,
        pas=pas,
        angle=angle,
        long_counter=bool(config_b & CONFIG_B_LC),
    )
    if program.code_offset != p - offset:
        raise FsmError("inconsistent program layout")
    return program, size


def iter_instructions(program):
    """Yield (address, opcode, params), addresses relative to CONFIG_A."""
    code = program.code
    base = program.code_offset
    i = 0
    while i < len(code):
        op = code[i]
        nparams = COMMANDS[op][1] if op in COMMANDS else 0
        yield base + i, op, bytes(code[i + 1 : i + 1 + nparams])
        i += 1 + nparams


def _cond_pair(byte):
    return f"{CONDITIONS[byte >> 4]}|{CONDITIONS[byte & 0xF]}"


def disassemble(program):
    lines = []
    for i, t in enumerate(program.thresholds):
        lines.append(f"thresh {i + 1} {t:+.4f}")
    if program.hysteresis is not None:
        lines.append(f"hyst {program.hysteresis:.4f}")
    for i, m in enumerate(program.masks):
        lines.append(f"mask {'ABC'[i]} {mask_str(m)}")
    for t, v in sorted(program.timers.items()):
        lines.append(f"timer TI{t} {v}")
    if program.decimation is not None:
        lines.append(f"decimation {program.decimation}")
    if program.pas:
        lines.append("pas")
    if program.angle:
        lines.append("angle")
    if program.long_counter:
        lines.append("long_counter")
    lines.append("code:")
    for addr, op, params in iter_instructions(program):
        if op == JMP:
            text = f"JMP {_cond_pair(params[0])} {params[1]} {params[2]}"
        elif op in (MNEMONICS["STHR1"], MNEMONICS["STHR2"]):
            value = half_to_float(params[0] | (params[1] << 8))
            text = f"{COMMANDS[op][0]} {value:+.4f}"
        elif op in (MNEMONICS["SMA"], MNEMONICS["SMB"], MNEMONICS["SMC"]):
            text = f"{COMMANDS[op][0]} {mask_str(params[0])}"
        elif op in COMMANDS:
            text = " ".join([COMMANDS[op][0]] + [str(b) for b in params])
        else:
            text = _cond_pair(op)
        lines.append(f"  {addr:3d}: {text}")
    return "\n".join(lines)


def _timer_samples(value, odr):
    if value.endswith("ms"):
        if odr is None:
            raise FsmError(
                f"timer {value} needs an FSM rate (odr directive or argument)"
            )
        return max(1, round(float(value[:-2]) * odr / 1000))
    return int(value)


def assemble(text, name="", odr=None, overrides=None):
    """Assemble a program from the textual form disassemble() produces.

    Labels ("name:") can be used as JMP targets. The PAS byte is allocated
    automatically when the code needs it. Timers can be given in ms ("190ms"),
    converted at the FSM rate: odr if given, else the program's odr directive.
    overrides replaces directive values, e.g. {"thresh2": "0.4", "TI3": "150ms"}.
    """
    thresholds = {}
    masks = {}
    timers = {}
    program = Program(thresholds=[], masks=[], timers={}, code=b"", name=name)
    program_odr = None
    code_lines = []
    in_code = False
    for raw_line in text.splitlines():
        line = raw_line.split(";", 1)[0].split("#", 1)[0].strip()
        if not line:
            continue
        line = re.sub(r"^\d+:\s*", "", line)
        if in_code:
            code_lines.append(line)
            continue
        words = line.split()
        key = words[0].lower()
        if key == "code:":
            in_code = True
        elif key == "thresh":
            thresholds[int(words[1])] = words[2]
        elif key == "hyst":
            program.hysteresis = float(words[1])
        elif key == "mask":
            masks["ABC".index(words[1].upper())] = parse_mask(" ".join(words[2:]))
        elif key == "timer":
            timers[int(words[1].upper().removeprefix("TI"))] = words[2]
        elif key == "decimation":
            program.decimation = int(words[1])
        elif key == "pas":
            program.pas = True
        elif key == "angle":
            program.angle = True
        elif key == "long_counter":
            program.long_counter = True
        elif key == "frame":
            if words[1] not in ("sensor", "watch"):
                raise FsmError(f"unknown frame {words[1]!r}")
            program.frame = words[1]
        elif key == "odr":
            program_odr = float(words[1])
        else:
            raise FsmError(f"unknown directive {line!r}")

    for key, value in (overrides or {}).items():
        k = key.lower()
        if k.startswith("thresh") and int(k[6:]) in thresholds:
            thresholds[int(k[6:])] = value
        elif k.startswith("ti") and int(k[2:]) in timers:
            timers[int(k[2:])] = value
        elif k == "hyst" and program.hysteresis is not None:
            program.hysteresis = float(value)
        else:
            raise FsmError(f"cannot override {key!r}")

    rate = odr if odr is not None else program_odr
    program.timers = {t: _timer_samples(v, rate) for t, v in timers.items()}
    thresholds = {i: float(v) for i, v in thresholds.items()}
    if sorted(thresholds) != list(range(1, len(thresholds) + 1)):
        raise FsmError("thresholds must be numbered from 1")
    if sorted(masks) != list(range(len(masks))):
        raise FsmError("masks must be allocated from A")
    program.thresholds = [thresholds[i] for i in sorted(thresholds)]
    program.masks = [masks[i] for i in sorted(masks)]

    def cond_pair(word):
        try:
            reset, nxt = word.upper().split("|")
            return (CONDITION_CODES[reset] << 4) | CONDITION_CODES[nxt]
        except (KeyError, ValueError):
            raise FsmError(f"bad condition pair {word!r}") from None

    def encode(labels):
        out = bytearray()
        for line in code_lines:
            if line.endswith(":"):
                labels[line[:-1]] = program.code_offset + len(out)
                continue
            words = line.split()
            op = words[0].upper()
            if op == "JMP":
                targets = [
                    int(w) if w.isdigit() else labels.get(w, 0) for w in words[2:4]
                ]
                out += bytes([JMP, cond_pair(words[1])] + targets)
            elif op in ("STHR1", "STHR2"):
                value = float_to_half(float(words[1]))
                out += bytes([MNEMONICS[op], value & 0xFF, value >> 8])
            elif op in ("SMA", "SMB", "SMC"):
                out += bytes([MNEMONICS[op], parse_mask(" ".join(words[1:]))])
            elif op in MNEMONICS:
                params = [int(w, 0) for w in words[1:]]
                if len(params) != COMMANDS[MNEMONICS[op]][1]:
                    raise FsmError(
                        f"{op} takes {COMMANDS[MNEMONICS[op]][1]} parameters"
                    )
                out += bytes([MNEMONICS[op]] + params)
            elif "|" in op:
                out.append(cond_pair(op))
            else:
                raise FsmError(f"unknown instruction {line!r}")
        return bytes(out)

    def needs_pas(code):
        for _, op, params in iter_instructions(dataclasses.replace(program, code=code)):
            if op in COMMANDS and COMMANDS[op][0] in NEEDS_PAS:
                return True
            conds = [params[0]] if op == JMP else ([] if op in COMMANDS else [op])
            for c in conds:
                if {c >> 4, c & 0xF} & {PZC, NZC}:
                    return True
        return False

    labels = {}
    program.code = encode(labels)
    if not program.pas and needs_pas(program.code):
        program.pas = True
    # Twice, so that forward labels see the final layout
    program.code = encode(labels)
    program.code = encode(labels)
    return program


@dataclasses.dataclass
class Event:
    sample: int
    outs: int


class Simulator:
    """Runs one program on accelerometer samples in g, in the sensor frame."""

    def __init__(self, program):
        self.program = program
        self.code = {
            addr: (op, params) for addr, op, params in iter_instructions(program)
        }
        self.start = program.code_offset
        if program.angle:
            raise FsmError("angle computation needs gyroscope data")
        for op, params in self.code.values():
            self._check(op, params)
        self.reset_state()

    def _check(self, op, params):
        if op in COMMANDS:
            name = COMMANDS[op][0]
            if name in UNSIMULATED:
                raise FsmError(f"{name} is not simulated")
            if name == "SINMUX" and params != b"\x00":
                raise FsmError("only the accelerometer input is simulated")
            if name in NEEDS_PAS and not self.program.pas:
                raise FsmError(f"{name} needs the PAS byte")
            if op == JMP:
                for cond in (params[0] >> 4, params[0] & 0xF):
                    self._check_condition(cond)
        else:
            for cond in (op >> 4, op & 0xF):
                self._check_condition(cond)

    def _check_condition(self, cond):
        if cond not in CONDITIONS:
            raise FsmError(f"invalid condition 0x{cond:x}")
        if cond in (PZC, NZC) and not self.program.pas:
            raise FsmError("zero-crossing conditions need the PAS byte")
        if cond in TIMERS and cond not in self.program.timers:
            raise FsmError(f"{CONDITIONS[cond]} is not allocated")
        needed = {GNTH2: 2, LNTH2: 2}.get(cond, 1)
        if GNTH1 <= cond <= LRTH1 and (
            len(self.program.thresholds) < needed or not self.program.masks
        ):
            raise FsmError(f"{CONDITIONS[cond]} needs thresholds and a mask")

    def reset_state(self):
        p = self.program
        self.thresholds = list(p.thresholds)
        self.timers = dict(p.timers)
        self.masks = list(p.masks)
        self.tmasks = list(p.masks)
        self.mask_sel = 0
        self.signed = True
        self.r_tam = False
        self.thrs3sel = False
        self.sctc1 = False
        self.it_mask = "UMSKIT"
        self.outs = 0
        self.rp = self.start
        self.tc = None
        self.tc_timer = None
        self._goto(self.start)
        self.prev = None
        self.desc = p.decimation
        self.stopped = False
        self.events = []
        self.index = 0
        self._run_commands()

    # Flow

    def _timer_of(self, addr):
        op, params = self.code.get(addr, (0x00, b""))
        if op == JMP:
            conds = [params[0] >> 4, params[0] & 0xF]
        elif op in COMMANDS:
            return None
        else:
            conds = [op >> 4, op & 0xF]
        timers = [c for c in conds if c in TIMERS]
        return timers[0] if timers else None

    def _goto(self, addr, preload=True):
        self.pp = addr
        self.preload = preload

    def _enter(self):
        timer = self._timer_of(self.pp)
        if timer is None:
            self.tc = None
        elif (
            self.preload or not self.sctc1 or timer != self.tc_timer or self.tc is None
        ):
            self.tc = self.timers[timer]
        self.tc_timer = timer

    def _output(self):
        new = self.tmasks[self.mask_sel] if self.tmasks else 0
        changed = new != self.outs
        self.outs = new
        if self.it_mask == "MSKIT" or (self.it_mask == "MSKITEQ" and not changed):
            return
        self.events.append(Event(self.index, new))

    def _release(self, index=None):
        for i in range(len(self.masks)) if index is None else [index]:
            self.tmasks[i] = self.masks[i]

    def _run_commands(self):
        while not self.stopped:
            if self.pp not in self.code:
                raise FsmError(f"program pointer {self.pp} out of the code")
            op, params = self.code[self.pp]
            if op not in COMMANDS or op == JMP:
                self._enter()
                return
            name = COMMANDS[op][0]
            nxt = self.pp + 1 + len(params)
            if name == "STOP":
                self._output()
                self.stopped = True
                return
            if name in ("CONT", "CONTREL"):
                if name == "CONTREL":
                    self._release(self.mask_sel)
                self._output()
                self._goto(self.rp)
                continue
            if name == "SRP":
                self.rp = nxt
            elif name == "CRP":
                self.rp = self.start
            elif name in ("SELMA", "SELMB", "SELMC"):
                self.mask_sel = "ABC".index(name[-1])
                if self.mask_sel >= len(self.masks):
                    raise FsmError(f"{name} selects an unallocated mask")
            elif name == "OUTC":
                self._output()
            elif name in ("STHR1", "STHR2"):
                self.thresholds[int(name[-1]) - 1] = half_to_float(
                    params[0] | (params[1] << 8)
                )
            elif name == "SELTHR1":
                self.thrs3sel = False
            elif name == "SELTHR3":
                self.thrs3sel = True
            elif name == "SISW":
                t = self.tmasks[self.mask_sel]
                self.tmasks[self.mask_sel] = ((t & 0xAA) >> 1) | ((t & 0x55) << 1)
            elif name == "REL":
                self._release(self.mask_sel)
            elif name in ("SSIGN0", "SSIGN1"):
                self.signed = name == "SSIGN1"
            elif name in ("SRTAM0", "SRTAM1"):
                self.r_tam = name == "SRTAM1"
            elif name in ("STIMER3", "STIMER4"):
                self.timers[int(name[-1])] = params[0]
            elif name == "SWAPMSK":
                self.masks[0], self.masks[1] = self.masks[1], self.masks[0]
                self.tmasks[0], self.tmasks[1] = self.tmasks[1], self.tmasks[0]
            elif name in ("SMA", "SMB", "SMC"):
                i = "ABC".index(name[-1])
                self.masks[i] = self.tmasks[i] = params[0]
            elif name in ("SCTC0", "SCTC1"):
                self.sctc1 = name == "SCTC1"
            elif name in ("UMSKIT", "MSKITEQ", "MSKIT"):
                self.it_mask = name
            self.pp = nxt

    # Conditions

    def _threshold(self, cond):
        if cond in (GNTH2, LNTH2):
            th = self.thresholds[1]
        else:
            th = self.thresholds[2] if self.thrs3sel else self.thresholds[0]
        hyst = self.program.hysteresis or 0.0
        greater = cond in (GNTH1, GNTH2, GLTH1, GRTH1)
        th = th + hyst if greater else th - hyst
        if cond in (GRTH1, LRTH1):
            th = -th
        return th, greater

    def _eval(self, cond, values):
        """Return the axes (mask bits) meeting cond, 0 if it does not hold."""
        if cond == NOP:
            return 0
        if cond in TIMERS:
            return -1 if self.tc is not None and self.tc <= 0 else 0
        tmask = self.tmasks[self.mask_sel]
        axes = [i for i in range(8) if tmask & (0x80 >> i)]
        hit = 0
        if cond in (PZC, NZC):
            if self.prev is None:
                return 0
            for i in axes:
                was, now = self.prev[i] >= 0, values[i] >= 0
                if (cond == PZC and not was and now) or (
                    cond == NZC and was and not now
                ):
                    hit |= 0x80 >> i
            return hit
        th, greater = self._threshold(cond)
        for i in axes:
            v = values[i]
            t = th
            if not self.signed:
                v, t = abs(v), abs(t)
            if (v > t) if greater else (v <= t):
                hit |= 0x80 >> i
        if cond in (GLTH1, LLTH1):
            return hit if hit == tmask and tmask else 0
        return hit

    def _hold(self, hit):
        if hit > 0:
            self.tmasks[self.mask_sel] = hit
        return hit != 0

    def _process(self, values):
        if self.tc is not None:
            self.tc -= 1
        op, params = self.code[self.pp]
        if op == JMP:
            for cond, target in (
                (params[0] >> 4, params[1]),
                (params[0] & 0xF, params[2]),
            ):
                if self._hold(self._eval(cond, values)):
                    self._next(target)
                    return
            return
        reset, nxt = op >> 4, op & 0xF
        if self._hold(self._eval(reset, values)):
            self._release()
            self._goto(self.rp)
            self._run_commands()
        elif self._hold(self._eval(nxt, values)):
            self._next(self.pp + 1)

    def _next(self, addr):
        if self.r_tam:
            self._release(self.mask_sel)
        self._goto(addr, preload=False)
        self._run_commands()

    def step(self, sample):
        """Feed one sample (g, sensor frame); returns the interrupts it raised."""
        n = len(self.events)
        x, y, z = sample
        v = math.sqrt(x * x + y * y + z * z)
        values = [x, -x, y, -y, z, -z, v, -v]
        use = True
        if self.desc is not None:
            self.desc -= 1
            use = self.desc <= 0
            if use:
                self.desc = self.program.decimation
        if use and not self.stopped:
            self._process(values)
        if use:
            self.prev = values
        self.index += 1
        return self.events[n:]

    def run(self, samples):
        for s in samples:
            self.step(s)
        return self.events
