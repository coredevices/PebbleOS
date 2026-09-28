# Thevenin

`FUEL_GAUGE_THEVENIN` estimates the SOC with an extended Kalman filter over a
Thevenin equivalent circuit: the cell's $R_0$ in series with one RC
polarization pair. Coulomb counting predicts the SOC, and the measured
terminal voltage corrects it, so the estimate doesn't drift with current
errors nor jump with the load.

## Model

The cell is an equivalent circuit: the OCV of the {ref}`cell <fuel-gauge-cell>`, in
series with its resistor $R_0$ and one resistor-capacitor pair
$R_1 \parallel C_1$.

```{figure} thevenin_circuit.svg
:align: center
:alt: OCV source in series with R0 and the R1 parallel C1 pair

Thevenin equivalent circuit of the cell, with the current $i$ positive when
discharging.
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

$R_1 \parallel C_1$ is the slower polarization (charge transfer and
diffusion). It builds up under load and relaxes with the time constant
$\tau_1$ once the load is removed. Without the pair ($R_1 = 0$), the model
is $R_0$ alone.

A single RC pair keeps the model small and its fit well conditioned.
Dynamics much faster than the sample interval can't be resolved anyway, so
more pairs only pay off with faster sampling.

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

## Filter

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
interval. With sparse samples and a bursty load, this dominates over the
error of the current measurement itself.

**Correct** (voltage). The predicted terminal voltage and its linearization:

$$
\hat v_k = \mathrm{OCV}(z_k^-) - v_{1,k}^- - R_0\, s(T_k)\, i_k,
\qquad H_k = \begin{bmatrix} \mathrm{OCV}'(z_k^-) & -1 \end{bmatrix}
$$

$$
\begin{aligned}
e_k &= v_k - \hat v_k \\
S_k &= H_k P_k^- H_k^T + \sigma_v^2 \\
K_k &= P_k^- H_k^T / S_k \\
x_k &= x_k^- + K_k\, e_k \\
P_k &= P_k^- - K_k S_k K_k^T
\end{aligned}
$$

$e_k$ is how far the measured voltage is from the predicted one, and $S_k$
its expected variance. $\sigma_v$ (`voltage_noise`) covers the voltage
measurement error and the model error.

How much the correction moves the SOC depends on the OCV slope
$\mathrm{OCV}'$, but the gain on $z$ isn't proportional to it. Ignoring
$v_1$, the gain is
$K_z = P_{zz}\,\mathrm{OCV}' / (\mathrm{OCV}'^2 P_{zz} + \sigma_v^2)$: it
first rises with the slope, then falls as $1/\mathrm{OCV}'$ where the curve
is steep, since there a voltage error is a small SOC error. What rises with
the slope is $H K$, the fraction of the voltage error $e_k$ the
correction removes. With $\sqrt{P_{zz}} = 2\%$ and $\sigma_v = 15$ mV, on
the OCV curve of a small Li-po cell charged to 4.45 V:

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

## Parameters

`struct pbl_fuel_gauge_estimator` holds them. In the firmware they come from
Kconfig, through `PBL_FUEL_GAUGE_ESTIMATOR_KCONFIG`:

| Parameter | Kconfig |
|---|---|
| $R_1$ | `FUEL_GAUGE_THEVENIN_RC_MILLIOHM` |
| $\tau_1$ | `FUEL_GAUGE_THEVENIN_RC_TAU_MS` |
| $\kappa$ | `FUEL_GAUGE_THEVENIN_R_TEMP_COEFF_PPM` |
| $\sigma_v$ | `FUEL_GAUGE_THEVENIN_VOLTAGE_NOISE_UV` |
| $\sigma_i$ | `FUEL_GAUGE_THEVENIN_CURRENT_NOISE_UA` |

## Limitations

- The OCV is modelled at 25 °C, and the capacity doesn't depend on
  temperature. Only the resistances do.
- $R_0$ and $R_1$ don't depend on the SOC, while those of a real cell rise
  near empty. Close to empty, the model then underestimates the drop under
  load.
- There is no OCV hysteresis between charge and discharge. The fitted curve
  sits between the two branches when the logs include charging.
- Capacity fade with aging is not tracked.
