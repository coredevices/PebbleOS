# Fuel gauge characterization on the nPM1300-EK

`tools/fuel_gauge/bench_npm1300_ek.py` runs the
[characterization procedure](characterization.md) on an
nPM1300-EK with an nPM-FG load board. The EK firmware is only used as an
instrument, through its serial shell.

## Hardware

- **nPM1300-EK**: the nPM1300 charger plus an interface MCU exposing a
  shell over USB. It has two USB ports: one for the interface MCU (the
  shell, always connected) and one for the PMIC's VBUS (the charger input).
- **nPM-FG**: the active load board, attached to the EK. It sinks a
  programmed current and reports the load current and voltage.
- **The cell**, on the EK's battery connector. If the pack has a thermistor
  wire, check it before connecting: it should read about 10 kΩ to the
  negative terminal at room temperature (the watches use a 10 kΩ,
  β = 3380 NTC). Wire colours are not standardized.

The nPM-FG measures the voltage on the EK, not at the cell terminals, so
the connector, wiring and PCB resistance between the cell and that point
end up in the fitted $R_0$. Measure that resistance to correct for it, see
{ref}`fuel-gauge-instrument`.

Without an NTC, pass the ambient temperature with `--temp`.

## Running

Use the cell's rated capacity and the watch's charge settings:

```shell
python tools/fuel_gauge/bench_npm1300_ek.py /dev/cu.usbmodemXXXX getafix_25c.csv \
    --capacity-mah 70 --vterm 4.45 --ichg 70 --ntc 10k
```

The EK exposes two serial ports; the shell is the one that answers. Use
`--skip-charge` to go straight to the discharge with a cell that is already
full, and pass `--ntc none --temp <C>` for a cell without thermistor.
`--help` lists the profile's options, with the defaults of the procedure.

The script:

1. **Resets** the PMIC and restarts the EK application. The charger latches
   errors, e.g. after swapping to a flat or badly seated cell, and refuses
   to charge until reset.
2. **Configures** the charger with the cell's charge voltage, current and
   termination current, and the thermistor.
3. **Charges** (VBUS connected, prompted before the reset, which also powers
   the PMIC when the cell is flat): CC/CV until the charger reports full,
   logging the charger ADC once a second to `<output>_charge.csv`. It gives
   up after `--charge-timeout` hours (4 by default).
4. **Discharges** (VBUS disconnected, prompted) with the nPM-FG: the initial
   rest, the pulses down to the cutoff plus `--tail-margin`, then the
   quarter-rate tail down to the cutoff. This is logged at 1 Hz to
   `<output>`, timed with the EK's log timestamps, with the current made
   positive for discharge.

The EK's load schedule can't end a step on a voltage slope, so the rests
have a fixed length (`--rest`, 15 minutes).

The settings are saved next to the log as `<output>.json`. The script holds
the serial port exclusively, so nothing else can talk to the EK during a
run, which takes about 15 hours plus up to two hours of charging.

## Shell interface

The script talks to the EK shell over the interface MCU's USB serial port.
Commands reply with `Success: ...` or `Value: ...`, and measurements arrive
as log lines from `module_pmic_adc` (charger ADC) and `module_cc_profiling`
(load). The commands used:

| Command | Purpose |
|---|---|
| `npmx charger module charger set 0/1` | disable/enable the charger |
| `npmx charger termination_voltage normal set <mV>` | charge voltage |
| `npmx charger charging_current set <uA>` | charge current (mA on older EK firmware, detected from the `get` reply) |
| `npmx charger termination_current set 10/20` | termination current, % of the charge current |
| `npmx charger module recharge set 0` | no automatic recharge |
| `npmx adc ntc type set <ohm>`, `npmx adc ntc beta set <beta>` | thermistor, 0 to ignore it |
| `npmx charger status all get` | charger status bits: 0x01 battery detected, 0x02 full, 0x04 trickle, 0x08 CC, 0x10 CV |
| `npmx reset` | reset the PMIC, clearing latched charger errors |
| `delayed_reboot <ms>` | restart the EK application, needed after a PMIC reset |
| `npm_adc sample <sample ms> <report ms>` | charger ADC logging |
| `cc_sink available` | whether the nPM-FG is attached |
| `cc_profile profile set <report ms> <ntc ms> <cutoff V> "<step>" ...` | load schedule |
| `cc_profile start` / `cc_profile stop` | run/stop the schedule |

Each schedule step is `"t_load_ms,t_rest_ms,i_load_A,i_rest_A,cycles[,v_cutoff_V]"`:
a load period then a rest period, repeated `cycles` times, or until the
loaded voltage falls below `v_cutoff_V` when `cycles` is `NaN`. The run ends
at the global cutoff voltage, logging `vcutoff reached`. While a schedule
runs, the load reports `iload` (negative when discharging), `vload` and
`tbat` at the report interval, and the charger ADC logging stops.

## Troubleshooting

- **`IO error` on charger commands**: the PMIC is unpowered, typically after
  swapping in a flat cell with VBUS unplugged. Connect VBUS.
- **Not charging, status `0x00`**: the charger sees no battery. Check the
  `module_pmic_adc` logs: `vbat` near 0 V means the cell isn't connected, its
  protection has tripped, or it is dead; measure it with a multimeter.
  With a good cell, a latched charger error also shows this; the PMIC reset
  at the start of a run clears it.
- **`NTCCOLD`/`NTCCOOL`/`NTCHOT` events and a jumping `tbat`**: the
  thermistor contact is intermittent, and the charger stops when it reads
  out of range. Fix the contact, or use `--ntc none --temp <C>` and watch
  the charge.
- **`VBATLOW` and `VTRICKLE` charger errors**: trickle charging didn't raise
  the voltage above the low-battery threshold. The cell can't be recovered
  by the charger; don't use it.
