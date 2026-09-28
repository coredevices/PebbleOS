# Fuel gauge

`subsys/fuel_gauge/` estimates the state of charge (SOC) of a single Li-ion
or Li-po cell from voltage, current and temperature measurements. Its API is
`include/pbl/fuel_gauge/fuel_gauge.h`. The library is plain C over `<math.h>`
with no OS dependencies: the caller feeds it measurements.

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

## Limitations

- The OCV is modelled at 25 C, and the capacity doesn't depend on
  temperature. Only the resistances do.
- There is no OCV hysteresis between charge and discharge. The fitted curve
  sits between the two branches when the logs include charging.
- Capacity fade with aging is not tracked.
