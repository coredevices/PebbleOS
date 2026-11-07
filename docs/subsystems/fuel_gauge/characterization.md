# Fuel gauge cell characterization

This page describes how to measure a cell and fit the
[fuel gauge](index.md)'s cell model to it: what to measure, with which
instrument, and how to check the result. Models come only from our own
measurements. The method doesn't depend on an instrument; the tools that
implement it are listed at the end.

The flow is:

1. Charge the cell as the product does, rest it, and discharge it in pulses
   down to the cutoff, logging voltage, current and temperature.
2. Fit the model to the log with `tools/fuel_gauge/fuel_gauge.py fit`, which
   writes `fw/services/battery/fuel_gauge/models/<name>.c`.
3. Check it, with `fuel_gauge.py replay` on the bench log, then on a second
   cell or on logs from a watch.

## Full and empty

The model's SOC runs from 0 to 1 between two points that the procedure
defines, so they have to be the product's.

- **Full** ($z = 1$) is where the charger terminates. Charge with the
  product's charge profile: the same termination voltage, charge current and
  termination current as the watch (`Npm1300Config NPM1300_CONFIG` in
  `fw/board/boards/board_<board>.c` on nPM1300 boards). A different profile
  moves "full" and changes the capacity.
- **Empty** ($z = 0$) is where the discharge stops: the cutoff voltage, under
  the load of the last pulses. The estimator reports 0% at $z = 0$, and the
  battery service treats 0% while unplugged as critical, so the cutoff
  defines where the watch reports empty. Choosing it is a product decision:
  a lower cutoff gives more runtime, a higher one leaves more margin before
  the cell's protection or the PMIC's brownout.

The capacity $Q$ is the charge delivered between these two points, at the
procedure's discharge rates.

| Board   | Cell | Capacity | Charge voltage | Charge current | Termination |
|---------|------|----------|----------------|----------------|-------------|
| getafix | PTR2 | 70 mAh   | 4.45 V         | 70 mA          | 10%         |

The getafix model uses a 3.0 V cutoff.

(fuel-gauge-instrument)=

## Instrument

The procedure needs a programmable load and a charger (or a way to swap
between them), and logs of the following, with the reasons:

- **Voltage at the cell.** Everything between the cell and the voltage
  sense point ends up in $R_0$. Use 4-wire sensing at the cell terminals, or
  measure the fixture's resistance $R_\text{fixture}$ between the cell and
  the sense point. The watch measures at its own PMIC, through its own path
  $R_\text{watch path}$, so the model's resistance is
  $$
  R_{0,\text{model}} = R_{0,\text{fit}} - R_\text{fixture} + R_\text{watch path}.
  $$
  Resolution of a few millivolts is enough: the fit residual of a good model
  is 5-10 mV.
- **Current, with a known error.** The fit takes the SOC from coulomb
  counting, so a current error becomes a capacity error, and through it an
  error on the SOC of every OCV point. With a gain error $\epsilon_i$ and an
  offset $i_\text{off}$ over a test of length $T$:
  $$
  \frac{\delta Q}{Q} \approx \epsilon_i + \frac{i_\text{off}\, T}{Q}.
  $$
  The offset counts over the whole test, rests included, which are most of
  it. For the getafix cell, 70.9 mAh over a 15 h test, a 1% capacity error
  allows only about 47 µA of offset. Check the instrument's specification
  against this budget, and check the current logged during the rests, which
  should read 0.
- **Sample rate** of about 1 Hz. The fit sees $R_0$ in the voltage step at
  each pulse edge and $\tau_1$ in the relaxation after it, which takes tens
  of seconds; the samples have to resolve both.
- **Temperature** of the cell, or at least of the ambient, since the
  resistances depend on it. Characterize at a stable temperature, away from
  heat sources.

## Test profile

A constant-current discharge can't separate the OCV from the resistive
drops: at a constant $i$, $R_0 i$ and $R_1 i$ are just offsets on the OCV.
Pulses with rests make every parameter visible:

```{eval-rst}
.. plot::
   :filename-prefix: fuel-gauge-pulse

   from fuel_gauge_plots import plot_pulse
   fig, ax = plt.subplots(figsize=(8, 3.2))
   plot_pulse(ax)
```

