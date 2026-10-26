# Fuel gauge cell characterization

This page explains how a cell model for the [fuel gauge](fuel_gauge.md) is
produced from measurements: the mathematical model, how its parameters are
identified, the bench procedure and the tools. The whole flow uses our own
scripts:

1. `tools/fuel_gauge_bench.py` charges a cell and discharges it in pulses on
   an nPM1300-EK with an nPM-FG load, logging voltage, current and
   temperature.
2. `tools/fuel_gauge.py fit` fits the model to the log and writes a C model
   file, `fw/services/battery/fuel_gauge/models/<board>.c`.
3. `tools/fuel_gauge.py replay` runs the firmware estimator over a log to
   check the model.

The EK firmware is only used as a measurement instrument, through its serial
shell. No Nordic model-generation tools or Nordic-generated model data are
involved.

## Cell model

### Equivalent circuit

The cell is modelled as an open-circuit voltage source in series with a
resistor $R_0$ and one parallel resistor-capacitor pair $R_1 \parallel C_1$:

```
          R0          R1
  +--/\/\/\----+---/\/\/\---+--- terminal (v)
  |            |            |
 OCV(z)        +----||------+
  |                 C1
  +----------------------------- ground
```

With $z$ the state of charge (SOC, 0 to 1), $i$ the current (positive when
discharging), $Q$ the capacity in coulombs and $v_1$ the voltage across the
RC pair:

$$
\begin{aligned}
\dot z &= -\frac{i}{Q} \\
\dot v_1 &= -\frac{v_1}{\tau_1} + \frac{i}{C_1}, \qquad \tau_1 = R_1 C_1 \\
v &= \mathrm{OCV}(z) - v_1 - R_0\, i
\end{aligned}
$$

- $\mathrm{OCV}(z)$ is the voltage of the cell after a long rest. It is
  stored as a table of points and interpolated linearly. It must increase
  with $z$.
- $R_0$ is the instantaneous drop when the current steps: electrolyte,
  contacts, tabs and anything else in series up to where the voltage is
  measured.
- $R_1 \parallel C_1$ is the slower polarization (charge transfer and
  diffusion). It builds up under load and relaxes with the time constant
  $\tau_1$ once the load is removed.

A single RC pair keeps the model small and its fit well conditioned. The
battery service samples once a minute, which can't resolve faster dynamics
anyway.

### Discretization

Samples come at times $t_k$, $\Delta t_k = t_k - t_{k-1}$, and the current
$i_k$ is taken as constant over $(t_{k-1}, t_k]$ (it is the average current
of the interval on the watch). For a constant current, the RC equation has
the exact solution

$$
v_{1,k} = a_k\, v_{1,k-1} + R_1 (1 - a_k)\, i_k, \qquad a_k = e^{-\Delta t_k / \tau_1}
$$

and coulomb counting gives

$$
z_k = z_{k-1} - \frac{i_k\, \Delta t_k}{Q}.
$$

Both the estimator and the fit use these same equations, so a model fitted
on bench logs behaves the same way in the firmware, whatever the sample
interval.

### Temperature

Resistances grow as the cell gets colder. Both $R_0$ and $R_1$ are scaled by

$$
s(T) = e^{\kappa\,(25 - T)}
$$

with $T$ in °C, clamped to -20..60 °C, and $\kappa$ the resistance
temperature coefficient (`r_temp_coeff`). The OCV and the capacity don't
depend on temperature in this model.

### Model file

`struct pbl_fuel_gauge_model` holds:

| Field          | Symbol                  | Unit |
|----------------|-------------------------|------|
| `capacity_ah`  | $Q / 3600$              | Ah   |
| `ocv`          | $(z_j, \mathrm{OCV}_j)$ | -, V |
| `r0`           | $R_0$ at 25 °C          | Ω    |
| `r1`           | $R_1$ at 25 °C          | Ω    |
| `tau1`         | $\tau_1$                | s    |
| `r_temp_coeff` | $\kappa$                | 1/°C |

## Estimator

The firmware estimator is an extended Kalman filter (EKF) over the state
$x = [z, v_1]^T$ with covariance $P$. Each measurement $(v_k, i_k, T_k)$
goes through a prediction and a correction.

**Predict.** The discretized model above, with $g$ the sensitivity of the
state to the current:

$$
\begin{aligned}
x_k^- &= \begin{bmatrix} z_{k-1} \\ a_k v_{1,k-1} \end{bmatrix} + g_k\, i_k,
\qquad g_k = \begin{bmatrix} -\Delta t_k / Q \\ R_1 s(T_k) (1 - a_k) \end{bmatrix} \\
P_k^- &= F_k P_{k-1} F_k^T + g_k g_k^T \sigma_i^2,
\qquad F_k = \begin{bmatrix} 1 & 0 \\ 0 & a_k \end{bmatrix}
\end{aligned}
$$

