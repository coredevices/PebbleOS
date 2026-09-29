# Fuel gauge

`subsys/fuel_gauge/` estimates the state of charge (SOC) of a single Li-ion
or Li-po cell from voltage, current and temperature measurements. Its API is
`include/pbl/fuel_gauge/fuel_gauge.h`. The library is plain C over `<math.h>`
with no OS dependencies: the battery service feeds it measurements, and the
tooling in `tools/fuel_gauge.py` builds the same file for the host to replay
logs through it.

It is an open alternative to the prebuilt Nordic nRF Fuel Gauge library. The
battery service selects the estimator with the `BATTERY_SOC` choice:

- `CONFIG_BATTERY_SOC_VOLTAGE`: voltage curve lookup (QEMU and older boards)
- `CONFIG_NRF_FUEL_GAUGE`: Nordic library, the default with the nPM1300
- `CONFIG_BATTERY_SOC_FUEL_GAUGE`: this estimator, with the board's cell
  model from `src/fw/services/battery/fuel_gauge/models/<board>.c`

## Cell model

The cell is an equivalent circuit: an open-circuit voltage (OCV) that depends
on the SOC, in series with a resistor R0 and one resistor-capacitor pair
R1||C1.

```
          R0          R1
  +--/\/\/\----+---/\/\/\---+--- terminal (v)
  |            |            |
 OCV(soc)      +----||------+
  |                 C1
  +----------------------------- ground
```

With the current `i` positive when discharging:

```
v = OCV(soc) - v1 - R0 * i
```

- `OCV(soc)` is a table of points, interpolated linearly. It is the voltage
  of the cell after a long rest.
- R0 is the instantaneous drop when the current steps: electrolyte, contacts
  and tabs.
- R1||C1 is the slower polarization (charge transfer and diffusion) that
  builds up under load and relaxes over the time constant `tau1 = R1 * C1`.
  Its voltage `v1` evolves as
  `v1' = exp(-dt / tau1) * v1 + R1 * (1 - exp(-dt / tau1)) * i`.
- Resistances grow as the cell gets colder, as
  `R(T) = R(25 C) * exp(r_temp_coeff * (25 - T))`.

The model (`struct pbl_fuel_gauge_model`) holds the capacity, the OCV table,
R0, R1, tau1 and `r_temp_coeff`. A single RC pair keeps the model small and
the fit well conditioned. The one-minute sampling of the battery service can't
resolve faster dynamics anyway.

## Estimator

The estimator is an extended Kalman filter over the state `x = [soc, v1]`
and its 2x2 covariance `P`.

**Predict** (coulomb counting). Each measurement's current is taken as the
average over the `dt` since the previous one:

```
soc' = soc - i * dt / (3600 * capacity_ah)
v1'  = a * v1 + R1 * (1 - a) * i          a = exp(-dt / tau1)
P'   = F P F^T + g g^T current_noise^2     F = diag(1, a)
                                           g = [-dt / (3600 * capacity_ah), R1 * (1 - a)]
```

Both states are driven by the same current error, so the process noise is
the outer product of one vector. `current_noise` is how wrong a single
sample can be as the average of the last interval. The battery service samples
once a minute while the watch load is bursty (backlight, vibration, radio), so
this dominates over the ADC error.

**Correct** (voltage). The terminal voltage is predicted from the state and
compared to the measurement:

```
v_pred = OCV(soc) - v1 - R0(T) * i
H      = [dOCV/dsoc, -1]
S      = H P H^T + voltage_noise^2
K      = P H^T / S
x     += K * (v - v_pred)
P     -= K S K^T
```

`voltage_noise` covers the ADC resolution and the model error. The filter
trusts the voltage more where the OCV curve is steep (large `dOCV/dsoc`, so a
small voltage error means a small SOC error) and relies on coulomb counting on
the flat plateau. Current gain and offset errors, which make pure coulomb
counting drift without bound, are corrected continuously.

**Charge complete.** When the charger terminates, the SOC is set to 1 with a
small uncertainty.

**Reported SOC.** `pbl_fuel_gauge_update()` returns the SOC to show. While
the estimate is still converging (SOC standard deviation above 2%, e.g.
right after starting from the voltage), it follows the estimate. After that,
it doesn't rise while discharging or drop while charging, so the percentage
never moves against the direction the battery is going. The raw estimate is
available through `pbl_fuel_gauge_soc_get()`.

## Time to empty and time to full

Both use an exponentially weighted average current (3 h time constant). It
restarts whenever the charge state changes and is reported once it covers 10
minutes.

- **Time to empty** is the reported SOC times the capacity, divided by the
  average discharge current.
- **Time to full** follows the charger. In constant current (CC), the charger
  switches to constant voltage (CV) when `OCV(soc) + i * (R0 + R1)` reaches
  the termination voltage. Inverting the OCV table gives the SOC where that
  happens, and the CC time is the charge up to it divided by the current. In
  CV, the current decays roughly exponentially from `i0` to the termination
  current, which delivers `q` in `q / (i0 - i_term) * ln(i0 / i_term)`.

