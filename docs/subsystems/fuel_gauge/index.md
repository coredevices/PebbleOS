# Fuel gauge

`subsys/fuel_gauge/` estimates the state of charge (SOC) of a single Li-ion
or Li-po cell from voltage, current and temperature measurements. Its API is
`include/pbl/fuel_gauge/fuel_gauge.h`. The library is plain C over `<math.h>`
with no OS dependencies: the battery service feeds it measurements, and
`tools/fuel_gauge/fuel_gauge.py` builds the same file for the host to replay
logs through it.

It is an open alternative to the prebuilt Nordic nRF Fuel Gauge library. The
battery service selects the estimator with the `BATTERY_SOC` choice:

- `CONFIG_BATTERY_SOC_VOLTAGE`: voltage curve lookup (QEMU and older boards)
- `CONFIG_NRF_FUEL_GAUGE`: Nordic library, the default with the nPM1300
- `CONFIG_BATTERY_SOC_FUEL_GAUGE`: this estimator, with the cell model
  `fw/services/battery/fuel_gauge/models/<name>.c` named by
  `CONFIG_BATTERY_SOC_FUEL_GAUGE_MODEL`, which the board's defconfig sets

[Cell characterization](characterization.md) describes how a cell
model is measured and fitted.

```{toctree}
:maxdepth: 1

characterization.md
npm1300_ek.md
watch_logs.md
```

## Cell model

The cell is an equivalent circuit: an open-circuit voltage (OCV) that depends
on the SOC, in series with a resistor $R_0$ and one resistor-capacitor pair
$R_1 \parallel C_1$.

```{eval-rst}
.. plot::
   :filename-prefix: fuel-gauge-circuit

   from fuel_gauge_plots import draw_circuit
   fig, ax = plt.subplots(figsize=(5, 2.4))
   draw_circuit(ax)
```

With $z$ the SOC (0 to 1), $i$ the current (positive when discharging), $Q$
the capacity in coulombs and $v_1$ the voltage across the RC pair, the
terminal voltage $v$ follows

$$
\begin{aligned}
\dot z &= -\frac{i}{Q} \\
\dot v_1 &= -\frac{v_1}{\tau_1} + \frac{i}{C_1}, \qquad \tau_1 = R_1 C_1 \\
v &= \mathrm{OCV}(z) - v_1 - R_0\, i
\end{aligned}
$$

- $\mathrm{OCV}(z)$ is the voltage of the cell after a long rest. It is a
  table of points, interpolated linearly, that must increase with $z$.
- $R_0$ is the instantaneous drop when the current steps: electrolyte,
  contacts, tabs and anything else in series up to where the voltage is
  measured.
- $R_1 \parallel C_1$ is the slower polarization (charge transfer and
  diffusion). It builds up under load and relaxes with the time constant
  $\tau_1$ once the load is removed.

A single RC pair keeps the model small and its fit well conditioned. The
battery service samples once a minute, which can't resolve faster dynamics
anyway.

This is the OCV curve of the getafix model:

```{eval-rst}
.. plot::
   :filename-prefix: fuel-gauge-ocv

   from fuel_gauge_plots import plot_ocv
   fig, ax = plt.subplots(figsize=(8, 3.2))
   plot_ocv(ax)
```

(fuel-gauge-discretization)=

### Discretization

Samples come at times $t_k$, with $\Delta t_k = t_k - t_{k-1}$. The current
$i_k$ is a single sample taken at $t_k$, and it is held over the whole
interval $(t_{k-1}, t_k]$. For a constant current, the RC equation has the
exact solution

$$
v_{1,k} = a_k\, v_{1,k-1} + R_1 (1 - a_k)\, i_k,
\qquad a_k = e^{-\Delta t_k / \tau_1}
$$

and coulomb counting gives

$$
z_k = z_{k-1} - \frac{i_k\, \Delta t_k}{Q}.
$$

The estimator and the fit use these same equations, so a model fitted on
bench logs behaves the same way in the firmware, whatever the sample interval.

### Temperature

Resistances grow as the cell gets colder. Both $R_0$ and $R_1$ are scaled by

$$
s(T) = e^{\kappa\,(25 - T)}
$$

with $T$ in °C, clamped to -20..60 °C, and $\kappa$ the resistance
temperature coefficient. The OCV and the capacity don't depend on
temperature.

`struct pbl_fuel_gauge_model` in the header holds these parameters.

## Estimator

The estimator is an extended Kalman filter over the state
$x = [z, v_1]^T$ and its covariance $P$. Each measurement
$(v_k, i_k, T_k)$ goes through a prediction and a correction.

**Predict** (coulomb counting), with $g_k$ the sensitivity of the state to
the current:

