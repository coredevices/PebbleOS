# Introduction

The fuel gauge estimates the SOC with a model of the cell, chosen at build
time. The rest is shared by all models: the description of the cell, the
reported level, the time estimates and the saved state.

## Features

- the SOC, raw or as a reported level that only moves in the direction the
  battery is going
- the time to empty, and the time to full through constant-current and
  constant-voltage charging
- a state to save and restore across reboots, discarded when it no longer
  matches the cell

(fuel-gauge-cell)=

## Cell

The cell is described by its capacity $Q$, its open-circuit voltage (OCV)
curve and its series resistance $R_0$, all at 25 °C
(`struct pbl_fuel_gauge_cell`):

- $\mathrm{OCV}(z)$ is the voltage of the cell after a long rest at SOC $z$
  (0 to 1). It is a table of points, interpolated linearly, that must
  increase with $z$, from 0 (empty) to 1 (full).
- $R_0$ is the instantaneous drop when the current steps: electrolyte,
  contacts, tabs and anything else in series up to where the voltage is
  measured.

Anything else, such as polarization or how resistances change with
temperature, is up to the [model](models/index.md) the SOC is estimated
with, and is one of its parameters.

## Reported SOC

`pbl_fuel_gauge_update()` returns the SOC to show. While the estimate is
still converging (its standard deviation above 2%, e.g. right after starting
from the voltage), it follows the estimate. After that, it doesn't rise while
discharging or drop while charging, so the percentage never moves against
the direction the battery is going. When the charger terminates, it is 100%.
The raw estimate is available through `pbl_fuel_gauge_soc_get()`.

## Time to empty and time to full

Both use an exponentially weighted average current $\bar\imath$ (3 h time
constant). It restarts whenever the charge state changes, and is reported
once it covers 10 minutes.

- **Time to empty** is the reported SOC times the capacity, divided by the
  average discharge current.
- **Time to full** follows the charger. In constant current (CC), the
  charger switches to constant voltage (CV) when
  $\mathrm{OCV}(z) + \bar\imath\,R_\text{dc}(T)$ reaches the termination
  voltage, with $R_\text{dc}(T)$ the resistance the estimator gives for a
  constant current. Inverting the OCV table gives the SOC where that
  happens, and the CC time is the charge up to it divided by the current. In
  CV, the current decays roughly exponentially from $i_0$ to the termination
  current $i_\text{term}$, which delivers a charge $q$ in
  $\frac{q}{i_0 - i_\text{term}} \ln \frac{i_0}{i_\text{term}}$.

## Persistence

`pbl_fuel_gauge_state_get()` returns a `struct pbl_fuel_gauge_state` for the
caller to store, and `pbl_fuel_gauge_init()` resumes from it unless one of
these holds:

- It was produced with another cell, estimator parameters or state layout:
  `model_id` is a hash of all three.
- Its SOC disagrees by more than 25% with the SOC implied by the voltage.
  That happens after a battery swap, or a long time without power.
- The OCV of its reported SOC is more than 50 mV below the voltage with the
  load removed, as the estimator computes it. The reported SOC doesn't rise
  while discharging, so such a level would stay too low, e.g. a 0% saved on
  a cell that then got some charge back.

In these cases it starts over from the voltage.
