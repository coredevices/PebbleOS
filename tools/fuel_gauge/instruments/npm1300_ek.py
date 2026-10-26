# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""nPM1300-EK with an nPM-FG active load.

Drives the EK shell over its USB serial port: the EK's charger charges the
cell, and the nPM-FG runs the discharge. The cell must be on the EK battery
connector with the nPM-FG attached. VBUS (the EK's PMIC USB port) must be
connected for charging, and disconnected for the discharge; the backend asks
when.
"""

import math
import re
import sys
import time

import serial
from fuel_gauge_log import ChargeState

PROMPT = "shell:~$ "
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
LOG_LINE = re.compile(r"^\[(\d+):(\d\d):(\d\d)\.(\d+),(\d+)\] <(\w+)> (\w+): (.*)$")
REPLY = re.compile(r"\b(Success|Value|Error|error|usage):")

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


def _configure_charger(sh, profile):
    iterm = round(100.0 * profile.term_current / profile.current)
    if iterm not in (10, 20):
        sys.exit("the nPM1300 terminates at 10% or 20% of the charge current")
    sh.cmd("npmx charger module charger set 0")
    sh.cmd(
        f"npmx charger termination_voltage normal set {round(profile.voltage * 1000)}"
    )
    # Older EK firmware takes the charge current in mA, newer in uA.
    unit = sh.cmd("npmx charger charging_current get")
    ma = profile.current * 1000.0
    sh.cmd(
        f"npmx charger charging_current set {round(ma if unit.endswith('mA.') else ma * 1000)}"
    )
    sh.cmd(f"npmx charger termination_current set {iterm}")
    sh.cmd("npmx charger module recharge set 0")


def _configure_ntc(sh, ntc):
    ntc_type, ntc_beta = NTC[ntc]
    sh.cmd(f"npmx adc ntc type set {ntc_type}")
    if ntc_beta:
        sh.cmd(f"npmx adc ntc beta set {ntc_beta}")


def _charge(sh, profile, log, temp_of):
    t0 = time.monotonic()
    state = [ChargeState.DISCHARGING]
    first = []

    def on_log(t, level, module, message):
        if module != "module_pmic_adc":
            return
        d = _kv(message)
        if not all(k in d and math.isfinite(d[k]) for k in ("vbat", "ibat")):
            return
        temp = temp_of(d)
        # Only charging here, so negative whatever sign the EK reports.
        first[:] = first or [t]
        log.write(t - first[0], d["vbat"], -abs(d["ibat"]), temp, state[0])

    sh.on_log = on_log
    sh.cmd("npm_adc sample 1000 1000")
    sh.cmd("npmx charger module charger set 1")
    while True:
        status = int(sh.value("npmx charger status all get"))
        if status & STATUS_FULL:
            state[0] = ChargeState.CHARGE_COMPLETE
            break
        state[0] = (
            ChargeState.CHARGING_CV
            if status & STATUS_CV
            else ChargeState.CHARGING_CC
            if status & (STATUS_CC | STATUS_TRICKLE)
            else ChargeState.DISCHARGING
        )
        if state[0] == ChargeState.DISCHARGING:
            print(f"warning: not charging (status 0x{status:02x})", file=sys.stderr)
        if time.monotonic() - t0 > profile.timeout:
            sh.cmd("npmx charger module charger set 0")
            sys.exit(
                f"\nno charge completion after {profile.timeout / 3600.0:g} h "
                f"(status 0x{status:02x})"
            )
        sh.poll(10.0)
        print(f"\rcharging {(time.monotonic() - t0) / 60:.0f} min", end="", flush=True)
    sh.cmd("npmx charger module charger set 0")
    sh.on_log = lambda *a: None
    print(f"\ncharge complete after {(time.monotonic() - t0) / 60:.0f} min")


def _discharge(sh, profile, log, temp_of):
    rest_ms = round(profile.rest * 1000)
    pulse_ms = round(profile.pulse_length * 1000)
    tail_ms = round(profile.tail_length * 1000)
    steps = [
        # 2 s cycles of no load.
        f'"1000,1000,0,0,{round(profile.initial_rest / 2)}"',
        # Pulses until the loaded voltage falls below the tail voltage.
        f'"{pulse_ms},{rest_ms},{profile.pulse_current:.4f},0,NaN,{profile.tail_voltage:.3f}"',
        f'"{tail_ms},{rest_ms},{profile.tail_current:.4f},0,NaN"',
    ]

    input("Disconnect VBUS (the EK's PMIC USB port), then press Enter to discharge.")
    if not sh.value("cc_sink available"):
        sys.exit("nPM-FG load not detected")
    sh.cmd("npmx charger module charger set 0")

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
        temp = temp_of(d)
        first[:] = first or [t]
        # The load reports discharge current as negative.
        log.write(t - first[0], d["vload"], -d["iload"], temp, ChargeState.DISCHARGING)

    sh.on_log = on_log
    sh.cmd(f"cc_profile profile set 1000 8000 {profile.cutoff:.3f} {' '.join(steps)}")
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


class Npm1300Ek:
    def __init__(self, args):
        self._ntc = args.ntc
        self._temp = args.temp
        self._sh = Shell(args.port)
        # The charger latches errors, e.g. after a swap to a flat or badly
        # seated cell, and only a PMIC reset clears them. The EK application
        # then needs a restart.
        self._sh._s.write(b"npmx reset\r\n")
        time.sleep(1.0)
        self._sh.reboot()
        self._sh.cmd("npmx charger module charger set 0")
        _configure_ntc(self._sh, self._ntc)

    def _temp_of(self, d):
        return d.get("tbat", self._temp) if self._ntc != "none" else self._temp

    def charge(self, profile, log):
        _configure_charger(self._sh, profile)
        _charge(self._sh, profile, log, self._temp_of)

    def discharge(self, profile, log):
        _discharge(self._sh, profile, log, self._temp_of)

    def close(self):
        self._sh._s.close()


def add_arguments(parser):
    parser.add_argument("port", help="EK shell serial port, e.g. /dev/cu.usbmodem11201")
    parser.add_argument("--ntc", choices=NTC, default="none", help="cell thermistor")


def open(args):
    if not args.skip_charge:
        # Also powers the PMIC if the cell is flat or was just swapped.
        input("Connect VBUS (the EK's PMIC USB port), then press Enter to charge.")
    return Npm1300Ek(args)
