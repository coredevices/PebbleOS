#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Bench characterization of a cell on the nPM1300-EK with an nPM-FG load.

Drives the EK shell over its USB serial port: charges the cell CC/CV, rests
it, then discharges it in pulses through the nPM-FG active load down to the
cutoff voltage. Writes CSV logs for tools/fuel_gauge.py fit (see
docs/subsystems/fuel_gauge.md).

The cell must be on the EK battery connector with the nPM-FG attached. VBUS
(the EK's PMIC USB port) must be connected for charging, and disconnected
for the discharge; the script asks when.
"""

import argparse
import csv
import json
import math
import pathlib
import re
import sys
import time

import serial

PROMPT = "shell:~$ "
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
LOG_LINE = re.compile(r"^\[(\d+):(\d\d):(\d\d)\.(\d+),(\d+)\] <(\w+)> (\w+): (.*)$")
REPLY = re.compile(r"\b(Success|Value|Error|error|usage):")

LOG_COLUMNS = ("time_s", "voltage_v", "current_a", "temperature_c", "charge_state")
DISCHARGING, CHARGING_CC, CHARGING_CV, CHARGE_COMPLETE = range(4)

STATUS_FULL = 0x02
STATUS_TRICKLE = 0x04
STATUS_CC = 0x08
STATUS_CV = 0x10

NTC = {
    "none": (0, None),
    "10k": (10000, 3380),
    "47k": (47000, 4050),
    "100k": (100000, 4250),
}


def _kv(message):
    out = {}
    for part in message.split(","):
        k, _, v = part.partition("=")
        try:
            out[k.strip()] = float(v)
        except ValueError:
            pass
    return out


class Shell:
    """The EK shell. Log lines go to on_log, the rest are command replies."""

    def __init__(self, port):
        self._port = port
        self._s = self._open()
        self._buf = ""
        self.on_log = lambda t, level, module, message: None

    def _open(self):
        # Exclusive, so nothing else reads replies or logs during a run.
        return serial.Serial(self._port, 115200, timeout=0.2, exclusive=True)

    def reboot(self):
        """Restarts the EK application, which re-enumerates its USB port."""
        self._s.write(b"delayed_reboot 100\r\n")
        self._s.close()
        end = time.monotonic() + 30.0
        while time.monotonic() < end:
            time.sleep(1.0)
            try:
                self._s = self._open()
                self._buf = ""
                self.cmd("cc_sink available")
                return
            except (serial.SerialException, OSError, RuntimeError):
                try:
                    self._s.close()
                except (serial.SerialException, OSError):
                    pass
        sys.exit(f"{self._port} did not come back after rebooting the EK")

    def _pump(self):
        """Reads what is available, returns complete non-log lines."""
        chunk = ANSI.sub("", self._s.read(4096).decode(errors="replace"))
        self._buf += chunk.replace(PROMPT, "")
        lines = []
        while "\n" in self._buf:
            line, self._buf = self._buf.split("\n", 1)
            line = line.strip()
            m = LOG_LINE.match(line)
            if m:
                h, mi, sec, ms, us = (int(g) for g in m.groups()[:5])
                t = h * 3600 + mi * 60 + sec + ms / 1e3 + us / 1e6
                self.on_log(t, *m.groups()[5:])
            elif line:
                lines.append(line)
        return lines

    def poll(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self._pump()

    def cmd(self, command, timeout=5.0):
        """Sends a command, returns its Success/Value reply."""
        self._pump()
        self._s.write((command + "\r\n").encode())
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for line in self._pump():
                # Long commands are echoed wrapped, so match the reply, not
                # the echo.
                m = REPLY.search(line)
                if not m:
                    continue
                if m.group(1) in ("Success", "Value"):
                    return line[m.start() :]
                raise RuntimeError(f"{command!r}: {line[m.start() :]}")
        raise RuntimeError(f"{command!r}: no reply")

    def value(self, command):
        m = re.search(r"Value:\s*(-?[\d.]+)", self.cmd(command))
        if not m:
            raise RuntimeError(f"{command!r}: no value")
        return float(m.group(1))


class CsvLog:
    def __init__(self, path):
        # Held open for the whole run, flushed on every row.
        self._f = open(path, "w", newline="")  # noqa: SIM115
        self._w = csv.writer(self._f)
        self._w.writerow(LOG_COLUMNS)
        self.rows = 0

    def write(self, t, v, i, temp, state):
        self._w.writerow([f"{t:.3f}", f"{v:.4f}", f"{i:.6f}", f"{temp:.2f}", state])
        self._f.flush()
        self.rows += 1


def configure(sh, args):
    ntc_type, ntc_beta = NTC[args.ntc]
    sh.cmd("npmx charger module charger set 0")
    sh.cmd(f"npmx charger termination_voltage normal set {round(args.vterm * 1000)}")
    # Older EK firmware takes the charge current in mA, newer in uA.
    unit = sh.cmd("npmx charger charging_current get")
    ichg = round(args.ichg) if unit.endswith("mA.") else round(args.ichg * 1000)
    sh.cmd(f"npmx charger charging_current set {ichg}")
    sh.cmd(f"npmx charger termination_current set {args.iterm}")
    sh.cmd("npmx charger module recharge set 0")
    sh.cmd(f"npmx adc ntc type set {ntc_type}")
    if ntc_beta:
        sh.cmd(f"npmx adc ntc beta set {ntc_beta}")


def charge(sh, args, out):
    log = CsvLog(out)
    t0 = time.monotonic()
    state = [DISCHARGING]
    first = []

    def on_log(t, level, module, message):
        if module != "module_pmic_adc":
            return
        d = _kv(message)
        if not all(k in d and math.isfinite(d[k]) for k in ("vbat", "ibat")):
            return
        temp = d.get("tbat", args.temp) if args.ntc != "none" else args.temp
        # Only charging here, so negative whatever sign the EK reports.
        first[:] = first or [t]
        log.write(t - first[0], d["vbat"], -abs(d["ibat"]), temp, state[0])

    sh.on_log = on_log
    sh.cmd("npm_adc sample 1000 1000")
    sh.cmd("npmx charger module charger set 1")
    while True:
        status = int(sh.value("npmx charger status all get"))
        if status & STATUS_FULL:
            state[0] = CHARGE_COMPLETE
            break
        state[0] = (
            CHARGING_CV
            if status & STATUS_CV
            else CHARGING_CC
            if status & (STATUS_CC | STATUS_TRICKLE)
            else DISCHARGING
        )
        if state[0] == DISCHARGING:
            print(f"warning: not charging (status 0x{status:02x})", file=sys.stderr)
        sh.poll(10.0)
        print(f"\rcharging {(time.monotonic() - t0) / 60:.0f} min", end="", flush=True)
    sh.cmd("npmx charger module charger set 0")
    sh.on_log = lambda *a: None
    print(f"\ncharge complete after {(time.monotonic() - t0) / 60:.0f} min")


def discharge(sh, args, out):
    """Rest, then pulses at args.rate C, then at a quarter of it near empty."""
    cap = args.capacity_mah / 1000.0
    i_load = args.rate * cap
    pulse_ms = round(args.pulse * 1000)
    rest_ms = round(args.rest * 1000)
    profiles = [
        # 2 s cycles of no load.
        f'"1000,1000,0,0,{round(args.initial_rest / 2)}"',
        f'"{pulse_ms},{rest_ms},{i_load:.4f},0,NaN,{args.cutoff + args.tail_margin:.3f}"',
        f'"{pulse_ms * 4},{rest_ms},{i_load / 4:.4f},0,NaN"',
    ]

    input("Disconnect VBUS (the EK's PMIC USB port), then press Enter to discharge.")
    if not sh.value("cc_sink available"):
        sys.exit("nPM-FG load not detected")
    sh.cmd("npmx charger module charger set 0")

    log = CsvLog(out)
    done = [None]
    first = []
    t0 = time.monotonic()

    def on_log(t, level, module, message):
        if module == "module_pmic" and "Power Failure" in message:
            done[0] = message
        if module != "module_cc_profiling":
            return
        if any(s in message for s in ("completed", "vcutoff", "thermal")):
            done[0] = message
            return
        d = _kv(message)
        if not all(k in d for k in ("vload", "iload")):
            return
        temp = d.get("tbat", args.temp) if args.ntc != "none" else args.temp
        first[:] = first or [t]
        # The load reports discharge current as negative.
        log.write(t - first[0], d["vload"], -d["iload"], temp, DISCHARGING)

    sh.on_log = on_log
    sh.cmd(f"cc_profile profile set 1000 8000 {args.cutoff:.3f} {' '.join(profiles)}")
    sh.cmd("cc_profile start")
    try:
        while not done[0]:
            sh.poll(10.0)
            print(
                f"\rdischarging {(time.monotonic() - t0) / 3600:.1f} h, {log.rows} samples",
                end="",
                flush=True,
            )
    finally:
        try:
            sh.cmd("cc_profile stop")
        except RuntimeError:
            pass
    print(f"\n{done[0]}")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("port", help="EK shell serial port, e.g. /dev/cu.usbmodem11201")
    parser.add_argument("output", type=pathlib.Path, help="discharge CSV log")
    parser.add_argument(
        "--capacity-mah", type=float, required=True, help="rated capacity"
    )
    parser.add_argument("--vterm", type=float, required=True, help="charge voltage [V]")
    parser.add_argument("--ichg", type=float, required=True, help="charge current [mA]")
    parser.add_argument("--iterm", type=int, choices=(10, 20), default=10, help="[%%]")
    parser.add_argument("--ntc", choices=NTC, default="none")
    parser.add_argument(
        "--temp", type=float, default=25.0, help="ambient [C], logged without NTC"
    )
    parser.add_argument("--cutoff", type=float, default=3.0, help="discharge end [V]")
    parser.add_argument("--rate", type=float, default=0.5, help="pulse current [C]")
    parser.add_argument("--pulse", type=float, default=144.0, help="pulse length [s]")
    parser.add_argument(
        "--rest", type=float, default=900.0, help="rest after pulses [s]"
    )
    parser.add_argument(
        "--initial-rest", type=float, default=1800.0, help="rest after charge [s]"
    )
    parser.add_argument(
        "--tail-margin",
        type=float,
        default=0.2,
        help="switch to quarter-rate pulses this far above the cutoff [V]",
    )
    parser.add_argument(
        "--skip-charge", action="store_true", help="cell is already full"
    )
    args = parser.parse_args(argv)

    args.output.with_suffix(".json").write_text(
        json.dumps(vars(args), default=str, indent=2)
    )
    if not args.skip_charge:
        # Also powers the PMIC if the cell is flat or was just swapped.
        input("Connect VBUS (the EK's PMIC USB port), then press Enter to charge.")
    sh = Shell(args.port)
    # The charger latches errors, e.g. after a swap to a flat or badly seated
    # cell, and only a PMIC reset clears them. The EK application then needs
    # a restart.
    sh._s.write(b"npmx reset\r\n")
    time.sleep(1.0)
    sh.reboot()
    configure(sh, args)
    if not args.skip_charge:
        charge(sh, args, args.output.with_name(args.output.stem + "_charge.csv"))
    discharge(sh, args, args.output)


if __name__ == "__main__":
    main()
