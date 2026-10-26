#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Bench characterization of a cell.

Runs the procedure of docs/libraries/fuel_gauge/models/thevenin.md on an
instrument: charges the cell CC/CV, rests it, then discharges it in pulses
down to the cutoff voltage, writing CSV logs (fuel_gauge_log.py) for
fuel_gauge.py fit. The procedure is the same on every instrument; a backend
in instruments/ runs it on its hardware.
"""

import argparse
import dataclasses
import importlib
import json
import pathlib

from fuel_gauge_log import LogWriter

# Instrument names and their backend modules in instruments/
INSTRUMENTS = {
    "npm1300-ek": "npm1300_ek",
}


@dataclasses.dataclass
class ChargeProfile:
    """CC/CV charge, as the product charges the cell."""

    voltage: float  # [V]
    current: float  # [A]
    term_current: float  # [A]
    timeout: float  # give up after this time [s]


@dataclasses.dataclass
class DischargeProfile:
    """A rest, pulses down to the tail threshold, then the tail down to the cutoff.

    Each pulse is followed by a rest. The tail pulses draw less current for
    longer, so near empty the cell delivers closer to what a light load gets.
    """

    initial_rest: float  # [s]
    pulse_current: float  # [A]
    pulse_length: float  # [s]
    rest: float  # [s]
    tail_voltage: float  # loaded voltage that starts the tail [V]
    tail_current: float  # [A]
    tail_length: float  # [s]
    cutoff: float  # loaded voltage that ends the discharge [V]


def profiles(args):
    capacity = args.capacity_mah / 1000.0
    pulse_current = args.rate * capacity
    charge = ChargeProfile(
        voltage=args.vterm,
        current=args.ichg / 1000.0,
        term_current=args.iterm / 100.0 * args.ichg / 1000.0,
        timeout=args.charge_timeout * 3600.0,
    )
    discharge = DischargeProfile(
        initial_rest=args.initial_rest,
        pulse_current=pulse_current,
        pulse_length=args.pulse,
        rest=args.rest,
        tail_voltage=args.cutoff + args.tail_margin,
        tail_current=pulse_current / 4.0,
        tail_length=args.pulse * 4.0,
        cutoff=args.cutoff,
    )
    return charge, discharge


def _add_procedure_arguments(parser):
    parser.add_argument("output", type=pathlib.Path, help="discharge CSV log")
    parser.add_argument(
        "--capacity-mah", type=float, required=True, help="rated capacity"
    )
    parser.add_argument("--vterm", type=float, required=True, help="charge voltage [V]")
    parser.add_argument("--ichg", type=float, required=True, help="charge current [mA]")
    parser.add_argument(
        "--iterm", type=float, default=10.0, help="termination current [%% of --ichg]"
    )
    parser.add_argument(
        "--temp", type=float, default=25.0, help="ambient [C], logged without a sensor"
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
        "--charge-timeout",
        type=float,
        default=4.0,
        help="give up if the charge doesn't complete within this time [h]",
    )
    parser.add_argument(
        "--skip-charge", action="store_true", help="cell is already full"
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="instrument", required=True, metavar="instrument")
    backends = {}
    for name, module in INSTRUMENTS.items():
        backend = importlib.import_module(f"instruments.{module}")
        p = sub.add_parser(name, help=backend.__doc__.splitlines()[0])
        backend.add_arguments(p)
        _add_procedure_arguments(p)
        backends[name] = backend
    args = parser.parse_args(argv)

    charge, discharge = profiles(args)
    args.output.with_suffix(".json").write_text(
        json.dumps(vars(args), default=str, indent=2)
    )

    instrument = backends[args.instrument].open(args)
    try:
        if not args.skip_charge:
            log = LogWriter(args.output.with_name(args.output.stem + "_charge.csv"))
            try:
                instrument.charge(charge, log)
            finally:
                log.close()
        log = LogWriter(args.output)
        try:
            instrument.discharge(discharge, log)
        finally:
            log.close()
    finally:
        instrument.close()


if __name__ == "__main__":
    main()