The process noise comes only from the current error $\sigma_i$
(`current_noise`): both states are driven by the same current, so its
covariance is the outer product of $g_k$ with itself. On the watch,
$\sigma_i$ mostly covers how far a single sample is from the true average
current of the last minute, since the load is bursty.

**Correct.** The predicted terminal voltage and its linearization:

$$
\hat v_k = \mathrm{OCV}(z_k^-) - v_{1,k}^- - R_0 s(T_k)\, i_k,
\qquad H_k = \begin{bmatrix} \mathrm{OCV}'(z_k^-) & -1 \end{bmatrix}
$$

$$
\begin{aligned}
S_k &= H_k P_k^- H_k^T + \sigma_v^2 \\
K_k &= P_k^- H_k^T / S_k \\
x_k &= x_k^- + K_k (v_k - \hat v_k) \\
P_k &= P_k^- - K_k S_k K_k^T
\end{aligned}
$$

$\sigma_v$ (`voltage_noise`) covers the ADC resolution and the model error.
The gain on $z$ is proportional to the OCV slope: where the curve is steep a
voltage error means little SOC error and the filter trusts the voltage; on
the flat plateau it relies on coulomb counting. Errors in the current gain or
offset, which make pure coulomb counting drift without bound, are corrected
continuously.

The [fuel gauge page](fuel_gauge.md) describes the rest of the estimator:
initialization from the voltage, the reported SOC, time to empty and full,
and persistence.

## Parameter identification

### Known SOC path

The fit needs the SOC at every sample, which coulomb counting provides when
the log starts from a known state:

$$
z_k = z_0 - \frac{1}{Q} \sum_{m \le k} i_m\, \Delta t_m.
$$

A bench log starts from a full, rested cell ($z_0 = 1$) and ends at the
discharge cutoff ($z_\text{end} = 0$), so its net discharged charge defines
the capacity:

$$
Q = \frac{\sum_k i_k\, \Delta t_k}{z_0 - z_\text{end}}.
$$

The capacity is therefore "charge delivered from the charger's termination
to the cutoff voltage, at the bench's discharge rates". Over a short run the
coulomb counting error of the bench is negligible, unlike on the watch.

### Linear least squares

For a fixed $\tau_1$ and $\kappa$, the terminal voltage is linear in all the
other unknowns. Writing the OCV as linear interpolation over a grid of SOC
points $z_1 < \dots < z_n$,

$$
\mathrm{OCV}(z_k) = \sum_j w_j(z_k)\, \mathrm{OCV}_j
$$

where $w_j$ are hat functions (each sample has at most two non-zero
weights), the model becomes

$$
v_k = \sum_j w_j(z_k)\, \mathrm{OCV}_j - R_0\, s(T_k)\, i_k - R_1\, L_{\tau_1}[s\,i]_k
$$

with $L_{\tau_1}$ the first-order lag of the discretization above,
$L_k = a_k L_{k-1} + (1 - a_k)\, s(T_k)\, i_k$. Stacking all samples gives
an overdetermined linear system $A\theta \approx v$ in
$\theta = [\mathrm{OCV}_1 \dots \mathrm{OCV}_n, R_0, R_1]$.

Three constraints make the solution physical:

- **Increasing OCV.** The OCV points are written as a base voltage plus
  steps, $\mathrm{OCV}_j = u_1 + \sum_{m=2}^{j} u_m$ (a lower-triangular
  matrix of ones, $\mathrm{OCV} = L u$), with every step
  $u_m \ge 0.1\ \text{mV}$. The estimator requires a strictly increasing
  curve, and a non-increasing one would make SOC ambiguous.
- **Non-negative resistances**, $R_0, R_1 \ge 0$.
- **Smoothness.** A penalty on the second difference of the OCV,
  $\lambda \lVert D_2 L u \rVert^2$, fills SOC ranges without data and damps
  noise. $\lambda$ is `--smoothing` scaled by $\sqrt{N/n}$ (samples over grid
  points), so its effect doesn't depend on the log length.

The result is a bounded linear least-squares problem, solved exactly with
BVLS (`scipy.optimize.lsq_linear`).

### Nonlinear parameters

$\tau_1$ and $\kappa$ enter nonlinearly. For each candidate the linear
problem is solved and its residual kept, which is variable projection: the
outer search only explores one or two dimensions.

- $\tau_1$: bounded 1-D minimization of the mean squared residual over
  $\ln \tau_1 \in [\ln 5, \ln 5000]$ s.