- The **step** at each pulse edge is $R_0\, s(T)\, \Delta i$.
- The **relaxation** after a pulse of current $i$ and length $t_p$ is
  $$
  R_1\, s(T)\, i\, \left(1 - e^{-t_p/\tau_1}\right) e^{-t/\tau_1},
  $$
  which gives $R_1$ and $\tau_1$. The RC pair only charges to
  $1 - e^{-t_p/\tau_1}$ of $R_1 s(T) i$ during the pulse: 89% for a 144 s
  pulse at $\tau_1 = 65$ s, 51% at $\tau_1 = 200$ s.
- The **end of each rest** is close to the OCV at that SOC, as long as the
  relaxation has died out.

The profile's parameters, and what sets them:

| Parameter | Default | Why |
|---|---|---|
| Initial rest | 30 min | The log has to start from a full cell at rest: right after charging, the polarization of the CV phase is still relaxing. |
| Pulse depth | 2% of $Q$ | Each pulse gives one OCV point, so 2% gives about 50, at least two per interval of a 21-25 point OCV grid. Shallower pulses lengthen the test. |
| Pulse rate | 0.5C | The step $R_0 \Delta i$ has to stand well above the voltage resolution (106 mV for getafix), without heating the cell. |
| Pulse length | depth / rate, 144 s | At least about $\tau_1$, so the RC pair charges enough for its relaxation to show. |
| Rest | until relaxed | See below. |
| Tail | quarter rate, 4× long | Near the cutoff, from 0.2 V above it under load: a lighter load draws more charge before the cutoff, closer to the watch's. |

**Rest criterion.** A rest should end once the remaining relaxation is
below the error the OCV points can tolerate. For an exponential, the
voltage still to relax is $\tau_1\, |dv/dt|$, so with $\tau_1$ up to 200 s,
leaving less than 1 mV takes $|dv/dt| < 5$ µV/s, about 0.3 mV/min. Measure
the slope over a few minutes, as it is below the voltage resolution over a
single second. An instrument that can't end a step on a condition needs a
fixed rest long enough for the slowest expected $\tau_1$: 15 minutes is 4.5
to 9 time constants for $\tau_1$ of 100 to 200 s, and about 14 for the
getafix cell's 65 s.

The discharge takes about 15 hours, plus the charge.

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
row, like on the watch.

## Fit

```shell
M=fw/services/battery/fuel_gauge/models/getafix.c
python tools/fuel_gauge/fuel_gauge.py fit getafix_25c.csv --name getafix -o $M \
    --grid 0 0.01 0.02 0.03 0.05 0.075 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 \
    0.5 0.55 0.6 0.65 0.7 0.75 0.8 0.85 0.9 0.95 1
```

By default each log starts full (`--start-soc 1`), and the first log ends
empty (`--end-soc 0`), which defines the capacity. `--capacity-ah` sets it
instead. The fit prints the parameters, the voltage residual, and a warning
for any SOC range the logs don't cover. `fit --help` has the options.

To fit the temperature coefficient, run the procedure at other temperatures
too (e.g. 5 °C and 40 °C, in a climate chamber) and pass all logs to `fit`.
The first log defines the capacity, so put the 25 °C one first.

### Known SOC path

The fit needs the SOC at every sample, which coulomb counting provides when
the log starts from a known state:

$$
z_k = z_0 - \frac{1}{Q} \sum_{m \le k} i_m\, \Delta t_m.
$$

A log that starts full ($z_0 = 1$) and ends at the cutoff
($z_\text{end} = 0$) defines the capacity:

$$
Q = \frac{\sum_k i_k\, \Delta t_k}{z_0 - z_\text{end}}.
$$

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
v_k = \sum_j w_j(z_k)\, \mathrm{OCV}_j - R_0\, s(T_k)\, i_k - R_1\, \ell_k
$$

with $\ell$ the first-order lag of the
{ref}`discretization <fuel-gauge-discretization>`,
$\ell_k = a_k \ell_{k-1} + (1 - a_k)\, s(T_k)\, i_k$. Stacking all $N$
samples gives an overdetermined linear system $A\theta \approx v$ in
$\theta = [\mathrm{OCV}_1 \dots \mathrm{OCV}_n, R_0, R_1]$.

Three constraints make the solution physical:

- **Increasing OCV.** The OCV points are written as a base voltage plus
  steps, $\mathrm{OCV}_j = u_1 + \sum_{m=2}^{j} u_m$, i.e.
  $\mathrm{OCV} = T u$ with $T$ a lower-triangular matrix of ones, and every
  step $u_m \ge 0.1$ mV. The estimator requires a strictly increasing
  curve, and a non-increasing one would make the SOC ambiguous.