$$
\begin{aligned}
x_k^- &= \begin{bmatrix} z_{k-1} \\ a_k\, v_{1,k-1} \end{bmatrix} + g_k\, i_k,
\qquad g_k = \begin{bmatrix} -\Delta t_k / Q \\ R_1 s(T_k) (1 - a_k) \end{bmatrix} \\
P_k^- &= F_k P_{k-1} F_k^T + g_k g_k^T \sigma_i^2,
\qquad F_k = \begin{bmatrix} 1 & 0 \\ 0 & a_k \end{bmatrix}
\end{aligned}
$$

The process noise comes only from the current error $\sigma_i$
(`current_noise`). Both states are driven by the same current, so its
covariance is the outer product of $g_k$ with itself. $\sigma_i$ is how far
the single sample $i_k$ can be from the true average current of the
interval. The battery service samples once a minute while the watch load is
bursty (backlight, vibration, radio), so this dominates over the ADC error.

**Correct** (voltage). The predicted terminal voltage and its linearization:

$$
\hat v_k = \mathrm{OCV}(z_k^-) - v_{1,k}^- - R_0\, s(T_k)\, i_k,
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

How much the correction moves the SOC depends on the OCV slope
$\mathrm{OCV}'$, but the gain on $z$ isn't proportional to it. Ignoring
$v_1$, the gain is
$K_z = P_{zz}\,\mathrm{OCV}' / (\mathrm{OCV}'^2 P_{zz} + \sigma_v^2)$: it
first rises with the slope, then falls as $1/\mathrm{OCV}'$ where the curve
is steep, since there a voltage error is a small SOC error. What rises with
the slope is $H K$, the fraction of the voltage innovation the state
absorbs. With $\sqrt{P_{zz}} = 2\%$ and $\sigma_v = 15$ mV on the getafix
model:

| Where | $\mathrm{OCV}'$ (V) | $K_z$ (1/V) | $H K$ |
|---|---|---|---|
| plateau, 7.5-10% | 0.43 | 0.58 | 0.25 |
| near full, 95-100% | 1.1 | 0.62 | 0.68 |
| near empty, 0-1% | 26.6 | 0.038 | 1.00 |

So on the flat plateau the filter mostly keeps the coulomb count, and where
the curve is steep it follows the voltage almost entirely. Current gain and
offset errors, which make pure coulomb counting drift without bound, are
corrected continuously.

**Charge complete.** When the charger terminates, the SOC is set to 1 with a
small uncertainty.

**Reported SOC.** `pbl_fuel_gauge_update()` returns the SOC to show. While
the estimate is still converging (SOC standard deviation above 2%, e.g.
right after starting from the voltage), it follows the estimate. After that,
it doesn't rise while discharging or drop while charging, so the percentage
never moves against the direction the battery is going. The raw estimate is
available through `pbl_fuel_gauge_soc_get()`.

## Time to empty and time to full

Both use an exponentially weighted average current $\bar\imath$ (3 h time
constant). It restarts whenever the charge state changes, and is reported
once it covers 10 minutes.

- **Time to empty** is the reported SOC times the capacity, divided by the
  average discharge current.
- **Time to full** follows the charger. In constant current (CC), the
  charger switches to constant voltage (CV) when
  $\mathrm{OCV}(z) + \bar\imath\,(R_0 + R_1)\,s(T)$ reaches the termination
  voltage. Inverting the OCV table gives the SOC where that happens, and the
  CC time is the charge up to it divided by the current. In CV, the current
  decays roughly exponentially from $i_0$ to the termination current
  $i_\text{term}$, which delivers a charge $q$ in
  $\frac{q}{i_0 - i_\text{term}} \ln \frac{i_0}{i_\text{term}}$.

## Persistence

`pbl_fuel_gauge_state_get()` returns a `struct pbl_fuel_gauge_state` that
the battery service stores in the `fgs` settings file (or the MFG battery
state region). `pbl_fuel_gauge_init()` resumes from it unless one of these
holds:

- It was produced with another model or state layout: `model_id` is a hash
  of both.
- Its SOC disagrees by more than 25% with the SOC implied by the voltage.
  That happens after a battery swap, or a long time without power.
- The OCV of its reported SOC is more than 50 mV below the voltage,
  compensated for the load ($v + v_1 + R_0\, s(T)\, i$). The reported SOC
  doesn't rise while discharging, so such a level would stay too low, e.g. a
  0% saved on a cell that then got some charge back.

In these cases it starts over from the voltage.

## Limitations

- The OCV is modelled at 25 °C, and the capacity doesn't depend on
  temperature. Only the resistances do.
- $R_0$ and $R_1$ don't depend on the SOC, while those of a real cell rise
  near empty. Close to empty, the model then underestimates the drop under
  load.
- There is no OCV hysteresis between charge and discharge. The fitted curve
  sits between the two branches when the logs include charging.
- Capacity fade with aging is not tracked.