- $\kappa$: when the logs span at least 10 °C, a bounded 1-D minimization
  over $[0, 0.08]$ 1/°C, each candidate with its best $\tau_1$. Otherwise
  it is fixed at 0.03 1/°C, a typical value for Li-ion cells.

### Output

The fit writes the model file and prints the parameters, plus the voltage
residual $v_k - \hat v_k$ (RMS and maximum). Grid points that no sample
falls next to are reported, since their OCV comes only from the smoothness
penalty. The default grid is 21 evenly spaced points (`--grid-points`, or
`--grid` for explicit points, e.g. denser near empty where the curve bends).

### Why pulses

A constant-current discharge can't separate the OCV from the resistive
drops: at a constant $i$, $R_0 i$ and $R_1 i$ are just offsets on the OCV.
Pulses with rests make every parameter visible:

- the **step** at each pulse edge is $R_0 s(T)\, \Delta i$;
- the **relaxation** after each pulse is $R_1 s(T)\, i\, e^{-t/\tau_1}$,
  which gives $R_1$ and $\tau_1$;
- the **end of each rest** is close to the OCV at that SOC, as long as the
  rest is several $\tau_1$ long.

Each pulse removes about 2% of the capacity, giving around 50 OCV points,
more than the 21-point grid needs. With $\tau_1$ typically 100-200 s, a
15-minute rest relaxes 5-9 time constants.

## Bench setup

### Hardware

- **nPM1300-EK**: the nPM1300 charger plus an interface MCU exposing a
  shell over USB. It has two USB ports: one for the interface MCU (the
  shell, always connected) and one for the PMIC's VBUS (the charger input).
- **nPM-FG**: the active load board, attached to the EK. It sinks a
  programmed current and reports the load current and voltage.
- **The cell**, on the EK's battery connector. If the pack has a thermistor
  wire, check it before connecting: it should read about 10 kΩ to the
  negative terminal at room temperature (the watches use a 10 kΩ,
  β = 3380 NTC). Wire colours are not standardized.

Characterize at a stable room temperature, away from heat sources. Without
an NTC, pass the ambient temperature with `--temp`.

### Shell interface

`tools/fuel_gauge_bench.py` talks to the EK shell over the interface MCU's
USB serial port. Commands reply with `Success: ...` or `Value: ...`, and
measurements arrive as log lines from `module_pmic_adc` (charger ADC) and
`module_cc_profiling` (load). The commands used:

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

### Procedure

`tools/fuel_gauge_bench.py` runs:

1. **Reset** the PMIC and restart the EK application. The charger latches
   errors, e.g. after swapping to a flat or badly seated cell, and refuses
   to charge until reset.
2. **Configure** the charger with the cell's charge voltage, current and
   termination current, and the thermistor.
3. **Charge** (VBUS connected, prompted before the reset, which also powers
   the PMIC when the cell is flat): CC/CV until the charger reports full,
   logging the charger ADC once a second to `<output>_charge.csv`.
4. **Discharge** (VBUS disconnected, prompted) with the nPM-FG:
   - 30 minutes of rest (`--initial-rest`), so the log starts from a full,
     rested cell;
   - pulses of 0.5C (`--rate`) for 144 s (`--pulse`, 2% of the capacity),
     each followed by 15 minutes of rest (`--rest`), until the loaded
     voltage falls below the cutoff plus 0.2 V (`--tail-margin`);
   - then pulses at a quarter of the current, four times as long, down to
     the cutoff (`--cutoff`). The lower current gets closer to the charge
     a light load like the watch's can draw before the cutoff.

   This is logged at 1 Hz to `<output>`, timed with the EK's log
   timestamps, with the current made positive for discharge.

The settings are saved next to the log as `<output>.json`. The discharge
takes about 15 hours, plus up to two hours of charging. The script holds the
serial port exclusively, so nothing else can talk to the EK during a run.

### Cell settings

Use the cell's rated capacity and the same charge settings as the watch
(`struct pmic_config` in `fw/board/boards/board_<board>.c`), so that "full"
on the bench is "full" on the watch.

| Board   | Cell | Capacity | Charge voltage | Charge current | Termination |
|---------|------|----------|----------------|----------------|-------------|
| asterix | C2D  | 130 mAh  | 4.20 V         | 128 mA         | 10%         |
| obelix  | PT2  | 185 mAh  | 4.35 V         | 190 mA         | 10%         |
| getafix | PTR2 | 70 mAh   | 4.45 V         | 70 mA          | 10%         |

Use a 3.0 V cutoff: the firmware considers the battery empty well above it,
and the OCV curve needs to cover the whole range.

```shell
python tools/fuel_gauge_bench.py /dev/cu.usbmodemXXXX obelix_25c.csv \
    --capacity-mah 185 --vterm 4.35 --ichg 190 --ntc 10k
```