- **Non-negative resistances**, $R_0, R_1 \ge 0$.
- **Smoothness.** A penalty $\lambda \lVert D_2 T u \rVert^2$ on the second
  difference of the OCV fills SOC ranges without data and damps noise, with
  $\lambda = \text{smoothing}^2 N / n$ (`--smoothing`). The data term grows
  with $N$, so scaling $\lambda$ with it keeps the penalty's effect
  independent of the log length.

### Nonlinear parameters

$\tau_1$ and $\kappa$ enter nonlinearly. For each candidate, the linear
problem is solved and its residual kept (variable projection), so the outer
search only explores one or two dimensions:

- $\tau_1$: bounded 1-D minimization of the mean squared residual over
  $\ln \tau_1 \in [\ln 5, \ln 5000]$ s.
- $\kappa$: when the logs span at least 10 °C, a bounded 1-D minimization
  over $[0, 0.08]$ 1/°C, each candidate with its best $\tau_1$. Otherwise it
  is fixed at 0.03 1/°C, a typical value for Li-ion cells.

### Grid

The default grid is 21 evenly spaced points (`--grid-points`, or `--grid`
for explicit points). The OCV bends sharply near empty, so a grid denser
there, like above, fits the end of the discharge noticeably better. Grid
points that no sample falls next to are reported, since their OCV comes only
from the smoothness penalty.

## Validate

- **Residual.** Around 5-10 mV RMS is a good fit. Large maximum residuals
  tend to sit at the very end of the discharge, where the OCV bends sharply;
  a denser grid there helps.
- **Coverage warnings.** An SOC range without data means the procedure
  didn't reach it, typically near full (charge not completing) or near
  empty (cutoff too high).
- **Parameters.** $R_0$ and $R_1$ of a few hundred milliohms up to a few
  ohms are normal for small cells, $\tau_1$ of tens to a few hundred
  seconds.
- **Replay.** `replay --start-soc 1` runs the firmware estimator over the
  log and compares it against coulomb counting:

  ```shell
  python tools/fuel_gauge/fuel_gauge.py replay $M getafix_25c.csv --start-soc 1 --plot soc.png
  ```

  This checks the model and the estimator together, but on the data the
  model was fitted to. A second cell of the same type, or
  [logs from a watch](watch_logs.md), is a stronger check.

`synth` simulates the bench procedure on an existing model, quarter-rate
tail included, which shows how well a profile identifies the parameters
without running it:

```shell
python tools/fuel_gauge/fuel_gauge.py synth $M synth.csv --lsb 0.0049
python tools/fuel_gauge/fuel_gauge.py fit synth.csv --name test -o fit.c
```

On simulated watch usage (one sample a minute of a 1 mA load with 20 mA
bursts, 5 mV voltage resolution), a model that matches the cell keeps the
SOC within about 1%. That still holds with a ±10% current gain error or a
±0.3 mA offset. A start 20% off converges within minutes. On a real cell,
the model error dominates, so the fit residual is the number to watch.

The tools need numpy and scipy, matplotlib for plots, and a host C compiler
for `replay` and `synth`.

## Limitations of the method

- **Resistances from discharge pulses.** $R_0$ and $R_1$ are fitted on 0.5C
  discharge pulses, and the model uses them for charging at up to 1C too.
  Charge and discharge resistances differ, and so may the resistance at
  other rates, and the fixture resistance adds to the fitted $R_0$ unless it
  is measured. The model's $R_0$ is the one to check on watch charge logs.
- **Constant resistances.** $R_0$ and $R_1$ are fitted as one value over the
  whole SOC range, while a real cell's rise near empty, so the fit there
  trades resistance error for OCV error.
- **One temperature.** Without logs at several temperatures, $\kappa$ is
  the default 0.03 1/°C.
- **No hysteresis.** The procedure only discharges. The OCV after charging
  sits slightly higher than after discharging; the model has a single
  curve.
- **Rate-dependent capacity.** The capacity is what the profile draws down
  to the cutoff. A lighter load gets a little more out of the cell; the
  low-current tail reduces the difference.
- **Aging.** One fresh cell is measured, and capacity fade isn't modelled.

## Tools

- [nPM1300-EK](npm1300_ek.md): runs the whole procedure on an
  nPM1300 evaluation kit with an nPM-FG load board.
- [Watch logs](watch_logs.md): logs from a watch's own
  measurements, to check a model, or fit a coarser one.