## Persistence

`pbl_fuel_gauge_state_get()` returns a `struct pbl_fuel_gauge_state` that
the battery service stores in the `fgs` settings file (or the MFG battery
state region). `pbl_fuel_gauge_init()` resumes from it unless one of these
holds:

- It was produced with another model or state layout: `model_id` is a hash
  of both.
- It disagrees with the SOC implied by the current voltage by more than 25%.
  That happens after a battery swap, or a long time without power.

In both cases it starts over from the voltage.

## Characterizing a cell

`tools/fuel_gauge.py` fits a model to measurements and replays logs through
the estimator. It needs numpy and scipy (both in the development venv), plus
matplotlib for plots and a host C compiler for `replay` and `synth`.

Logs are CSV files with the columns `time_s`, `voltage_v`, `current_a`
(positive when discharging), `temperature_c` and `charge_state` (the
`enum pbl_fuel_gauge_charge_state` value).

### Bench procedure

A source-measure unit or a programmable load logging voltage and current at
about 1 Hz gives the best model. The fit needs a known SOC path, so:

1. Charge the cell fully (CC/CV to the charger's termination voltage and
   current, as on the watch) and let it rest for 30 minutes.
2. Discharge in pulses: 0.5C for 144 s (2% of capacity), then 15 minutes of
   rest, repeated until the cell reaches its cutoff voltage. The pulse edges
   identify R0, the relaxation after each pulse identifies R1 and tau1, and
   the end of every rest gives an OCV point.
3. Optionally repeat at other temperatures, e.g. 5 C and 40 C, to fit
   `r_temp_coeff`.

Then fit:

```shell
python tools/fuel_gauge.py fit pulse_25c.csv pulse_5c.csv --name obelix \
    -o src/fw/services/battery/fuel_gauge/models/obelix.c
```

By default each log starts full (`--start-soc 1`), and the first log ends
empty (`--end-soc 0`), which defines the capacity. Use `--capacity-ah` to set
it instead. The fit prints the parameters, the voltage residual, and a
warning for any SOC range the logs don't cover.

How the fit works: for a given tau1 and temperature coefficient, the terminal
voltage is linear in the OCV points, R0 and R1, so each candidate is a
bounded linear least-squares problem. The OCV points are written as a base
voltage plus non-negative steps, so the curve comes out increasing. A small
curvature penalty (`--smoothing`) fills gaps in the data. tau1 (and the
temperature coefficient, when the logs span at least 10 C) is found by a 1-D
search around it.

### Watch logs

With the verbose level enabled for the battery service, the fuel gauge
backend logs every sample:

```
Battery state: v_mv: 4012, i_ua: 1500, t_mc: 24500, td_ms: 60000, fg: 0, ...
```

`extract` turns a log capture into a CSV log:

```shell
python tools/fuel_gauge.py extract watch.log watch.csv
```

A full charge followed by a discharge to shutdown can be fitted like bench
data (with `--start-soc` / `--end-soc` matching the capture). It is a coarser
source: one sample a minute of a bursty load, with a 5 mV voltage resolution.

### Replay and simulation

`replay` builds `subsys/fuel_gauge/fuel_gauge.c` with a model file for the
host and runs a log through it. With `--start-soc`, it compares the estimate
against coulomb counting from that SOC. `--current-noise` and
`--voltage-noise` try other tunings, and `--plot` draws voltage, current and
SOC:

```shell
M=src/fw/services/battery/fuel_gauge/models/obelix.c
python tools/fuel_gauge.py replay $M pulse_25c.csv --start-soc 1 --plot soc.png
```

`synth` simulates the bench procedure on a model. Fitting its output shows
how well a procedure identifies the parameters:

```shell
python tools/fuel_gauge.py synth $M synth.csv --lsb 0.0049
python tools/fuel_gauge.py fit synth.csv --name test -o fit.c
```

On simulated watch usage (one sample a minute of a 1 mA load with 20 mA
bursts, 5 mV voltage resolution), a model that matches the cell keeps the
SOC within about 1%. That still holds with a ±10% current gain error or a
±0.3 mA offset. A start 20% off converges within minutes. On a real cell,
the model error dominates, so the fit residual is the number to watch.

## Limitations

- The OCV is modelled at 25 C, and the capacity doesn't depend on
  temperature. Only the resistances do.
- There is no OCV hysteresis between charge and discharge. The fitted curve
  sits between the two branches when the logs include charging.
- Capacity fade with aging is not tracked.
- The board models in the tree are placeholders until the cells are
  characterized. The OCV comes from the voltage-curve backend's discharge
  table, which tops out at 4.23 V while the nPM1300 charges to 4.35 V. The
  reported SOC therefore stays at 100% for a while after a full charge, and
  time to full leaves out the CV phase.