The EK exposes two serial ports; the shell is the one that answers. Use
`--skip-charge` to go straight to the discharge with a cell that is already
full, and pass `--ntc none --temp <C>` for a cell without thermistor.

### Troubleshooting

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

## Fitting and checking

```shell
M=fw/services/battery/fuel_gauge/models/getafix.c
python tools/fuel_gauge.py fit getafix_25c.csv --name getafix -o $M \
    --grid 0 0.01 0.02 0.03 0.05 0.075 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 \
    0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1
python tools/fuel_gauge.py replay $M getafix_25c.csv --start-soc 1 --plot soc.png
```

The grid is denser near empty, where the OCV bends sharply; the default
evenly spaced grid fits the end of the discharge noticeably worse.

To fit the temperature coefficient, run the bench procedure at other
temperatures too (e.g. 5 °C and 40 °C, in a climate chamber) and pass all
logs to `fit`. The first log defines the capacity, so put the 25 °C one
first.

What to look at:

- **Residual.** Around 5-10 mV RMS is a good fit. Large maximum residuals
  tend to sit at the very end of the discharge, where the OCV bends sharply;
  a denser grid there (`--grid`) helps.
- **Coverage warnings.** Any SOC range without data means the procedure
  didn't reach it, typically near full (charge not completing) or near
  empty (cutoff too high).
- **Replay.** `replay --start-soc 1` compares the estimator against coulomb
  counting on the same log. It checks the model and the estimator together,
  but on the data the model was fitted to; a second cell of the same type,
  or a log from a watch, is a stronger check.
- **Parameters.** $R_0$ and $R_1$ of a few hundred milliohms up to a couple
  of ohms are normal for small cells, $\tau_1$ of tens to a few hundred
  seconds.

`synth` simulates the bench procedure on an existing model, which shows how
well a schedule identifies the parameters without running it:

```shell
python tools/fuel_gauge.py synth $M synth.csv --lsb 0.0049
python tools/fuel_gauge.py fit synth.csv --name test -o fit.c
```

The tools need numpy and scipy, matplotlib for plots, pyserial for the bench
script, and a host C compiler for `replay` and `synth`.

## Limitations of bench data

- **Where the voltage is measured.** The nPM-FG measures the voltage on the
  EK, not at the cell terminals, so connector, wiring and PCB resistance
  between the cell and that point end up in $R_0$. The watch measures at its
  own PMIC, with its own path, so the fitted $R_0$ can be somewhat higher
  than the watch sees. Replaying watch logs is the way to check it, see
  below.
- **One temperature.** Without logs at several temperatures, $\kappa$ is the
  default 0.03 1/°C, and only the resistances follow temperature anyway.
- **No hysteresis.** The procedure only discharges. The OCV after charging
  sits slightly higher than after discharging; the model has a single
  curve.
- **Rate-dependent capacity.** The capacity is what the bench schedule
  draws down to the cutoff. A lighter load gets a little more out of the
  cell; the low-current tail of the schedule reduces the difference.
- **Aging.** One fresh cell is measured, and capacity fade isn't modelled.

## Checking on a watch

Build the firmware with the open fuel gauge and verbose battery logs, and
record the console through a full charge and a full discharge:

```shell
pbl configure --board getafix@dvt2 \
    -DCONFIG_BATTERY_SOC_FUEL_GAUGE=y \
    -DCONFIG_SERVICE_BATTERY_LOG_LEVEL_DEBUG_VERBOSE=y
pbl build
script -q watch.log pbl console
```

The console's log level filters verbose messages out, so send
`log level pulse 255` after every boot to see the battery state lines.
Release builds have no console, so use a debug build. `extract` turns the
`Battery state` lines into a CSV log that `replay` takes like a bench log:

```shell
python tools/fuel_gauge.py extract watch.log watch.csv
python tools/fuel_gauge.py replay $M watch.csv -o soc.csv
```

Compare the replayed SOC with the charge counted from a known point, such as
charger termination (full) or the shutdown at 0%. Things to look for:

- **Charging lag.** If the SOC lags the charged amount during constant
  current charging, $R_0$ is too high for the watch, typically because of
  the bench wiring. Lower it until the replay tracks; the getafix model
  went from the fitted 4.37 Ω to 3 Ω this way.
- **Discharge.** At watch currents of a few milliamps, the resistances
  barely matter and the OCV curve dominates. A level stuck at 100% after
  unplugging means the curve tops out below the watch's full voltage, e.g.
  because the bench charge didn't complete.
- **Absolute current.** At a few milliamps, the PMIC's current readings are
  coarse and read high, so counting with them overestimates the charge
  drawn. Normalize to a known start and end rather than trusting the total.
