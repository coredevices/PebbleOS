# Models

The `FUEL_GAUGE_ESTIMATOR` choice selects the model the SOC is estimated with.
Its header, included by `fuel_gauge.h`, defines its parameters
(`struct pbl_fuel_gauge_estimator`) and its state
(`struct pbl_fuel_gauge_estimator_state`).

| Model | Kconfig | Description |
|---|---|---|
| [Thevenin](thevenin.md) | `FUEL_GAUGE_THEVENIN` | $R_0$ and one RC pair, extended Kalman filter |

```{toctree}
:maxdepth: 1

thevenin.md
```
