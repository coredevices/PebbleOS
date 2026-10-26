# Characterization tools

The tools in `tools/fuel_gauge/` run the
{ref}`characterization <fuel-gauge-characterization>` of a cell and fit its
model. Their extra dependencies are in `tools/fuel_gauge/requirements.txt`.

## Bench runs

`bench.py` runs the measurement procedure on an instrument: the charge, the
rest and the pulsed discharge down to the cutoff. The procedure and its
defaults are the same on every instrument. A backend in
`tools/fuel_gauge/instruments/` runs it on its hardware: it charges with a
CC/CV profile until the charger terminates, runs a discharge profile down to
its cutoff, and logs both. Supporting an instrument means adding a backend
module and listing it in `bench.INSTRUMENTS`.

```shell
python tools/fuel_gauge/bench.py INSTRUMENT [instrument options] mycell_25c.csv \
    --capacity-mah 70 --vterm 4.45 --ichg 70
```

It writes the discharge log, the charge log next to it as
`mycell_25c_charge.csv`, and the settings as `mycell_25c.json`. `--help`
lists the profile's options; `--skip-charge` starts from a cell that is
already full.

### nPM1300-EK

```{figure} npm1300_ek_wiring.svg
:align: center
:alt: Host connected to the nPM1300-EK shell, USB power on the EK's PMIC
  port while charging, the nPM-FG load on the EK, and the cell and its
  thermistor on the EK's battery and NTC connectors

nPM1300-EK setup. The EK's charger charges the cell, and the nPM-FG load
board runs the discharge.
```

The `npm1300-ek` backend drives the EK's shell over the interface MCU's USB
port. The PMIC's USB port powers the charger: connect it to charge and
disconnect it to discharge, when the backend asks. The voltage is measured
on the EK rather than at the cell terminals, so measure the resistance in
between, see {ref}`fuel-gauge-instrument`. The EK can't end a rest on a
voltage slope, so the rests have a fixed length.

```shell
python tools/fuel_gauge/bench.py npm1300-ek /dev/cu.usbmodemXXXX mycell_25c.csv \
    --capacity-mah 70 --vterm 4.45 --ichg 70 --ntc 10k
```

`--ntc` gives the thermistor type; without one, `--temp` gives the ambient
temperature to log.

## Fit and replay

`fuel_gauge.py` works on the logs:

- `fit` fits the model to bench logs, and prints the board's battery node
  properties and Kconfig defaults.
- `replay` runs the firmware estimator over a log, with a model file or the
  model of a board's firmware.
- `synth` simulates the bench procedure on a model.
- `extract` turns the battery service's verbose logs from a device into a
  log.

## Log format

Every instrument writes the same CSV log, which is what the fit and the
replay read (`tools/fuel_gauge/fuel_gauge_log.py`):

| Column | Unit | |
|---|---|---|
| `time_s` | s | from the start of the log |
| `voltage_v` | V | at the cell |
| `current_a` | A | positive when discharging |
| `temperature_c` | °C | |
| `charge_state` | | optional: `enum pbl_fuel_gauge_charge_state`, for `replay` |

Each row's current is taken as held over the interval since the previous
row, like on the device.
